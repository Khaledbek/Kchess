#pragma once

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
};

struct CoachClaim {
  CoachClaimKind kind{CoachClaimKind::general};
  std::string text;
  std::string subject;
  std::string value;
  std::vector<std::string> evidence_ids;
  double confidence{0.0};
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

struct StructuredCoachContent {
  std::string answer;
  std::string follow_up_question;
  std::vector<CoachClaim> claims;
  std::vector<CoachConceptReference> concepts;
  std::vector<CoachRecommendation> recommendations;
  std::vector<std::string> evidence_ids;

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
