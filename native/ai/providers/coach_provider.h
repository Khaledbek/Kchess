#pragma once

#include <memory>

#include "llm_provider.h"

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Coach provider factory
// -----------------------------------------------------------------------------

// Builds the remote provider selected in config/coach_provider.json. A missing
// config or secrets/<provider>_api_key.txt yields an unavailable provider.
[[nodiscard]] std::shared_ptr<const LLMProvider> make_coach_provider();

}  // namespace kchess::ai
