#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace kchess::ai::interaction {

enum class SemanticGroup {
  task,
  scope,
  subject,
  topic,
  action,
  need,
  source,
  modifier,
};

struct SemanticToken {
  std::string_view id;
  SemanticGroup group;
};

[[nodiscard]] const std::vector<SemanticToken>& semantic_dictionary();
[[nodiscard]] std::optional<SemanticGroup> semantic_group_for(std::string_view id);
[[nodiscard]] bool is_known_semantic_id(std::string_view id);

}  // namespace kchess::ai::interaction
