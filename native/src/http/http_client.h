#pragma once

#include <atomic>
#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>

namespace kchess {

enum class HttpError {
  none,
  offline,
  timeout,
  dns,
  tls,
  not_found,
  gone,
  rate_limited,
  server,
  invalid_response,
  cancelled,
  transport,
};

class CancelToken {
 public:
  void cancel() noexcept { cancelled_.store(true); }
  bool is_cancelled() const noexcept { return cancelled_.load(); }

 private:
  std::atomic_bool cancelled_{false};
};

struct HttpRequest {
  std::string url;
  std::map<std::string, std::string> headers;
  int timeout_ms{15'000};
  int max_redirects{5};
  std::size_t max_body_bytes{64U * 1024U * 1024U};
  std::shared_ptr<CancelToken> cancel_token;
  std::function<bool(std::string_view)> on_chunk;
};

struct HttpResponse {
  int status{0};
  std::map<std::string, std::string> headers;
  std::string body;
  HttpError error{HttpError::none};
  std::string error_message;

  std::string header(const std::string& name) const;
  bool ok() const noexcept { return error == HttpError::none && status >= 200 && status < 300; }
};

class HttpClient {
 public:
  virtual ~HttpClient() = default;
  virtual HttpResponse get(const HttpRequest& request) = 0;
};

std::unique_ptr<HttpClient> make_platform_http_client();
std::string http_error_name(HttpError error);
HttpError http_status_error(int status);

}  // namespace kchess
