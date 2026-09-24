#pragma once

#include "llm_provider.h"
#include "provider_config.h"
#include "remote_provider.h"

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Vendor wire adapters
// -----------------------------------------------------------------------------

// Each adapter serializes the shared Coach prompt into one vendor wire format
// and maps the reply back to provider-neutral structured content.
[[nodiscard]] LLMProviderResult complete_gemini(
    const ProviderConfig& config,
    const LLMProviderRequest& request,
    const RemoteProviderConfig& remote);
[[nodiscard]] LLMProviderResult complete_claude(
    const ProviderConfig& config,
    const LLMProviderRequest& request,
    const RemoteProviderConfig& remote);
[[nodiscard]] LLMProviderResult complete_openai_compatible(
    const ProviderConfig& config,
    const LLMProviderRequest& request,
    const RemoteProviderConfig& remote);

}  // namespace kchess::ai
