#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

#include "../coach_types.h"
#include "../teaching/spaced_repetition_scheduler.h"

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Coach request contract
// -----------------------------------------------------------------------------

struct CoachRequest {
  std::string user_text;
  std::string locale{"en"};
  CoachMode mode{CoachMode::answer};
  ResponseDepth depth{ResponseDepth::standard};
  std::optional<std::string> position_fen;
  std::optional<std::string> game_pgn;
  std::optional<std::string> session_id;
  std::optional<std::string> user_move_uci;
  // Set only by native completed-move analysis, never trusted from request JSON.
  // A strong native classification may confirm a quiz answer even when the
  // played move was outside the small candidate set retained when the question
  // was asked.
  bool user_move_success_confirmed{false};
  bool user_move_error_confirmed{false};
  std::optional<std::string> user_move_classification;
  std::optional<std::string> surface;
  std::optional<std::string> context_id;
  std::optional<int> context_ply;
  std::optional<std::string> profile_id;
  std::optional<std::string> player_color;
  std::optional<std::string> hint_move_uci;
  std::optional<std::string> variation_job_id;
  // Native teaching policy, never a measured player-profile claim.
  std::optional<std::string> teaching_target;
  // Internal native practice scheduling state by stable skill ID (legacy coarse
  // keys may still be present as cold-start priors); never serialized to the LLM.
  std::unordered_map<std::string, PracticeProgress> practice_progress;
  // Explicit UI request for a personalized quiz position from the player's
  // already-learned own-game examples. Native selection remains authoritative.
  bool personal_training_requested{false};
  // Internal native-only marker: the profile has already been consumed to pick
  // one concrete own-game exercise, so the provider turn must not fetch the
  // full personal Knowledge packet again merely to ask the quiz question.
  bool personal_training_position_selected{false};
  bool automatic_turn{false};
};

}  // namespace kchess::ai
