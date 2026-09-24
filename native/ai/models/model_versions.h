#pragma once

#include <string_view>
#include <vector>

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Versioned AI contracts/assets
// -----------------------------------------------------------------------------

struct ModelVersionInfo {
  std::string_view component;
  std::string_view version;
};

class ModelVersionRegistry {
 public:
  [[nodiscard]] static const std::vector<ModelVersionInfo>& all();
  [[nodiscard]] static std::string_view version_of(std::string_view component);
};

}  // namespace kchess::ai
