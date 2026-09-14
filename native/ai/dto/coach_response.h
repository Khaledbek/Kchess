#pragma once

#include <string>
#include <vector>

#include "../coach_types.h"
#include "evidence.h"
#include "structured_coach_response.h"

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Coach response contract
// -----------------------------------------------------------------------------

struct CoachResponse {
  CoachIntent intent{CoachIntent::unknown};
  CoachAnswerType answer_type{CoachAnswerType::direct_answer};
  std::string answer;
  std::string follow_up_question;
  std::vector<CoachClaim> claims;
  std::vector<CoachConceptReference> concepts;
  std::vector<CoachRecommendation> recommendations;
  std::vector<std::string> evidence_references;
  std::vector<EvidenceItem> evidence;
  std::vector<std::string> validation_issues;
  std::string provider_error_code;
  // Stable machine-only fallback kind for a native-safe UI prompt when an LLM
  // quiz/hint response cannot cross the validation boundary. Flutter maps this
  // key to ARB text; C++ never owns the visible fallback wording.
  std::string safe_fallback_kind;
  bool accepted{true};
  bool validation_passed{true};
  bool validation_repaired{false};
};

}  // namespace kchess::ai
