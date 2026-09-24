#include <algorithm>
#include <cmath>
#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "services/statistics_domain.h"
#include "services/statistics_service.h"

namespace kchess {
namespace {
using nlohmann::json;
using namespace statistics;

// -----------------------------------------------------------------------------
// Section: Comparison samples and significance
// -----------------------------------------------------------------------------

Tally read_tally(const json& value) {
  return {value.value("games", 0), value.value("wins", 0),
          value.value("draws", 0), value.value("losses", 0)};
}

bool comparison_edge(const Tally& user, const Tally& opponent) {
  const double user_count = user.decided();
  const double opponent_count = opponent.decided();
  if (user_count < 5 || opponent_count < 5) return false;
  const double gap = user.wins / user_count - opponent.wins / opponent_count;
  if (gap <= 0) return false;
  if (gap >= 0.20) return true;
  const double pooled =
      (user.wins + opponent.wins) / (user_count + opponent_count);
  const double variance =
      pooled * (1 - pooled) * (1 / user_count + 1 / opponent_count);
  return variance > 0 && gap * gap >= 1.645 * 1.645 * variance;
}

json flag_rate(const json& terms, const int losses) {
  if (losses <= 0) return nullptr;
  for (const auto& term : terms) {
    if (term.value("type", "") == "timeout") {
      return static_cast<double>(term.value("losses", 0)) / losses;
    }
  }
  return 0.0;
}

std::string trimmed(std::string value) {
  const auto start = value.find_first_not_of(" \t\r\n");
  if (start == std::string::npos) return {};
  const auto end = value.find_last_not_of(" \t\r\n");
  return value.substr(start, end - start + 1);
}
}  // namespace

// -----------------------------------------------------------------------------
// Section: Recent form and rating series from native library rows
// -----------------------------------------------------------------------------

std::string StatisticsService::timeline_json(
    const std::string& games_json) const {
  const auto games = json::parse(games_json);
  json recent = json::array();
  std::string streak_outcome;
  int streak_length = 0;
  bool streak_finished = false;
  std::map<std::string, std::vector<json>> series;
  for (const auto& game : games) {
    std::string outcome = game.value("statisticsOutcome", "unknown");
    if (outcome == "unknown") {
      outcome = effective_outcome(
          game.value("providerOutcome", std::string{"unknown"}),
          game.value("profileColor", std::string{"unknown"}),
          game.value("result", std::string{}));
    }
    if (outcome != "unknown") {
      if (recent.size() < 15) {
        auto recent_game = game;
        recent_game["statisticsOutcome"] = outcome;
        recent.push_back(std::move(recent_game));
      }
      if (!streak_finished) {
        if (streak_outcome.empty()) streak_outcome = outcome;
        if (outcome == streak_outcome)
          ++streak_length;
        else
          streak_finished = true;
      }
    }
    const std::string color = game.value("profileColor", "unknown");
    if (color != "white" && color != "black") continue;
    const auto& rating =
        game.at(color == "white" ? "whiteRating" : "blackRating");
    const auto ended_at = game.value("endedAt", int64_t{0});
    if (!rating.is_number_integer() || rating.get<int>() <= 0 || ended_at <= 0)
      continue;
    series[game.value("timeControlType", "unknown")].push_back(
        {{"endedAt", ended_at}, {"rating", rating}});
  }

  json rating_series = json::array();
  const auto emit = [&](const std::string control) {
    auto found = series.find(control);
    if (found == series.end()) return;
    auto points = std::move(found->second);
    series.erase(found);
    if (points.size() >= 3) {
      std::vector<int> ratings;
      for (const auto& point : points)
        ratings.push_back(point.at("rating").get<int>());
      std::sort(ratings.begin(), ratings.end());
      const int floor =
          static_cast<int>(std::round(ratings[ratings.size() / 2] * 0.6));
      std::vector<json> cleaned;
      for (const auto& point : points) {
        if (point.at("rating").get<int>() >= floor) cleaned.push_back(point);
      }
      if (cleaned.size() >= 2) points = std::move(cleaned);
    }
    std::stable_sort(points.begin(), points.end(),
                     [](const json& a, const json& b) {
                       return a.at("endedAt").get<int64_t>() <
                              b.at("endedAt").get<int64_t>();
                     });
    if (!points.empty()) {
      rating_series.push_back({{"timeControl", control},
                               {"points", points},
                               {"currentRating", points.back().at("rating")}});
    }
  };
  for (const char* control :
       {"bullet", "blitz", "rapid", "daily", "classical", "correspondence"}) {
    emit(control);
  }
  while (!series.empty()) emit(series.begin()->first);
  json streak = nullptr;
  if (streak_length >= 2 &&
      (streak_outcome == "win" || streak_outcome == "loss")) {
    streak = {{"outcome", streak_outcome}, {"length", streak_length}};
  }
  return json{{"recentGames", recent},
              {"streak", streak},
              {"ratingSeries", rating_series}}
      .dump();
}

// -----------------------------------------------------------------------------
// Section: Local versus public archive comparison
// -----------------------------------------------------------------------------

std::string StatisticsService::comparison_json(
    const std::string& report_json) const {
  auto report = json::parse(report_json);
  const auto profile = database_.active_profile();
  const auto overview = json::parse(overview_json());
  const auto openings = json::parse(openings_json());
  const auto terminations = json::parse(terminations_json());
  const std::string user =
      profile
          ? trimmed(profile->provider_username.value_or(profile->display_name))
          : "";
  const auto& remote = report.at("profile");
  const std::string opponent =
      trimmed(remote.contains("providerUsername") &&
                      remote.at("providerUsername").is_string()
                  ? remote.at("providerUsername").get<std::string>()
                  : remote.value("displayName", ""));
  const bool is_self = profile && profile->type == ProfileType::chess_com &&
                       names_equal(user, opponent);
  Tally h2h;
  if (profile && !is_self && !user.empty() && !opponent.empty()) {
    for (const auto& game : database_.games_for_statistics(profile->id)) {
      const auto white = trimmed(game.white_name);
      const auto black = trimmed(game.black_name);
      if ((names_equal(white, user) && names_equal(black, opponent)) ||
          (names_equal(black, user) && names_equal(white, opponent))) {
        const auto outcome = effective_outcome(
            game.provider_outcome, game_color(user, white, black), game.result);
        if (outcome != "unknown") add_outcome(h2h, outcome);
      }
    }
  }

  std::map<std::string, Tally> opponents;
  for (const auto& opening : report.at("openings")) {
    const std::string eco = opening.value("eco", "");
    if (eco.empty()) continue;
    auto& tally = opponents[eco + '\x1f' + opening.value("color", "unknown")];
    const auto other = read_tally(opening);
    tally.games += other.games;
    tally.wins += other.wins;
    tally.draws += other.draws;
    tally.losses += other.losses;
  }
  json matchups = json::array();
  json recommendations = json::array();
  std::vector<json> weaknesses;
  for (const auto& family : openings.value("families", json::array())) {
    const auto tally = read_tally(family);
    if (tally.games >= 5 && tally.decided() > 0 &&
        static_cast<double>(tally.wins) / tally.decided() < 0.45) {
      weaknesses.push_back(family);
    }
    const std::string color = family.value("color", "unknown");
    if (color != "white" && color != "black") continue;
    const std::string opposite = color == "white" ? "black" : "white";
    const std::string eco = family.value("eco", "");
    const auto found = opponents.find(eco + '\x1f' + opposite);
    if (eco.empty() || found == opponents.end() || tally.games < 5 ||
        found->second.games < 5 || matchups.size() >= 12)
      continue;
    const bool exploitable = !is_self && comparison_edge(tally, found->second);
    json matchup{{"family", family},
                 {"opponent", tally_json(found->second)},
                 {"opponentColor", opposite},
                 {"exploitable", exploitable}};
    matchups.push_back(matchup);
    if (exploitable) recommendations.push_back(std::move(matchup));
  }
  std::stable_sort(
      weaknesses.begin(), weaknesses.end(), [](const json& a, const json& b) {
        return a.at("winRate").get<double>() < b.at("winRate").get<double>();
      });
  if (weaknesses.size() > 5) weaknesses.resize(5);
  const auto own_overall = overview.value("overall", json::object());
  report["comparison"] = {
      {"profileId", profile ? profile->id : ""},
      {"isSelf", is_self},
      {"headToHead", tally_json(h2h)},
      {"userOverview", overview},
      {"userOpenings", openings},
      {"userTerminations", terminations},
      {"userFlagRate",
       flag_rate(terminations.value("terminations", json::array()),
                 own_overall.value("losses", 0))},
      {"opponentFlagRate", flag_rate(report.at("terminations"),
                                     report.at("overall").value("losses", 0))},
      {"matchups", matchups},
      {"recommendations", recommendations},
      {"weaknesses", weaknesses}};
  return report.dump();
}
}  // namespace kchess
