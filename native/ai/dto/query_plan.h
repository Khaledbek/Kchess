#pragma once

#include <cstddef>
#include <vector>

#include "../coach_types.h"
#include "evidence.h"

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Query planning contract
// -----------------------------------------------------------------------------

struct QueryPlan {
  CoachIntent intent{CoachIntent::unknown};
  CoachIntent context_intent{CoachIntent::unknown};
  double routing_confidence{0.0};
  EngineBudget engine_budget{EngineBudget::none};
  ResponseDepth response_depth{ResponseDepth::standard};
  bool needs_position{false};
  bool needs_profile{false};
  bool needs_theory{false};
  bool needs_opening{false};
  bool has_conversation_context{false};
  std::size_t concept_limit{5};
  std::vector<EvidenceKind> evidence;
};

}  // namespace kchess::ai
