#pragma once

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Stable coach contract enums
// -----------------------------------------------------------------------------

enum class CoachMode {
  explain,
  hint,
  answer,
  compare,
  quiz,
  plan,
  review,
  teach,
};

enum class QueryFamily {
  unknown,
  personal_chess,
  general_chess,
  position,
};

enum class CoachIntent {
  unknown,
  position,
  move_explanation,
  plan,
  tactic,
  opening,
  endgame,
  chess_concept,
  chess_rules,
  chess_history,
  player_development,
  game_review,
  training,
  follow_up,
  off_topic,
};

enum class ResponseDepth {
  concise,
  standard,
  detailed,
};

enum class EngineBudget {
  none,
  probe,
  deep,
};

// Native semantic position-analysis request. Structural request state may select
// one of these modes authoritatively; free-text selection is a compatibility
// fallback. The enum describes an available analysis operation, not the complete
// meaning of a user's question.
enum class PositionAnalysisMode {
  none,
  position_overview,
  best_move,
  worst_move,
  fastest_loss,
  avoid_trade,
  threat,
  explain_move,
  what_if_move,
  compare_candidates,
  learner_move_evaluation,
};

}  // namespace kchess::ai
