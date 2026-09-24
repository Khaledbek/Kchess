#pragma once

#include <string>

#include "llm_provider.h"

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Provider-neutral Coach prompt
// -----------------------------------------------------------------------------

// Every vendor adapter sends the same instruction and input text; only the wire
// format differs. Native validation stays authoritative for every provider.
[[nodiscard]] std::string coach_system_instruction(
    const LLMProviderRequest& request);
[[nodiscard]] std::string coach_provider_input(
    const LLMProviderRequest& request);

}  // namespace kchess::ai
