#include "http/http_client.h"

#include <algorithm>
#include <cctype>

namespace kchess {
namespace {

std::string lowercase(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char character) {
    return static_cast<char>(std::tolower(character));
  });
  return value;
}

}  // namespace

std::string HttpResponse::header(const std::string& name) const {
  const auto found = headers.find(lowercase(name));
  return found == headers.end() ? std::string{} : found->second;
}

HttpError http_status_error(const int status) {
  if (status >= 200 && status < 400) return HttpError::none;
  if (status == 404) return HttpError::not_found;
  if (status == 410) return HttpError::gone;
  if (status == 429) return HttpError::rate_limited;
  if (status >= 500 && status < 600) return HttpError::server;
  return HttpError::invalid_response;
}

std::string http_error_name(const HttpError error) {
  switch (error) {
    case HttpError::none: return "none";
    case HttpError::offline: return "offline";
    case HttpError::timeout: return "timeout";
    case HttpError::dns: return "dns";
    case HttpError::tls: return "tls";
    case HttpError::not_found: return "notFound";
    case HttpError::gone: return "gone";
    case HttpError::rate_limited: return "rateLimited";
    case HttpError::server: return "server";
    case HttpError::invalid_response: return "invalidResponse";
    case HttpError::cancelled: return "cancelled";
    case HttpError::transport: return "transport";
  }
  return "transport";
}

}  // namespace kchess
