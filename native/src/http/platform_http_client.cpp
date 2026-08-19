#include "http/http_client.h"

#include <array>
#include <memory>
#include <stdexcept>
#include <utility>

#if defined(_WIN32)
#  define NOMINMAX
#  include <windows.h>
#  include <winhttp.h>
#endif

#if defined(__ANDROID__)
namespace kchess {
std::unique_ptr<HttpClient> make_android_http_client();
}
#endif

namespace kchess {
namespace {

#if defined(_WIN32)

struct WinHttpCloser {
  void operator()(void* handle) const noexcept {
    if (handle != nullptr) WinHttpCloseHandle(handle);
  }
};
using WinHttpHandle = std::unique_ptr<void, WinHttpCloser>;

std::wstring widen(const std::string& value) {
  if (value.empty()) return {};
  const int size = MultiByteToWideChar(
      CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
  if (size <= 0) throw std::invalid_argument("invalid UTF-8 HTTP value");
  std::wstring result(static_cast<std::size_t>(size), L'\0');
  MultiByteToWideChar(
      CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
      result.data(), size);
  return result;
}

std::string narrow(const std::wstring& value) {
  if (value.empty()) return {};
  const int size = WideCharToMultiByte(
      CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
      nullptr, 0, nullptr, nullptr);
  if (size <= 0) return {};
  std::string result(static_cast<std::size_t>(size), '\0');
  WideCharToMultiByte(
      CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
      result.data(), size, nullptr, nullptr);
  return result;
}

HttpError windows_error(const DWORD error) {
  switch (error) {
    case ERROR_WINHTTP_TIMEOUT: return HttpError::timeout;
    case ERROR_WINHTTP_NAME_NOT_RESOLVED: return HttpError::dns;
    case ERROR_WINHTTP_SECURE_FAILURE:
    case ERROR_WINHTTP_CLIENT_AUTH_CERT_NEEDED: return HttpError::tls;
    case ERROR_WINHTTP_OPERATION_CANCELLED: return HttpError::cancelled;
    case ERROR_WINHTTP_CANNOT_CONNECT:
    case ERROR_WINHTTP_CONNECTION_ERROR: return HttpError::offline;
    default: return HttpError::transport;
  }
}

HttpResponse winhttp_failure(const DWORD error, const std::string& operation) {
  return {
      .error = windows_error(error),
      .error_message = operation + " failed (WinHTTP " + std::to_string(error) + ")",
  };
}

std::string query_header(void* request, const wchar_t* name) {
  DWORD size = 0;
  WinHttpQueryHeaders(
      request, WINHTTP_QUERY_CUSTOM, name, WINHTTP_NO_OUTPUT_BUFFER, &size,
      WINHTTP_NO_HEADER_INDEX);
  if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || size < sizeof(wchar_t)) return {};
  std::wstring value(size / sizeof(wchar_t), L'\0');
  if (!WinHttpQueryHeaders(
          request, WINHTTP_QUERY_CUSTOM, name, value.data(), &size,
          WINHTTP_NO_HEADER_INDEX)) {
    return {};
  }
  value.resize(size / sizeof(wchar_t));
  while (!value.empty() && value.back() == L'\0') value.pop_back();
  return narrow(value);
}

class WinHttpClient final : public HttpClient {
 public:
  HttpResponse get(const HttpRequest& request) override {
    if (request.cancel_token && request.cancel_token->is_cancelled()) {
      return {.error = HttpError::cancelled, .error_message = "request cancelled"};
    }
    const auto url = widen(request.url);
    URL_COMPONENTS components{};
    components.dwStructSize = sizeof(components);
    components.dwHostNameLength = static_cast<DWORD>(-1);
    components.dwUrlPathLength = static_cast<DWORD>(-1);
    components.dwExtraInfoLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(url.c_str(), static_cast<DWORD>(url.size()), 0, &components)
        || components.nScheme != INTERNET_SCHEME_HTTPS) {
      return {.error = HttpError::invalid_response, .error_message = "only HTTPS URLs are allowed"};
    }
    const std::wstring host(components.lpszHostName, components.dwHostNameLength);
    std::wstring path(components.lpszUrlPath, components.dwUrlPathLength);
    if (components.dwExtraInfoLength != 0) {
      path.append(components.lpszExtraInfo, components.dwExtraInfoLength);
    }

    WinHttpHandle session(WinHttpOpen(
        L"KChess/0.1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session) return winhttp_failure(GetLastError(), "WinHttpOpen");
    WinHttpSetTimeouts(
        session.get(), request.timeout_ms, request.timeout_ms,
        request.timeout_ms, request.timeout_ms);
    DWORD decompression = WINHTTP_DECOMPRESSION_FLAG_GZIP | WINHTTP_DECOMPRESSION_FLAG_DEFLATE;
    WinHttpSetOption(
        session.get(), WINHTTP_OPTION_DECOMPRESSION, &decompression, sizeof(decompression));
    DWORD max_redirects = static_cast<DWORD>(request.max_redirects);
    WinHttpSetOption(
        session.get(), WINHTTP_OPTION_MAX_HTTP_AUTOMATIC_REDIRECTS,
        &max_redirects, sizeof(max_redirects));

    WinHttpHandle connection(WinHttpConnect(
        session.get(), host.c_str(), components.nPort, 0));
    if (!connection) return winhttp_failure(GetLastError(), "WinHttpConnect");
    const wchar_t* accepted[] = {L"application/json", L"application/x-ndjson", L"*/*", nullptr};
    WinHttpHandle handle(WinHttpOpenRequest(
        connection.get(), L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
        accepted, WINHTTP_FLAG_SECURE));
    if (!handle) return winhttp_failure(GetLastError(), "WinHttpOpenRequest");
    DWORD redirect_policy = WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;
    WinHttpSetOption(
        handle.get(), WINHTTP_OPTION_REDIRECT_POLICY, &redirect_policy,
        sizeof(redirect_policy));
    for (const auto& [name, value] : request.headers) {
      const auto header = widen(name + ": " + value);
      if (!WinHttpAddRequestHeaders(
              handle.get(), header.c_str(), static_cast<DWORD>(header.size()),
              WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE)) {
        return winhttp_failure(GetLastError(), "WinHttpAddRequestHeaders");
      }
    }
    if (!WinHttpSendRequest(
            handle.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
      return winhttp_failure(GetLastError(), "WinHttpSendRequest");
    }
    if (!WinHttpReceiveResponse(handle.get(), nullptr)) {
      return winhttp_failure(GetLastError(), "WinHttpReceiveResponse");
    }
    DWORD status = 0;
    DWORD status_size = sizeof(status);
    if (!WinHttpQueryHeaders(
            handle.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_size,
            WINHTTP_NO_HEADER_INDEX)) {
      return winhttp_failure(GetLastError(), "WinHttpQueryHeaders");
    }
    HttpResponse response;
    response.status = static_cast<int>(status);
    constexpr std::array<std::pair<const char*, const wchar_t*>, 8> headers{{
        {"etag", L"ETag"},
        {"last-modified", L"Last-Modified"},
        {"cache-control", L"Cache-Control"},
        {"content-type", L"Content-Type"},
        {"content-encoding", L"Content-Encoding"},
        {"retry-after", L"Retry-After"},
        {"location", L"Location"},
        {"date", L"Date"},
    }};
    for (const auto& [key, name] : headers) {
      const auto value = query_header(handle.get(), name);
      if (!value.empty()) response.headers.emplace(key, value);
    }
    std::array<char, 32U * 1024U> buffer{};
    std::size_t total = 0;
    while (true) {
      if (request.cancel_token && request.cancel_token->is_cancelled()) {
        return {.status = response.status, .headers = std::move(response.headers),
                .error = HttpError::cancelled, .error_message = "request cancelled"};
      }
      DWORD read = 0;
      if (!WinHttpReadData(
              handle.get(), buffer.data(), static_cast<DWORD>(buffer.size()), &read)) {
        return winhttp_failure(GetLastError(), "WinHttpReadData");
      }
      if (read == 0) break;
      total += read;
      if (total > request.max_body_bytes) {
        return {.status = response.status, .headers = std::move(response.headers),
                .error = HttpError::invalid_response,
                .error_message = "HTTP response exceeded configured size limit"};
      }
      const std::string_view chunk(buffer.data(), read);
      if (request.on_chunk) {
        if (!request.on_chunk(chunk)) {
          return {.status = response.status, .headers = std::move(response.headers),
                  .error = HttpError::cancelled, .error_message = "response stream cancelled"};
        }
      } else {
        response.body.append(chunk);
      }
    }
    return response;
  }
};

#else

class UnsupportedHttpClient final : public HttpClient {
 public:
  HttpResponse get(const HttpRequest&) override {
    return {
        .error = HttpError::transport,
        .error_message = "No platform HTTP transport is available",
    };
  }
};

#endif

}  // namespace

std::unique_ptr<HttpClient> make_platform_http_client() {
#if defined(_WIN32)
  return std::make_unique<WinHttpClient>();
#elif defined(__ANDROID__)
  return make_android_http_client();
#else
  return std::make_unique<UnsupportedHttpClient>();
#endif
}

}  // namespace kchess
