#pragma once

#include <optional>
#include <string>
#include <vector>

#include "ai/practicality/player_practicality.h"
#include "ai/profile/chess_profile.h"
#include "ai/profile/profile_analysis_funnel.h"
#include "persistence/database.h"

namespace kchess {

// -----------------------------------------------------------------------------
// Section: Persistence-to-coach profile bridge
// -----------------------------------------------------------------------------

[[nodiscard]] std::optional<ai::ChessProfile> coach_chess_profile(
    Database& database, const std::optional<std::string>& profile_id);

[[nodiscard]] std::vector<ai::ProfileGameMetadata> coach_profile_game_metadata(
    Database& database, const std::optional<std::string>& profile_id);

[[nodiscard]] ai::ProfileMetadataSummary coach_profile_metadata_summary(
    Database& database, const std::optional<std::string>& profile_id,
    std::int64_t now_epoch_seconds);

[[nodiscard]] std::vector<ai::InitialSampleEntry> coach_initial_profile_sample(
    Database& database, const std::optional<std::string>& profile_id,
    std::int64_t now_epoch_seconds, std::size_t limit = 20);

[[nodiscard]] std::vector<ai::ProfileGameRelevance> coach_profile_game_relevance(
    Database& database, const std::optional<std::string>& profile_id,
    std::int64_t now_epoch_seconds,
    const ai::ProfileRelevanceContext& context = {});

[[nodiscard]] std::vector<ai::InterestingProfileGame> coach_interesting_profile_games(
    Database& database, const std::optional<std::string>& profile_id,
    std::int64_t now_epoch_seconds,
    const ai::ProfileRelevanceContext& context = {},
    const ai::InterestingGamePolicy& policy = {});

[[nodiscard]] std::vector<ai::ProfilePositionCandidate> coach_profile_position_candidates(
    Database& database, const std::optional<std::string>& profile_id,
    const ai::InterestingProfileGame& game,
    const ai::ProfileCandidatePolicy& policy = {});

[[nodiscard]] std::vector<ai::ProfileEvidenceDecision> coach_profile_position_evidence(
    Database& database, const std::optional<std::string>& profile_id,
    const ai::InterestingProfileGame& game,
    const ai::ProfileCandidatePolicy& candidate_policy = {},
    const ai::ProfileExistingEvidencePolicy& evidence_policy = {});

[[nodiscard]] std::optional<ai::PracticalityPlayerContext>
coach_practicality_player(
    Database& database, const std::optional<std::string>& profile_id);

[[nodiscard]] bool coach_repeated_personal_mistake(
    Database& database, const std::string& profile_id,
    const std::string& classification);

}  // namespace kchess
