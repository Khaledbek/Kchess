// -----------------------------------------------------------------------------
// Section: Play under time pressure: clock budgets, buckets and error rates
// -----------------------------------------------------------------------------

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "services/time_pressure.h"

namespace {

using namespace kchess;
using namespace kchess::statistics;

void expect(const bool condition, const std::string& message) {
  if (!condition) throw std::runtime_error(message);
}

bool near(const double a, const double b, const double tolerance = 1e-6) {
  return std::abs(a - b) <= tolerance;
}

void test_time_control_tags() {
  const auto blitz = parse_time_control("180+2");
  expect(blitz.base_ms.has_value() && *blitz.base_ms == 180000, "Three minutes");
  expect(blitz.increment_ms == 2000, "Two seconds a move");

  const auto rapid = parse_time_control("600");
  expect(rapid.base_ms.has_value() && *rapid.base_ms == 600000 && rapid.increment_ms == 0,
      "A bare number is the whole budget");

  const auto tournament = parse_time_control("40/7200:1800");
  expect(tournament.base_ms.has_value() && *tournament.base_ms == 7200000,
      "A tournament control starts from its first period");

  for (const char* untimed : {"-", "?", "", "   "}) {
    expect(!parse_time_control(untimed).base_ms.has_value(),
        std::string("No budget from '") + untimed + "'");
  }
}

// Shares, not seconds, so a bullet game and a rapid game read the same way.
void test_buckets_are_shares_of_the_budget() {
  expect(clock_bucket(170000, 180000) == ClockBucket::comfortable, "Most of the clock");
  expect(clock_bucket(90000, 180000) == ClockBucket::fair, "Exactly half is not comfortable");
  expect(clock_bucket(45000, 180000) == ClockBucket::fair, "A quarter is still fair");
  expect(clock_bucket(30000, 180000) == ClockBucket::low, "A sixth is low");
  expect(clock_bucket(18000, 180000) == ClockBucket::low, "A tenth is not yet critical");
  expect(clock_bucket(9000, 180000) == ClockBucket::critical, "Under a tenth is critical");
  // The same nine seconds are nothing in a rapid game and plenty in bullet.
  expect(clock_bucket(9000, 600000) == ClockBucket::critical, "Nine seconds of ten minutes");
  expect(clock_bucket(9000, 10000) == ClockBucket::comfortable, "Nine seconds of ten");
}

TimedMove move(ClockBucket bucket, bool blunder, bool mistake, double accuracy) {
  return {.bucket = bucket,
          .blunder = blunder,
          .mistake = mistake,
          .sample = {.accuracy = accuracy, .weight = 1.0}};
}

void test_report() {
  std::vector<TimedMove> moves;
  // Twenty calm moves, all accurate.
  for (int i = 0; i < 20; ++i) {
    moves.push_back(move(ClockBucket::comfortable, false, false, 95.0));
  }
  // Ten moves in time trouble: three blunders and one mistake.
  for (int i = 0; i < 10; ++i) {
    moves.push_back(move(ClockBucket::critical, i < 3, i == 3, i < 4 ? 20.0 : 70.0));
  }
  // One blunder away from the clock, so the share is not trivially everything.
  moves.push_back(move(ClockBucket::fair, true, false, 10.0));

  const auto report = time_pressure(moves, 8, 5);
  expect(report.moves == 31 && report.games == 8, "Every move and game is counted");
  expect(report.blunders == 4, "Blunders are counted across the buckets");

  const auto& calm = report.buckets[static_cast<std::size_t>(ClockBucket::comfortable)];
  expect(calm.moves == 20 && calm.errors == 0 && near(calm.errors_per_hundred, 0.0),
      "Nothing goes wrong with a full clock");
  expect(calm.accuracy.has_value() && near(*calm.accuracy, 95.0),
      "Bucket accuracy uses the moves played on that clock");

  const auto& scramble = report.buckets[static_cast<std::size_t>(ClockBucket::critical)];
  expect(scramble.moves == 10 && scramble.blunders == 3 && scramble.errors == 4,
      "Mistakes and blunders both count as errors");
  expect(near(scramble.errors_per_hundred, 40.0), "Forty errors per hundred moves");
  expect(scramble.accuracy.has_value() && *scramble.accuracy < *calm.accuracy,
      "Accuracy drops when the clock does");

  expect(report.blunder_share_in_time_trouble.has_value() &&
             near(*report.blunder_share_in_time_trouble, 0.75),
      "Three of the four blunders came in time trouble");
  expect(report.games_in_time_trouble.has_value() &&
             near(*report.games_in_time_trouble, 0.625),
      "Five of the eight games reached the last tenth");
}

void test_report_without_evidence() {
  const auto empty = time_pressure({}, 0, 0);
  expect(empty.moves == 0 && !empty.blunder_share_in_time_trouble.has_value() &&
             !empty.games_in_time_trouble.has_value(),
      "No clocks, no claims");

  // Moves that were never scored still count as played.
  std::vector<TimedMove> unscored(4, TimedMove{.bucket = ClockBucket::low, .mistake = true});
  const auto report = time_pressure(unscored, 1, 0);
  const auto& low = report.buckets[static_cast<std::size_t>(ClockBucket::low)];
  expect(low.moves == 4 && low.errors == 4 && near(low.errors_per_hundred, 100.0),
      "Error rates do not need per-move accuracy");
  expect(!low.accuracy.has_value(), "Accuracy stays empty until the moves are scored");
}

}  // namespace

int main() {
  try {
    test_time_control_tags();
    test_buckets_are_shares_of_the_budget();
    test_report();
    test_report_without_evidence();
  } catch (const std::exception& error) {
    std::cerr << "time pressure tests failed: " << error.what() << std::endl;
    return 1;
  }
  std::cout << "time pressure tests passed" << std::endl;
  return 0;
}
