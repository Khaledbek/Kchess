#include "profile_evidence_adapter.h"

#include <algorithm>
#include <cctype>

namespace kchess::ai {
namespace {

std::string lower_ascii(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return value;
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Shared normalization
// -----------------------------------------------------------------------------

ProfileTimeControl profile_time_control(const std::string& value) {
  const auto normalized = lower_ascii(value);
  if (normalized == "bullet") return ProfileTimeControl::bullet;
  if (normalized == "blitz") return ProfileTimeControl::blitz;
  if (normalized == "rapid") return ProfileTimeControl::rapid;
  if (normalized == "classical" || normalized == "standard") {
    return ProfileTimeControl::classical;
  }
  return ProfileTimeControl::other;
}

std::string profile_time_control_id(const ProfileTimeControl value) {
  switch (value) {
    case ProfileTimeControl::bullet: return "bullet";
    case ProfileTimeControl::blitz: return "blitz";
    case ProfileTimeControl::rapid: return "rapid";
    case ProfileTimeControl::classical: return "classical";
    case ProfileTimeControl::other: return "other";
  }
  return "other";
}

ProfilePhase profile_phase_for_ply(const int ply) {
  // Keep the same broad move-number boundaries already used by Statistics for
  // phase summaries: full moves 1-12 opening, 13-30 middlegame, 31+ endgame.
  const int move_number = std::max(1, ply / 2 + 1);
  if (move_number <= 12) return ProfilePhase::opening;
  if (move_number <= 30) return ProfilePhase::middlegame;
  return ProfilePhase::endgame;
}

std::string profile_phase_id(const ProfilePhase value) {
  switch (value) {
    case ProfilePhase::opening: return "opening";
    case ProfilePhase::middlegame: return "middlegame";
    case ProfilePhase::endgame: return "endgame";
  }
  return "middlegame";
}

bool profile_major_error(const std::string& classification) {
  return classification == "miss" || classification == "mistake" ||
         classification == "blunder";
}

// -----------------------------------------------------------------------------
// Section: Evidence adapter
// -----------------------------------------------------------------------------

ProfileGameEvidence ProfileEvidenceAdapter::adapt(
    const ProfileGameObservation& observation) const {
  ProfileGameEvidence evidence;
  evidence.game_id = observation.game_id;
  evidence.played_at = observation.played_at;
  evidence.source_version = observation.source_version;
  evidence.time_control = profile_time_control(observation.time_control_type);
  evidence.result = observation.result;
  evidence.provider_outcome = observation.provider_outcome;
  evidence.player_color = observation.player_color;
  evidence.player_rating = observation.player_rating;
  evidence.opponent_rating = observation.opponent_rating;
  evidence.accuracy = observation.accuracy;
  evidence.opening_eco = observation.opening_eco;
  evidence.opening_name = observation.opening_name;
  evidence.move_count = observation.move_count;
  evidence.analysis_complete = observation.analysis_complete;
  evidence.classifications = observation.classifications;
  evidence.phases = observation.phases;
  evidence.average_expected_score_loss = observation.average_expected_score_loss;
  evidence.maximum_expected_score_loss = observation.maximum_expected_score_loss;
  evidence.moves.reserve(observation.moves.size());
  for (const auto& move : observation.moves) {
    evidence.moves.push_back(ProfileMoveEvidence{
        .ply = move.ply,
        .phase = profile_phase_for_ply(move.ply),
        .classification = move.classification,
        .expected_score_loss = move.expected_score_loss,
        .theory = move.theory,
        .fen_before = move.fen_before,
        .uci = move.uci,
    });
  }
  return evidence;
}

std::vector<ProfileGameEvidence> ProfileEvidenceAdapter::adapt(
    const std::vector<ProfileGameObservation>& observations) const {
  std::vector<ProfileGameEvidence> result;
  result.reserve(observations.size());
  for (const auto& observation : observations) result.push_back(adapt(observation));
  return result;
}

}  // namespace kchess::ai
