#pragma once

#include <array>
#include <chrono>
#include <mutex>

#include "http/http_client.h"
#include "providers/provider_models.h"

namespace kchess {

class ProviderRequestScheduler {
 public:
  HttpResponse get(ProviderType provider, HttpClient& client, const HttpRequest& request);
  std::int64_t retry_after_seconds(ProviderType provider) const;
  void reset_for_tests();

 private:
  struct State {
    mutable std::mutex mutex;
    std::chrono::steady_clock::time_point blocked_until{};
    int consecutive_rate_limits{0};
  };

  static std::size_t index(ProviderType provider);
  std::array<State, 2> states_;
};

}  // namespace kchess
