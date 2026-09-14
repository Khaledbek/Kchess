#include "adaptive_profile_analyzer.h"

namespace kchess::ai {

ProfileAnalysisRequest AdaptiveProfileAnalyzer::plan(
    const ProfileGameEvidence& game,
    const double relevance_score) const {
  const auto decision = MissingEvidenceDetector{}.inspect(game, relevance_score);
  switch (decision.action) {
    case MissingEvidenceAction::none:
      return ProfileAnalysisRequest{
          .action = ProfileAnalysisAction::none,
          .game_id = game.game_id,
          .ply = std::nullopt,
          .reason = decision.reason,
      };
    case MissingEvidenceAction::quick_probe:
      return ProfileAnalysisRequest{
          .action = ProfileAnalysisAction::quick_game_analysis,
          .game_id = game.game_id,
          .ply = std::nullopt,
          .reason = decision.reason,
      };
    case MissingEvidenceAction::deep_probe:
      return ProfileAnalysisRequest{
          .action = ProfileAnalysisAction::deep_move_refinement,
          .game_id = game.game_id,
          .ply = decision.ply,
          .reason = decision.reason,
      };
  }
  return ProfileAnalysisRequest{};
}

}  // namespace kchess::ai
