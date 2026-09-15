#include "teaching_planner.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <string>

#include "../conversation/coach_session.h"
#include "skill_taxonomy.h"

namespace kchess::ai {
namespace {

std::string objective_for_intent(const CoachIntent topic) {
  switch (topic) {
    case CoachIntent::tactic:
      return "recognize_tactical_motif";
    case CoachIntent::plan:
      return "choose_position_plan";
    case CoachIntent::opening:
      return "understand_opening_decision";
    case CoachIntent::endgame:
      return "understand_endgame_decision";
    case CoachIntent::move_explanation:
      return "understand_move_decision";
    case CoachIntent::position:
      return "understand_position_priority";
    case CoachIntent::game_review:
      return "learn_from_game_review";
    case CoachIntent::player_development:
      return "practice_personal_priority";
    case CoachIntent::training:
      return "practice_chess_decision";
    case CoachIntent::chess_concept:
      return "understand_chess_concept";
    case CoachIntent::chess_rules:
      return "understand_chess_rule";
    case CoachIntent::chess_history:
      return "answer_chess_history_question";
    case CoachIntent::follow_up:
      return "continue_current_lesson";
    case CoachIntent::unknown:
    case CoachIntent::off_topic:
    default:
      return "answer_chess_question";
  }
}

std::int64_t unix_time_seconds() {
  using namespace std::chrono;
  return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

std::string quiz_target(const CoachRequest& request, const CoachIntent topic,
                        const std::string& skill_id) {
  auto found = request.practice_progress.find(skill_id);
  // Update-136 databases may contain only the original coarse motif buckets.
  // Reuse them as a temporary cold-start prior without writing new attempts
  // back into those coarse buckets.
  if (found == request.practice_progress.end()) {
    found = request.practice_progress.find(teaching_motif(topic));
  }
  if (found == request.practice_progress.end()) {
    return "candidate_choice:find_strongest_move";
  }
  const auto& progress = found->second;
  const double successes = std::max(0, progress.independent_successes);
  const double weak_attempts = std::max(0, progress.verified_weak_attempts);
  const double attempts = successes + weak_attempts;
  const double alpha = successes + 1.0;
  const double beta = weak_attempts + 1.0;
  const double mean = alpha / (alpha + beta);
  const double uncertainty = 1.64 * std::sqrt(
      alpha * beta /
      ((alpha + beta) * (alpha + beta) * (alpha + beta + 1.0)));

  // Scheduling changes *when/how hard* KChess revisits a verified exercise; it
  // never turns the counters into a player-rating claim.
  const bool due = practice_is_due(progress, unix_time_seconds());
  if (attempts >= 3.0 && mean + uncertainty < 0.55) {
    return "candidate_choice:guided";
  }
  if (!due && progress.success_streak >= 3) {
    return "candidate_choice:compare_with_opponent_reply";
  }
  if (attempts >= 8.0 && mean - uncertainty > 0.70) {
    return "candidate_choice:compare_with_opponent_reply";
  }
  return "candidate_choice:find_strongest_move";
}

}  // namespace

TeachingPlan TeachingPlanner::plan(const CoachRequest& request,
                                   const CoachIntent topic,
                                   const QueryPlan& query_plan,
                                   const std::vector<EvidenceItem>& evidence,
                                   const CoachSessionState* session) const {
  TeachingPlan result;
  const auto skill = resolve_teaching_skill(request, topic, evidence);
  result.skill_id = skill.id;
  result.skill_family = skill.family;
  result.objective_id = objective_for_intent(topic);
  result.max_concepts =
      query_plan.response_depth == ResponseDepth::detailed ? 2 : 1;

  if (request.teaching_target && !request.teaching_target->empty()) {
    result.target = *request.teaching_target;
  }

  switch (request.mode) {
    case CoachMode::quiz:
      result.objective_id = "practice_candidate_selection";
      result.delivery_mode = "guided_question";
      result.reveal_level = 0;
      result.ask_question = true;
      result.max_recommendations = 0;
      result.max_concepts = 1;
      if (result.target.empty()) {
        result.target = quiz_target(request, topic, result.skill_id);
      }
      break;
    case CoachMode::hint:
      result.objective_id = "discover_position_idea";
      result.delivery_mode = "hint_ladder";
      result.reveal_level = 0;
      result.ask_question = true;
      result.max_recommendations = 0;
      result.max_concepts = 1;
      break;
    case CoachMode::compare:
      result.objective_id = "compare_candidate_tradeoffs";
      result.delivery_mode = "candidate_comparison";
      result.reveal_level = 2;
      result.ask_question = false;
      result.max_recommendations = 2;
      break;
    case CoachMode::review:
      result.objective_id = "learn_from_game_review";
      result.delivery_mode = "reflective_review";
      result.reveal_level = 2;
      result.ask_question = false;
      result.max_recommendations = 2;
      break;
    case CoachMode::teach:
      result.delivery_mode = "guided_explanation";
      result.reveal_level = 1;
      result.ask_question = true;
      result.max_recommendations = 1;
      break;
    case CoachMode::plan:
      result.objective_id = "choose_position_plan";
      result.delivery_mode = "plan_explanation";
      result.reveal_level = 2;
      result.ask_question = false;
      result.max_recommendations = 2;
      break;
    case CoachMode::explain:
      result.delivery_mode = "focused_explanation";
      result.reveal_level = 2;
      result.ask_question = false;
      result.max_recommendations = 2;
      break;
    case CoachMode::answer:
    default:
      result.delivery_mode = "direct_explanation";
      result.reveal_level = 2;
      result.ask_question = false;
      result.max_recommendations = 2;
      break;
  }

  if (request.automatic_turn) {
    result.objective_id = "explain_one_critical_point";
    result.delivery_mode = "critical_point";
    result.reveal_level = std::min(result.reveal_level, 1);
    result.ask_question = false;
    result.max_recommendations = std::min(result.max_recommendations, 1);
    result.max_concepts = 1;
  }

  // A completed move that answered an open question is a feedback turn, not
  // another unsolicited lesson. Let the provider address the actual attempt
  // before the session opens a new exercise or hides the played solution.
  if (session != nullptr && !session->last_attempt_status.empty() &&
      request.mode != CoachMode::quiz && request.mode != CoachMode::hint) {
    result.objective_id = "explain_verified_learner_attempt";
    const bool has_contrast = std::any_of(
        evidence.begin(), evidence.end(), [](const EvidenceItem& item) {
          return item.kind == EvidenceKind::move_contrast;
        });
    result.delivery_mode = has_contrast
        ? "evidence_contrast" : "attempt_feedback";
    result.reveal_level = 2;
    result.ask_question = false;
    result.max_recommendations = 1;
    result.max_concepts = 1;
  }

  return result;
}

}  // namespace kchess::ai
