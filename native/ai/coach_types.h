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

}  // namespace kchess::ai
