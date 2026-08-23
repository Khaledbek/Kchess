#pragma once

#include <string>

namespace kchess {

class Database;

// Owns validation and persistence for application settings.
// Core deliberately delegates here so future settings do not expand Core's
// orchestration surface or leak persistence details into the FFI layer.
class SettingsService {
 public:
  explicit SettingsService(Database& database) noexcept : database_(database) {}

  std::string settings_json() const;
  void set_engine_settings(int depth, int multi_pv, int time_limit_seconds);
  void set_analysis_depth_range(int minimum_depth, int maximum_depth);
  void set_engine_resources(int threads, int hash_mb);
  void set_show_board_arrows(bool enabled);
  void set_boolean_setting(const std::string& key, bool enabled);
  void set_theme_mode(const std::string& mode);
  void set_locale(const std::string& locale);

 private:
  Database& database_;
};

}  // namespace kchess
