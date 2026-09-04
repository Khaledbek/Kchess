#include "services/statistics_service.h"

#include "services/termination.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace kchess {
namespace {

struct Tally {
  int games{0};
  int wins{0};
  int draws{0};
  int losses{0};
  int decided() const { return wins + draws + losses; }
};

void add_outcome(Tally& tally, const std::string& outcome) {
  tally.games += 1;
  if (outcome == "win") {
    tally.wins += 1;
  } else if (outcome == "loss") {
    tally.losses += 1;
  } else if (outcome == "draw") {
    tally.draws += 1;
  }
}

nlohmann::json tally_json(const Tally& tally) {
  nlohmann::json node{
      {"games", tally.games},
      {"wins", tally.wins},
      {"draws", tally.draws},
      {"losses", tally.losses},
      {"undecided", tally.games - tally.decided()},
  };
  const int decided = tally.decided();
  if (decided > 0) {
    node["winRate"] = static_cast<double>(tally.wins) / decided;
    node["scorePercent"] = (static_cast<double>(tally.wins) + 0.5 * tally.draws) / decided;
  } else {
    node["winRate"] = nullptr;
    node["scorePercent"] = nullptr;
  }
  return node;
}

// Case-insensitive equality; provider game player names and the stored profile
// handle can differ only in case.
bool names_equal(const std::string& a, const std::string& b) {
  if (a.empty() || a.size() != b.size()) return false;
  for (std::size_t index = 0; index < a.size(); ++index) {
    if (std::tolower(static_cast<unsigned char>(a[index]))
        != std::tolower(static_cast<unsigned char>(b[index]))) {
      return false;
    }
  }
  return true;
}

// Which side the profile played: "white", "black", or "unknown".
std::string game_color(
    const std::string& username,
    const std::string& white_name,
    const std::string& black_name) {
  if (username.empty()) return "unknown";
  if (names_equal(white_name, username)) return "white";
  if (names_equal(black_name, username)) return "black";
  return "unknown";
}

// Prefer the authoritative provider outcome; otherwise derive it from the game
// result and the profile's color (covers local imports where the profile's
// username matches a player). Returns "unknown" when it cannot be determined.
std::string effective_outcome(
    const std::string& provider_outcome,
    const std::string& color,
    const std::string& result) {
  if (provider_outcome == "win" || provider_outcome == "loss"
      || provider_outcome == "draw") {
    return provider_outcome;
  }
  if (color != "white" && color != "black") return "unknown";
  if (result == "1/2-1/2") return "draw";
  if (result == "1-0") return color == "white" ? "win" : "loss";
  if (result == "0-1") return color == "white" ? "loss" : "win";
  return "unknown";
}

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

}  // namespace

StatisticsService::StatisticsService(Database& database) : database_(database) {}

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
  std::vector<std::string> recent_form;  // Most recent first, decided games only.

  for (const auto& row : rows) {
    const std::string color = game_color(username, row.white_name, row.black_name);
    const std::string outcome = effective_outcome(row.provider_outcome, color, row.result);

    add_outcome(overall, outcome);
    if (color == "white") {
      add_outcome(white, outcome);
    } else if (color == "black") {
      add_outcome(black, outcome);
    }
    add_outcome(by_time_control[row.time_control_type], outcome);

    if (recent_form.size() < 10
        && (outcome == "win" || outcome == "loss" || outcome == "draw")) {
      recent_form.push_back(outcome);
    }
  }

  // Emit time controls fastest-to-slowest so the UI reads naturally; only
  // buckets with games appear, and any unexpected type is appended.
  static constexpr std::array<const char*, 7> kOrder{
      "bullet", "blitz", "rapid", "classical", "daily", "correspondence", "unknown"};
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

std::string StatisticsService::openings_json() const {
  const auto profile = database_.active_profile();
  if (!profile.has_value()) {
    return nlohmann::json{{"hasProfile", false}, {"gamesWithOpening", 0}}.dump();
  }

  const std::string username =
      profile->provider_username.value_or(profile->display_name);
  const auto rows = database_.games_for_statistics(profile->id);

  struct Variation {
    std::string eco;
    std::string name;  // full opening name, e.g. "Scandinavian Defense: Main Line"
    Tally tally;
  };
  struct Family {
    std::string family;  // base name, e.g. "Scandinavian Defense"
    std::string color;
    Tally tally;
    std::map<std::string, Variation> variations;  // key: full opening name
  };
  std::map<std::string, Family> families;  // key: family + unit separator + color
  int games_with_opening = 0;
  int games_without_opening = 0;

  for (const auto& row : rows) {
    if (row.opening_name.empty()) {
      games_without_opening += 1;
      continue;
    }
    games_with_opening += 1;
    const std::string color = game_color(username, row.white_name, row.black_name);
    const std::string outcome = effective_outcome(row.provider_outcome, color, row.result);
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
  std::sort(ordered.begin(), ordered.end(), [](const Family* a, const Family* b) {
    if (a->tally.games != b->tally.games) return a->tally.games > b->tally.games;
    if (a->family != b->family) return a->family < b->family;
    return a->color < b->color;
  });

  constexpr std::size_t kMaxFamilies = 60;
  nlohmann::json list = nlohmann::json::array();
  for (std::size_t index = 0; index < ordered.size() && index < kMaxFamilies; ++index) {
    const Family& family = *ordered[index];
    // Variations, most played first; the family's base ECO follows the dominant
    // line so the collapsed header shows a representative code.
    std::vector<const Variation*> variations;
    variations.reserve(family.variations.size());
    for (const auto& item : family.variations) variations.push_back(&item.second);
    std::sort(variations.begin(), variations.end(),
              [](const Variation* a, const Variation* b) {
                if (a->tally.games != b->tally.games) return a->tally.games > b->tally.games;
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
    node["variations"] = std::move(variation_list);
    list.push_back(std::move(node));
  }

  nlohmann::json root{
      {"hasProfile", true},
      {"gamesWithOpening", games_with_opening},
      {"gamesWithoutOpening", games_without_opening},
      {"distinctFamilies", static_cast<int>(families.size())},
      {"families", list},
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
  const auto games = database_.games(profile->id);
  std::map<std::string, int> counts;
  for (const auto& game : games) {
    counts[termination_bucket(game.pgn, game.result)] += 1;
  }

  static constexpr std::array<const char*, 5> kOrder{
      "checkmate", "resignation", "timeout", "draw", "other"};
  nlohmann::json list = nlohmann::json::array();
  for (const char* type : kOrder) {
    const auto found = counts.find(type);
    if (found == counts.end() || found->second == 0) continue;
    list.push_back({{"type", type}, {"count", found->second}});
  }

  nlohmann::json root{
      {"hasProfile", true},
      {"totalGames", static_cast<int>(games.size())},
      {"terminations", list},
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

  Tally opening;
  Tally middlegame;
  Tally endgame;
  int classified = 0;
  for (const auto& row : rows) {
    if (row.max_ply < 0) continue;  // no stored moves (e.g. FEN import)
    const int move_number = (row.max_ply / 2) + 1;
    const std::string color = game_color(username, row.white_name, row.black_name);
    const std::string outcome = effective_outcome(row.provider_outcome, color, row.result);
    Tally& bucket = move_number <= kOpeningMax
        ? opening
        : (move_number <= kMiddlegameMax ? middlegame : endgame);
    add_outcome(bucket, outcome);
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
  };
  return root.dump();
}

}  // namespace kchess
