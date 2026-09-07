#include "services/settings_service.h"

#include <algorithm>
#include <stdexcept>
#include <thread>

#include <nlohmann/json.hpp>

#include "core/settings_registry.h"
#include "diagnostics/logger.h"
#include "persistence/database.h"

namespace kchess {
namespace {

int maximum_engine_threads() noexcept {
  const auto logical = std::thread::hardware_concurrency();
  const int half = logical == 0 ? kThreadsSetting.max_int
                                : static_cast<int>(logical / 2);
  return std::clamp(half, kThreadsSetting.min_int, kThreadsSetting.max_int);
}

}  // namespace

std::string SettingsService::settings_json() const {
  const auto settings = database_.settings();
  return nlohmann::json{
      {"minAnalysisDepth", settings.min_analysis_depth},
      {"depth", settings.depth},
      {"maxAnalysisDepth", settings.depth},
      {"multiPv", settings.multi_pv},
      {"timeLimitSeconds", settings.time_limit_seconds},
      {"threads", settings.threads},
      {"maxThreads", maximum_engine_threads()},
      {"hashMb", settings.hash_mb},
      {"showBoardArrows", settings.show_board_arrows},
      {"showBestMoveArrow", settings.show_board_arrows},
      {"showThreatArrow", settings.show_threat_arrow},
      {"showEvaluationBar", settings.show_evaluation_bar},
      {"showEngineLines", settings.show_engine_lines},
      {"showClassifications", settings.show_classifications},
      {"showAccuracy", settings.show_accuracy},
      {"showTheory", settings.show_theory},
      {"showResultSymbols", settings.show_result_symbols},
      {"adaptiveEarlyStop", settings.adaptive_early_stop},
      {"showBoardCoordinates", settings.show_board_coordinates},
      {"highlightLastMove", settings.highlight_last_move},
      {"highlightSelectedSquare", settings.highlight_selected_square},
      {"autoSyncOnline", settings.auto_sync_online},
      {"confirmBeforeDelete", settings.confirm_before_delete},
      {"useGlobalAnalysisCache", settings.use_global_analysis_cache},
      {"diagnosticLogging", settings.diagnostic_logging},
      {"themeMode", settings.theme_mode},
      {"locale", settings.locale},
  }.dump();
}

void SettingsService::set_engine_settings(
    const int depth, const int multi_pv, const int time_limit_seconds) {
  if (!valid_integer_setting(kDepthSetting, depth)) {
    throw std::invalid_argument("depth must be between 1 and 64");
  }
  if (!valid_integer_setting(kMultiPvSetting, multi_pv)) {
    throw std::invalid_argument("number of lines must be between 1 and 8");
  }
  if (!valid_integer_setting(kTimeLimitSetting, time_limit_seconds)) {
    throw std::invalid_argument("time limit must be between 0 and 60 seconds");
  }
  database_.set_engine_settings(depth, multi_pv, time_limit_seconds);
}

void SettingsService::set_analysis_depth_range(
    const int minimum_depth, const int maximum_depth) {
  if (!valid_integer_setting(kDepthSetting, minimum_depth)
      || !valid_integer_setting(kDepthSetting, maximum_depth)) {
    throw std::invalid_argument("analysis depth must be between 1 and 64");
  }
  const int normalized_minimum = std::min(minimum_depth, maximum_depth);
  const auto current = database_.settings();
  database_.set_setting("minAnalysisDepth", std::to_string(normalized_minimum));
  database_.set_engine_settings(maximum_depth, current.multi_pv, current.time_limit_seconds);
}

void SettingsService::set_engine_resources(const int threads, const int hash_mb) {
  if (!valid_integer_setting(kThreadsSetting, threads)
      || threads > maximum_engine_threads()) {
    throw std::invalid_argument("threads exceed the safe device limit");
  }
  if (!valid_integer_setting(kHashMbSetting, hash_mb))
    throw std::invalid_argument("hash must be between 16 and 2048 MB");
  database_.set_engine_resources(threads, hash_mb);
}

void SettingsService::set_show_board_arrows(const bool enabled) {
  // Legacy export kept for ABI compatibility.  The current UI calls this
  // setting "Best move arrow".
  database_.set_setting("showBoardArrows", enabled ? "true" : "false");
  database_.set_setting("showBestMoveArrow", enabled ? "true" : "false");
}

void SettingsService::set_boolean_setting(const std::string& key, const bool enabled) {
  const auto* descriptor = find_setting(key);
  if (descriptor == nullptr || descriptor->type != SettingValueType::boolean
      || descriptor->scope != SettingScope::app) {
    throw std::invalid_argument("unknown or non-boolean app setting");
  }
  if (key == "showBoardArrows") {
    set_show_board_arrows(enabled);
    return;
  }
  database_.set_setting(key, enabled ? "true" : "false");
  if (key == "diagnosticLogging") {
    diagnostics::set_enabled(enabled);
  }
}

void SettingsService::set_theme_mode(const std::string& mode) {
  if (mode != "system" && mode != "light" && mode != "dark") {
    throw std::invalid_argument("invalid theme mode");
  }
  database_.set_setting("themeMode", mode);
}

void SettingsService::set_locale(const std::string& locale) {
  if (locale != "de" && locale != "en" && locale != "ar") {
    throw std::invalid_argument("invalid locale");
  }
  database_.set_setting("locale", locale);
}

}  // namespace kchess
