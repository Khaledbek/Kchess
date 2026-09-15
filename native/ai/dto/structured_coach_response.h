#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "../coach_types.h"

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Structured response primitives
// -----------------------------------------------------------------------------

enum class CoachAnswerType {
  explanation,
  hint,
  direct_answer,
  comparison,
  quiz,
  plan,
  review,
  teaching,
};

enum class CoachClaimKind {
  general,
  legal_move,
  gives_check,
  gives_mate,
  material_cp,
  piece_on_square,
  tactical_motif,
  engine_evaluation,
  opening_fact,
  profile_fact,
  profile_inference,
  position_contrast_fact,
};

struct CoachClaim {
  CoachClaimKind kind{CoachClaimKind::general};
  std::string text;
  // Exact excerpt from answer/follow_up_question containing this assertion.
  // Native validation checks the excerpt and the typed source independently.
  std::string answer_quote;
  std::string subject;
  std::string value;
  std::vector<std::string> evidence_ids;
  double confidence{0.0};
  // Personal claims copy the provider-visible profile chunk epistemic status.
  // profile_inference uses this to distinguish a cautious inference from a
  // hypothesis while exact profile_fact claims mirror the source chunk.
  std::string epistemic_status;
  // profile_inference must list the exact profile chunk scalar subjects used
  // as premises (nodeId#/data/json-pointer). This is grounding metadata, not
  // natural-language reasoning.
  std::vector<std::string> support_subjects;
};

struct CoachConceptReference {
  std::string concept_id;
  std::vector<std::string> evidence_ids;
  double confidence{0.0};
};

struct CoachRecommendation {
  std::string move_uci;
  std::string text;
  std::vector<std::string> evidence_ids;
  double confidence{0.0};
};

enum class CoachSegmentKind { factual, general, uncertainty, dialogue };

struct CoachAnswerSegment {
  std::string text;
  CoachSegmentKind kind{CoachSegmentKind::dialogue};
  std::vector<std::size_t> claim_indices;
};

struct StructuredCoachContent {
  std::string answer;
  std::string follow_up_question;
  std::vector<CoachAnswerSegment> answer_segments;
  std::vector<CoachAnswerSegment> follow_up_segments;
  std::vector<CoachClaim> claims;
  std::vector<CoachConceptReference> concepts;
  std::vector<CoachRecommendation> recommendations;
  std::vector<std::string> evidence_ids;

  // Explicit provider declaration for personal grounding. Values:
  // not_used, grounded, insufficient_evidence. Empty remains compatible with
  // older non-profile callers, but profile requests require an explicit value.
  std::string profile_status;

  [[nodiscard]] bool empty() const { return answer.empty(); }
};

// -----------------------------------------------------------------------------
// Section: Deterministic answer type
// -----------------------------------------------------------------------------

[[nodiscard]] constexpr CoachAnswerType answer_type_for_mode(CoachMode mode) {
  switch (mode) {
    case CoachMode::explain:
      return CoachAnswerType::explanation;
    case CoachMode::hint:
      return CoachAnswerType::hint;
    case CoachMode::compare:
      return CoachAnswerType::comparison;
    case CoachMode::quiz:
      return CoachAnswerType::quiz;
    case CoachMode::plan:
      return CoachAnswerType::plan;
    case CoachMode::review:
      return CoachAnswerType::review;
    case CoachMode::teach:
      return CoachAnswerType::teaching;
    case CoachMode::answer:
    default:
      return CoachAnswerType::direct_answer;
  }
}

}  // namespace kchess::ai
