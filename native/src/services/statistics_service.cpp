// -----------------------------------------------------------------------------
// Section: Profile statistics aggregation
// -----------------------------------------------------------------------------

#include "services/statistics_service.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <map>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

#include "services/statistics_domain.h"
#include "services/termination.h"

namespace kchess {
namespace {

using namespace statistics;

// Base opening family: everything before the first ':' or ',' (trimmed).
// "Scandinavian Defense: Mieses-Kotroc Variation" -> "Scandinavian Defense".
std::string opening_family(const std::string& name) {
  const auto pos = name.find_first_of(":,");
  std::string base = pos == std::string::npos ? name : name.substr(0, pos);
  const auto last = base.find_last_not_of(" \t");
  if (last == std::string::npos) return {};
  base.erase(last + 1);
  return base.substr(base.find_first_not_of(" \t"));
}

constexpr int kNemesisMinimumGames = 5;
constexpr double kNemesisMaximumWinRate = 0.45;

}  // namespace

StatisticsService::StatisticsService(Database& database)
    : database_(database) {}

std::string StatisticsService::overview_json() const {
  const auto profile = database_.active_profile();
  if (!profile.has_value()) {
    return nlohmann::json{{"hasProfile", false}, {"totalGames", 0}}.dump();
  }

  const std::string username =
      profile->provider_username.value_or(profile->display_name);
  const auto rows = database_.games_for_statistics(profile->id);

  Tally overall;
  Tally white;
  Tally black;
  std::map<std::string, Tally> by_time_control;
  std::vector<std::string>
      recent_form;  // Most recent first, decided games only.

  for (const auto& row : rows) {
    const std::string color =
        game_color(username, row.white_name, row.black_name);
    const std::string outcome =
        effective_outcome(row.provider_outcome, color, row.result);

    add_outcome(overall, outcome);
    if (color == "white") {
      add_outcome(white, outcome);
    } else if (color == "black") {
      add_outcome(black, outcome);
    }
    add_outcome(by_time_control[row.time_control_type], outcome);

    if (recent_form.size() < 10 &&
        (outcome == "win" || outcome == "loss" || outcome == "draw")) {
      recent_form.push_back(outcome);
    }
  }

  // Emit time controls fastest-to-slowest so the UI reads naturally; only
  // buckets with games appear, and any unexpected type is appended.
  static constexpr std::array<const char*, 7> kOrder{
      "bullet", "blitz",          "rapid",  "classical",
      "daily",  "correspondence", "unknown"};
  nlohmann::json time_controls = nlohmann::json::array();
  std::map<std::string, bool> emitted;
  auto emit = [&](const std::string& type) {
    const auto found = by_time_control.find(type);
    if (found == by_time_control.end() || found->second.games == 0) return;
    nlohmann::json node = tally_json(found->second);
    node["type"] = type;
    time_controls.push_back(std::move(node));
    emitted[type] = true;
  };
  for (const char* type : kOrder) emit(type);
  for (const auto& [type, tally] : by_time_control) {
    if (!emitted[type] && tally.games > 0) {
      nlohmann::json node = tally_json(tally);
      node["type"] = type;
      time_controls.push_back(std::move(node));
    }
  }

  nlohmann::json root{
      {"hasProfile", true},
      {"profileId", profile->id},
      {"totalGames", overall.games},
      {"overall", tally_json(overall)},
      {"byColor", {{"white", tally_json(white)}, {"black", tally_json(black)}}},
      {"byTimeControl", time_controls},
      {"recentForm", recent_form},
  };
  return root.dump();
}

std::string StatisticsService::openings_json(
    const std::string& time_control) const {
  const auto profile = database_.active_profile();
  if (!profile.has_value()) {
    return nlohmann::json{{"hasProfile", false}, {"gamesWithOpening", 0}}
        .dump();
  }

  const std::string username =
      profile->provider_username.value_or(profile->display_name);
  const auto rows = database_.games_for_statistics(profile->id);

  struct Variation {
    std::string eco;
    std::string
        name;  // full opening name, e.g. "Scandinavian Defense: Main Line"
    Tally tally;
  };
  struct Family {
    std::string family;  // base name, e.g. "Scandinavian Defense"
    std::string color;
    Tally tally;
    std::map<std::string, Variation> variations;  // key: full opening name
  };
  std::map<std::string, Family>
      families;  // key: family + unit separator + color
  int games_with_opening = 0;
  int games_without_opening = 0;

  // "all" keeps every game; anything else must match the row's bucket exactly.
  // Filtered-out games are skipped before the with/without-opening counters so
  // the card's own totals describe the filtered set rather than the library.
  const bool filtered = time_control != "all" && !time_control.empty();

  for (const auto& row : rows) {
    if (filtered && row.time_control_type != time_control) continue;
    if (row.opening_name.empty()) {
      games_without_opening += 1;
      continue;
    }
    games_with_opening += 1;
    const std::string color =
        game_color(username, row.white_name, row.black_name);
    const std::string outcome =
        effective_outcome(row.provider_outcome, color, row.result);
    const std::string family_name = opening_family(row.opening_name);

    auto& family = families[family_name + '\x1f' + color];
    if (family.tally.games == 0) {
      family.family = family_name;
      family.color = color;
    }
    add_outcome(family.tally, outcome);

    auto& variation = family.variations[row.opening_name];
    if (variation.tally.games == 0) {
      variation.eco = row.opening_eco;
      variation.name = row.opening_name;
    }
    add_outcome(variation.tally, outcome);
  }

  std::vector<const Family*> ordered;
  ordered.reserve(families.size());
  for (const auto& item : families) ordered.push_back(&item.second);
  std::sort(ordered.begin(), ordered.end(),
            [](const Family* a, const Family* b) {
              if (a->tally.games != b->tally.games)
                return a->tally.games > b->tally.games;
              if (a->family != b->family) return a->family < b->family;
              return a->color < b->color;
            });

  const Family* nemesis = nullptr;
  double nemesis_win_rate = 1.0;
  for (const Family* family : ordered) {
    if (family->tally.games < kNemesisMinimumGames ||
        family->tally.decided() == 0) {
      continue;
    }
    const double win_rate = static_cast<double>(family->tally.wins) /
                            family->tally.decided();
    if (win_rate >= kNemesisMaximumWinRate || win_rate >= nemesis_win_rate) {
      continue;
    }
    nemesis = family;
    nemesis_win_rate = win_rate;
  }

  const auto family_json = [](const Family& family) {
    std::vector<const Variation*> variations;
    variations.reserve(family.variations.size());
    for (const auto& item : family.variations) {
      variations.push_back(&item.second);
    }
    std::sort(variations.begin(), variations.end(),
              [](const Variation* a, const Variation* b) {
                if (a->tally.games != b->tally.games) {
                  return a->tally.games > b->tally.games;
                }
                return a->name < b->name;
              });

    nlohmann::json variation_list = nlohmann::json::array();
    for (const Variation* variation : variations) {
      nlohmann::json node = tally_json(variation->tally);
      node["eco"] = variation->eco;
      node["name"] = variation->name;
      variation_list.push_back(std::move(node));
    }

    nlohmann::json node = tally_json(family.tally);
    node["family"] = family.family;
    node["color"] = family.color;
    node["eco"] = variations.empty() ? std::string{} : variations.front()->eco;
    node["hasDistinctVariations"] =
        variations.size() > 1 ||
        (variations.size() == 1 && variations.front()->name != family.family);
    node["variations"] = std::move(variation_list);
    return node;
  };

  constexpr std::size_t kMaxFamilies = 60;
  nlohmann::json list = nlohmann::json::array();
  for (std::size_t index = 0; index < ordered.size() && index < kMaxFamilies;
       ++index) {
    list.push_back(family_json(*ordered[index]));
  }

  nlohmann::json ranked = nlohmann::json::array();
  std::map<std::string, int> color_counts;
  for (auto& family : list) {
    color_counts[family.at("color").get<std::string>()] +=
        family.at("games").get<int>();
    if (family.at("games").get<int>() >= 3 && !family.at("winRate").is_null()) {
      ranked.push_back(family);
    }
  }
  std::stable_sort(ranked.begin(), ranked.end(),
                   [](const auto& a, const auto& b) {
                     if (a.at("winRate") != b.at("winRate"))
                       return a.at("winRate") > b.at("winRate");
                     return a.at("games") > b.at("games");
                   });
  std::string default_color = "white";
  int most_games = -1;
  for (const char* color : {"white", "black", "unknown"}) {
    if (color_counts[color] > most_games) {
      most_games = color_counts[color];
      default_color = color;
    }
  }

  nlohmann::json root{
      {"hasProfile", true},
      {"gamesWithOpening", games_with_opening},
      {"gamesWithoutOpening", games_without_opening},
      {"distinctFamilies", static_cast<int>(families.size())},
      {"families", list},
      {"bestWinRateFamilies", ranked},
      {"nemesis", nemesis == nullptr ? nlohmann::json(nullptr)
                                      : family_json(*nemesis)},
      {"defaultColor", default_color},
  };
  return root.dump();
}

std::string StatisticsService::terminations_json() const {
  const auto profile = database_.active_profile();
  if (!profile.has_value()) {
    return nlohmann::json{{"hasProfile", false}, {"totalGames", 0}}.dump();
  }

  // Termination reason lives only in the stored PGN, so read full records here
  // (unlike the lean overview/openings queries).
  const std::string username =
      profile->provider_username.value_or(profile->display_name);
  const auto games = database_.games(profile->id);
  std::map<std::string, Tally> tallies;
  for (const auto& game : games) {
    const std::string bucket = termination_bucket(game.pgn, game.result);
    const std::string color =
        game_color(username, game.white_name, game.black_name);
    const std::string outcome =
        effective_outcome(game.provider_outcome, color, game.result);
    add_outcome(tallies[bucket], outcome);
  }

  static constexpr std::array<const char*, 5> kOrder{
      "checkmate", "resignation", "timeout", "draw", "other"};
  nlohmann::json list = nlohmann::json::array();
  for (const char* type : kOrder) {
    const auto found = tallies.find(type);
    if (found == tallies.end() || found->second.games == 0) continue;
    nlohmann::json node = tally_json(found->second);
    node["type"] = type;
    list.push_back(std::move(node));
  }

  nlohmann::json spotlight = nullptr;
  for (const auto& entry : list) {
    if (spotlight.is_null() || entry.at("games") > spotlight.at("games"))
      spotlight = entry;
  }
  if (!spotlight.is_null() && !games.empty()) {
    const int count = spotlight.at("games").get<int>();
    const int loss_percent = static_cast<int>(
        std::round(100.0 * spotlight.at("losses").get<int>() / count));
    spotlight["sharePercent"] =
        static_cast<int>(std::round(100.0 * count / games.size()));
    spotlight["lossPercent"] = loss_percent;
    spotlight["costly"] = loss_percent >= 50;
  }

  nlohmann::json root{
      {"hasProfile", true},
      {"totalGames", static_cast<int>(games.size())},
      {"terminations", list},
      {"spotlight", spotlight},
  };
  return root.dump();
}

std::string StatisticsService::phases_json() const {
  const auto profile = database_.active_profile();
  if (!profile.has_value()) {
    return nlohmann::json{{"hasProfile", false}, {"totalGames", 0}}.dump();
  }

  const std::string username =
      profile->provider_username.value_or(profile->display_name);
  const auto rows = database_.games_for_phases(profile->id);

  // Heuristic phase boundaries by the game's ending full-move number.
  constexpr int kOpeningMax = 12;     // moves 1-12
  constexpr int kMiddlegameMax = 30;  // moves 13-30; 31+ is endgame

  Tally overall;
  Tally opening;
  Tally middlegame;
  Tally endgame;
  int classified = 0;
  for (const auto& row : rows) {
    if (row.max_ply < 0) continue;  // no stored moves (e.g. FEN import)
    const int move_number = (row.max_ply / 2) + 1;
    const std::string color =
        game_color(username, row.white_name, row.black_name);
    const std::string outcome =
        effective_outcome(row.provider_outcome, color, row.result);
    Tally& bucket =
        move_number <= kOpeningMax
            ? opening
            : (move_number <= kMiddlegameMax ? middlegame : endgame);
    add_outcome(bucket, outcome);
    add_outcome(overall, outcome);
    classified += 1;
  }

  auto phase_node = [](const char* phase, const Tally& tally) {
    nlohmann::json node = tally_json(tally);
    node["phase"] = phase;
    return node;
  };
  nlohmann::json phases = nlohmann::json::array();
  phases.push_back(phase_node("opening", opening));
  phases.push_back(phase_node("middlegame", middlegame));
  phases.push_back(phase_node("endgame", endgame));

  nlohmann::json root{
      {"hasProfile", true},
      {"totalGames", static_cast<int>(rows.size())},
      {"classified", classified},
      {"phases", phases},
      {"overall", tally_json(overall)},
  };
  return root.dump();
}

}  // namespace kchess
