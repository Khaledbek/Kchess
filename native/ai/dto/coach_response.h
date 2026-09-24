#pragma once

#include <optional>
#include <string>
#include <vector>

#include "../coach_types.h"
#include "chess_verdict.h"
#include "evidence.h"
#include "structured_coach_response.h"

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Native-decided client action transport
// -----------------------------------------------------------------------------

struct CoachClientAction {
  std::string id;
  std::optional<int> elo;
  std::optional<std::string> color;
};

// -----------------------------------------------------------------------------
// Section: Coach response contract
// -----------------------------------------------------------------------------

struct CoachResponse {
  CoachIntent intent{CoachIntent::unknown};
  CoachAnswerType answer_type{CoachAnswerType::direct_answer};
  std::string answer;
  // Machine-only presentation hint for deterministic native answers that do
  // not require provider prose. Flutter localizes the visible wording from ARB
  // and must not infer chess semantics from an empty answer.
  std::string native_answer_kind;
  // Authoritative native chess judgment for evaluative turns. Provider/UI may
  // explain/render it but may not reinterpret the chess result.
  std::optional<ChessVerdictContract> chess_verdict;
  std::string follow_up_question;
  std::vector<CoachClaim> claims;
  std::vector<CoachConceptReference> concepts;
  std::vector<CoachRecommendation> recommendations;
  // Native-only board overlays that are not semantic recommendations. This is
  // required for analyses such as worst_move/fastest_loss where the verified
  // focus is intentionally harmful and must never be relabelled as advice.
  std::vector<std::string> board_moves;
  // Product/session actions are selected in native C++ and transported as
  // machine-only commands. Flutter may execute only these supplied actions; it
  // must not infer an action from prose.
  std::vector<CoachClientAction> client_actions;
  // Separate from provider/content validation: a response can be perfectly
  // valid prose and still fail the requested product action. Native code sets
  // this after client-action transport is assembled.
  bool action_fulfillment_required{false};
  bool action_fulfillment_passed{true};
  std::vector<std::string> action_fulfillment_issues;
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
