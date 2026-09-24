#pragma once

#include <cstdint>
#include <vector>

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Evidence-planning contract
// -----------------------------------------------------------------------------

// Stable source IDs used by deterministic evidence planning. They describe
// where information may come from, not how the final coach answer is worded.
enum class EvidenceSource {
  existing_analysis,
  engine,
  human_model,
  position_features,
  tactical_detector,
  opening_knowledge,
  player_profile,
  game_history,
  conversation,
  chess_concepts,
};

// Stable compositional information requirements used by deterministic evidence planning.
// New natural-language questions should usually map to combinations of these needs
// instead of proliferating hard-coded intent classes.
enum class EvidenceNeed {
  legal_moves,
  candidate_moves,
  move_evaluations,
  best_reply,
  principal_variation,
  material_consequences,
  forced_mate,
  threats,
  tactical_motifs,
  piece_activity,
  controlled_squares,
  position_structure,
  strategic_plans,
  opening_context,
  practical_candidates,
  human_move_prediction,
  player_tendencies,
  historical_examples,
  conversation_reference,
  concept_explanation,
  move_comparison,
};

enum class EvidencePerspective {
  side_to_move,
  white,
  black,
  learner,
  opponent,
  neutral,
};

enum class EvidenceAnalysisScope {
  none,
  single_move,
  multiple_moves,
  all_legal_moves,
  whole_position,
  game_segment,
  player_history,
};

enum class EvidenceAnalysisDepth {
  minimal,
  shallow,
  medium,
  deep,
};

enum class EvidenceFreshness {
  reuse_only,
  reuse_first,
  fresh_if_missing,
  fresh_required,
};

enum class EvidenceInteraction {
  explain,
  compare,
  predict,
  search,
  teach,
  review,
};

enum class EvidenceEloTarget {
  none,
  current_user,
  opponent,
  elo_800,
  elo_1200,
  elo_1600,
  elo_2000,
  master,
};

// Priority is separate from depth. A high-priority plan may still be shallow
// when the user asks a quick question; it only controls scheduling and how
// aggressively optional sources may be skipped under latency pressure.
enum class EvidencePlanPriority : std::uint8_t {
  background = 0,
  normal = 1,
  foreground = 2,
  critical = 3,
};

struct EvidencePlan {
  std::vector<EvidenceSource> sources;
  std::vector<EvidenceNeed> needs;
  EvidencePerspective perspective{EvidencePerspective::side_to_move};
  EvidenceAnalysisScope scope{EvidenceAnalysisScope::whole_position};
  EvidenceAnalysisDepth depth{EvidenceAnalysisDepth::medium};
  EvidenceFreshness freshness{EvidenceFreshness::reuse_first};
  EvidenceInteraction interaction{EvidenceInteraction::explain};
  EvidenceEloTarget elo_target{EvidenceEloTarget::none};
  EvidencePlanPriority priority{EvidencePlanPriority::foreground};

  // Confidence describes deterministic routing certainty, not evidence quality.
  double confidence{0.0};
  bool allow_optional_sources{true};
};

}  // namespace kchess::ai
