#pragma once

#include <functional>
#include <string>

#include "llm_provider.h"

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Remote transport adapter
// -----------------------------------------------------------------------------

struct RemoteProviderConfig {
  std::string provider_id{"remote"};
  std::string model_id;
};

using RemoteCompletionFunction = std::function<LLMProviderResult(
    const LLMProviderRequest&, const RemoteProviderConfig&)>;

class RemoteProvider final : public LLMProvider {
 public:
  explicit RemoteProvider(RemoteProviderConfig config = {},
                          RemoteCompletionFunction completion = {});

  [[nodiscard]] std::string_view id() const noexcept override;
  [[nodiscard]] bool available() const noexcept override;
  [[nodiscard]] LLMProviderResult complete(
      const LLMProviderRequest& request) const override;

 private:
  RemoteProviderConfig config_;
  RemoteCompletionFunction completion_;
};

}  // namespace kchess::ai
