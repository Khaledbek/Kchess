#pragma once

#include <cstddef>
#include <optional>
#include <string>

#include "conversation/coach_session.h"
#include "dto/coach_request.h"
#include "dto/query_plan.h"

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Provider-neutral prompt context
// -----------------------------------------------------------------------------

struct CoachContext {
  std::string user_text;
  std::string locale;
  std::optional<std::string> position_fen;
  std::string session_summary;
  std::string pgn_excerpt;
  std::size_t input_token_budget{0};
  std::size_t estimated_tokens{0};
  bool pgn_truncated{false};
};

class ContextBuilder {
 public:
  [[nodiscard]] CoachContext build(const CoachRequest& request,
                                   const QueryPlan& plan,
                                   const CoachSessionState* session) const;
};

}  // namespace kchess::ai
