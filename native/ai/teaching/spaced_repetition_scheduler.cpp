#include "spaced_repetition_scheduler.h"

#include <algorithm>
#include <array>

namespace kchess::ai {
namespace {

constexpr std::int64_t kHour = 60 * 60;
constexpr std::int64_t kDay = 24 * kHour;
constexpr std::array<std::int64_t, 7> kIntervals{
    0, kDay, 3 * kDay, 7 * kDay, 14 * kDay, 30 * kDay, 60 * kDay};

}  // namespace

PracticeProgress schedule_practice_attempt(
    const PracticeProgress& previous, const bool independent_success,
    const std::int64_t now_seconds) {
  PracticeProgress next = previous;
  next.last_practiced_at = std::max<std::int64_t>(0, now_seconds);

  if (independent_success) {
    ++next.independent_successes;
    next.success_streak = std::max(0, previous.success_streak) + 1;
    next.schedule_level = std::min<int>(
        static_cast<int>(kIntervals.size()) - 1,
        std::max(0, previous.schedule_level) + 1);
    next.interval_seconds = kIntervals[static_cast<std::size_t>(next.schedule_level)];
  } else {
    ++next.verified_weak_attempts;
    next.success_streak = 0;
    next.schedule_level = std::max(0, previous.schedule_level - 1);
    // A verified weak attempt should return soon, but not immediately interrupt
    // the learner again in the same short session.
    next.interval_seconds = 6 * kHour;
  }

  next.next_practice_at = next.last_practiced_at + next.interval_seconds;
  return next;
}

bool practice_is_due(
    const PracticeProgress& progress, const std::int64_t now_seconds) {
  return progress.last_practiced_at <= 0 || progress.next_practice_at <= 0 ||
         now_seconds >= progress.next_practice_at;
}

double practice_due_priority(
    const PracticeProgress& progress, const std::int64_t now_seconds) {
  if (progress.last_practiced_at <= 0 || progress.next_practice_at <= 0) {
    return 1.0;
  }
  const auto interval = std::max<std::int64_t>(1, progress.interval_seconds);
  if (now_seconds >= progress.next_practice_at) {
    const double overdue = static_cast<double>(now_seconds - progress.next_practice_at);
    return 1.0 + std::min(1.0, overdue / static_cast<double>(interval));
  }
  const double remaining = static_cast<double>(progress.next_practice_at - now_seconds);
  return std::clamp(1.0 - remaining / static_cast<double>(interval), 0.0, 1.0);
}

}  // namespace kchess::ai
