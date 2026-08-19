#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace kchess {

enum class SettingValueType : std::uint8_t {
  integer,
  boolean,
  string_enum,
};

enum class SettingScope : std::uint8_t {
  app,
  profile,
};

struct SettingDescriptor {
  std::string_view key;
  SettingValueType type;
  SettingScope scope;
  bool cache_relevant;
  // Stable token used by the existing analysis hash. Empty for non-cache settings.
  // Do not rename a token unless cached analyses should intentionally invalidate.
  std::string_view cache_token{};
  int min_int{0};
  int max_int{0};
  int default_int{0};
  bool default_bool{false};
  std::string_view default_string{};
};

inline constexpr SettingDescriptor kDepthSetting{
    .key = "depth",
    .type = SettingValueType::integer,
    .scope = SettingScope::profile,
    .cache_relevant = true,
    .cache_token = "depth",
    .min_int = 1,
    .max_int = 64,
    .default_int = 18,
};

inline constexpr SettingDescriptor kMultiPvSetting{
    .key = "multiPv",
    .type = SettingValueType::integer,
    .scope = SettingScope::profile,
    .cache_relevant = true,
    .cache_token = "multipv",
    .min_int = 1,
    .max_int = 8,
    .default_int = 3,
};

inline constexpr SettingDescriptor kTimeLimitSetting{
    .key = "timeLimitSeconds",
    .type = SettingValueType::integer,
    .scope = SettingScope::profile,
    .cache_relevant = true,
    .cache_token = "time",
    .min_int = 0,
    .max_int = 60,
    .default_int = 0,
};


#if defined(__ANDROID__)
inline constexpr int kDefaultEngineThreads = 2;
inline constexpr int kDefaultEngineHashMb = 128;
#else
inline constexpr int kDefaultEngineThreads = 4;
inline constexpr int kDefaultEngineHashMb = 256;
#endif

inline constexpr SettingDescriptor kThreadsSetting{
    .key = "threads", .type = SettingValueType::integer, .scope = SettingScope::profile,
    .cache_relevant = true, .cache_token = "threads", .min_int = 1, .max_int = 32,
    .default_int = kDefaultEngineThreads,
};

inline constexpr SettingDescriptor kHashMbSetting{
    .key = "hashMb", .type = SettingValueType::integer, .scope = SettingScope::profile,
    .cache_relevant = true, .cache_token = "hash", .min_int = 16, .max_int = 2048,
    .default_int = kDefaultEngineHashMb,
};

inline constexpr SettingDescriptor kShowBoardArrowsSetting{
    .key = "showBoardArrows",
    .type = SettingValueType::boolean,
    .scope = SettingScope::app,
    .cache_relevant = false,
    .default_bool = true,
};


inline constexpr SettingDescriptor kShowBestMoveArrowSetting{
    .key = "showBestMoveArrow",
    .type = SettingValueType::boolean,
    .scope = SettingScope::app,
    .cache_relevant = false,
    .default_bool = true,
};

inline constexpr SettingDescriptor kShowThreatArrowSetting{
    .key = "showThreatArrow",
    .type = SettingValueType::boolean,
    .scope = SettingScope::app,
    .cache_relevant = false,
    .default_bool = true,
};

inline constexpr SettingDescriptor kShowEvaluationBarSetting{
    .key = "showEvaluationBar",
    .type = SettingValueType::boolean,
    .scope = SettingScope::app,
    .cache_relevant = false,
    .default_bool = true,
};

inline constexpr SettingDescriptor kShowEngineLinesSetting{
    .key = "showEngineLines",
    .type = SettingValueType::boolean,
    .scope = SettingScope::app,
    .cache_relevant = false,
    .default_bool = true,
};

inline constexpr SettingDescriptor kShowClassificationsSetting{
    .key = "showClassifications",
    .type = SettingValueType::boolean,
    .scope = SettingScope::app,
    .cache_relevant = false,
    .default_bool = true,
};

inline constexpr SettingDescriptor kShowAccuracySetting{
    .key = "showAccuracy",
    .type = SettingValueType::boolean,
    .scope = SettingScope::app,
    .cache_relevant = false,
    .default_bool = true,
};

inline constexpr SettingDescriptor kShowTheorySetting{
    .key = "showTheory",
    .type = SettingValueType::boolean,
    .scope = SettingScope::app,
    .cache_relevant = false,
    .default_bool = true,
};

inline constexpr SettingDescriptor kThemeModeSetting{
    .key = "themeMode",
    .type = SettingValueType::string_enum,
    .scope = SettingScope::app,
    .cache_relevant = false,
    .default_string = "system",
};

inline constexpr SettingDescriptor kShowBoardCoordinatesSetting{
    .key = "showBoardCoordinates",
    .type = SettingValueType::boolean,
    .scope = SettingScope::app,
    .cache_relevant = false,
    .default_bool = true,
};

inline constexpr SettingDescriptor kHighlightLastMoveSetting{
    .key = "highlightLastMove",
    .type = SettingValueType::boolean,
    .scope = SettingScope::app,
    .cache_relevant = false,
    .default_bool = true,
};

inline constexpr SettingDescriptor kHighlightSelectedSquareSetting{
    .key = "highlightSelectedSquare",
    .type = SettingValueType::boolean,
    .scope = SettingScope::app,
    .cache_relevant = false,
    .default_bool = true,
};

inline constexpr SettingDescriptor kAutoSyncOnlineSetting{
    .key = "autoSyncOnline",
    .type = SettingValueType::boolean,
    .scope = SettingScope::app,
    .cache_relevant = false,
    .default_bool = true,
};

inline constexpr SettingDescriptor kConfirmBeforeDeleteSetting{
    .key = "confirmBeforeDelete",
    .type = SettingValueType::boolean,
    .scope = SettingScope::app,
    .cache_relevant = false,
    .default_bool = true,
};

inline constexpr SettingDescriptor kUseGlobalAnalysisCacheSetting{
    .key = "useGlobalAnalysisCache",
    .type = SettingValueType::boolean,
    .scope = SettingScope::app,
    .cache_relevant = false,
    .default_bool = true,
};

inline constexpr SettingDescriptor kDiagnosticLoggingSetting{
    .key = "diagnosticLogging",
    .type = SettingValueType::boolean,
    .scope = SettingScope::app,
    .cache_relevant = false,
    .default_bool = true,
};

inline constexpr SettingDescriptor kLocaleSetting{
    .key = "locale",
    .type = SettingValueType::string_enum,
    .scope = SettingScope::app,
    .cache_relevant = false,
    .default_string = "de",
};

inline constexpr std::array<SettingDescriptor, 22> kSettingsRegistry{
    kDepthSetting,
    kMultiPvSetting,
    kTimeLimitSetting,
    kThreadsSetting,
    kHashMbSetting,
    kShowBoardArrowsSetting,
    kShowBestMoveArrowSetting,
    kShowThreatArrowSetting,
    kShowEvaluationBarSetting,
    kShowEngineLinesSetting,
    kShowClassificationsSetting,
    kShowAccuracySetting,
    kShowTheorySetting,
    kThemeModeSetting,
    kShowBoardCoordinatesSetting,
    kHighlightLastMoveSetting,
    kHighlightSelectedSquareSetting,
    kAutoSyncOnlineSetting,
    kConfirmBeforeDeleteSetting,
    kUseGlobalAnalysisCacheSetting,
    kDiagnosticLoggingSetting,
    kLocaleSetting,
};

constexpr bool valid_integer_setting(const SettingDescriptor& descriptor, const int value) {
  return descriptor.type == SettingValueType::integer && value >= descriptor.min_int
      && value <= descriptor.max_int;
}

constexpr const SettingDescriptor* find_setting(const std::string_view key) {
  for (const auto& descriptor : kSettingsRegistry) {
    if (descriptor.key == key) return &descriptor;
  }
  return nullptr;
}

}  // namespace kchess
