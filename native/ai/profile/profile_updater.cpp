#include "profile_updater.h"

#include <algorithm>
#include <cmath>

namespace kchess::ai {
namespace {

double clamp01(const double value) { return std::clamp(value, 0.0, 1.0); }

double sample_confidence(const int samples, const int full_sample) {
  if (samples <= 0) return 0.0;
  return clamp01(static_cast<double>(samples) / static_cast<double>(full_sample));
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

}  // namespace

// -----------------------------------------------------------------------------
// Section: Deterministic profile learning
// -----------------------------------------------------------------------------

ChessProfile ChessProfileUpdater::update(
    const ChessProfileObservation& observation,
    const std::optional<ChessProfile>& previous) const {
  ChessProfile profile;
  profile.profile_id = observation.profile_id;
  profile.rating = observation.rating;
  profile.source_games = observation.games;
  profile.source_analyzed_moves = observation.analyzed_moves;

  if (previous) {
    profile.preferences = previous->preferences;
    profile.risk_tolerance = previous->risk_tolerance;
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

bool repeated_personal_mistake(
    const ChessProfile& profile, const std::string& classification) {
  std::string id;
  if (classification == "blunder") id = "blunder";
  else if (classification == "mistake") id = "mistake";
  else if (classification == "miss") id = "missed_opportunity";
  else return false;

  return std::any_of(profile.common_mistakes.begin(), profile.common_mistakes.end(),
                     [&](const CommonMistake& mistake) {
    return mistake.id == id && mistake.occurrences >= 3 &&
           mistake.rate >= 0.04 && mistake.confidence >= 0.25;
  });
}

}  // namespace kchess::ai
