#pragma once

#include <string>
#include <vector>

#include "interaction/conversation_state.h"

namespace kchess::ai::interaction {

// UI-facing, logic-free projection of one active coach turn.
// Flutter renders this structure but never derives planner/action semantics from it.
struct CoachTurnView {
  std::string user_text;
  std::string coach_text;
  std::vector<FollowUpAction> followups;
  bool focus_latest_turn{true};
  bool history_scrollable{true};
};

class CoachTurnViewBuilder {
 public:
  [[nodiscard]] CoachTurnView build(std::string user_text,
                                    std::string coach_text,
                                    std::vector<FollowUpAction> followups,
                                    std::size_t followup_limit = 4) const;
};

}  // namespace kchess::ai::interaction
