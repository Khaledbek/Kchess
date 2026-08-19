#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "http/http_client.h"
#include "providers/provider_models.h"

namespace kchess {

std::string url_encode(const std::string& value);
HttpRequest provider_request(
    const std::string& url,
    const std::string& accept,
    const CacheValidators& validators,
    const std::shared_ptr<CancelToken>& cancel);
ResponseCacheInfo response_cache_info(const HttpResponse& response);
bool accept_provider_response(const HttpResponse& response);
std::int64_t unix_time_seconds_provider();

}  // namespace kchess
