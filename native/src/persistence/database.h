#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "chess/pgn.h"
#include "core/models.h"
#include "engine/chess_engine.h"
#include "providers/provider_models.h"
#include "theory/opening_theory_provider.h"

struct sqlite3;

namespace kchess {

struct PersistedAnalysis {
  std::string status;
  std::string config_hash;
  std::string engine_version;
  int completed_plies{0};
  int total_plies{0};
  AnalysisSummary summary;
  int latest_ply{-1};
  std::string best_move;
  std::string recommended_move;
  std::optional<MoveCategory> classification;
  int classifier_version{0};
  std::optional<double> expected_score_before;
  std::optional<double> expected_score_best;
  std::optional<double> expected_score_played;
  std::optional<double> expected_score_loss;
  std::optional<TheoryMoveInfo> theory;
  std::vector<EngineLine> lines;
  std::string error;
};

struct MoveClassificationRecord {
  int ply{0};
  MoveCategory classification{MoveCategory::unknown};
  int classifier_version{0};
  std::optional<double> expected_score_before;
  std::optional<double> expected_score_best;
  std::optional<double> expected_score_played;
  std::optional<double> expected_score_loss;
  std::string recommended_move;
  TheoryMoveInfo theory;
};

struct GameRecord {
  std::string id;
  std::string profile_id;
  std::string kind;
  std::string white_name;
  std::string black_name;
  std::optional<int> white_rating;
  std::optional<int> black_rating;
  std::string result;
  std::string event;
  std::string site;
  std::string date;
  std::string time_control;
  std::string pgn;
  std::string starting_fen;
  std::int64_t created_at{0};
  std::optional<std::string> provider_game_id;
  std::optional<std::string> provider_url;
  std::optional<double> provider_accuracy_white;
  std::optional<double> provider_accuracy_black;
  std::optional<double> local_accuracy_white;
  std::optional<double> local_accuracy_black;
  std::string provider_outcome{"unknown"};
  std::string time_control_type{"unknown"};
  std::int64_t ended_at{0};
  bool favorite{false};
  std::optional<std::string> favorite_collection_id;
  bool downloaded{false};
  bool analyzed{false};
  std::vector<ParsedMove> moves;
};

struct FavoriteCollectionRecord {
  std::string id;
  std::string profile_id;
  std::string name;
  int game_count{0};
  std::int64_t created_at{0};
};

struct ProviderCacheRecord {
  std::string payload_json;
  CacheValidators validators;
  std::int64_t fetched_at{0};
  std::int64_t expires_at{0};
  int normalization_version{1};
};

struct ProviderStoredGame {
  ProviderGame provider_game;
  ParsedGame parsed_game;
};

class Database {
 public:
  explicit Database(std::filesystem::path data_directory);
  ~Database();

  Database(const Database&) = delete;
  Database& operator=(const Database&) = delete;

  void open_and_migrate();
  void close() noexcept;

  std::vector<Profile> profiles() const;
  Profile create_profile(
      ProfileType type,
      const std::string& display_name,
      const std::optional<std::string>& provider_username,
      const std::string& avatar_asset);
  Profile create_provider_profile(
      const ProviderProfile& profile,
      const ResponseCacheInfo& cache,
      const std::string& normalized_json);
  Profile update_provider_profile(
      const std::string& profile_id,
      const ProviderProfile& profile,
      const ResponseCacheInfo& cache,
      const std::string& normalized_json);
  void set_active_profile(const std::string& profile_id);
  std::optional<Profile> delete_profile(const std::string& profile_id);
  std::optional<Profile> active_profile() const;
  std::optional<Profile> profile(const std::string& profile_id) const;
  std::vector<std::string> profile_game_ids(const std::string& profile_id) const;

  std::optional<ProviderCacheRecord> provider_cache(
      const std::string& profile_id, const std::string& cache_key) const;
  void put_provider_cache(
      const std::string& profile_id,
      const std::string& cache_key,
      const std::string& payload_json,
      const ResponseCacheInfo& cache);
  std::vector<std::string> cached_months(const std::string& profile_id) const;
  int upsert_provider_games(
      const std::string& profile_id,
      const std::string& month,
      const std::vector<ProviderStoredGame>& games,
      const ResponseCacheInfo& cache);
  void set_favorite(const std::string& profile_id, const std::string& game_id, bool value);
  std::vector<FavoriteCollectionRecord> favorite_collections(
      const std::string& profile_id) const;
  FavoriteCollectionRecord create_favorite_collection(
      const std::string& profile_id, const std::string& name);
  void rename_favorite_collection(
      const std::string& profile_id,
      const std::string& collection_id,
      const std::string& name);
  void delete_favorite_collection(
      const std::string& profile_id, const std::string& collection_id);
  void set_favorite_collection(
      const std::string& profile_id,
      const std::string& game_id,
      const std::optional<std::string>& collection_id);
  void set_downloaded(const std::string& profile_id, const std::string& game_id, bool value);
  void delete_local_game(const std::string& profile_id, const std::string& game_id);
  int clear_cached_month(const std::string& profile_id, const std::string& month);
  void set_profile_avatar_file(
      const std::string& profile_id, const std::optional<std::string>& file_path);
  void set_provider_sync_state(
      const std::string& profile_id,
      ProviderType provider,
      const std::string& status,
      const std::string& last_error,
      std::int64_t retry_after);

  AppSettings settings() const;
  void set_engine_settings(int depth, int multi_pv, int time_limit_seconds);
  void set_engine_resources(int threads, int hash_mb);
  void set_setting(const std::string& key, const std::string& value);

  std::string import_pgn(const std::string& profile_id, const ParsedGame& game);
  std::string import_fen(
      const std::string& profile_id,
      const std::string& fen,
      const std::string& display_name);
  std::vector<GameRecord> games(const std::string& profile_id) const;
  std::optional<GameRecord> game(const std::string& game_id) const;

  PersistedAnalysis prepare_analysis(
      const std::string& game_id,
      const std::string& config_hash,
      const std::string& engine_version,
      int total_plies,
      int depth,
      int multi_pv,
      int time_limit_seconds = 0);
  void persist_engine_result(
      const std::string& game_id,
      const std::string& config_hash,
      int ply,
      int completed_plies,
      const AnalysisResult& result,
      std::int64_t analysis_timestamp);
  void set_analysis_status(
      const std::string& game_id,
      const std::string& config_hash,
      const std::string& status,
      const std::string& error = {});
  std::vector<int> analyzed_position_slots(
      const std::string& game_id,
      const std::string& config_hash) const;
  bool classification_is_current(
      const std::string& game_id,
      const std::string& config_hash,
      int classifier_version,
      int accuracy_version,
      const std::string& opening_book_version) const;
  void persist_classifications(
      const std::string& game_id,
      const std::string& config_hash,
      const std::vector<MoveClassificationRecord>& records,
      const std::optional<double>& white_accuracy,
      const std::optional<double>& black_accuracy,
      int classifier_version,
      int accuracy_version,
      const std::string& opening_book_version,
      bool finalize = true);
  std::optional<PersistedAnalysis> analysis(
      const std::string& game_id,
      const std::string& config_hash,
      int requested_ply = -1) const;
  std::optional<PersistedAnalysis> compatible_analysis(
      const std::string& game_id,
      const std::string& engine_version,
      const AppSettings& requested,
      int requested_ply = -1) const;

  std::optional<AnalysisResult> compatible_position_analysis(
      const std::string& position_fen,
      const std::string& engine_version,
      const AppSettings& requested) const;
  void persist_position_analysis(
      const std::string& position_fen,
      const std::string& engine_version,
      const AppSettings& settings,
      const AnalysisResult& result,
      std::int64_t analysis_timestamp);
  void clear_global_position_cache();

  // Lightweight bounded housekeeping for generated/cache-only data.
  // User games, favorites, downloads, and completed current analyses are never
  // removed here.
  void run_maintenance();

 private:
  void execute(const std::string& sql) const;
  int schema_version() const;
  std::optional<std::string> setting(const std::string& key) const;

  std::filesystem::path data_directory_;
  sqlite3* db_{nullptr};
};

}  // namespace kchess
