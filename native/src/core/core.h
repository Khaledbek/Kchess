#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

#include "core/models.h"
#include "persistence/database.h"
#include "services/analysis_service.h"
#include "services/game_library_service.h"
#include "services/profile_service.h"
#include "services/provider_service.h"
#include "services/settings_service.h"
#include "services/statistics_service.h"
#include "theory/opening_name_index.h"
#include "theory/opening_theory_provider.h"

namespace kchess {

class Core {
 public:
  explicit Core(std::filesystem::path data_directory);
  ~Core();

  Core(const Core&) = delete;
  Core& operator=(const Core&) = delete;

  void initialize();

  std::string profiles_json();
  std::string create_profile_json(
      ProfileType type,
      const std::string& display_name,
      const std::string& provider_username);
  void set_active_profile(const std::string& profile_id);
  void delete_profile(const std::string& profile_id);
  void merge_local_profile(const std::string& source_profile_id, const std::string& target_profile_id);
  std::string active_profile_json();

  std::string settings_json();
  void set_engine_settings(int depth, int multi_pv, int time_limit_seconds);
  void set_analysis_depth_range(int minimum_depth, int maximum_depth);
  void set_engine_resources(int threads, int hash_mb);
  void set_show_board_arrows(bool enabled);
  void set_boolean_setting(const std::string& key, bool enabled);
  void set_theme_mode(const std::string& mode);
  void set_locale(const std::string& locale);

  std::string games_json();
  std::string favorite_games_json();
  std::string game_json(const std::string& game_id);
  std::string import_pgn_json(const std::string& pgn);
  std::string import_fen_json(const std::string& fen, const std::string& display_name);
  void set_favorite(const std::string& game_id, bool value);
  std::string favorite_collections_json();
  std::string create_favorite_collection_json(const std::string& name);
  void rename_favorite_collection(
      const std::string& collection_id, const std::string& name);
  void delete_favorite_collection(const std::string& collection_id);
  void set_favorite_collection(
      const std::string& game_id, const std::optional<std::string>& collection_id);
  void set_downloaded(const std::string& game_id, bool value);
  void delete_local_game(const std::string& game_id);
  void clear_cached_month(const std::string& profile_id, const std::string& month);

  std::string start_provider_profile_json(ProfileType type, const std::string& username);
  std::string start_provider_sync_json(
      const std::string& profile_id, int year, int month);
  std::string provider_job_status_json(const std::string& job_id);
  void cancel_provider_job(const std::string& job_id);
  std::string provider_overview_json(const std::string& profile_id);

  std::string statistics_overview_json();
  std::string statistics_openings_json();

  std::string start_analysis_json(const std::string& game_id);
  std::string analysis_status_json(const std::string& game_id);
  std::string move_analysis_status_json(const std::string& game_id, int ply);
  std::string start_move_refinement_json(const std::string& game_id, int ply);
  void cancel_analysis(const std::string& game_id);
  void clear_engine_cache();
  std::string start_variation_analysis_json(
      const std::string& fen, const std::string& uci);
  std::string start_variation_analysis_with_settings_json(
      const std::string& fen,
      const std::string& uci,
      int depth,
      int multi_pv,
      int threads,
      int hash_mb);
  std::string variation_analysis_status_json(const std::string& job_id);
  void cancel_variation_analysis(const std::string& job_id);

  const std::string& last_error() const noexcept { return last_error_; }
  int32_t last_status() const noexcept { return last_status_; }
  void set_last_error(const int32_t status, std::string message) noexcept {
    last_status_ = status;
    last_error_ = std::move(message);
  }

 private:
  // Classify up to `limit` unclassified stored games (<= 0 means all) with the
  // opening-name index and persist each result. Idempotent and cheap; a single
  // unparseable game is marked processed rather than aborting the sweep.
  void classify_pending_openings(int limit);

  std::filesystem::path data_directory_;
  Database database_;
  ProfileService profile_service_;
  GameLibraryService game_library_service_;
  SettingsService settings_service_;
  ProviderService provider_service_;
  std::unique_ptr<OpeningTheoryProvider> opening_theory_;
  std::unique_ptr<OpeningNameIndex> opening_names_;
  AnalysisService analysis_service_;
  StatisticsService statistics_service_;
  bool initialized_{false};
  int32_t last_status_{0};
  std::string last_error_;
};

}  // namespace kchess
