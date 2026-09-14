#include "profile_updater.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>
#include <string>
#include <vector>

#include "coach_priority_engine.h"
#include "hypothesis_manager.h"
#include "pattern_matcher.h"
#include "trend_engine.h"

namespace kchess::ai {
namespace {

double clamp01(const double value) { return std::clamp(value, 0.0, 1.0); }

double sample_confidence(const int samples, const int full_sample) {
  if (samples <= 0) return 0.0;
  return clamp01(static_cast<double>(samples) / static_cast<double>(full_sample));
}

double saturating_confidence(const int samples, const double scale) {
  if (samples <= 0 || scale <= 0.0) return 0.0;
  return clamp01(1.0 - std::exp(-static_cast<double>(samples) / scale));
}

struct ProfileSummaryMetrics {
  std::optional<double> average_accuracy;
  std::optional<int> estimated_strength_rating;
  double estimated_strength_confidence{0.0};
  int analyzed_games{0};
};

ProfileSummaryMetrics profile_summary_metrics(
    const std::vector<ProfileGameEvidence>& games, const double profile_confidence) {
  double weighted_accuracy_sum = 0.0;
  double accuracy_weight = 0.0;
  std::vector<int> player_ratings;
  double opponent_rating_sum = 0.0;
  double score_sum = 0.0;
  int performance_games = 0;
  int analyzed_games = 0;

  for (const auto& game : games) {
    if (game.classifications.analyzed_moves > 0) ++analyzed_games;
    if (game.accuracy) {
      const double weight = static_cast<double>(
          std::max(1, game.classifications.analyzed_moves));
      weighted_accuracy_sum += std::clamp(*game.accuracy, 0.0, 100.0) * weight;
      accuracy_weight += weight;
    }
    if (game.player_rating && *game.player_rating > 0) {
      player_ratings.push_back(*game.player_rating);
    }
    if (!game.opponent_rating || *game.opponent_rating <= 0) continue;
    double score = -1.0;
    if (game.provider_outcome == "win") score = 1.0;
    else if (game.provider_outcome == "draw") score = 0.5;
    else if (game.provider_outcome == "loss") score = 0.0;
    if (score < 0.0) continue;
    opponent_rating_sum += *game.opponent_rating;
    score_sum += score;
    ++performance_games;
  }

  ProfileSummaryMetrics result;
  result.analyzed_games = analyzed_games;
  if (accuracy_weight > 0.0) {
    result.average_accuracy = weighted_accuracy_sum / accuracy_weight;
  }

  std::optional<double> observed_rating;
  if (!player_ratings.empty()) {
    std::sort(player_ratings.begin(), player_ratings.end());
    const std::size_t mid = player_ratings.size() / 2;
    observed_rating = player_ratings.size() % 2 == 0
        ? 0.5 * (player_ratings[mid - 1] + player_ratings[mid])
        : static_cast<double>(player_ratings[mid]);
  }

  std::optional<double> performance_rating;
  if (performance_games >= 4) {
    const double average_opponent = opponent_rating_sum / performance_games;
    const double score = std::clamp(score_sum / performance_games, 0.08, 0.92);
    const double offset = std::clamp(
        400.0 * std::log10(score / (1.0 - score)), -400.0, 400.0);
    performance_rating = average_opponent + offset;
  }

  if (performance_rating && observed_rating) {
    result.estimated_strength_rating = static_cast<int>(std::lround(std::clamp(
        0.55 * *performance_rating + 0.45 * *observed_rating, 400.0, 3500.0)));
  } else if (performance_rating) {
    result.estimated_strength_rating = static_cast<int>(
        std::lround(std::clamp(*performance_rating, 400.0, 3500.0)));
  } else if (observed_rating) {
    result.estimated_strength_rating = static_cast<int>(
        std::lround(std::clamp(*observed_rating, 400.0, 3500.0)));
  }

  const int strength_samples = std::max(performance_games,
                                        static_cast<int>(player_ratings.size()));
  result.estimated_strength_confidence = clamp01(
      saturating_confidence(strength_samples, 28.0) *
      (0.45 + 0.55 * profile_confidence));
  return result;
}

double calibrated_profile_confidence(
    const std::vector<ProfileGameEvidence>& games,
    const ProfileBackgroundProgress& background) {
  int analyzed_games = 0;
  int analyzed_moves = 0;
  for (const auto& game : games) {
    if (game.classifications.analyzed_moves <= 0) continue;
    ++analyzed_games;
    analyzed_moves += game.classifications.analyzed_moves;
  }

  // Independent games are the strongest confidence signal. Thousands of cheap
  // metadata rows (openings/results/time controls) improve profile context but
  // must never make the Coach look highly certain after only a handful of
  // analytically-backed games.
  const double game_evidence = saturating_confidence(analyzed_games, 90.0);
  const double move_evidence = saturating_confidence(analyzed_moves, 2500.0);
  const double resolved_evidence = saturating_confidence(
      background.resolved_relevant_games, 90.0);
  const double relevant_coverage = background.relevant_games > 0
      ? clamp01(static_cast<double>(background.resolved_relevant_games) /
                static_cast<double>(background.relevant_games))
      : (analyzed_games > 0 ? 1.0 : 0.0);
  const double coverage_evidence = std::sqrt(relevant_coverage);

  double confidence = 0.45 * game_evidence + 0.25 * move_evidence +
                      0.20 * resolved_evidence + 0.10 * coverage_evidence;

  // Until provider history is known to be complete, the learned profile can be
  // useful but cannot claim near-final certainty about the player's habits.
  if (!background.history_complete) confidence = std::min(confidence, 0.65);
  return clamp01(confidence);
}

ProfileSignal known_signal(const double value, const double confidence) {
  return ProfileSignal{.value = clamp01(value), .confidence = clamp01(confidence)};
}

void add_mistake(std::vector<CommonMistake>& output, const char* id,
                 const int count, const int total, const double confidence) {
  if (count <= 0 || total <= 0) return;
  output.push_back(CommonMistake{
      .id = id,
      .occurrences = count,
      .rate = static_cast<double>(count) / static_cast<double>(total),
      .confidence = confidence,
  });
}

std::vector<OpeningPattern> opening_patterns_from_games(
    const std::vector<ProfileGameEvidence>& games) {
  struct Tally {
    std::string eco;
    std::string name;
    int games{0};
    int wins{0};
    int draws{0};
    int losses{0};
  };
  std::map<std::string, Tally> tallies;
  for (const auto& game : games) {
    if (game.opening_eco.empty() && game.opening_name.empty()) continue;
    const std::string key = game.opening_eco + "\n" + game.opening_name;
    auto& tally = tallies[key];
    tally.eco = game.opening_eco;
    tally.name = game.opening_name;
    ++tally.games;
    if (game.provider_outcome == "win") ++tally.wins;
    else if (game.provider_outcome == "draw") ++tally.draws;
    else if (game.provider_outcome == "loss") ++tally.losses;
  }
  std::vector<Tally> ranked;
  ranked.reserve(tallies.size());
  for (auto& [key, tally] : tallies) ranked.push_back(std::move(tally));
  std::sort(ranked.begin(), ranked.end(), [](const Tally& a, const Tally& b) {
    return a.games > b.games;
  });

  std::vector<OpeningPattern> result;
  for (std::size_t i = 0; i < ranked.size() && i < 8; ++i) {
    const auto& opening = ranked[i];
    result.push_back(OpeningPattern{
        .eco = opening.eco,
        .name = opening.name,
        .games = opening.games,
        .wins = opening.wins,
        .draws = opening.draws,
        .losses = opening.losses,
        .confidence = sample_confidence(opening.games, 12),
    });
  }
  return result;
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Aggregated fallback learning
// -----------------------------------------------------------------------------

ChessProfile ChessProfileUpdater::update(
    const ChessProfileObservation& observation,
    const std::optional<ChessProfile>& previous) const {
  ChessProfile profile;
  profile.profile_id = observation.profile_id;
  profile.rating = observation.rating;
  profile.source_games = observation.games;
  profile.source_analyzed_moves = observation.analyzed_moves;
  profile.analyzed_games = observation.analyzed_moves > 0 ? observation.games : 0;
  profile.average_accuracy = observation.average_accuracy;
  profile.estimated_strength_rating = observation.rating;

  if (previous) {
    profile.preferences = previous->preferences;
    profile.risk_tolerance = previous->risk_tolerance;
    profile.positional_strength = previous->positional_strength;
    profile.defensive_strength = previous->defensive_strength;
    profile.patterns = previous->patterns;
    profile.time_control_profiles = previous->time_control_profiles;
    profile.hypotheses = previous->hypotheses;
    profile.coach_priorities = previous->coach_priorities;
    profile.background = previous->background;
    profile.updated_at = previous->updated_at;
  }

  const double move_confidence = sample_confidence(observation.analyzed_moves, 240);
  const double game_confidence = sample_confidence(observation.games, 40);
  profile.confidence = 0.7 * move_confidence + 0.3 * game_confidence;

  if (observation.average_accuracy) {
    const double accuracy = clamp01(*observation.average_accuracy / 100.0);
    const int major_errors = observation.miss + observation.mistake + observation.blunder;
    const double error_rate = observation.analyzed_moves > 0
        ? static_cast<double>(major_errors) / observation.analyzed_moves
        : 0.0;
    profile.calculation_strength = known_signal(
        0.75 * accuracy + 0.25 * (1.0 - clamp01(error_rate * 4.0)), move_confidence);

    const int sharp_moves = observation.brilliant + observation.critical + observation.best;
    const double sharp_rate = observation.analyzed_moves > 0
        ? static_cast<double>(sharp_moves) / observation.analyzed_moves
        : 0.0;
    const double tactical = 0.55 * accuracy + 0.25 * clamp01(sharp_rate * 2.5)
        + 0.20 * (1.0 - clamp01(static_cast<double>(observation.miss) /
                                std::max(1, observation.analyzed_moves) * 5.0));
    profile.tactical_strength = known_signal(tactical, move_confidence * 0.8);
  }

  add_mistake(profile.common_mistakes, "missed_opportunity", observation.miss,
              observation.analyzed_moves, move_confidence);
  add_mistake(profile.common_mistakes, "mistake", observation.mistake,
              observation.analyzed_moves, move_confidence);
  add_mistake(profile.common_mistakes, "blunder", observation.blunder,
              observation.analyzed_moves, move_confidence);
  std::sort(profile.common_mistakes.begin(), profile.common_mistakes.end(),
            [](const CommonMistake& a, const CommonMistake& b) {
              if (a.rate != b.rate) return a.rate > b.rate;
              return a.occurrences > b.occurrences;
            });

  const double major_error_rate = observation.analyzed_moves > 0
      ? static_cast<double>(observation.mistake + observation.blunder) /
            observation.analyzed_moves
      : 0.0;
  const double theory_rate = observation.analyzed_moves > 0
      ? static_cast<double>(observation.theory) / observation.analyzed_moves
      : 0.0;
  if (move_confidence >= 0.25 && major_error_rate <= 0.04) {
    profile.strengths.push_back("move_consistency");
  }
  if (move_confidence >= 0.25 && theory_rate >= 0.16) {
    profile.strengths.push_back("opening_theory");
  }
  if (move_confidence >= 0.25 && observation.blunder > 0 &&
      static_cast<double>(observation.blunder) / observation.analyzed_moves >= 0.035) {
    profile.weaknesses.push_back("blunder_control");
  }
  if (move_confidence >= 0.25 && observation.miss > 0 &&
      static_cast<double>(observation.miss) / observation.analyzed_moves >= 0.06) {
    profile.weaknesses.push_back("missed_opportunities");
  }

  profile.opening_patterns.reserve(std::min<std::size_t>(8, observation.openings.size()));
  for (std::size_t i = 0; i < observation.openings.size() && i < 8; ++i) {
    const auto& opening = observation.openings[i];
    profile.opening_patterns.push_back(OpeningPattern{
        .eco = opening.eco,
        .name = opening.name,
        .games = opening.games,
        .wins = opening.wins,
        .draws = opening.draws,
        .losses = opening.losses,
        .confidence = sample_confidence(opening.games, 12),
    });
  }
  return profile;
}

// -----------------------------------------------------------------------------
// Section: Evidence-driven player model
// -----------------------------------------------------------------------------

ChessProfile ChessProfileUpdater::update_from_evidence(
    const std::string& profile_id,
    const std::optional<int>& rating,
    const std::vector<ProfileGameEvidence>& games,
    const std::int64_t now_seconds,
    const ProfileBackgroundProgress& background,
    const std::optional<ChessProfile>& previous,
    const std::vector<ProfileGameEvidence>& library_games) const {
  ChessProfile profile;
  profile.profile_id = profile_id;
  profile.rating = rating;
  profile.source_games = static_cast<int>(games.size());
  profile.background = background;
  profile.updated_at = now_seconds;
  if (previous) {
    profile.preferences = previous->preferences;
    profile.risk_tolerance = previous->risk_tolerance;
    profile.positional_strength = previous->positional_strength;
    profile.defensive_strength = previous->defensive_strength;
  }

  auto matched = PatternMatcher{}.match(games, now_seconds);
  ProfileTrendEngine{}.apply(
      matched.patterns, matched.time_control_profiles, games, now_seconds);
  profile.patterns = std::move(matched.patterns);
  profile.time_control_profiles = std::move(matched.time_control_profiles);
  profile.common_mistakes = std::move(matched.common_mistakes);
  profile.strengths = std::move(matched.strengths);
  profile.weaknesses = std::move(matched.weaknesses);
  profile.tactical_strength = matched.tactical_strength;
  profile.calculation_strength = matched.calculation_strength;
  profile.endgame_strength = matched.endgame_strength;
  profile.source_analyzed_moves = matched.analyzed_moves;
  profile.confidence = calibrated_profile_confidence(games, background);
  const auto summary = profile_summary_metrics(games, profile.confidence);
  profile.average_accuracy = summary.average_accuracy;
  profile.estimated_strength_rating = summary.estimated_strength_rating;
  profile.estimated_strength_confidence = summary.estimated_strength_confidence;
  profile.analyzed_games = summary.analyzed_games;
  // Repertoire frequency is cheap library metadata, not engine evidence. Keep
  // confidence and ability learning tied to the bounded analyzed sample above.
  profile.opening_patterns = opening_patterns_from_games(
      library_games.empty() ? games : library_games);
  profile.hypotheses = HypothesisManager{}.update(
      profile.patterns, games, now_seconds, previous);
  profile.coach_priorities = CoachPriorityEngine{}.rank(profile.patterns);
  return profile;
}

bool repeated_personal_mistake(
    const ChessProfile& profile, const std::string& classification) {
  std::string id;
  if (classification == "blunder") id = "blunder";
  else if (classification == "mistake") id = "mistake";
  else if (classification == "miss") id = "missed_opportunity";
  else return false;

  const bool aggregate = std::any_of(
      profile.common_mistakes.begin(), profile.common_mistakes.end(),
      [&](const CommonMistake& mistake) {
        return mistake.id == id && mistake.occurrences >= 3 &&
               mistake.rate >= 0.04 && mistake.confidence >= 0.25;
      });
  if (aggregate) return true;

  return std::any_of(profile.patterns.begin(), profile.patterns.end(),
                     [&](const PlayerPattern& pattern) {
    if (pattern.type != "weakness" || pattern.confidence < 0.55) return false;
    if (classification == "blunder") return pattern.id == "blunder_control";
    if (classification == "miss") return pattern.id == "missed_opportunities";
    return pattern.id == "calculation_consistency" || pattern.id == "time_pressure_errors";
  });
}

}  // namespace kchess::ai
