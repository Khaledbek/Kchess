#pragma once

#include <optional>
#include <string>

#include "ai/dto/evidence.h"
#include "ai/practicality/player_practicality.h"
#include "ai/profile/chess_profile.h"
#include "persistence/database.h"

namespace kchess {

// -----------------------------------------------------------------------------
// Section: Persistence-to-coach profile bridge
// -----------------------------------------------------------------------------

[[nodiscard]] std::optional<ai::ChessProfile> coach_chess_profile(
    Database& database, const std::optional<std::string>& profile_id);

[[nodiscard]] std::optional<ai::EvidenceItem> coach_profile_evidence(
    Database& database, const std::optional<std::string>& profile_id);

[[nodiscard]] std::optional<ai::PracticalityPlayerContext>
coach_practicality_player(
    Database& database, const std::optional<std::string>& profile_id);

[[nodiscard]] bool coach_repeated_personal_mistake(
    Database& database, const std::string& profile_id,
    const std::string& classification);

}  // namespace kchess
