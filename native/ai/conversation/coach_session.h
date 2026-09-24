#pragma once

#include <mutex>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "../coach_types.h"
#include "../dto/coach_request.h"
#include "../dto/coach_response.h"
#include "../dto/candidate_moves.h"
#include "../dto/query_plan.h"
#include "../teaching/teaching_plan.h"

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Compact conversation state
// -----------------------------------------------------------------------------

struct CoachSessionState {
  std::optional<std::string> profile_id;
  QueryFamily query_family{QueryFamily::unknown};
  ProfileQueryScope profile_scope;
  bool needs_profile{false};
  std::optional<std::string> current_board;
  CoachIntent current_topic{CoachIntent::unknown};
  std::string current_goal;
  std::string last_claim;
  std::string last_recommendation;
  std::string referenced_concept;
  std::string unresolved_question;
  std::string question_board;
  std::string expected_move;
  std::string expected_reply;
  std::string exercise_skill_id;
  bool score_pending_move_question{false};
  std::vector<std::string> acceptable_moves;
  std::string last_attempt_status;
  std::string last_attempt_classification;
  std::uint64_t last_used{0};

  [[nodiscard]] bool empty() const;
};

struct CoachLearningAttempt {
  std::optional<std::string> profile_id;
  std::string skill_id;
  bool independent_success{false};
};

struct ResolvedCoachTurn {
  CoachRequest request;
  CoachSessionState previous;
  bool has_previous{false};
  std::optional<CoachLearningAttempt> learning_attempt;
};

[[nodiscard]] std::string teaching_motif(CoachIntent topic);

// -----------------------------------------------------------------------------
// Section: In-memory session memory
// -----------------------------------------------------------------------------

class CoachSessionMemory {
 public:
  [[nodiscard]] ResolvedCoachTurn resolve(const CoachRequest& request);
  [[nodiscard]] bool has_pending_move_question(
      const std::optional<std::string>& session_id,
      const std::optional<std::string>& profile_id,
      const std::optional<std::string>& question_fen) const;
  void remember(const ResolvedCoachTurn& turn, CoachIntent topic,
                const CoachResponse& response, const QueryPlan* plan = nullptr,
                const CandidateMoveSet* candidates = nullptr,
                const TeachingPlan* teaching_plan = nullptr);

 private:
  mutable std::mutex mutex_;
  std::unordered_map<std::string, CoachSessionState> sessions_;
  std::uint64_t next_use_{0};
};

}  // namespace kchess::ai
