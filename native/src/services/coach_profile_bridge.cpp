#include "services/coach_profile_bridge.h"

#include <algorithm>

#include "ai/profile/profile_updater.h"

namespace kchess {
namespace {

std::optional<Profile> resolve_profile(
    Database& database, const std::optional<std::string>& profile_id) {
  if (profile_id) return database.profile(*profile_id);
  return database.active_profile();
}

ai::ChessProfileObservation observation(
    const Profile& profile, const PlayerLearningStats& stats) {
  ai::ChessProfileObservation value;
  value.profile_id = profile.id;
  value.rating = profile.fide ? profile.fide : stats.average_rating;
  value.games = stats.games;
  value.analyzed_moves = stats.analyzed_moves;
  value.average_accuracy = stats.average_accuracy;
  value.theory = stats.theory;
  value.brilliant = stats.brilliant;
  value.critical = stats.critical;
  value.best = stats.best;
  value.excellent = stats.excellent;
  value.miss = stats.miss;
  value.mistake = stats.mistake;
  value.blunder = stats.blunder;
  for (const auto& opening : stats.openings) {
    value.openings.push_back(ai::ProfileOpeningObservation{
        .eco = opening.eco,
        .name = opening.name,
        .games = opening.games,
        .wins = opening.wins,
        .draws = opening.draws,
        .losses = opening.losses,
    });
  }
  return value;
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Learned profile refresh and mapping
// -----------------------------------------------------------------------------

std::vector<ai::ProfileGameMetadata> coach_profile_game_metadata(
    Database& database, const std::optional<std::string>& profile_id) {
  const auto profile = resolve_profile(database, profile_id);
  if (!profile) return {};

  const auto rows = database.profile_games_metadata(profile->id);
  std::vector<ai::ProfileGameMetadata> result;
  result.reserve(rows.size());
  for (const auto& row : rows) {
    result.push_back(ai::ProfileGameMetadata{
        .game_id = row.game_id,
        .played_at = row.played_at,
        .player_color = row.player_color,
        .outcome = row.provider_outcome,
        .time_control = row.time_control_type,
        .opening_eco = row.opening_eco,
        .opening_name = row.opening_name,
        .termination_type = row.termination_type,
        .player_rating = row.player_rating,
        .opponent_rating = row.opponent_rating,
        .plies = row.plies,
        .sample_eligible = row.plies >= ai::kProfileSamplingMinimumPlies,
        .has_complete_analysis = row.has_complete_analysis,
        .accuracy = row.accuracy,
        .miss_count = row.miss_count,
        .mistake_count = row.mistake_count,
        .blunder_count = row.blunder_count,
        .evidence_level = row.has_complete_analysis
            ? ai::ProfileEvidenceLevel::analyzed_summary
            : ai::ProfileEvidenceLevel::metadata_only,
    });
  }
  return result;
}

ai::ProfileMetadataSummary coach_profile_metadata_summary(
    Database& database, const std::optional<std::string>& profile_id,
    const std::int64_t now_epoch_seconds) {
  return ai::summarize_profile_metadata(
      coach_profile_game_metadata(database, profile_id), now_epoch_seconds);
}

std::vector<ai::InitialSampleEntry> coach_initial_profile_sample(
    Database& database, const std::optional<std::string>& profile_id,
    const std::int64_t now_epoch_seconds, const std::size_t limit) {
  return ai::build_initial_profile_sample(
      coach_profile_game_metadata(database, profile_id), now_epoch_seconds, limit);
}

std::vector<ai::ProfileGameRelevance> coach_profile_game_relevance(
    Database& database, const std::optional<std::string>& profile_id,
    const std::int64_t now_epoch_seconds,
    const ai::ProfileRelevanceContext& context) {
  return ai::rank_profile_games_by_relevance(
      coach_profile_game_metadata(database, profile_id), now_epoch_seconds, context);
}

std::vector<ai::InterestingProfileGame> coach_interesting_profile_games(
    Database& database, const std::optional<std::string>& profile_id,
    const std::int64_t now_epoch_seconds,
    const ai::ProfileRelevanceContext& context,
    const ai::InterestingGamePolicy& policy) {
  const auto games = coach_profile_game_metadata(database, profile_id);
  const auto ranked = ai::rank_profile_games_by_relevance(
      games, now_epoch_seconds, context);
  return ai::select_interesting_profile_games(games, ranked, context, policy);
}

std::vector<ai::ProfilePositionCandidate> coach_profile_position_candidates(
    Database& database, const std::optional<std::string>& profile_id,
    const ai::InterestingProfileGame& game,
    const ai::ProfileCandidatePolicy& policy) {
  const auto metadata = coach_profile_game_metadata(database, profile_id);
  const auto selected = std::find_if(
      metadata.begin(), metadata.end(),
      [&](const ai::ProfileGameMetadata& value) { return value.game_id == game.game_id; });
  if (selected == metadata.end()) return {};

  const auto rows = database.player_profile_move_sources(
      game.game_id, selected->player_color);
  std::vector<ai::ProfilePositionSignal> signals;
  signals.reserve(rows.size());
  for (const auto& row : rows) {
    signals.push_back(ai::ProfilePositionSignal{
        .ply = row.ply,
        .classification = row.classification,
        .expected_score_loss = row.expected_score_loss,
        .theory = row.theory,
        .fen_before = row.fen_before,
        .uci = row.uci,
    });
  }
  return ai::select_profile_position_candidates(game, signals, policy);
}

std::vector<ai::ProfileEvidenceDecision> coach_profile_position_evidence(
    Database& database, const std::optional<std::string>& profile_id,
    const ai::InterestingProfileGame& game,
    const ai::ProfileCandidatePolicy& candidate_policy,
    const ai::ProfileExistingEvidencePolicy& evidence_policy) {
  return ai::filter_existing_profile_evidence(
      coach_profile_position_candidates(database, profile_id, game, candidate_policy),
      evidence_policy);
}

std::optional<ai::ChessProfile> coach_chess_profile(
    Database& database, const std::optional<std::string>& profile_id) {
  const auto profile = resolve_profile(database, profile_id);
  if (!profile) return std::nullopt;
  const auto owner_id = database.player_profile_owner_id(profile->id);
  const auto owner_profile = database.profile(owner_id);
  if (!owner_profile) return std::nullopt;

  // PlayerProfileService owns the progressive v2 profile. Never overwrite a
  // valid persisted profile here with the legacy aggregate fallback merely
  // because background processing has not reached every game yet.
  if (const auto payload = database.ai_chess_profile_payload(owner_id)) {
    if (auto learned = ai::chess_profile_from_json(*payload)) return learned;
  }

  // A profile may be requested before the background service has written its
  // first snapshot. Seed a conservative profile from existing aggregate data;
  // PlayerProfileService will replace it with evidence-driven v2 data.
  const auto stats = database.player_learning_stats(owner_id);
  auto learned = ai::ChessProfileUpdater{}.update(observation(*owner_profile, stats));
  database.set_ai_chess_profile_payload(owner_id, ai::chess_profile_json(learned));
  return learned;
}

std::optional<ai::PracticalityPlayerContext> coach_practicality_player(
    Database& database, const std::optional<std::string>& profile_id) {
  const auto profile = coach_chess_profile(database, profile_id);
  if (!profile || profile->confidence <= 0.0) return std::nullopt;
  return ai::PracticalityPlayerContext{
      .rating = profile->rating,
      .calculation_strength = profile->calculation_strength.value,
      .tactical_strength = profile->tactical_strength.value,
      .risk_tolerance = profile->risk_tolerance.value,
      .confidence = profile->confidence,
  };
}

bool coach_repeated_personal_mistake(
    Database& database, const std::string& profile_id,
    const std::string& classification) {
  const auto profile = coach_chess_profile(database, profile_id);
  return profile && ai::repeated_personal_mistake(*profile, classification);
}

}  // namespace kchess
