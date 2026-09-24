#pragma once

#include <cstdint>

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Provider-neutral spaced-repetition state
// -----------------------------------------------------------------------------

// Practice scheduling metadata only. These values describe when KChess should
// revisit a verified exercise; they are not a chess-skill rating or evidence
// that the player has mastered a concept.
struct PracticeProgress {
  int independent_successes{0};
  int verified_weak_attempts{0};
  int guided_successes{0};
  int success_streak{0};
  int schedule_level{0};
  std::int64_t last_practiced_at{0};
  std::int64_t interval_seconds{0};
  std::int64_t next_practice_at{0};
};

[[nodiscard]] PracticeProgress schedule_practice_attempt(
    const PracticeProgress& previous, bool independent_success,
    std::int64_t now_seconds);

[[nodiscard]] bool practice_is_due(
    const PracticeProgress& progress, std::int64_t now_seconds);

// Returns a bounded scheduler priority in [0, 2]. A never-practiced skill is
// immediately due (1.0); overdue skills rise toward 2.0. Not-yet-due skills
// stay below 1.0. This is scheduling priority, never player-strength evidence.
[[nodiscard]] double practice_due_priority(
    const PracticeProgress& progress, std::int64_t now_seconds);

}  // namespace kchess::ai
