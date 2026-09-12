#include "model_versions.h"

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Stable version registry
// -----------------------------------------------------------------------------

const std::vector<ModelVersionInfo>& ModelVersionRegistry::all() {
  static const std::vector<ModelVersionInfo> versions = {
      {"PositionFeatures", "v1"},
      {"ConceptCatalog", "v1"},
      {"Practicality", "v1"},
      {"CoachPrompt", "v2"},
      {"RouterModel", "v1"},
      {"ContextPlannerModel", "v1"},
      {"EmbeddingModel", "v1"},
      {"ChessProfile", "v1"},
  };
  return versions;
}

std::string_view ModelVersionRegistry::version_of(std::string_view component) {
  for (const auto& item : all()) {
    if (item.component == component) return item.version;
  }
  return {};
}

}  // namespace kchess::ai
