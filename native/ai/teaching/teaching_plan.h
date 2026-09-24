#pragma once

#include <string>

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Provider-neutral native teaching policy
// -----------------------------------------------------------------------------

struct TeachingPlan {
  // Stable machine objective selected by native KChess policy.
  std::string objective_id{"answer_chess_question"};
  // Delivery style is a policy hint, not natural-language copy.
  std::string delivery_mode{"direct_explanation"};
  // 0 = do not reveal a concrete move, 1 = guided/partial reveal,
  // 2 = direct supported answer may reveal a concrete move.
  int reveal_level{2};
  // Native hint-ladder step for the current board. 0 = general orientation,
  // 1 = source piece, 2 = target square, 3 = tactical/strategic idea,
  // 4 = reveal the verified move. Non-hint turns keep this at 0.
  int hint_level{0};
  bool ask_question{false};
  int max_recommendations{2};
  int max_concepts{1};
  // Stable native learner-skill identity. This groups only verified practice
  // attempts; it is not itself a claim about measured player ability.
  std::string skill_id{"calculation.candidate_selection"};
  std::string skill_family{"calculation"};
  // Optional native task target. This replaces ad-hoc quiz-only target selection
  // while preserving the existing provider field for compatibility.
  std::string target;
};

}  // namespace kchess::ai
