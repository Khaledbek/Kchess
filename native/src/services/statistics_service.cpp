// -----------------------------------------------------------------------------
// Section: Profile statistics aggregation
// -----------------------------------------------------------------------------

#include "services/statistics_service.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <vector>

#include "services/accuracy_statistics.h"
#include "services/opening_weakness.h"
#include "services/statistics_domain.h"
#include "services/termination.h"

namespace kchess {
namespace {

using namespace statistics;

constexpr int kNemesisMinimumGames = 5;
constexpr double kNemesisMaximumWinRate = 0.45;
constexpr std::size_t kMaxWeaknesses = 6;
constexpr std::size_t kPlayerKnowledgeCacheMaximumEntries = 64;

std::string position_key(const std::string& fen) {
  std::string key;
  int fields = 0;
  for (const char character : fen) {
    if (character == ' ' && ++fields == 4) break;
    key += character;
  }
  return key;
}

nlohmann::json weakness_json(const OpeningWeakness& weakness) {
  nlohmann::json node = tally_json(weakness.tally);
  node["level"] = weakness.level;
  node["name"] = weakness.name;
  node["family"] = weakness.family;
  node["eco"] = weakness.eco;
  node["color"] = weakness.color;
  node["analysedGames"] = weakness.analysed_games;
  node["gamesWithOpeningErrors"] = weakness.games_with_errors;
  node["openingErrors"] = weakness.errors;
  node["openingBlunders"] = weakness.blunders;
  node["poorResults"] = weakness.poor_results;
  node["frequentErrors"] = weakness.frequent_errors;
  node["severity"] = weakness.severity;
  if (weakness.recurring) {
    const auto& mistake = *weakness.recurring;
    node["recurringMistake"] = {
        {"fen", mistake.position}, {"san", mistake.san},
        {"recommended", mistake.recommended.empty()
            ? nlohmann::json(nullptr) : nlohmann::json(mistake.recommended)},
        {"category", mistake.category}, {"moveNumber", mistake.move_number},
        {"side", mistake.side}, {"count", mistake.count}};
  } else {
    node["recurringMistake"] = nullptr;
  }
  return node;
}

void append_cache_key_field(std::string& key, const std::string_view value) {
  key.append(std::to_string(value.size()));
  key.push_back(':');
  key.append(value);
  key.push_back('|');
}

std::string player_knowledge_cache_key(
    const std::string& owner, const std::vector<std::string>& time_controls,
    const std::vector<std::string>& player_colors, const std::int64_t since) {
  std::string key;
  key.reserve(owner.size() + 64 + (time_controls.size() + player_colors.size()) * 12);
  append_cache_key_field(key, owner);
  append_cache_key_field(key, std::to_string(since));
  append_cache_key_field(key, std::to_string(time_controls.size()));
  for (const auto& value : time_controls) append_cache_key_field(key, value);
  append_cache_key_field(key, std::to_string(player_colors.size()));
  for (const auto& value : player_colors) append_cache_key_field(key, value);
  return key;
}

}  // namespace

StatisticsService::StatisticsService(Database& database)
    : database_(database) {}

// -----------------------------------------------------------------------------
// Section: Shared-player knowledge read model
// -----------------------------------------------------------------------------

std::string StatisticsService::player_knowledge_json(
    const std::string& profile_id, const std::vector<std::string>& time_controls,
    const std::vector<std::string>& player_colors, const std::int64_t since) const {
  using Json = nlohmann::json;
  const auto started = std::chrono::steady_clock::now();
  player_knowledge_requests_.fetch_add(1, std::memory_order_relaxed);
  const auto finish_metrics = [&](const std::size_t rows_scanned) {
    const auto elapsed = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started)
            .count());
    player_knowledge_rows_scanned_.fetch_add(rows_scanned, std::memory_order_relaxed);
    player_knowledge_total_ms_.fetch_add(elapsed, std::memory_order_relaxed);
    player_knowledge_last_ms_.store(elapsed, std::memory_order_relaxed);
  };
  const auto owner = database_.player_profile_owner_id(profile_id);
  if (owner.empty() || !database_.profile(owner)) {
    finish_metrics(0);
    return Json::array().dump();
  }

  const auto source_revision = database_.statistics_source_revision();
  const auto cache_key =
      player_knowledge_cache_key(owner, time_controls, player_colors, since);
  {
    std::lock_guard lock(player_knowledge_cache_mutex_);
    if (player_knowledge_cache_revision_ != source_revision) {
      player_knowledge_cache_.clear();
      player_knowledge_cache_revision_ = source_revision;
    }
    if (const auto cached = player_knowledge_cache_.find(cache_key);
        cached != player_knowledge_cache_.end()) {
      player_knowledge_cache_hits_.fetch_add(1, std::memory_order_relaxed);
      finish_metrics(0);
      return cached->second;
    }
  }
  player_knowledge_cache_misses_.fetch_add(1, std::memory_order_relaxed);
  const auto rows = database_.player_profile_game_sources(owner);
  struct Aggregate {
    Tally results;
    int analyzed_games{0}, moves{0}, errors{0}, accuracy_games{0};
    double accuracy_sum{0.0};
    std::int64_t first{0}, last{0};
    void add(const PlayerProfileGameSourceRow& row, int analyzed, int major_errors) {
      add_outcome(results, effective_outcome(row.provider_outcome, row.player_color, row.result));
      if (analyzed > 0) ++analyzed_games;
      moves += analyzed;
      errors += major_errors;
      if (row.played_at > 0) {
        first = first == 0 ? row.played_at : std::min(first, row.played_at);
        last = std::max(last, row.played_at);
      }
    }
    Json data() const {
      auto out = tally_json(results);
      out["analyzedGames"] = analyzed_games;
      out["analyzedMoves"] = moves;
      // A zero error count without analyzed moves is not an ability measure.
      if (moves > 0) {
        out["majorErrors"] = errors;
        out["majorErrorRate"] = static_cast<double>(errors) / moves;
      }
      if (accuracy_games > 0) {
        out["averageAccuracy"] = accuracy_sum / accuracy_games;
        out["accuracyGames"] = accuracy_games;
      }
      out["firstPlayedAt"] = first;
      out["lastPlayedAt"] = last;
      return out;
    }
  };
  Json facts = Json::array();
  const auto controls = time_controls.empty() ? std::vector<std::string>{"all"} : time_controls;
  const auto colors = player_colors.empty() ? std::vector<std::string>{"all"} : player_colors;
  for (const auto& tc : controls) for (const auto& color : colors) {
    Aggregate total;
    std::map<std::pair<std::string, std::string>, Aggregate> openings;
    std::map<std::string, Aggregate> phases;
    std::map<std::string, Aggregate> terminations;
    std::map<std::pair<std::string, std::string>, Aggregate> splits;
    std::map<std::pair<std::string, std::string>, const PlayerProfileGameSourceRow*> ratings;
    int named_games = 0;
    for (const auto& row : rows) {
      if ((tc != "all" && row.time_control_type != tc) ||
          (color != "all" && row.player_color != color) || row.played_at < since) continue;
      total.add(row, row.analyzed_moves, row.miss + row.mistake + row.blunder);
      const auto errors = row.miss + row.mistake + row.blunder;
      if (tc == "all" && color == "all") {
        splits[{row.time_control_type, "all"}].add(row, row.analyzed_moves, errors);
        if (row.player_color == "white" || row.player_color == "black") {
          splits[{"all", row.player_color}].add(row, row.analyzed_moves, errors);
          splits[{row.time_control_type, row.player_color}].add(row, row.analyzed_moves, errors);
        }
      }
      terminations[row.termination_type].add(row, row.analyzed_moves, errors);
      if (row.accuracy) { total.accuracy_sum += *row.accuracy; ++total.accuracy_games; }
      if (!row.opening_name.empty() || !row.opening_eco.empty()) {
        ++named_games;
        openings[{row.opening_eco, row.opening_name}].add(row,
            row.opening_analyzed_moves, row.opening_miss + row.opening_mistake + row.opening_blunder);
      }
      const auto phase = [&](const char* name, int moves, int errors) {
        if (moves > 0) phases[name].add(row, moves, errors);
      };
      phase("opening", row.opening_analyzed_moves,
            row.opening_miss + row.opening_mistake + row.opening_blunder);
      phase("middlegame", row.middlegame_analyzed_moves,
            row.middlegame_miss + row.middlegame_mistake + row.middlegame_blunder);
      phase("endgame", row.endgame_analyzed_moves,
            row.endgame_miss + row.endgame_mistake + row.endgame_blunder);
      if (row.player_rating && *row.player_rating > 0) {
        auto& latest = ratings[{row.time_control_type, row.source_profile_id}];
        if (!latest || row.played_at > latest->played_at ||
            (row.played_at == latest->played_at && row.game_id > latest->game_id)) latest = &row;
      }
    }
    if (total.results.games == 0) continue;
    const std::string base = "library:" + tc + ":" + color + ":" + std::to_string(since) + ":";
    const auto append = [&](const std::string& key, const std::string& topic, Json data) {
      data["topic"] = topic;
      data["profile_id"] = owner;
      if (tc != "all") data["time_control"] = tc;
      if (color != "all") data["player_color"] = color;
      data["temporal_scope"] = since > 0 ? "recent" : "lifetime";
      if (since > 0) data["periodStart"] = since;
      data["statistic_key"] = base + key;
      facts.push_back(std::move(data));
    };
    auto overview = total.data();
    overview["namedOpeningGames"] = named_games;
    overview["unknownOpeningGames"] = total.results.games - named_games;
    append("overview", "statistics", std::move(overview));
    for (const auto& [axes, tally] : splits) {
      auto data = tally.data();
      if (axes.first != "all") data["time_control"] = axes.first;
      if (axes.second != "all") data["player_color"] = axes.second;
      append("split:" + axes.first + ":" + axes.second, "statistics", std::move(data));
    }
    for (const auto& [type, tally] : terminations) {
      auto data = tally.data();
      data["termination_type"] = type;
      append("termination:" + type, "termination", std::move(data));
    }
    for (const auto& [key, tally] : openings) {
      auto data = tally.data();
      data["opening_eco"] = key.first;
      data["opening_name"] = key.second;
      data["opening_family"] = opening_family(key.second);
      data["phase"] = "opening";
      data["libraryGames"] = total.results.games;
      data["namedOpeningGames"] = named_games;
      data["resultMeaning"] = "association_not_opening_causation";
      append("opening:" + key.first + ":" + key.second, "opening", std::move(data));
    }
    for (const auto& [name, tally] : phases) {
      auto data = tally.data();
      data["phase"] = name;
      data["phaseDefinition"] = name == "opening" ? "moves_1_to_12" :
          name == "middlegame" ? "moves_13_to_30" : "moves_31_onward";
      data["endgameMaterialTypeClassified"] = false;
      data["resultMeaning"] = "association_not_phase_causation";
      append("phase:" + name, name, std::move(data));
    }
    for (const auto& [rating_scope, latest] : ratings) {
      const auto& [control, account_id] = rating_scope;
      Json data{{"time_control", control}, {"latestRecordedGameRating", *latest->player_rating},
                {"playedAt", latest->played_at}, {"gameId", latest->game_id},
                {"ratingMeaning", "recorded_game_not_live_or_fide"}};
      if (const auto account = database_.profile(account_id)) {
        data["sourceAccount"] = account->provider_username.value_or(account->display_name);
        data["sourceAccountId"] = account->id;
        data["sourceProvider"] = account->type == ProfileType::chess_com ? "chess.com" :
            account->type == ProfileType::lichess ? "lichess" : "local";
      }
      append("rating:" + control + ":" + account_id, "rating", std::move(data));
    }
  }
  auto payload = facts.dump();
  // Cache only a stable read. If games or completed/classified analysis changed
  // while this aggregation was running, the result remains valid for this
  // caller but is deliberately not reused by a later Coach question.
  if (database_.statistics_source_revision() == source_revision) {
    std::lock_guard lock(player_knowledge_cache_mutex_);
    if (player_knowledge_cache_revision_ == source_revision) {
      if (player_knowledge_cache_.size() >= kPlayerKnowledgeCacheMaximumEntries) {
        player_knowledge_cache_.clear();
      }
      player_knowledge_cache_[cache_key] = payload;
    }
  }
  finish_metrics(rows.size());
  return payload;
}

std::string StatisticsService::performance_diagnostics_json() const {
  const auto requests = player_knowledge_requests_.load(std::memory_order_relaxed);
  const auto total_ms = player_knowledge_total_ms_.load(std::memory_order_relaxed);
  return nlohmann::json({
      {"schema", "statistics.performance.v1"},
      {"playerKnowledge", {
          {"requests", requests},
          {"rowsScanned", player_knowledge_rows_scanned_.load(std::memory_order_relaxed)},
          {"lastDurationMs", player_knowledge_last_ms_.load(std::memory_order_relaxed)},
          {"totalDurationMs", total_ms},
          {"averageDurationMs",
           requests > 0 ? static_cast<double>(total_ms) / requests : 0.0},
          {"cacheEnabled", true},
          {"cacheHits", player_knowledge_cache_hits_.load(std::memory_order_relaxed)},
          {"cacheMisses", player_knowledge_cache_misses_.load(std::memory_order_relaxed)},
          {"sourceRevision", database_.statistics_source_revision()},
      }},
  }).dump();
}

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

  // The profile's own flagged opening moves, grouped by game. The query is
  // bounded by the widest opening phase; each game is cut to its own below.
  const auto move_errors = database_.move_errors_for_statistics(
      profile->id, kOpeningPhaseMaxPly);
  std::map<std::string, std::vector<const GameMoveErrorRow*>> errors_by_game;
  for (const auto& error : move_errors) {
    errors_by_game[error.game_id].push_back(&error);
  }
  std::vector<OpeningGameEvidence> evidence;
  int analysed_opening_games = 0;

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

    if (row.analysed) analysed_opening_games += 1;
    OpeningGameEvidence game{.eco = row.opening_eco,
                             .name = row.opening_name,
                             .color = color,
                             .outcome = outcome,
                             .analysed = row.analysed};
    const auto found = errors_by_game.find(row.game_id);
    if (found != errors_by_game.end() && (color == "white" || color == "black")) {
      const int end_ply = opening_phase_end_ply(row.opening_ply);
      const int own_parity = color == "white" ? 0 : 1;
      for (const auto* error : found->second) {
        if (error->ply >= end_ply || error->ply % 2 != own_parity) continue;
        game.errors.push_back({.ply = error->ply,
                               .category = error->category,
                               .san = error->san,
                               .position = position_key(error->fen_before),
                               .recommended = error->recommended_move});
      }
    }
    evidence.push_back(std::move(game));
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

  // Lines the profile keeps losing or keeps misplaying, for the statistics
  // warning and the training tab's weak spots.
  nlohmann::json weaknesses = nlohmann::json::array();
  for (const auto& weakness : find_opening_weaknesses(evidence, kMaxWeaknesses)) {
    weaknesses.push_back(weakness_json(weakness));
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
      {"weaknesses", weaknesses},
      {"analysedOpeningGames", analysed_opening_games},
      {"defaultColor", default_color},
  };
  return root.dump();
}

std::string StatisticsService::accuracy_json(const std::string& time_control) const {
  const auto profile = database_.active_profile();
  if (!profile.has_value()) {
    return nlohmann::json{{"hasProfile", false}, {"analysedGames", 0}}.dump();
  }
  const std::string username =
      profile->provider_username.value_or(profile->display_name);
  const bool filtered = time_control != "all" && !time_control.empty();

  const auto move_rows = database_.accuracy_moves_for_statistics(profile->id);
  std::map<std::string, std::vector<const AccuracyMoveRow*>> moves_by_game;
  for (const auto& row : move_rows) moves_by_game[row.game_id].push_back(&row);

  std::vector<AnalysedGame> games;
  for (const auto& row : database_.accuracy_games_for_statistics(profile->id)) {
    if (filtered && row.time_control_type != time_control) continue;
    const auto color = game_color(username, row.white_name, row.black_name);
    if (color != "white" && color != "black") continue;
    const auto accuracy = color == "white" ? row.white_accuracy : row.black_accuracy;
    if (!accuracy.has_value()) continue;

    AnalysedGame game{.ended_at = row.ended_at,
                      .color = color,
                      .time_control = row.time_control_type,
                      .accuracy = *accuracy};
    const int own_parity = color == "white" ? 0 : 1;
    if (const auto found = moves_by_game.find(row.game_id); found != moves_by_game.end()) {
      for (const auto* move : found->second) {
        if (move->ply % 2 != own_parity) continue;
        const auto phase = static_cast<std::size_t>(phase_of_ply(move->ply));
        const bool error = move->category == "mistake" || move->category == "blunder";
        if (move->category == "blunder") game.blunders += 1;
        if (move->category == "mistake") game.mistakes += 1;
        if (error) game.phase_errors[phase] += 1;
        // A NULL weight is a move classified before per-move accuracy was
        // stored; background analysis fills it in. Until then the game has no
        // phase breakdown rather than a partial one.
        if (!move->weight.has_value()) continue;
        game.phase_samples[phase].push_back(
            {.theory = move->theory, .accuracy = move->accuracy, .weight = *move->weight});
      }
    }
    games.push_back(std::move(game));
  }

  const auto average = [](const std::vector<const AnalysedGame*>& subset) {
    if (subset.empty()) return nlohmann::json(nullptr);
    double total = 0.0;
    for (const auto* game : subset) total += game->accuracy;
    return nlohmann::json(total / static_cast<double>(subset.size()));
  };
  const auto group = [&](const std::vector<const AnalysedGame*>& subset) {
    return nlohmann::json{{"games", static_cast<int>(subset.size())},
                          {"accuracy", average(subset)}};
  };

  std::vector<const AnalysedGame*> all, white, black;
  std::map<std::string, std::vector<const AnalysedGame*>> by_control;
  int blunders = 0;
  for (const auto& game : games) {
    all.push_back(&game);
    (game.color == "white" ? white : black).push_back(&game);
    by_control[game.time_control].push_back(&game);
    blunders += game.blunders;
  }

  nlohmann::json controls = nlohmann::json::array();
  for (const auto& [control, subset] : by_control) {
    auto node = group(subset);
    node["timeControl"] = control;
    controls.push_back(std::move(node));
  }
  std::stable_sort(controls.begin(), controls.end(), [](const auto& a, const auto& b) {
    return a.at("games").template get<int>() > b.at("games").template get<int>();
  });

  nlohmann::json phases = nlohmann::json::array();
  const auto per_phase = phase_accuracy(games);
  for (std::size_t index = 0; index < per_phase.size(); ++index) {
    const auto& phase = per_phase[index];
    phases.push_back({{"phase", phase_name(static_cast<GamePhase>(index))},
                      {"games", phase.games},
                      {"accuracy", phase.accuracy.has_value()
                                       ? nlohmann::json(*phase.accuracy)
                                       : nlohmann::json(nullptr)},
                      {"errorsPerGame", phase.errors_per_game}});
  }

  // The chart: every analysed game with a rolling average, capped to the
  // latest stretch so a large library stays a readable line.
  constexpr std::size_t kTimelinePoints = 150;
  constexpr int kRollingWindow = 10;
  const auto rolling = rolling_accuracy(games, kRollingWindow);
  nlohmann::json timeline = nlohmann::json::array();
  const std::size_t first =
      games.size() > kTimelinePoints ? games.size() - kTimelinePoints : 0;
  for (std::size_t index = first; index < games.size(); ++index) {
    timeline.push_back({{"endedAt", games[index].ended_at},
                        {"accuracy", games[index].accuracy},
                        {"average", rolling[index]}});
  }

  const auto trend = accuracy_trend(games);
  const auto optional_number = [](const std::optional<double>& value) {
    return value.has_value() ? nlohmann::json(*value) : nlohmann::json(nullptr);
  };

  nlohmann::json root{
      {"hasProfile", true},
      {"analysedGames", static_cast<int>(games.size())},
      {"averageAccuracy", average(all)},
      {"blundersPerGame", games.empty()
                              ? nlohmann::json(nullptr)
                              : nlohmann::json(static_cast<double>(blunders) / games.size())},
      {"byColor", {{"white", group(white)}, {"black", group(black)}}},
      {"byTimeControl", controls},
      {"byPhase", phases},
      {"timeline", timeline},
      {"trend",
       {{"verdict", trend.verdict},
        {"window", trend.window},
        {"gamesNeeded", trend.games_needed},
        {"recentAccuracy", optional_number(trend.recent_accuracy)},
        {"previousAccuracy", optional_number(trend.previous_accuracy)},
        {"recentBlunders", optional_number(trend.recent_blunders)},
        {"previousBlunders", optional_number(trend.previous_blunders)}}},
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
