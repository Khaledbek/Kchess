#include "remote_provider.h"

#include <utility>

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Remote provider lifecycle
// -----------------------------------------------------------------------------

RemoteProvider::RemoteProvider(RemoteProviderConfig config,
                               RemoteCompletionFunction completion)
    : config_(std::move(config)), completion_(std::move(completion)) {}

std::string_view RemoteProvider::id() const noexcept {
  return config_.provider_id;
}

bool RemoteProvider::available() const noexcept {
  return static_cast<bool>(completion_);
}

LLMProviderResult RemoteProvider::complete(
    const LLMProviderRequest& request) const {
  if (!completion_) {
    return {
        .status = LLMProviderStatus::unavailable,
        .provider_id = config_.provider_id,
        .model_id = config_.model_id,
        .error_code = "remote_transport_unavailable",
    };
  }

  auto result = completion_(request, config_);
  if (result.provider_id.empty()) result.provider_id = config_.provider_id;
  if (result.model_id.empty()) result.model_id = config_.model_id;
  return result;
}

}  // namespace kchess::ai
