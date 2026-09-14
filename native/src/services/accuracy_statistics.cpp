// -----------------------------------------------------------------------------
// Section: Accuracy statistics over analysed games
// -----------------------------------------------------------------------------

#include "services/accuracy_statistics.h"

#include <algorithm>

namespace kchess::statistics {

GamePhase phase_of_ply(const int ply) {
  const int move_number = ply / 2 + 1;
  if (move_number <= kOpeningLastMove) return GamePhase::opening;
  if (move_number <= kMiddlegameLastMove) return GamePhase::middlegame;
  return GamePhase::endgame;
}

const char* phase_name(const GamePhase phase) {
  switch (phase) {
    case GamePhase::opening: return "opening";
    case GamePhase::middlegame: return "middlegame";
    case GamePhase::endgame: return "endgame";
  }
  return "opening";
}

std::array<PhaseAccuracy, 3> phase_accuracy(const std::vector<AnalysedGame>& games) {
  std::array<PhaseAccuracy, 3> result{};
  std::array<double, 3> accuracy_totals{};
  std::array<int, 3> accuracy_games{};
  std::array<int, 3> error_totals{};
  for (const auto& game : games) {
    for (std::size_t phase = 0; phase < 3; ++phase) {
      const auto& samples = game.phase_samples[phase];
      if (samples.empty()) continue;
      result[phase].games += 1;
      error_totals[phase] += game.phase_errors[phase];
      // A phase of only theory moves scores 100 by the game formula; it says
      // nothing about the player, so it does not pull the average up.
      const bool only_theory = std::all_of(samples.begin(), samples.end(),
          [](const AccuracySample& sample) { return sample.theory; });
      if (only_theory) continue;
      if (const auto accuracy = aggregate_accuracy(samples)) {
        accuracy_totals[phase] += *accuracy;
        accuracy_games[phase] += 1;
      }
    }
  }
  for (std::size_t phase = 0; phase < 3; ++phase) {
    if (accuracy_games[phase] > 0) {
      result[phase].accuracy = accuracy_totals[phase] / accuracy_games[phase];
    }
    if (result[phase].games > 0) {
      result[phase].errors_per_game =
          static_cast<double>(error_totals[phase]) / result[phase].games;
    }
  }
  return result;
}

AccuracyTrend accuracy_trend(const std::vector<AnalysedGame>& chronological) {
  AccuracyTrend trend;
  const int count = static_cast<int>(chronological.size());
  if (count < kTrendMinimumGames) {
    trend.verdict = "insufficient";
    trend.games_needed = kTrendMinimumGames - count;
    return trend;
  }
  const int window = std::min(kTrendMaxWindow, count / 2);
  trend.window = window;
  const auto mean = [&](int from, int to, bool blunders) {
    double total = 0.0;
    for (int index = from; index < to; ++index) {
      const auto& game = chronological[static_cast<std::size_t>(index)];
      total += blunders ? game.blunders : game.accuracy;
    }
    return total / (to - from);
  };
  trend.recent_accuracy = mean(count - window, count, false);
  trend.previous_accuracy = mean(count - 2 * window, count - window, false);
  trend.recent_blunders = mean(count - window, count, true);
  trend.previous_blunders = mean(count - 2 * window, count - window, true);
  const double delta = *trend.recent_accuracy - *trend.previous_accuracy;
  trend.verdict = delta >= kTrendThreshold
      ? "improving"
      : delta <= -kTrendThreshold ? "declining" : "steady";
  return trend;
}

std::vector<double> rolling_accuracy(
    const std::vector<AnalysedGame>& chronological, const int window) {
  std::vector<double> result;
  result.reserve(chronological.size());
  double total = 0.0;
  for (std::size_t index = 0; index < chronological.size(); ++index) {
    total += chronological[index].accuracy;
    if (window > 0 && index >= static_cast<std::size_t>(window)) {
      total -= chronological[index - static_cast<std::size_t>(window)].accuracy;
    }
    const std::size_t span = window > 0
        ? std::min(index + 1, static_cast<std::size_t>(window))
        : index + 1;
    result.push_back(total / static_cast<double>(span));
  }
  return result;
}

}  // namespace kchess::statistics
