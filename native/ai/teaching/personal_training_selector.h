#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

#include "../profile/chess_profile.h"
#include "spaced_repetition_scheduler.h"

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Personal training position selection
// -----------------------------------------------------------------------------

struct PersonalTrainingSelection {
  ProfileExamplePosition example;
  std::string pattern_id;
  std::string schedule_skill_id;
  double priority{0.0};
};

// Chooses one already-persisted example position from the player's learned
// weakness profile. This function is pure policy: it never reads persistence,
// starts engine work, or turns scheduling metadata into a skill claim.
[[nodiscard]] std::optional<PersonalTrainingSelection>
select_personal_training_position(
    const ChessProfile& profile,
    const std::unordered_map<std::string, PracticeProgress>& practice_progress,
    std::int64_t now_seconds);

}  // namespace kchess::ai
