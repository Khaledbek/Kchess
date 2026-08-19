#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace kchess::diagnostics {

enum class LogLevel {
  debug,
  info,
  warning,
  error,
};

// Process-wide bounded diagnostic logger. It intentionally accepts only
// already-sanitized messages; callers should avoid raw PGN/FEN/provider
// payloads or other user data.
void configure_logging(const std::filesystem::path& data_directory) noexcept;
void set_enabled(bool enabled) noexcept;
void log(LogLevel level, std::string_view component, std::string_view message) noexcept;

inline void debug(std::string_view component, std::string_view message) noexcept {
  log(LogLevel::debug, component, message);
}
inline void info(std::string_view component, std::string_view message) noexcept {
  log(LogLevel::info, component, message);
}
inline void warning(std::string_view component, std::string_view message) noexcept {
  log(LogLevel::warning, component, message);
}
inline void error(std::string_view component, std::string_view message) noexcept {
  log(LogLevel::error, component, message);
}

}  // namespace kchess::diagnostics
