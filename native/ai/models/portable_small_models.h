#pragma once

#include <filesystem>
#include <string>

#include "small_models.h"

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Optional portable tiny-model runtime
// -----------------------------------------------------------------------------

struct SmallModelComponentStatus {
  bool file_present{false};
  bool available{false};
  std::string id;
  std::string version;
  std::string error_code;
};

struct SmallModelRuntimeStatus {
  std::string schema{"coach.small_models.v1"};
  std::string source{"none"};
  SmallModelComponentStatus intent;
  SmallModelComponentStatus context_planner;
  SmallModelComponentStatus embeddings;
};

struct LoadedSmallModelSuite {
  SmallModelSuite suite;
  SmallModelRuntimeStatus status;
};

// Loads optional, dependency-free linear tiny models from a native model root.
// KCHESS_SMALL_MODEL_DIR overrides default_root for developer/evaluation runs.
// Missing/invalid assets fail closed and leave the deterministic C++ baselines
// fully operational.
[[nodiscard]] LoadedSmallModelSuite load_optional_small_model_suite(
    std::filesystem::path default_root) noexcept;

}  // namespace kchess::ai
