// -----------------------------------------------------------------------------
// Section: Play under time pressure
// -----------------------------------------------------------------------------

#include "services/time_pressure.h"

#include <algorithm>
#include <charconv>

namespace kchess::statistics {
namespace {

// Reads the leading run of digits, e.g. "7200" from "7200:1800".
std::optional<std::int64_t> leading_number(std::string_view text) {
  std::size_t end = 0;
  while (end < text.size() && text[end] >= '0' && text[end] <= '9') ++end;
  if (end == 0) return std::nullopt;
  std::int64_t value = 0;
  const auto* first = text.data();
  const auto result = std::from_chars(first, first + end, value);
  if (result.ec != std::errc{}) return std::nullopt;
  return value;
}

}  // namespace

TimeControlBudget parse_time_control(std::string_view tag) {
  TimeControlBudget budget;
  while (!tag.empty() && tag.front() == ' ') tag.remove_prefix(1);
  if (tag.empty() || tag.front() == '-' || tag.front() == '?') return budget;

  // A tournament control lists moves before the clock ("40/7200"); the clock
  // for the first period is what a share is measured against.
  if (const auto slash = tag.find('/'); slash != std::string_view::npos) {
    tag.remove_prefix(slash + 1);
  }
  const auto base = leading_number(tag);
  if (!base || *base <= 0) return budget;
  budget.base_ms = *base * 1000;

  if (const auto plus = tag.find('+'); plus != std::string_view::npos) {
    if (const auto increment = leading_number(tag.substr(plus + 1))) {
      budget.increment_ms = *increment * 1000;
    }
  }
  return budget;
}

ClockBucket clock_bucket(const std::int64_t remaining_ms, const std::int64_t base_ms) {
  if (base_ms <= 0) return ClockBucket::comfortable;
  const double share = static_cast<double>(remaining_ms) / static_cast<double>(base_ms);
  if (share > 0.5) return ClockBucket::comfortable;
  if (share > 0.25) return ClockBucket::fair;
  if (share > 0.1) return ClockBucket::low;
  return ClockBucket::critical;
}

const char* clock_bucket_name(const ClockBucket bucket) {
  switch (bucket) {
    case ClockBucket::comfortable: return "comfortable";
    case ClockBucket::fair: return "fair";
    case ClockBucket::low: return "low";
    case ClockBucket::critical: return "critical";
  }
  return "comfortable";
}

TimePressureReport time_pressure(
    const std::vector<TimedMove>& moves, const int games,
    const int games_in_time_trouble) {
  TimePressureReport report;
  report.games = games;
  report.moves = static_cast<int>(moves.size());

  std::array<std::vector<AccuracySample>, kClockBucketCount> samples;
  for (const auto& move : moves) {
    const auto index = static_cast<std::size_t>(move.bucket);
    auto& bucket = report.buckets[index];
    bucket.moves += 1;
    if (move.blunder) {
      bucket.blunders += 1;
      report.blunders += 1;
    }
    if (move.blunder || move.mistake) bucket.errors += 1;
    if (move.sample.theory || move.sample.weight > 0.0) {
      samples[index].push_back(move.sample);
    }
  }

  for (std::size_t index = 0; index < kClockBucketCount; ++index) {
    auto& bucket = report.buckets[index];
    if (bucket.moves > 0) {
      bucket.errors_per_hundred =
          100.0 * static_cast<double>(bucket.errors) / bucket.moves;
    }
    if (!samples[index].empty()) bucket.accuracy = aggregate_accuracy(samples[index]);
  }

  if (report.blunders > 0) {
    const auto& critical = report.buckets[static_cast<std::size_t>(ClockBucket::critical)];
    report.blunder_share_in_time_trouble =
        static_cast<double>(critical.blunders) / report.blunders;
  }
  if (games > 0) {
    report.games_in_time_trouble =
        static_cast<double>(std::clamp(games_in_time_trouble, 0, games)) / games;
  }
  return report;
}

}  // namespace kchess::statistics
