#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

#include "../coach_types.h"
#include "chess_verdict.h"
#include "../teaching/spaced_repetition_scheduler.h"

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Coach request contract
// -----------------------------------------------------------------------------

// Native-only Coach context state. These fields are hydrated from authoritative
// C++/SQLite state and are never trusted from Flutter/provider JSON.
struct CoachNativeContextState {
  std::string analysis_state{"unknown"};
  std::string profile_state{"unknown"};
  std::string history_state{"unknown"};
  std::optional<int> user_rating;
  std::optional<int> opponent_rating;
  std::optional<std::string> last_move_uci;
};

// Native attribution for one completed move that caused a Coach turn. This is
// role/context metadata, not an additional source of chess truth. Concrete move
// quality claims still require supplied evidence.
struct CoachMoveAttribution {
  std::optional<std::string> previous_fen;
  std::optional<std::string> current_fen;
  std::optional<std::string> played_move_uci;
  std::optional<std::string> mover_color;
  std::optional<std::string> learner_color;
  std::string mover_role{"unknown"};
  std::optional<std::string> classification;
  std::optional<double> expected_score_before;
  std::optional<double> expected_score_played;
  std::optional<double> expected_score_loss;
  bool verified_learner_move{false};
};

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
  std::optional<CoachMoveAttribution> move_attribution;
  // Native-only review of a previous authoritative judgment. User prose may
  // trigger this contract but never supplies the chess conclusion.
  std::optional<ChessVerdictChallenge> verdict_challenge;
  CoachNativeContextState native_context_state;
};

}  // namespace kchess::ai
