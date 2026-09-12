#include "coach_session.h"
#include <algorithm>

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

ResolvedCoachTurn CoachSessionMemory::resolve(const CoachRequest& request) const {
  ResolvedCoachTurn turn;
  turn.request = request;
  if (!request.session_id) return turn;

  std::lock_guard lock(mutex_);
  const auto it = sessions_.find(*request.session_id);
  if (it == sessions_.end() || it->second.empty()) return turn;

  turn.previous = it->second;
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

// -----------------------------------------------------------------------------
// Section: Turn persistence
// -----------------------------------------------------------------------------

void CoachSessionMemory::remember(const ResolvedCoachTurn& turn,
                                  CoachIntent topic,
                                  const CoachResponse& response) {
  if (!turn.request.session_id || !response.accepted || !response.validation_passed || response.answer.empty()) return;

  std::lock_guard lock(mutex_);
  if (!sessions_.contains(*turn.request.session_id) && sessions_.size() >= 64) {
    const auto oldest = std::min_element(sessions_.begin(), sessions_.end(),
        [](const auto& a, const auto& b) { return a.second.last_used < b.second.last_used; });
    sessions_.erase(oldest);
  }
  CoachSessionState& state = sessions_[*turn.request.session_id];
  state = turn.previous;
  state.last_used = ++next_use_;
  remember_topic(state, topic, turn.request, response);
}

}  // namespace kchess::ai
