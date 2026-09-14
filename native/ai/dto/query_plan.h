#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "../coach_types.h"
#include "evidence.h"

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Classified profile scope
// -----------------------------------------------------------------------------

// Classified once before retrieval. Values are stable native IDs, never provider
// prose. Empty axes are unrestricted; values on an axis are alternatives.
// Content topics (for example endgame) narrow generic goals (weaknesses), so
// "my endgame weaknesses" cannot retrieve unrelated middlegame weaknesses.
struct ProfileQueryScope {
  std::vector<std::string> topics;
  std::vector<std::string> time_controls;
  std::vector<std::string> phases;
  std::vector<std::string> player_colors;
  bool needs_endgame_material_type{false};
  bool wants_proof{false};
  bool compare_scopes{false};
};

// -----------------------------------------------------------------------------
// Section: Query planning contract
// -----------------------------------------------------------------------------

struct QueryPlan {
  QueryFamily query_family{QueryFamily::unknown};
  CoachIntent intent{CoachIntent::unknown};
  CoachIntent context_intent{CoachIntent::unknown};
  double routing_confidence{0.0};
  EngineBudget engine_budget{EngineBudget::none};
  ResponseDepth response_depth{ResponseDepth::standard};
  bool needs_position{false};
  bool needs_profile{false};
  bool needs_concepts{false};
  bool needs_theory{false};
  bool needs_opening{false};
  bool has_conversation_context{false};
  ProfileQueryScope profile_scope;
  std::size_t concept_limit{5};
  std::vector<EvidenceKind> evidence;
};

}  // namespace kchess::ai
