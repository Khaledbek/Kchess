#include "diagnostics/logger.h"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>

namespace kchess::diagnostics {
namespace {

constexpr std::uintmax_t kMaxLogBytes = 1024 * 1024;

std::mutex g_log_mutex;
std::filesystem::path g_log_file;
bool g_configured = false;
bool g_enabled = true;

const char* level_name(const LogLevel level) noexcept {
  switch (level) {
    case LogLevel::debug: return "DEBUG";
    case LogLevel::info: return "INFO";
    case LogLevel::warning: return "WARN";
    case LogLevel::error: return "ERROR";
  }
  return "INFO";
}

std::string timestamp_utc() {
  const auto now = std::chrono::system_clock::now();
  const std::time_t value = std::chrono::system_clock::to_time_t(now);
  std::tm utc{};
#if defined(_WIN32)
  gmtime_s(&utc, &value);
#else
  gmtime_r(&value, &utc);
#endif
  std::ostringstream output;
  output << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
  return output.str();
}

void rotate_if_needed() noexcept {
  try {
    if (!std::filesystem::exists(g_log_file)
        || std::filesystem::file_size(g_log_file) < kMaxLogBytes) {
      return;
    }
    const auto previous = g_log_file.parent_path() / "kchess.previous.log";
    std::error_code ignored;
    std::filesystem::remove(previous, ignored);
    ignored.clear();
    std::filesystem::rename(g_log_file, previous, ignored);
  } catch (...) {
  }
}

}  // namespace

void configure_logging(const std::filesystem::path& data_directory) noexcept {
  std::lock_guard lock(g_log_mutex);
  try {
    const auto log_directory = data_directory / "logs";
    std::filesystem::create_directories(log_directory);
    g_log_file = log_directory / "kchess.log";
    g_configured = true;
    rotate_if_needed();
  } catch (...) {
    g_configured = false;
    g_log_file.clear();
  }
}

void set_enabled(const bool enabled) noexcept {
  std::lock_guard lock(g_log_mutex);
  g_enabled = enabled;
}

void log(
    const LogLevel level,
    const std::string_view component,
    const std::string_view message) noexcept {
#ifndef NDEBUG
  constexpr bool kDebugEnabled = true;
#else
  constexpr bool kDebugEnabled = false;
#endif
  if (level == LogLevel::debug && !kDebugEnabled) return;

  std::lock_guard lock(g_log_mutex);
  if (!g_enabled || !g_configured || g_log_file.empty()) return;
  try {
    rotate_if_needed();
    std::ofstream output(g_log_file, std::ios::app);
    if (!output) return;
    output << timestamp_utc() << " [" << level_name(level) << "] ["
           << component << "] " << message << '\n';
  } catch (...) {
  }
}

}  // namespace kchess::diagnostics
