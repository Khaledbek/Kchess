#pragma once

// -----------------------------------------------------------------------------
// Section: Accuracy statistics over analysed games (pure; no database or JSON)
// -----------------------------------------------------------------------------

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "analysis/accuracy.h"

namespace kchess::statistics {

// Game phases by full-move number, the same boundaries as the phase card:
// moves 1-12 are the opening, 13-30 the middlegame, 31 onwards the endgame.
enum class GamePhase { opening = 0, middlegame = 1, endgame = 2 };
constexpr int kOpeningLastMove = 12;
constexpr int kMiddlegameLastMove = 30;
GamePhase phase_of_ply(int ply);
const char* phase_name(GamePhase phase);

// One analysed game from the profile's side.
struct AnalysedGame {
  std::int64_t ended_at{0};
  std::string color;         // white | black
  std::string time_control;
  double accuracy{0.0};      // the game's accuracy for the profile's side
  int blunders{0};
  int mistakes{0};
  // The profile's scored moves per phase, and their mistakes plus blunders.
  // Empty when the game was classified before per-move accuracy was stored.
  std::array<std::vector<AccuracySample>, 3> phase_samples;
  std::array<int, 3> phase_errors{};
};

struct PhaseAccuracy {
  std::optional<double> accuracy;  // mean of the per-game phase accuracies
  int games{0};                    // games that reached the phase with data
  double errors_per_game{0.0};     // mistakes plus blunders, per such game
};
std::array<PhaseAccuracy, 3> phase_accuracy(const std::vector<AnalysedGame>& games);

// Is the profile getting better? The latest games against the ones before.
//
// With n analysed games (at least kTrendMinimumGames) the most recent w games,
// w = min(20, n / 2), are compared with the w before them. Accuracy up by at
// least kTrendThreshold points is "improving", down by as much "declining",
// anything in between "steady". Fewer games give "insufficient" and say how
// many more are needed.
constexpr int kTrendMinimumGames = 10;
constexpr int kTrendMaxWindow = 20;
constexpr double kTrendThreshold = 2.0;

struct AccuracyTrend {
  std::string verdict;  // improving | steady | declining | insufficient
  int window{0};
  int games_needed{0};
  std::optional<double> recent_accuracy;
  std::optional<double> previous_accuracy;
  std::optional<double> recent_blunders;    // per game
  std::optional<double> previous_blunders;  // per game
};
AccuracyTrend accuracy_trend(const std::vector<AnalysedGame>& chronological);

// Mean accuracy of each game and the window before it, oldest first; smooths
// the chart line without hiding real change.
std::vector<double> rolling_accuracy(
    const std::vector<AnalysedGame>& chronological, int window);

}  // namespace kchess::statistics
