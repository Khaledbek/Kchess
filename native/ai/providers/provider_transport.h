#pragma once

#include <functional>
#include <map>
#include <optional>
#include <string>

#include "llm_provider.h"
#include "provider_config.h"
#include "remote_provider.h"

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Shared provider transport
// -----------------------------------------------------------------------------

[[nodiscard]] LLMProviderResult provider_failure(
    const ProviderConfig& config,
    const RemoteProviderConfig& remote,
    LLMProviderStatus status,
    std::string error_code);

// Reserves local quota for the serialized body and POSTs it to the configured
// endpoint. HTTP 429 starts the persisted backoff and is terminal for this
// call. Returns the response body on success; otherwise fills `failure`.
[[nodiscard]] std::optional<std::string> post_provider_request(
    const ProviderConfig& config,
    const LLMProviderRequest& request,
    const RemoteProviderConfig& remote,
    std::string serialized_body,
    std::map<std::string, std::string> headers,
    LLMProviderResult& failure);

// Unparseable or truncated output becomes "<provider>_response_invalid", which
// lets the orchestrator use its safe fallback.
using ProviderContentParser =
    std::function<StructuredCoachContent(const std::string& body)>;

[[nodiscard]] LLMProviderResult provider_content_result(
    const ProviderConfig& config,
    const RemoteProviderConfig& remote,
    const std::string& body,
    const ProviderContentParser& parse);

}  // namespace kchess::ai
