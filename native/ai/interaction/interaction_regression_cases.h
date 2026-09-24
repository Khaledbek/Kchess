#pragma once

#include <string>
#include <vector>

namespace kchess::ai::interaction {

struct InteractionRegressionCase {
  std::string id;
  std::string user_text;
  bool expect_planner{true};
  std::vector<std::string> expected_semantic_ids;
  std::vector<std::string> expected_actions;
  bool expect_answer_llm{true};
  std::string setup;
};

// Stable real-world cases that previously exposed routing weaknesses. This is
// deliberately data-only so the app/test harness can execute the same cases
// against any provider or planner implementation.
[[nodiscard]] const std::vector<InteractionRegressionCase>&
interaction_regression_cases();

}  // namespace kchess::ai::interaction
