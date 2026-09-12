#pragma once

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
};

struct AutomaticCoachDecision {
  bool trigger{false};
  double priority{0.0};
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
