#include "interaction/coach_turn_view.h"

#include <algorithm>
#include <utility>

namespace kchess::ai::interaction {

CoachTurnView CoachTurnViewBuilder::build(
    std::string user_text,
    std::string coach_text,
    std::vector<FollowUpAction> followups,
    std::size_t followup_limit) const {
  CoachTurnView view;
  view.user_text = std::move(user_text);
  view.coach_text = std::move(coach_text);

  if (followups.size() > followup_limit) {
    followups.resize(followup_limit);
  }
  view.followups = std::move(followups);
  return view;
}

}  // namespace kchess::ai::interaction
