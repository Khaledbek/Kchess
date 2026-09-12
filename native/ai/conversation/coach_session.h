#pragma once

#include <mutex>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

#include "../coach_types.h"
#include "../dto/coach_request.h"
#include "../dto/coach_response.h"

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Compact conversation state
// -----------------------------------------------------------------------------

struct CoachSessionState {
  std::optional<std::string> current_board;
  CoachIntent current_topic{CoachIntent::unknown};
  std::string current_goal;
  std::string last_claim;
  std::string last_recommendation;
  std::string referenced_concept;
  std::string unresolved_question;
  std::string question_board;
  std::uint64_t last_used{0};

  [[nodiscard]] bool empty() const;
};

struct ResolvedCoachTurn {
  CoachRequest request;
  CoachSessionState previous;
  bool has_previous{false};
};

// -----------------------------------------------------------------------------
// Section: In-memory session memory
// -----------------------------------------------------------------------------

class CoachSessionMemory {
 public:
  [[nodiscard]] ResolvedCoachTurn resolve(const CoachRequest& request) const;
  void remember(const ResolvedCoachTurn& turn, CoachIntent topic,
                const CoachResponse& response);

 private:
  mutable std::mutex mutex_;
  std::unordered_map<std::string, CoachSessionState> sessions_;
  std::uint64_t next_use_{0};
};

}  // namespace kchess::ai
