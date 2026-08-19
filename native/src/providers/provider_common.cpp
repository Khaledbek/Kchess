#include "providers/provider_common.h"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cctype>
#include <iomanip>
#include <sstream>

namespace kchess {

std::int64_t unix_time_seconds_provider() {
  return std::chrono::duration_cast<std::chrono::seconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

std::string url_encode(const std::string& value) {
  std::ostringstream encoded;
  encoded << std::uppercase << std::hex;
  for (const unsigned char character : value) {
    if (std::isalnum(character) || character == '-' || character == '_'
        || character == '.' || character == '~') {
      encoded << character;
    } else {
      encoded << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(character);
    }
  }
  return encoded.str();
}

HttpRequest provider_request(
    const std::string& url,
    const std::string& accept,
    const CacheValidators& validators,
    const std::shared_ptr<CancelToken>& cancel) {
  HttpRequest request;
  request.url = url;
  request.timeout_ms = 20'000;
  request.cancel_token = cancel;
  request.headers = {
      {"Accept", accept},
      {"Accept-Encoding", "gzip"},
      {"User-Agent", "KChess/0.1.0 (public-provider-client)"},
  };
  if (!validators.etag.empty()) request.headers["If-None-Match"] = validators.etag;
  if (!validators.last_modified.empty()) {
    request.headers["If-Modified-Since"] = validators.last_modified;
  }
  return request;
}

ResponseCacheInfo response_cache_info(const HttpResponse& response) {
  ResponseCacheInfo result;
  result.etag = response.header("etag");
  result.last_modified = response.header("last-modified");
  result.cache_control = response.header("cache-control");
  result.fetched_at = unix_time_seconds_provider();
  int max_age = 0;
  std::string lower = result.cache_control;
  std::transform(lower.begin(), lower.end(), lower.begin(), [](const unsigned char value) {
    return static_cast<char>(std::tolower(value));
  });
  const auto marker = lower.find("max-age=");
  if (marker != std::string::npos) {
    const auto begin = lower.data() + marker + 8;
    const auto end = lower.data() + lower.size();
    std::from_chars(begin, end, max_age);
  }
  result.expires_at = result.fetched_at + std::clamp(max_age, 60, 86'400);
  return result;
}

bool accept_provider_response(const HttpResponse& response) {
  if (response.status == 304 && response.error == HttpError::none) return false;
  if (response.error != HttpError::none) {
    throw ProviderException(
        response.error,
        response.error_message.empty() ? "provider request failed" : response.error_message);
  }
  if (response.status < 200 || response.status >= 300) {
    throw ProviderException(
        http_status_error(response.status),
        "provider returned HTTP " + std::to_string(response.status));
  }
  return true;
}

}  // namespace kchess
