#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "core/settings_registry.h"

namespace kchess {

enum class ProfileType : std::int32_t {
  chess_com = 0,
  lichess = 1,
  local_pgn_fen = 2,
};

struct Profile {
  std::string id;
  ProfileType type;
  std::string display_name;
  std::optional<std::string> provider_username;
  std::string avatar_asset;
  std::optional<std::string> title;
  std::optional<std::string> avatar_url;
  std::optional<std::string> avatar_file;
  std::optional<std::string> flair;
  std::optional<std::int64_t> joined_at;
  std::optional<std::int64_t> last_online_at;
  std::optional<std::string> country;
  std::optional<std::string> location;
  std::optional<std::string> public_url;
  std::optional<std::string> provider_specific_id;
  std::optional<int> followers;
  std::optional<int> fide;
  std::optional<int> provider_games;
  std::optional<int> provider_wins;
  std::optional<int> provider_losses;
  std::optional<int> provider_draws;
  std::optional<std::int64_t> play_time_seconds;
  std::optional<std::string> provider_status;
  bool provider_disabled{false};
  bool provider_tos_violation{false};
  std::int64_t profile_fetched_at{0};
  std::int64_t created_at;
  std::int64_t last_opened_at;
};

struct AppSettings {
  // Compatibility aliases. The registry is the single source of truth for
  // ranges/defaults and whether a setting affects analysis-cache compatibility.
  static constexpr int min_depth = kDepthSetting.min_int;
  static constexpr int max_depth = kDepthSetting.max_int;
  static constexpr int default_depth = kDepthSetting.default_int;
  static constexpr int min_multi_pv = kMultiPvSetting.min_int;
  static constexpr int max_multi_pv = kMultiPvSetting.max_int;
  static constexpr int default_multi_pv = kMultiPvSetting.default_int;
  static constexpr int min_time_limit_seconds = kTimeLimitSetting.min_int;
  static constexpr int max_time_limit_seconds = kTimeLimitSetting.max_int;
  static constexpr int default_time_limit_seconds = kTimeLimitSetting.default_int;

  int min_analysis_depth{12};
  int depth{default_depth};
  int multi_pv{default_multi_pv};
  int time_limit_seconds{default_time_limit_seconds};
  int threads{kThreadsSetting.default_int};
  int hash_mb{kHashMbSetting.default_int};
  bool show_board_arrows{kShowBoardArrowsSetting.default_bool};
  bool show_threat_arrow{kShowThreatArrowSetting.default_bool};
  bool show_evaluation_bar{kShowEvaluationBarSetting.default_bool};
  bool show_engine_lines{kShowEngineLinesSetting.default_bool};
  bool show_classifications{kShowClassificationsSetting.default_bool};
  bool show_accuracy{kShowAccuracySetting.default_bool};
  bool show_theory{kShowTheorySetting.default_bool};
  bool show_board_coordinates{kShowBoardCoordinatesSetting.default_bool};
  bool highlight_last_move{kHighlightLastMoveSetting.default_bool};
  bool highlight_selected_square{kHighlightSelectedSquareSetting.default_bool};
  bool auto_sync_online{kAutoSyncOnlineSetting.default_bool};
  bool confirm_before_delete{kConfirmBeforeDeleteSetting.default_bool};
  bool use_global_analysis_cache{kUseGlobalAnalysisCacheSetting.default_bool};
  bool diagnostic_logging{kDiagnosticLoggingSetting.default_bool};
  std::string theme_mode{std::string(kThemeModeSetting.default_string)};
  std::string locale{std::string(kLocaleSetting.default_string)};
};

enum class MoveCategory {
  theory,
  brilliant,
  critical,
  best,
  excellent,
  okay,
  miss,
  mistake,
  blunder,
  unknown,
};

struct PlayerAnalysisSummary {
  int theory{0};
  int brilliant{0};
  int critical{0};
  int best{0};
  int excellent{0};
  int okay{0};
  int miss{0};
  int mistake{0};
  int blunder{0};
  int total_moves{0};
  int analyzed_moves{0};
  std::optional<double> local_accuracy;
};

struct AnalysisSummary {
  PlayerAnalysisSummary white;
  PlayerAnalysisSummary black;
  int classifier_version{0};
  int accuracy_algorithm_version{0};
  std::string opening_book_version;
  int engine_depth{0};
};

}  // namespace kchess
