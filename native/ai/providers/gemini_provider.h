#pragma once

#include <memory>

#include "llm_provider.h"

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Gemini provider factory
// -----------------------------------------------------------------------------

[[nodiscard]] std::shared_ptr<const LLMProvider> make_gemini_provider();

}  // namespace kchess::ai
