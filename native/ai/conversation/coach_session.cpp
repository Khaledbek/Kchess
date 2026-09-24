#include "coach_session.h"
#include <algorithm>
#include <cmath>

#include "chess/move.h"

namespace kchess::ai {
namespace {

// -----------------------------------------------------------------------------
// Section: State updates
// -----------------------------------------------------------------------------

std::string compact(std::string text, std::size_t limit = 600) {
  if (text.size() <= limit) return text;
  while (limit > 0 && (static_cast<unsigned char>(text[limit]) & 0xc0) == 0x80) --limit;
  text.resize(limit);
  return text;
}

void remember_topic(CoachSessionState& state, CoachIntent topic,
                    const CoachRequest& request, const CoachResponse& response) {
  if (topic != CoachIntent::unknown && topic != CoachIntent::off_topic &&
      topic != CoachIntent::follow_up) {
    state.current_topic = topic;
  }

  if (!request.user_text.empty()) state.current_goal = compact(request.user_text);
  if (request.position_fen) state.current_board = request.position_fen;

  if (response.answer.empty()) {
    return;
  }

  state.last_claim = compact(response.answer, 900);
  state.unresolved_question = compact(response.follow_up_question);
  state.question_board = state.unresolved_question.empty() ? "" : request.position_fen.value_or("");
  state.expected_move.clear();
  state.expected_reply.clear();
  state.exercise_skill_id.clear();
  state.score_pending_move_question = false;
  state.acceptable_moves.clear();
  state.last_attempt_status.clear();
  state.last_attempt_classification.clear();

  if (topic == CoachIntent::move_explanation || topic == CoachIntent::plan) {
    state.last_recommendation = response.recommendations.empty()
                                    ? std::string{}
                                    : compact(response.recommendations.front().move_uci + " " + response.recommendations.front().text);
  }
  if (topic == CoachIntent::chess_concept || topic == CoachIntent::tactic ||
      topic == CoachIntent::opening || topic == CoachIntent::endgame) {
    state.referenced_concept = compact(request.user_text);
  }
}

}  // namespace

std::string teaching_motif(CoachIntent topic) {
  switch (topic) {
    case CoachIntent::tactic: return "tactic";
    case CoachIntent::opening: return "opening_decision";
    case CoachIntent::endgame: return "endgame_decision";
    case CoachIntent::plan: return "plan_choice";
    default: return "candidate_choice";
  }
}

// -----------------------------------------------------------------------------
// Section: State inspection
// -----------------------------------------------------------------------------

bool CoachSessionState::empty() const {
  return !current_board && current_topic == CoachIntent::unknown &&
         current_goal.empty() && last_claim.empty() &&
         last_recommendation.empty() && referenced_concept.empty() &&
         unresolved_question.empty();
}

// -----------------------------------------------------------------------------
// Section: Turn resolution
// -----------------------------------------------------------------------------

ResolvedCoachTurn CoachSessionMemory::resolve(const CoachRequest& request) {
  ResolvedCoachTurn turn;
  turn.request = request;
  if (!request.session_id) return turn;

  std::lock_guard lock(mutex_);
  const auto it = sessions_.find(*request.session_id);
  if (it == sessions_.end() || it->second.empty()) return turn;
  // Reusing a session ID after switching players must never inherit another
  // player's scope, claims or board context.
  if (it->second.profile_id != request.profile_id) return turn;

  turn.previous = it->second;
  if (request.user_move_uci && request.position_fen &&
      !turn.previous.question_board.empty() &&
      !turn.previous.expected_move.empty()) {
    try {
      const auto result = kchess::apply_legal_uci_move(
          turn.previous.question_board, *request.user_move_uci);
      if (result.fen_after == *request.position_fen) {
        const bool candidate_success =
            std::find(turn.previous.acceptable_moves.begin(),
                      turn.previous.acceptable_moves.end(),
                      *request.user_move_uci) !=
            turn.previous.acceptable_moves.end();
        const bool success = candidate_success || request.user_move_success_confirmed;
        if (turn.previous.score_pending_move_question &&
            (success || request.user_move_error_confirmed)) {
          turn.learning_attempt = CoachLearningAttempt{
              .profile_id = request.profile_id,
              .skill_id = turn.previous.exercise_skill_id,
              .independent_success = success};
        }
        if (candidate_success) {
          turn.previous.last_attempt_status =
              *request.user_move_uci == turn.previous.expected_move
                  ? "verified_best_candidate_found"
                  : "verified_near_equal_candidate_found";
        } else if (request.user_move_success_confirmed) {
          turn.previous.last_attempt_status = "verified_strong_move_played";
        } else if (request.user_move_error_confirmed) {
          turn.previous.last_attempt_status = "verified_weak_move_played";
        } else {
          turn.previous.last_attempt_status = "legal_alternative_not_graded";
        }
        turn.previous.last_attempt_classification =
            request.user_move_classification.value_or("");
        it->second.expected_move.clear();
        it->second.expected_reply.clear();
        it->second.exercise_skill_id.clear();
        it->second.score_pending_move_question = false;
        it->second.acceptable_moves.clear();
      }
    } catch (...) {
      // A tap or failed/illegal move is never a learning attempt.
    }
  }
  if (request.position_fen && turn.previous.current_board != request.position_fen) {
    // Keep the learning topic, never carry a previous board's claims into a new one.
    turn.previous.last_claim.clear();
    turn.previous.last_recommendation.clear();
    // Keep the trainer's question with its original board so a played attempt
    // can receive feedback; it is conversation context, not current-board proof.
  }
  turn.has_previous = true;
  if (!turn.request.position_fen && turn.previous.current_board) {
    turn.request.position_fen = turn.previous.current_board;
  }
  return turn;
}

bool CoachSessionMemory::has_pending_move_question(
    const std::optional<std::string>& session_id,
    const std::optional<std::string>& profile_id,
    const std::optional<std::string>& question_fen) const {
  if (!session_id || !question_fen) return false;
  std::lock_guard lock(mutex_);
  const auto it = sessions_.find(*session_id);
  return it != sessions_.end() && it->second.profile_id == profile_id &&
      !it->second.expected_move.empty() &&
      it->second.question_board == *question_fen;
}

// -----------------------------------------------------------------------------
// Section: Turn persistence
// -----------------------------------------------------------------------------

void CoachSessionMemory::remember(const ResolvedCoachTurn& turn,
                                  CoachIntent topic,
                                  const CoachResponse& response,
                                  const QueryPlan* plan,
                                  const CandidateMoveSet* candidates,
                                  const TeachingPlan* teaching_plan) {
  if (!turn.request.session_id || !response.accepted ||
      !response.validation_passed ||
      (response.answer.empty() && response.safe_fallback_kind.empty())) {
    return;
  }

  std::lock_guard lock(mutex_);
  if (!sessions_.contains(*turn.request.session_id) && sessions_.size() >= 64) {
    const auto oldest = std::min_element(sessions_.begin(), sessions_.end(),
        [](const auto& a, const auto& b) { return a.second.last_used < b.second.last_used; });
    sessions_.erase(oldest);
  }
  CoachSessionState& state = sessions_[*turn.request.session_id];
  state = turn.previous;
  state.profile_id = turn.request.profile_id;
  if (plan) {
    state.query_family = plan->query_family;
    state.profile_scope = plan->profile_scope;
    state.needs_profile = plan->needs_profile;
    if (!plan->has_conversation_context) {
      state.current_goal.clear();
      state.last_claim.clear();
      state.last_recommendation.clear();
      state.referenced_concept.clear();
      state.unresolved_question.clear();
      state.question_board.clear();
    }
  }
  state.last_used = ++next_use_;
  const auto previous_goal = state.current_goal;
  remember_topic(state, topic, turn.request, response);
  const bool native_quiz_fallback =
      response.safe_fallback_kind == "quiz_question";
  if (native_quiz_fallback) {
    state.last_claim.clear();
    state.unresolved_question.clear();
    state.expected_move.clear();
    state.expected_reply.clear();
    state.exercise_skill_id.clear();
    state.score_pending_move_question = false;
    state.acceptable_moves.clear();
    state.last_attempt_status.clear();
    state.last_attempt_classification.clear();
    state.question_board = turn.request.position_fen.value_or("");
  }
  const bool scored_quiz_question = turn.request.mode == CoachMode::quiz &&
      (!response.follow_up_question.empty() || native_quiz_fallback);
  const bool automatic_board_question = turn.request.automatic_turn &&
      !response.follow_up_question.empty();
  if ((scored_quiz_question || automatic_board_question) &&
      candidates && candidates->best && turn.request.position_fen) {
    state.expected_move = candidates->best->move_uci;
    state.expected_reply = candidates->critical_reply_uci.value_or("");
    state.acceptable_moves.push_back(state.expected_move);
    state.score_pending_move_question = scored_quiz_question;
    state.exercise_skill_id = scored_quiz_question
        ? (teaching_plan && !teaching_plan->skill_id.empty()
               ? teaching_plan->skill_id
               : teaching_motif(topic))
        : std::string{};
    for (const auto& alternative : candidates->alternatives) {
      if (state.acceptable_moves.size() >= 3) break;
      if (candidates->best->evaluation_cp && alternative.evaluation_cp &&
          std::abs(static_cast<long long>(*candidates->best->evaluation_cp) -
                   static_cast<long long>(*alternative.evaluation_cp)) <= 50)
        state.acceptable_moves.push_back(alternative.move_uci);
    }
  }
  if (plan && plan->intent == CoachIntent::follow_up && !previous_goal.empty()) {
    state.current_goal = previous_goal;
  }
}

}  // namespace kchess::ai
