#include "coach_provider.h"

#include <string>
#include <utility>

#include "provider_adapters.h"
#include "provider_config.h"
#include "remote_provider.h"

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Coach provider factory
// -----------------------------------------------------------------------------

std::shared_ptr<const LLMProvider> make_coach_provider() {
  std::string error;
  const auto config = load_coach_provider_config(&error);
  if (!config) {
    return std::make_shared<RemoteProvider>(
        RemoteProviderConfig{.provider_id = "coach_provider", .model_id = ""});
  }

  RemoteProviderConfig remote{.provider_id = config->id,
                              .model_id = config->model};
  RemoteCompletionFunction completion;
  switch (config->api) {
    case ProviderApi::gemini_interactions:
      completion = [provider = *config](const LLMProviderRequest& request,
                                        const RemoteProviderConfig& provider_remote) {
        return complete_gemini(provider, request, provider_remote);
      };
      break;
    case ProviderApi::anthropic_messages:
      completion = [provider = *config](const LLMProviderRequest& request,
                                        const RemoteProviderConfig& provider_remote) {
        return complete_claude(provider, request, provider_remote);
      };
      break;
    case ProviderApi::openai_chat_completions:
      completion = [provider = *config](const LLMProviderRequest& request,
                                        const RemoteProviderConfig& provider_remote) {
        return complete_openai_compatible(provider, request, provider_remote);
      };
      break;
  }
  return std::make_shared<RemoteProvider>(std::move(remote),
                                          std::move(completion));
}

}  // namespace kchess::ai
