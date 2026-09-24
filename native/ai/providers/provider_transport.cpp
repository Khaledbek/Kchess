#include "provider_transport.h"

#include <algorithm>
#include <utility>

#include "http/http_client.h"

namespace kchess::ai {
namespace {

// -----------------------------------------------------------------------------
// Section: Request accounting
// -----------------------------------------------------------------------------

ProviderRequestPriority request_priority(const LLMProviderRequest& request) {
  if (request.repair_candidate.has_value()) {
    return ProviderRequestPriority::repair;
  }
  return request.automatic_turn ? ProviderRequestPriority::automatic
                                : ProviderRequestPriority::manual;
}

std::size_t estimated_input_tokens(const std::string& serialized_body) {
  // Conservative local estimate. The vendor remains authoritative; this is only
  // used to reserve headroom before the request leaves KChess.
  return std::max<std::size_t>(1, (serialized_body.size() + 2) / 3);
}

int retry_after_seconds(const kchess::HttpResponse& response) {
  const auto value = response.header("retry-after");
  if (value.empty()) return 0;
  try {
    return std::max(0, std::stoi(value));
  } catch (...) {
    return 0;
  }
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Transport
// -----------------------------------------------------------------------------

LLMProviderResult provider_failure(const ProviderConfig& config,
                                   const RemoteProviderConfig& remote,
                                   LLMProviderStatus status,
                                   std::string error_code) {
  return {.status = status,
          .provider_id = remote.provider_id,
          .model_id = config.model,
          .error_code = std::move(error_code)};
}

std::optional<std::string> post_provider_request(
    const ProviderConfig& config,
    const LLMProviderRequest& request,
    const RemoteProviderConfig& remote,
    std::string serialized_body,
    std::map<std::string, std::string> headers,
    LLMProviderResult& failure) {
  std::string guard_error;
  if (!reserve_provider_request(config, request_priority(request),
                                estimated_input_tokens(serialized_body),
                                &guard_error)) {
    failure = provider_failure(config, remote, LLMProviderStatus::unavailable,
                               std::move(guard_error));
    return std::nullopt;
  }

  auto http = kchess::make_platform_http_client();
  kchess::HttpRequest http_request;
  http_request.url = config.endpoint;
  http_request.method = "POST";
  http_request.body = std::move(serialized_body);
  http_request.headers = std::move(headers);
  http_request.headers["Content-Type"] = "application/json";
  http_request.timeout_ms = config.timeout_ms;
  http_request.max_redirects = 0;
  http_request.max_body_bytes = 2U * 1024U * 1024U;

  auto response = http->get(http_request);
  if (response.status == 429 ||
      response.error == kchess::HttpError::rate_limited) {
    record_provider_rate_limit(config, retry_after_seconds(response));
    failure = provider_failure(config, remote, LLMProviderStatus::unavailable,
                               config.id + "_rate_limited");
    return std::nullopt;
  }
  if (!response.ok()) {
    failure = provider_failure(
        config, remote, LLMProviderStatus::error,
        response.error == kchess::HttpError::none
            ? config.id + "_http_" + std::to_string(response.status)
            : config.id + "_" + kchess::http_error_name(response.error));
    return std::nullopt;
  }

  record_provider_success(config);
  return std::move(response.body);
}

LLMProviderResult provider_content_result(const ProviderConfig& config,
                                          const RemoteProviderConfig& remote,
                                          const std::string& body,
                                          const ProviderContentParser& parse) {
  try {
    auto content = parse(body);
    if (content.empty()) {
      return provider_failure(config, remote, LLMProviderStatus::error,
                              config.id + "_output_empty");
    }
    return {.status = LLMProviderStatus::ok,
            .content = std::move(content),
            .provider_id = remote.provider_id,
            .model_id = config.model};
  } catch (...) {
    return provider_failure(config, remote, LLMProviderStatus::error,
                            config.id + "_response_invalid");
  }
}

}  // namespace kchess::ai
