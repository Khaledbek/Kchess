#include "move_contrast.h"

#include <nlohmann/json.hpp>

#include "chess/move.h"
#include "position_features.h"

namespace kchess::ai {
namespace {

using json = nlohmann::json;

// Material is factual; the remaining fields are transparent board-feature
// proxies and must not be described as a new Stockfish evaluation.
json side_snapshot(const PositionFeatures& features, const bool played_white) {
  const auto& own = played_white ? features.white : features.black;
  const auto& opponent = played_white ? features.black : features.white;
  return {
      {"materialBalanceCp", own.material_cp - opponent.material_cp},
      {"ownKingAttackedZoneSquares", own.strategic.king.attacked_zone_squares},
      {"ownKingExposedFiles", own.strategic.king.exposed_files},
      {"opponentKingAttackedZoneSquares",
       opponent.strategic.king.attacked_zone_squares},
      {"ownActivitySquares", own.activity_squares},
      {"opponentActivitySquares", opponent.activity_squares},
      {"ownIsolatedPawns", own.strategic.pawns.isolated_pawns},
  };
}

json difference(const json& after, const json& before) {
  json out = json::object();
  for (auto it = after.begin(); it != after.end(); ++it) {
    out[it.key()] = it.value().get<int>() - before.at(it.key()).get<int>();
  }
  return out;
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Verified played move and existing candidate comparison
// -----------------------------------------------------------------------------

std::optional<EvidenceItem> completed_move_contrast(
    const CoachRequest& request, const CoachSessionState* session,
    const PositionFeatures* current_features,
    const EvidenceItem* completed_analysis) {
  if (session == nullptr || session->last_attempt_status.empty() ||
      session->question_board.empty() || !request.user_move_uci ||
      !request.position_fen) return std::nullopt;

  try {
    const auto played = kchess::apply_legal_uci_move(
        session->question_board, *request.user_move_uci);
    if (played.fen_after != *request.position_fen) return std::nullopt;

    PositionFeatureExtractor extractor;
    const auto before_features = extractor.extract(session->question_board);
    const bool played_white = before_features.white_to_move;
    const auto before = side_snapshot(before_features, played_white);
    const auto played_snapshot = side_snapshot(
        current_features != nullptr ? *current_features
                                    : extractor.extract(played.fen_after),
        played_white);

    json payload{{"schema", "move.contrast.v1"},
                 {"playedMoveUci", *request.user_move_uci},
                 {"attemptStatus", session->last_attempt_status},
                 {"before", before},
                 {"played", played_snapshot},
                 {"playedMinusBefore", difference(played_snapshot, before)},
                 {"featureBasis", "deterministic_board_snapshot"},
                 {"qualityBasis", "native_attempt_status_only"}};

    if (!session->expected_move.empty() &&
        session->expected_move != *request.user_move_uci) {
      const auto candidate = kchess::apply_legal_uci_move(
          session->question_board, session->expected_move);
      const auto candidate_snapshot = side_snapshot(
          extractor.extract(candidate.fen_after), played_white);
      payload["originalQuestionCandidateUci"] = session->expected_move;
      payload["candidate"] = candidate_snapshot;
      payload["playedMinusCandidate"] =
          difference(played_snapshot, candidate_snapshot);
    }

    if (completed_analysis != nullptr &&
        completed_analysis->id == "analysis.completed_move.v1") {
      const auto analysis = json::parse(completed_analysis->payload,
                                        nullptr, false);
      if (analysis.is_object() &&
          analysis.value("analyzedFen", "") == session->question_board &&
          analysis.value("qualityComplete", false) &&
          analysis.value("classification", json{}).is_string() &&
          analysis.value("expectedScoreBest", json{}).is_number() &&
          analysis.value("expectedScorePlayed", json{}).is_number() &&
          analysis.value("expectedScoreLoss", json{}).is_number()) {
        payload["verifiedAnalysis"] = {
            {"engineVersion", analysis.value("engineVersion", "")},
            {"classification", analysis["classification"]},
            {"moverExpectedScoreBest", analysis["expectedScoreBest"]},
            {"moverExpectedScorePlayed", analysis["expectedScorePlayed"]},
            {"moverExpectedScoreLoss", analysis["expectedScoreLoss"]},
        };
        if (analysis.value("lines", json{}).is_array()) {
          for (const auto& line : analysis["lines"]) {
            if (!line.is_object() ||
                !line.value("moves", json{}).is_array() ||
                line["moves"].size() < 2 ||
                line["moves"][0] != *request.user_move_uci ||
                !line["moves"][1].is_string()) continue;
            payload["verifiedAnalysis"]["criticalReplyUci"] =
                line["moves"][1];
            break;
          }
        }
        payload["qualityBasis"] = "comparable_persisted_analysis";
      }
    }

    return EvidenceItem{.id = "move.contrast.v1",
                        .kind = EvidenceKind::move_contrast,
                        .payload = payload.dump(),
                        .confidence = 1.0};
  } catch (...) {
    // Invalid or stale board context cannot produce comparative evidence.
    return std::nullopt;
  }
}

}  // namespace kchess::ai
