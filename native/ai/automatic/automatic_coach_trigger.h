#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Automatic coaching event contract
// -----------------------------------------------------------------------------

enum class AutomaticCoachReason {
  inaccuracy,
  mistake,
  blunder,
  missed_tactic,
  new_motif,
  large_wdl_shift,
  phase_transition,
  repeated_personal_mistake,
  brilliant,
};

struct AutomaticCoachEvent {
  std::string classification;
  std::optional<double> expected_score_loss;
  std::optional<std::string> previous_fen;
  std::optional<std::string> current_fen;
  bool repeated_personal_mistake{false};

  // Learner-state scheduling signals are supplied by CoachService from the
  // existing native practice store. They are bounded policy inputs only; they
  // never become chess evidence or player-strength claims.
  double due_practice_relevance{0.0};
  std::optional<std::int64_t> seconds_since_last_automatic;
};

struct AutomaticCoachDecision {
  bool trigger{false};
  // Backward-compatible aggregate priority; equal to teaching_value.
  double priority{0.0};
  double objective_importance{0.0};
  double personal_relevance{0.0};
  double practice_relevance{0.0};
  double interruption_cost{0.0};
  double teaching_value{0.0};
  double minimum_teaching_value{0.72};
  bool primary_reason_present{false};
  bool critical_override{false};
  bool recency_blocked{false};
  std::vector<AutomaticCoachReason> reasons;
};

// -----------------------------------------------------------------------------
// Section: Native trigger policy
// -----------------------------------------------------------------------------

class AutomaticCoachTrigger {
 public:
  [[nodiscard]] AutomaticCoachDecision decide(
      const AutomaticCoachEvent& event) const;
};

[[nodiscard]] std::string_view automatic_coach_reason_name(
    AutomaticCoachReason reason) noexcept;

}  // namespace kchess::ai
