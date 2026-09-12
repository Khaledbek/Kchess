#include "services/coach_profile_bridge.h"

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

std::optional<ai::ChessProfile> coach_chess_profile(
    Database& database, const std::optional<std::string>& profile_id) {
  const auto profile = resolve_profile(database, profile_id);
  if (!profile) return std::nullopt;

  const auto stats = database.player_learning_stats(profile->id);
  std::optional<ai::ChessProfile> previous;
  if (const auto payload = database.ai_chess_profile_payload(profile->id)) {
    previous = ai::chess_profile_from_json(*payload);
    if (previous && previous->source_games == stats.games &&
        previous->source_analyzed_moves == stats.analyzed_moves) {
      return previous;
    }
  }

  auto learned = ai::ChessProfileUpdater{}.update(observation(*profile, stats), previous);
  database.set_ai_chess_profile_payload(profile->id, ai::chess_profile_json(learned));
  return learned;
}

std::optional<ai::EvidenceItem> coach_profile_evidence(
    Database& database, const std::optional<std::string>& profile_id) {
  const auto profile = coach_chess_profile(database, profile_id);
  if (!profile) return std::nullopt;
  return ai::EvidenceItem{
      .kind = ai::EvidenceKind::user_profile,
      .payload = ai::chess_profile_json(*profile),
      .confidence = profile->confidence,
  };
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
