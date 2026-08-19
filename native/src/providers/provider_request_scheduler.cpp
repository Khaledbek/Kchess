#include "providers/provider_request_scheduler.h"

#include <algorithm>
#include <charconv>
#include <string>

namespace kchess {
namespace {

int retry_after_header(const HttpResponse& response) {
  const auto value = response.header("retry-after");
  int seconds = 0;
  const auto parsed = std::from_chars(value.data(), value.data() + value.size(), seconds);
  return parsed.ec == std::errc{} && seconds > 0 ? seconds : 0;
}

}  // namespace

std::size_t ProviderRequestScheduler::index(const ProviderType provider) {
  return provider == ProviderType::lichess ? 1U : 0U;
}

HttpResponse ProviderRequestScheduler::get(
    const ProviderType provider,
    HttpClient& client,
    const HttpRequest& request) {
  auto& state = states_[index(provider)];
  std::unique_lock serial(state.mutex);
  const auto now = std::chrono::steady_clock::now();
  if (now < state.blocked_until) {
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
        state.blocked_until - now + std::chrono::seconds(1));
    return {
        .status = 429,
        .error = HttpError::rate_limited,
        .error_message = "provider cooldown active for " + std::to_string(seconds.count())
            + " seconds",
    };
  }
  auto response = client.get(request);
  if (response.error != HttpError::none) return response;
  response.error = http_status_error(response.status);
  if (response.error == HttpError::rate_limited) {
    ++state.consecutive_rate_limits;
    const int header_seconds = retry_after_header(response);
    const int required = provider == ProviderType::lichess
        ? 60
        : std::min(300, 30 * state.consecutive_rate_limits);
    const int delay = std::max(header_seconds, required);
    state.blocked_until = std::chrono::steady_clock::now() + std::chrono::seconds(delay);
    response.error_message = "provider rate limit; retry after " + std::to_string(delay)
        + " seconds";
  } else if (response.error == HttpError::none) {
    state.consecutive_rate_limits = 0;
  } else if (response.error_message.empty()) {
    response.error_message = "provider returned HTTP " + std::to_string(response.status);
  }
  return response;
}

std::int64_t ProviderRequestScheduler::retry_after_seconds(
    const ProviderType provider) const {
  auto& state = const_cast<State&>(states_[index(provider)]);
  std::lock_guard lock(state.mutex);
  const auto now = std::chrono::steady_clock::now();
  if (now >= state.blocked_until) return 0;
  return std::chrono::duration_cast<std::chrono::seconds>(
             state.blocked_until - now + std::chrono::seconds(1))
      .count();
}

void ProviderRequestScheduler::reset_for_tests() {
  for (auto& state : states_) {
    std::lock_guard lock(state.mutex);
    state.blocked_until = {};
    state.consecutive_rate_limits = 0;
  }
}

}  // namespace kchess
