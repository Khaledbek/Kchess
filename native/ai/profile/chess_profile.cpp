#include "chess_profile.h"

#include <nlohmann/json.hpp>

namespace kchess::ai {
namespace {

using json = nlohmann::json;

json signal_json(const ProfileSignal& signal) {
  json value = {{"confidence", signal.confidence}};
  if (signal.value) value["value"] = *signal.value;
  return value;
}

ProfileSignal read_signal(const json& value) {
  ProfileSignal signal;
  if (value.contains("value") && value["value"].is_number()) {
    signal.value = value["value"].get<double>();
  }
  signal.confidence = value.value("confidence", 0.0);
  return signal;
}

json example_json(const ProfileExamplePosition& example) {
  return {
      {"gameId", example.game_id},
      {"ply", example.ply},
      {"fen", example.fen},
      {"move", example.move},
      {"classification", example.classification},
  };
}

ProfileExamplePosition read_example(const json& value) {
  return ProfileExamplePosition{
      .game_id = value.value("gameId", ""),
      .ply = value.value("ply", 0),
      .fen = value.value("fen", ""),
      .move = value.value("move", ""),
      .classification = value.value("classification", ""),
  };
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Stable JSON persistence
// -----------------------------------------------------------------------------

std::string chess_profile_json(const ChessProfile& profile) {
  json value = {
      {"version", 2},
      {"profileId", profile.profile_id},
      {"preferences", profile.preferences},
      {"tacticalStrength", signal_json(profile.tactical_strength)},
      {"calculationStrength", signal_json(profile.calculation_strength)},
      {"positionalStrength", signal_json(profile.positional_strength)},
      {"defensiveStrength", signal_json(profile.defensive_strength)},
      {"endgameStrength", signal_json(profile.endgame_strength)},
      {"riskTolerance", signal_json(profile.risk_tolerance)},
      {"strengths", profile.strengths},
      {"weaknesses", profile.weaknesses},
      {"sourceGames", profile.source_games},
      {"sourceAnalyzedMoves", profile.source_analyzed_moves},
      {"confidence", profile.confidence},
      {"estimatedStrengthConfidence", profile.estimated_strength_confidence},
      {"analyzedGames", profile.analyzed_games},
      {"updatedAt", profile.updated_at},
  };
  if (profile.rating) value["rating"] = *profile.rating;
  if (profile.average_accuracy) value["averageAccuracy"] = *profile.average_accuracy;
  if (profile.estimated_strength_rating) {
    value["estimatedStrengthRating"] = *profile.estimated_strength_rating;
  }

  value["commonMistakes"] = json::array();
  for (const auto& mistake : profile.common_mistakes) {
    value["commonMistakes"].push_back({
        {"id", mistake.id},
        {"occurrences", mistake.occurrences},
        {"rate", mistake.rate},
        {"confidence", mistake.confidence},
    });
  }

  value["openingPatterns"] = json::array();
  for (const auto& opening : profile.opening_patterns) {
    value["openingPatterns"].push_back({
        {"eco", opening.eco},
        {"name", opening.name},
        {"games", opening.games},
        {"wins", opening.wins},
        {"draws", opening.draws},
        {"losses", opening.losses},
        {"confidence", opening.confidence},
    });
  }

  value["patterns"] = json::array();
  for (const auto& pattern : profile.patterns) {
    json item = {
        {"id", pattern.id},
        {"type", pattern.type},
        {"confidence", pattern.confidence},
        {"severity", pattern.severity},
        {"occurrences", pattern.occurrences},
        {"sampleGames", pattern.sample_games},
        {"analyzedMoves", pattern.analyzed_moves},
        {"timeControl", pattern.time_control},
        {"phase", pattern.phase},
        {"recent", pattern.recent},
        {"trend", pattern.trend},
        {"examplePositions", json::array()},
    };
    if (pattern.observed_error_rate) item["observedErrorRate"] = *pattern.observed_error_rate;
    if (pattern.posterior_error_lower) item["posteriorErrorLowerApprox"] = *pattern.posterior_error_lower;
    if (pattern.posterior_error_upper) item["posteriorErrorUpperApprox"] = *pattern.posterior_error_upper;
    for (const auto& example : pattern.example_positions) {
      item["examplePositions"].push_back(example_json(example));
    }
    value["patterns"].push_back(std::move(item));
  }

  value["timeControlProfiles"] = json::array();
  for (const auto& time_control : profile.time_control_profiles) {
    json item = {
        {"id", time_control.id},
        {"games", time_control.games},
        {"analyzedMoves", time_control.analyzed_moves},
        {"majorErrorRate", time_control.major_error_rate},
        {"confidence", time_control.confidence},
        {"trend", time_control.trend},
    };
    if (time_control.average_accuracy) item["averageAccuracy"] = *time_control.average_accuracy;
    value["timeControlProfiles"].push_back(std::move(item));
  }

  value["hypotheses"] = json::array();
  for (const auto& hypothesis : profile.hypotheses) {
    value["hypotheses"].push_back({
        {"id", hypothesis.id},
        {"patternId", hypothesis.pattern_id},
        {"status", hypothesis.status},
        {"confidence", hypothesis.confidence},
        {"evidenceFor", hypothesis.evidence_for},
        {"evidenceAgainst", hypothesis.evidence_against},
        {"updatedAt", hypothesis.updated_at},
    });
  }

  value["coachPriorities"] = json::array();
  for (const auto& priority : profile.coach_priorities) {
    value["coachPriorities"].push_back({
        {"patternId", priority.pattern_id},
        {"role", priority.role},
        {"score", priority.score},
    });
  }

  value["background"] = {
      {"status", profile.background.status},
      {"totalGames", profile.background.total_games},
      {"historyAccounts", profile.background.history_accounts},
      {"historyDiscoveredAccounts", profile.background.history_discovered_accounts},
      {"historyAvailableMonths", profile.background.history_available_months},
      {"historySyncedMonths", profile.background.history_synced_months},
      {"historyPendingMonths", profile.background.history_pending_months},
      {"historyComplete", profile.background.history_complete},
      {"indexedGames", profile.background.indexed_games},
      {"historicalSampleGames", profile.background.historical_sample_games},
      {"historicalSampleBudget", profile.background.historical_sample_budget},
      {"samplingEligibleGames", profile.background.sampling_eligible_games},
      {"samplingExcludedGames", profile.background.sampling_excluded_games},
      {"samplingMinimumGames", profile.background.sampling_minimum_games},
      {"samplingMaximumGames", profile.background.sampling_maximum_games},
      {"samplingRequiredStrata", profile.background.sampling_required_strata},
      {"samplingCoveredStrata", profile.background.sampling_covered_strata},
      {"samplingCoverage", profile.background.sampling_coverage},
      {"samplingDiversity", profile.background.sampling_diversity},
      {"samplingCapped", profile.background.sampling_capped},
      {"initialPreparationComplete", profile.background.initial_preparation_complete},
      {"interestingGames", profile.background.interesting_games},
      {"enginePromotedGames", profile.background.engine_promoted_games},
      {"relevantGames", profile.background.relevant_games},
      {"resolvedRelevantGames", profile.background.resolved_relevant_games},
      {"reusedAnalysisGames", profile.background.reused_analysis_games},
      {"queuedGames", profile.background.queued_games},
      {"enginePendingGames", profile.background.engine_pending_games},
      {"updatedAt", profile.background.updated_at},
  };
  if (profile.background.current_game_id) {
    value["background"]["currentGameId"] = *profile.background.current_game_id;
  }
  return value.dump();
}

std::optional<ChessProfile> chess_profile_from_json(const std::string& payload) {
  try {
    const auto value = json::parse(payload);
    const int version = value.value("version", 0);
    if (version != 1 && version != 2) return std::nullopt;

    ChessProfile profile;
    profile.profile_id = value.value("profileId", "");
    if (profile.profile_id.empty()) return std::nullopt;
    if (value.contains("rating") && value["rating"].is_number_integer()) {
      profile.rating = value["rating"].get<int>();
    }
    if (value.contains("averageAccuracy") && value["averageAccuracy"].is_number()) {
      profile.average_accuracy = value["averageAccuracy"].get<double>();
    }
    if (value.contains("estimatedStrengthRating") &&
        value["estimatedStrengthRating"].is_number_integer()) {
      profile.estimated_strength_rating = value["estimatedStrengthRating"].get<int>();
    }
    profile.estimated_strength_confidence = value.value(
        "estimatedStrengthConfidence", 0.0);
    profile.analyzed_games = value.value("analyzedGames", 0);
    profile.preferences = value.value("preferences", std::vector<std::string>{});
    profile.tactical_strength = read_signal(value.value("tacticalStrength", json::object()));
    profile.calculation_strength = read_signal(value.value("calculationStrength", json::object()));
    profile.positional_strength = read_signal(value.value("positionalStrength", json::object()));
    profile.defensive_strength = read_signal(value.value("defensiveStrength", json::object()));
    profile.endgame_strength = read_signal(value.value("endgameStrength", json::object()));
    profile.risk_tolerance = read_signal(value.value("riskTolerance", json::object()));
    profile.strengths = value.value("strengths", std::vector<std::string>{});
    profile.weaknesses = value.value("weaknesses", std::vector<std::string>{});
    profile.source_games = value.value("sourceGames", 0);
    profile.source_analyzed_moves = value.value("sourceAnalyzedMoves", 0);
    profile.confidence = value.value("confidence", 0.0);
    profile.updated_at = value.value("updatedAt", static_cast<std::int64_t>(0));

    for (const auto& item : value.value("commonMistakes", json::array())) {
      profile.common_mistakes.push_back(CommonMistake{
          .id = item.value("id", ""),
          .occurrences = item.value("occurrences", 0),
          .rate = item.value("rate", 0.0),
          .confidence = item.value("confidence", 0.0),
      });
    }
    for (const auto& item : value.value("openingPatterns", json::array())) {
      profile.opening_patterns.push_back(OpeningPattern{
          .eco = item.value("eco", ""),
          .name = item.value("name", ""),
          .games = item.value("games", 0),
          .wins = item.value("wins", 0),
          .draws = item.value("draws", 0),
          .losses = item.value("losses", 0),
          .confidence = item.value("confidence", 0.0),
      });
    }

    if (version >= 2) {
      for (const auto& item : value.value("patterns", json::array())) {
        PlayerPattern pattern{
            .id = item.value("id", ""),
            .type = item.value("type", ""),
            .confidence = item.value("confidence", 0.0),
            .severity = item.value("severity", 0.0),
            .occurrences = item.value("occurrences", 0),
            .sample_games = item.value("sampleGames", 0),
            .analyzed_moves = item.value("analyzedMoves", 0),
            .observed_error_rate = item.contains("observedErrorRate")
                ? std::optional<double>(item.at("observedErrorRate").get<double>()) : std::nullopt,
            .posterior_error_lower = item.contains("posteriorErrorLowerApprox")
                ? std::optional<double>(item.at("posteriorErrorLowerApprox").get<double>()) : std::nullopt,
            .posterior_error_upper = item.contains("posteriorErrorUpperApprox")
                ? std::optional<double>(item.at("posteriorErrorUpperApprox").get<double>()) : std::nullopt,
            .time_control = item.value("timeControl", "all"),
            .phase = item.value("phase", "all"),
            .recent = item.value("recent", false),
            .trend = item.value("trend", "insufficient_data"),
        };
        for (const auto& example : item.value("examplePositions", json::array())) {
          pattern.example_positions.push_back(read_example(example));
        }
        if (!pattern.id.empty()) profile.patterns.push_back(std::move(pattern));
      }

      for (const auto& item : value.value("timeControlProfiles", json::array())) {
        TimeControlProfile time_control{
            .id = item.value("id", "other"),
            .games = item.value("games", 0),
            .analyzed_moves = item.value("analyzedMoves", 0),
            .major_error_rate = item.value("majorErrorRate", 0.0),
            .confidence = item.value("confidence", 0.0),
            .trend = item.value("trend", "insufficient_data"),
        };
        if (item.contains("averageAccuracy") && item["averageAccuracy"].is_number()) {
          time_control.average_accuracy = item["averageAccuracy"].get<double>();
        }
        profile.time_control_profiles.push_back(std::move(time_control));
      }

      for (const auto& item : value.value("hypotheses", json::array())) {
        profile.hypotheses.push_back(ProfileHypothesis{
            .id = item.value("id", ""),
            .pattern_id = item.value("patternId", ""),
            .status = item.value("status", "tentative"),
            .confidence = item.value("confidence", 0.0),
            .evidence_for = item.value("evidenceFor", 0),
            .evidence_against = item.value("evidenceAgainst", 0),
            .updated_at = item.value("updatedAt", static_cast<std::int64_t>(0)),
        });
      }

      for (const auto& item : value.value("coachPriorities", json::array())) {
        profile.coach_priorities.push_back(CoachPriority{
            .pattern_id = item.value("patternId", ""),
            .role = item.value("role", "secondary"),
            .score = item.value("score", 0.0),
        });
      }

      const auto background = value.value("background", json::object());
      profile.background.status = background.value("status", "idle");
      profile.background.total_games = background.value("totalGames", 0);
      profile.background.history_accounts = background.value("historyAccounts", 0);
      profile.background.history_discovered_accounts = background.value(
          "historyDiscoveredAccounts", 0);
      profile.background.history_available_months = background.value(
          "historyAvailableMonths", 0);
      profile.background.history_synced_months = background.value(
          "historySyncedMonths", 0);
      profile.background.history_pending_months = background.value(
          "historyPendingMonths", 0);
      profile.background.history_complete = background.value(
          "historyComplete", true);
      const int legacy_processed_games = background.value("processedGames", 0);
      profile.background.indexed_games = background.value(
          "indexedGames", legacy_processed_games);
      profile.background.historical_sample_games = background.value(
          "historicalSampleGames", 0);
      profile.background.historical_sample_budget = background.value(
          "historicalSampleBudget", profile.background.historical_sample_games);
      profile.background.sampling_eligible_games = background.value(
          "samplingEligibleGames", 0);
      profile.background.sampling_excluded_games = background.value(
          "samplingExcludedGames", 0);
      profile.background.sampling_minimum_games = background.value(
          "samplingMinimumGames", 0);
      profile.background.sampling_maximum_games = background.value(
          "samplingMaximumGames", 0);
      profile.background.sampling_required_strata = background.value(
          "samplingRequiredStrata", 0);
      profile.background.sampling_covered_strata = background.value(
          "samplingCoveredStrata", 0);
      profile.background.sampling_coverage = background.value(
          "samplingCoverage", 0.0);
      profile.background.sampling_diversity = background.value(
          "samplingDiversity", 0.0);
      profile.background.sampling_capped = background.value(
          "samplingCapped", false);
      profile.background.initial_preparation_complete = background.value(
          "initialPreparationComplete", false);
      profile.background.interesting_games = background.value(
          "interestingGames", 0);
      profile.background.engine_promoted_games = background.value(
          "enginePromotedGames", 0);
      profile.background.relevant_games = background.value("relevantGames", 0);
      profile.background.resolved_relevant_games = background.value(
          "resolvedRelevantGames", 0);
      profile.background.reused_analysis_games = background.value(
          "reusedAnalysisGames", 0);
      profile.background.queued_games = background.value("queuedGames", 0);
      profile.background.engine_pending_games = background.value("enginePendingGames", 0);
      profile.background.updated_at = background.value(
          "updatedAt", static_cast<std::int64_t>(0));
      if (background.contains("currentGameId") && background["currentGameId"].is_string()) {
        profile.background.current_game_id = background["currentGameId"].get<std::string>();
      }
    }
    return profile;
  } catch (...) {
    return std::nullopt;
  }
}

}  // namespace kchess::ai
