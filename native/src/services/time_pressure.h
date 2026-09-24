#pragma once

// -----------------------------------------------------------------------------
// Section: Play under time pressure (pure; no database, PGN reader or JSON)
// -----------------------------------------------------------------------------

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "analysis/accuracy.h"

namespace kchess::statistics {

// What a PGN TimeControl tag grants at the start of a game: "180+2" is three
// minutes plus two seconds a move, "600" ten minutes, "40/7200:1800" a
// tournament control. Unknown or untimed ("-", "?") leaves the base empty and
// the game out of the clock statistics, because a share of an unknown budget
// means nothing.
struct TimeControlBudget {
  std::optional<std::int64_t> base_ms;
  std::int64_t increment_ms{0};
};
TimeControlBudget parse_time_control(std::string_view tag);

// How much of the starting clock was still there when a move was played.
// Shares rather than seconds, so a bullet game and a rapid game can be read
// on the same scale.
enum class ClockBucket {
  comfortable = 0,  // more than half the clock left
  fair = 1,         // half down to a quarter
  low = 2,          // a quarter down to a tenth
  critical = 3,     // under a tenth of the clock
};
constexpr std::size_t kClockBucketCount = 4;
ClockBucket clock_bucket(std::int64_t remaining_ms, std::int64_t base_ms);
const char* clock_bucket_name(ClockBucket bucket);

// One of the profile's own moves, with the clock it was played on.
struct TimedMove {
  ClockBucket bucket{ClockBucket::comfortable};
  bool blunder{false};
  bool mistake{false};
  AccuracySample sample;  // empty weight when the move was never scored
};

struct ClockBucketStats {
  int moves{0};
  int blunders{0};
  int errors{0};                   // mistakes plus blunders
  std::optional<double> accuracy;  // over the moves played on that clock
  double errors_per_hundred{0.0};
};

struct TimePressureReport {
  int games{0};  // analysed games that carried clock times
  int moves{0};
  int blunders{0};
  std::array<ClockBucketStats, kClockBucketCount> buckets{};
  // Share of all the profile's blunders played with under a tenth of the clock.
  std::optional<double> blunder_share_in_time_trouble;
  // Share of games in which at least one move was played that low.
  std::optional<double> games_in_time_trouble;
};

// Accuracy and errors by how much clock was left. Moves are pooled across
// games: one long scramble therefore weighs more than one quiet game, which is
// what a question about time pressure is asking for.
TimePressureReport time_pressure(
    const std::vector<TimedMove>& moves, int games, int games_in_time_trouble);

}  // namespace kchess::statistics
