#include "http/http_client.h"

#if defined(__ANDROID__)

#include <jni.h>

#include <array>
#include <memory>
#include <string>
#include <utility>

namespace {

JavaVM* g_java_vm = nullptr;

struct EnvScope {
  JNIEnv* env{nullptr};
  bool attached{false};

  EnvScope() {
    if (g_java_vm == nullptr) return;
    if (g_java_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
      if (g_java_vm->AttachCurrentThread(&env, nullptr) == JNI_OK) attached = true;
    }
  }
  ~EnvScope() {
    if (attached && g_java_vm != nullptr) g_java_vm->DetachCurrentThread();
  }
};

std::string from_jstring(JNIEnv* env, jstring value) {
  if (value == nullptr) return {};
  const char* text = env->GetStringUTFChars(value, nullptr);
  if (text == nullptr) return {};
  std::string result(text);
  env->ReleaseStringUTFChars(value, text);
  return result;
}

kchess::HttpResponse java_failure(JNIEnv* env, const std::string& operation) {
  jthrowable exception = env->ExceptionOccurred();
  env->ExceptionClear();
  kchess::HttpError kind = kchess::HttpError::transport;
  const auto is_kind = [&](const char* name) {
    jclass type = env->FindClass(name);
    if (type == nullptr) {
      env->ExceptionClear();
      return false;
    }
    const bool match = exception != nullptr && env->IsInstanceOf(exception, type);
    env->DeleteLocalRef(type);
    return match;
  };
  if (is_kind("java/net/SocketTimeoutException")) {
    kind = kchess::HttpError::timeout;
  } else if (is_kind("java/net/UnknownHostException")) {
    kind = kchess::HttpError::dns;
  } else if (is_kind("javax/net/ssl/SSLException")) {
    kind = kchess::HttpError::tls;
  } else if (is_kind("java/net/ConnectException")
             || is_kind("java/net/NoRouteToHostException")) {
    kind = kchess::HttpError::offline;
  }
  std::string detail;
  if (exception != nullptr) {
    jclass throwable = env->FindClass("java/lang/Throwable");
    jmethodID to_string = throwable == nullptr
        ? nullptr : env->GetMethodID(throwable, "toString", "()Ljava/lang/String;");
    if (to_string != nullptr) {
      auto description = static_cast<jstring>(env->CallObjectMethod(exception, to_string));
      if (!env->ExceptionCheck()) detail = from_jstring(env, description);
      else env->ExceptionClear();
      if (description != nullptr) env->DeleteLocalRef(description);
    }
    if (throwable != nullptr) env->DeleteLocalRef(throwable);
    env->DeleteLocalRef(exception);
  }
  return {.error = kind,
          .error_message = operation + " failed" + (detail.empty() ? "" : ": " + detail)};
}

bool is_redirect(const int status) {
  return status == 301 || status == 302 || status == 303 || status == 307 || status == 308;
}

}  // namespace

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void*) {
  g_java_vm = vm;
  return JNI_VERSION_1_6;
}

namespace kchess {
namespace {

class AndroidHttpClient final : public HttpClient {
 public:
  HttpResponse get(const HttpRequest& request) override {
    if (request.cancel_token && request.cancel_token->is_cancelled()) {
      return {.error = HttpError::cancelled, .error_message = "request cancelled"};
    }
    EnvScope scope;
    JNIEnv* env = scope.env;
    if (env == nullptr) {
      return {.error = HttpError::transport, .error_message = "Android JVM is unavailable"};
    }
    if (env->PushLocalFrame(64) != JNI_OK) {
      return {.error = HttpError::transport, .error_message = "JNI local frame allocation failed"};
    }
    struct FrameGuard {
      JNIEnv* env;
      ~FrameGuard() { env->PopLocalFrame(nullptr); }
    } frame{env};

    jclass url_class = env->FindClass("java/net/URL");
    jclass connection_class = env->FindClass("javax/net/ssl/HttpsURLConnection");
    jclass input_class = env->FindClass("java/io/InputStream");
    if (env->ExceptionCheck() || url_class == nullptr || connection_class == nullptr
        || input_class == nullptr) {
      return java_failure(env, "loading Android HTTPS classes");
    }
    const jmethodID url_constructor = env->GetMethodID(
        url_class, "<init>", "(Ljava/lang/String;)V");
    const jmethodID relative_url_constructor = env->GetMethodID(
        url_class, "<init>", "(Ljava/net/URL;Ljava/lang/String;)V");
    const jmethodID open_connection = env->GetMethodID(
        url_class, "openConnection", "()Ljava/net/URLConnection;");
    const jmethodID get_protocol = env->GetMethodID(
        url_class, "getProtocol", "()Ljava/lang/String;");
    const jmethodID set_follow_redirects = env->GetMethodID(
        connection_class, "setInstanceFollowRedirects", "(Z)V");
    const jmethodID set_connect_timeout = env->GetMethodID(
        connection_class, "setConnectTimeout", "(I)V");
    const jmethodID set_read_timeout = env->GetMethodID(
        connection_class, "setReadTimeout", "(I)V");
    const jmethodID set_method = env->GetMethodID(
        connection_class, "setRequestMethod", "(Ljava/lang/String;)V");
    const jmethodID set_property = env->GetMethodID(
        connection_class, "setRequestProperty", "(Ljava/lang/String;Ljava/lang/String;)V");
    const jmethodID connect = env->GetMethodID(connection_class, "connect", "()V");
    const jmethodID response_code = env->GetMethodID(
        connection_class, "getResponseCode", "()I");
    const jmethodID header_field = env->GetMethodID(
        connection_class, "getHeaderField", "(Ljava/lang/String;)Ljava/lang/String;");
    const jmethodID input_stream = env->GetMethodID(
        connection_class, "getInputStream", "()Ljava/io/InputStream;");
    const jmethodID error_stream = env->GetMethodID(
        connection_class, "getErrorStream", "()Ljava/io/InputStream;");
    const jmethodID disconnect = env->GetMethodID(connection_class, "disconnect", "()V");
    const jmethodID read = env->GetMethodID(input_class, "read", "([B)I");
    const jmethodID close = env->GetMethodID(input_class, "close", "()V");
    if (env->ExceptionCheck()) return java_failure(env, "resolving Android HTTPS methods");

    jstring initial_text = env->NewStringUTF(request.url.c_str());
    jobject current_url = env->NewObject(url_class, url_constructor, initial_text);
    if (env->ExceptionCheck() || current_url == nullptr) {
      return java_failure(env, "parsing HTTPS URL");
    }
    for (int redirect_count = 0; redirect_count <= request.max_redirects; ++redirect_count) {
      auto protocol = static_cast<jstring>(env->CallObjectMethod(current_url, get_protocol));
      if (env->ExceptionCheck()) return java_failure(env, "reading URL protocol");
      if (from_jstring(env, protocol) != "https") {
        return {.error = HttpError::invalid_response,
                .error_message = "only HTTPS URLs are allowed"};
      }
      jobject connection = env->CallObjectMethod(current_url, open_connection);
      if (env->ExceptionCheck() || connection == nullptr) {
        return java_failure(env, "opening HTTPS connection");
      }
      if (!env->IsInstanceOf(connection, connection_class)) {
        return {.error = HttpError::tls, .error_message = "URL is not an HTTPS connection"};
      }
      env->CallVoidMethod(connection, set_follow_redirects, JNI_FALSE);
      env->CallVoidMethod(connection, set_connect_timeout, request.timeout_ms);
      env->CallVoidMethod(connection, set_read_timeout, request.timeout_ms);
      jstring method = env->NewStringUTF("GET");
      env->CallVoidMethod(connection, set_method, method);
      for (const auto& [name, value] : request.headers) {
        jstring key = env->NewStringUTF(name.c_str());
        jstring content = env->NewStringUTF(value.c_str());
        env->CallVoidMethod(connection, set_property, key, content);
        env->DeleteLocalRef(key);
        env->DeleteLocalRef(content);
      }
      if (env->ExceptionCheck()) return java_failure(env, "configuring HTTPS request");
      env->CallVoidMethod(connection, connect);
      const jint status = env->CallIntMethod(connection, response_code);
      if (env->ExceptionCheck()) return java_failure(env, "receiving HTTPS response");

      HttpResponse response;
      response.status = status;
      constexpr std::array<std::pair<const char*, const char*>, 8> headers{{
          {"etag", "ETag"},
          {"last-modified", "Last-Modified"},
          {"cache-control", "Cache-Control"},
          {"content-type", "Content-Type"},
          {"content-encoding", "Content-Encoding"},
          {"retry-after", "Retry-After"},
          {"location", "Location"},
          {"date", "Date"},
      }};
      for (const auto& [key, java_name] : headers) {
        jstring header_name = env->NewStringUTF(java_name);
        auto header = static_cast<jstring>(
            env->CallObjectMethod(connection, header_field, header_name));
        env->DeleteLocalRef(header_name);
        if (env->ExceptionCheck()) return java_failure(env, "reading HTTPS headers");
        const auto value = from_jstring(env, header);
        if (!value.empty()) response.headers.emplace(key, value);
        if (header != nullptr) env->DeleteLocalRef(header);
      }
      if (is_redirect(status)) {
        const auto location = response.header("location");
        env->CallVoidMethod(connection, disconnect);
        if (location.empty()) return response;
        if (redirect_count == request.max_redirects) {
          return {.status = status, .headers = std::move(response.headers),
                  .error = HttpError::invalid_response,
                  .error_message = "HTTP redirect limit exceeded"};
        }
        jstring redirect_text = env->NewStringUTF(location.c_str());
        jobject redirected = env->NewObject(
            url_class, relative_url_constructor, current_url, redirect_text);
        if (env->ExceptionCheck() || redirected == nullptr) {
          return java_failure(env, "resolving HTTPS redirect");
        }
        current_url = redirected;
        continue;
      }

      jobject stream = nullptr;
      if (status >= 200 && status < 300) {
        stream = env->CallObjectMethod(connection, input_stream);
      } else if (status >= 400) {
        stream = env->CallObjectMethod(connection, error_stream);
      }
      if (env->ExceptionCheck()) return java_failure(env, "opening HTTPS response stream");
      if (stream == nullptr) {
        env->CallVoidMethod(connection, disconnect);
        return response;
      }
      if (response.header("content-encoding") == "gzip") {
        jclass gzip_class = env->FindClass("java/util/zip/GZIPInputStream");
        const jmethodID gzip_constructor = gzip_class == nullptr ? nullptr : env->GetMethodID(
            gzip_class, "<init>", "(Ljava/io/InputStream;)V");
        if (env->ExceptionCheck() || gzip_constructor == nullptr) {
          return java_failure(env, "loading gzip decoder");
        }
        stream = env->NewObject(gzip_class, gzip_constructor, stream);
        if (env->ExceptionCheck() || stream == nullptr) {
          return java_failure(env, "opening gzip response stream");
        }
      }
      jbyteArray buffer = env->NewByteArray(32 * 1024);
      std::array<char, 32U * 1024U> native_buffer{};
      std::size_t total = 0;
      while (true) {
        if (request.cancel_token && request.cancel_token->is_cancelled()) {
          env->CallVoidMethod(stream, close);
          env->CallVoidMethod(connection, disconnect);
          return {.status = response.status, .headers = std::move(response.headers),
                  .error = HttpError::cancelled, .error_message = "request cancelled"};
        }
        const jint count = env->CallIntMethod(stream, read, buffer);
        if (env->ExceptionCheck()) return java_failure(env, "reading HTTPS response");
        if (count < 0) break;
        if (count == 0) continue;
        total += static_cast<std::size_t>(count);
        if (total > request.max_body_bytes) {
          env->CallVoidMethod(stream, close);
          env->CallVoidMethod(connection, disconnect);
          return {.status = response.status, .headers = std::move(response.headers),
                  .error = HttpError::invalid_response,
                  .error_message = "HTTP response exceeded configured size limit"};
        }
        env->GetByteArrayRegion(
            buffer, 0, count, reinterpret_cast<jbyte*>(native_buffer.data()));
        const std::string_view chunk(native_buffer.data(), static_cast<std::size_t>(count));
        if (request.on_chunk) {
          if (!request.on_chunk(chunk)) {
            env->CallVoidMethod(stream, close);
            env->CallVoidMethod(connection, disconnect);
            return {.status = response.status, .headers = std::move(response.headers),
                    .error = HttpError::cancelled,
                    .error_message = "response stream cancelled"};
          }
        } else {
          response.body.append(chunk);
        }
      }
      env->CallVoidMethod(stream, close);
      env->CallVoidMethod(connection, disconnect);
      return response;
    }
    return {.error = HttpError::invalid_response, .error_message = "redirect handling failed"};
  }
};

}  // namespace

std::unique_ptr<HttpClient> make_android_http_client() {
  return std::make_unique<AndroidHttpClient>();
}

}  // namespace kchess

#endif
