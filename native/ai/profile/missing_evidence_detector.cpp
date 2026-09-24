#include "missing_evidence_detector.h"

#include <algorithm>

namespace kchess::ai {

MissingEvidenceDecision MissingEvidenceDetector::inspect(
    const ProfileGameEvidence& game,
    const double relevance_score) const {
  const int analyzed = game.classifications.analyzed_moves;
  const int major_errors = game.classifications.miss + game.classifications.mistake +
                           game.classifications.blunder;

  // A valid completed KChess analysis is authoritative. The profile must reuse
  // it rather than launching a second full Stockfish pass.
  if (game.analysis_complete && analyzed > 0) {
    if (major_errors > 0 && !game.maximum_expected_score_loss.has_value()) {
      for (const auto& move : game.moves) {
        if (profile_major_error(move.classification) &&
            !move.expected_score_loss.has_value()) {
          return MissingEvidenceDecision{
              .action = MissingEvidenceAction::deep_probe,
              .ply = move.ply,
              .reason = "legacy_major_error_without_loss",
          };
        }
      }
    }
    return MissingEvidenceDecision{};
  }

  // A game without a known player side can still contribute metadata/opening
  // context, but KChess must not invent which moves belong to the user.
  if (game.player_color != "white" && game.player_color != "black") {
    return MissingEvidenceDecision{};
  }

  // Low-value historical coverage is metadata-only. The background queue still
  // marks the game considered, while representative/new/hypothesis-targeted
  // games are boosted by PlayerProfileService before reaching this policy.
  if (relevance_score < 0.40) {
    return MissingEvidenceDecision{};
  }

  if (analyzed > 0) {
    return MissingEvidenceDecision{};
  }

  if (game.move_count <= 0) return MissingEvidenceDecision{};
  return MissingEvidenceDecision{
      .action = MissingEvidenceAction::quick_probe,
      .ply = std::nullopt,
      .reason = "relevant_game_without_native_analysis",
  };
}

}  // namespace kchess::ai
