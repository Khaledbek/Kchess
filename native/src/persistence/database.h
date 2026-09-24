// -----------------------------------------------------------------------------
// Section: Persistence records and database interface
// -----------------------------------------------------------------------------

#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
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
  int requested_depth{0};
  int requested_multi_pv{0};
  int requested_time_limit_seconds{0};
  bool adaptive_early_stop{false};
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
  // Response-only provenance for the move-classification snapshot. Ordinary
  // persisted rows leave these empty and therefore use the engine snapshot
  // above. Live refinement may intentionally freeze a classification from the
  // last fully published pre-analysis run; AnalysisService records that source
  // here so callers can detect the otherwise invisible mixed-snapshot state.
  std::string classification_config_hash;
  std::string classification_engine_version;
  int classification_requested_depth{0};
  int classification_requested_multi_pv{0};
  int classification_reached_depth{0};
  std::string classification_rank1_move;
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
  std::optional<double> move_accuracy;
  double accuracy_weight{0.0};
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
  std::optional<std::string> opening_eco;
  std::optional<std::string> opening_name;
  std::optional<int> opening_ply;
  std::int64_t ended_at{0};
  bool favorite{false};
  std::optional<std::string> favorite_collection_id;
  bool downloaded{false};
  bool analyzed{false};
  std::vector<ParsedMove> moves;
};

// Minimal per-game fields for statistics aggregation. Deliberately excludes the
// PGN and analysis joins so a full-library overview stays cheap.
struct GameStatRow {
  std::string game_id;
  std::string provider_outcome;   // win | loss | draw | unknown (profile perspective)
  std::string result;             // 1-0 | 0-1 | 1/2-1/2 | *
  std::string white_name;
  std::string black_name;
  std::string time_control_type;  // bullet | blitz | rapid | classical | daily | ...
  std::string opening_eco;        // empty when the game has no named opening
  std::string opening_name;       // empty when unclassified or no named opening
  std::optional<int> opening_ply;
  bool analysed{false};
};

struct GameMoveErrorRow {
  std::string game_id;
  int ply{0};
  std::string category;
  std::string san;
  std::string fen_before;
  std::string recommended_move;
};

struct AccuracyGameRow {
  std::string game_id;
  std::string provider_outcome;
  std::string result;
  std::string white_name;
  std::string black_name;
  std::string time_control_type;
  std::int64_t ended_at{0};
  std::optional<double> white_accuracy;
  std::optional<double> black_accuracy;
};

struct AccuracyMoveRow {
  std::string game_id;
  int ply{0};
  std::string category;
  bool theory{false};
  std::optional<double> accuracy;
  std::optional<double> weight;
};

struct CachedAccuracyBackfillRow {
  std::string game_id;
  std::string config_hash;
};

// Cheap one-row-per-game profile input. This intentionally contains no PGN,
// engine lines or FEN payloads so a complete library metadata sweep remains
// much cheaper than opening/analyzing every game.
struct ProfileGameMetadataRow {
  std::string game_id;
  std::int64_t played_at{0};
  std::string player_color{"unknown"};
  std::string provider_outcome{"unknown"};
  std::string time_control_type{"unknown"};
  std::string opening_eco;
  std::string opening_name;
  std::string termination_type{"unknown"};
  std::optional<int> player_rating;
  std::optional<int> opponent_rating;
  int plies{0};
  bool has_complete_analysis{false};
  std::optional<double> accuracy;
  int miss_count{0};
  int mistake_count{0};
  int blunder_count{0};
};

// Minimal per-game fields for the game-phase ("phase of death") breakdown:
// outcome plus the last recorded ply, from which the ending move number (and
// thus the phase) is derived. max_ply is -1 when the game has no stored moves.
struct GamePhaseRow {
  std::string provider_outcome;
  std::string result;
  std::string white_name;
  std::string black_name;
  int max_ply{-1};
};



struct PlayerOpeningLearningStat {
  std::string eco;
  std::string name;
  int games{0};
  int wins{0};
  int draws{0};
  int losses{0};
};

struct PlayerLearningStats {
  int games{0};
  int analyzed_moves{0};
  std::optional<int> average_rating;
  std::optional<double> average_accuracy;
  int theory{0};
  int brilliant{0};
  int critical{0};
  int best{0};
  int excellent{0};
  int good{0};
  int okay{0};
  int inaccuracy{0};
  int miss{0};
  int mistake{0};
  int blunder{0};
  std::vector<PlayerOpeningLearningStat> openings;
};

// Compact source rows for the AI player profile. They expose existing persisted
// KChess facts only; interpretation remains under native/ai/profile/.
struct PlayerProfileGameSourceRow {
  std::string game_id;
  std::int64_t played_at{0};
  std::int64_t source_version{0};
  std::string time_control_type{"unknown"};
  std::string result;
  std::string provider_outcome{"unknown"};
  std::string player_color{"unknown"};
  std::optional<int> player_rating;
  std::optional<int> opponent_rating;
  std::optional<double> accuracy;
  std::string opening_eco;
  std::string opening_name;
  int move_count{0};
  bool analysis_complete{false};
  int analyzed_moves{0};
  int theory{0};
  int brilliant{0};
  int critical{0};
  int best{0};
  int excellent{0};
  int miss{0};
  int mistake{0};
  int blunder{0};
  int opening_analyzed_moves{0};
  int opening_miss{0};
  int opening_mistake{0};
  int opening_blunder{0};
  int middlegame_analyzed_moves{0};
  int middlegame_miss{0};
  int middlegame_mistake{0};
  int middlegame_blunder{0};
  int endgame_analyzed_moves{0};
  int endgame_miss{0};
  int endgame_mistake{0};
  int endgame_blunder{0};
  std::optional<double> average_expected_score_loss;
  std::optional<double> maximum_expected_score_loss;
  std::string source_profile_id;
  std::string termination_type{"unknown"};
};

struct PlayerProfileMoveSourceRow {
  int ply{0};
  std::string classification;
  std::optional<double> expected_score_before;
  std::optional<double> expected_score_best;
  std::optional<double> expected_score_played;
  std::optional<double> expected_score_loss;
  bool theory{false};
  std::string fen_before;
  std::string uci;
};


// Lightweight locator into the authoritative profile evidence library. The
// registry never copies PGNs, engine lines, classifications or profile
// payloads; it only records where a later graph/retrieval layer can fetch the
// current source of truth. `evidence_id` is stable across source refreshes.
struct AiProfileEvidenceRegistryRow {
  std::string evidence_id;
  std::string source_kind;
  std::string source_table;
  std::string source_key;
  std::optional<std::string> game_id;
  std::optional<int> ply;
  std::string evidence_tier;
  int evidence_level{0};
  std::int64_t source_version{0};
  std::int64_t updated_at{0};
};

struct AiProfileQueueRecord {
  std::string profile_id;
  std::string game_id;
  std::string state{"queued"};
  double priority{0.0};
  std::string reason;
  int pipeline_stage{2};
  std::int64_t source_version{0};
  std::int64_t processed_version{-1};
  int attempts{0};
  std::int64_t updated_at{0};
};

struct AiProfileQueueProgress {
  int total{0};
  int considered{0};
  int historical_sample{0};
  int interesting{0};
  int relevant{0};
  int resolved_relevant{0};
  int queued{0};
  int engine_pending{0};
  int processing{0};
  int engine_promoted_games{0};
};

struct AiProfileSamplingState {
  std::int64_t bootstrap_cutoff_played_at{0};
  int total_games{0};
  int eligible_games{0};
  int excluded_games{0};
  int minimum_games{0};
  int recommended_games{0};
  int maximum_games{0};
  int selected_games{0};
  int required_strata{0};
  int covered_strata{0};
  double diversity_score{0.0};
  double covered_population_share{0.0};
  bool capped{false};
  std::int64_t updated_at{0};
};

struct CoachSessionRecord {
  std::string id;
  std::string profile_id;
  int session_number{0};
  std::string name;
  std::int64_t created_at{0};
  std::int64_t updated_at{0};
  std::int64_t last_opened_at{0};
  std::string compact_state_json{"{}"};
};

struct CoachSessionMessageRecord {
  std::string session_id;
  int sequence{0};
  std::string role;
  std::string content;
  std::string payload_json{"{}"};
  bool automatic_turn{false};
  std::int64_t created_at{0};
};

struct AiCoachSkillProgressRow {
  std::string motif_id;
  int independent_successes{0};
  int other_attempts{0};
  std::int64_t last_practiced_at{0};
  int verified_weak_attempts{0};
  int guided_successes{0};
  int success_streak{0};
  int schedule_level{0};
  std::int64_t interval_seconds{0};
  std::int64_t next_practice_at{0};
};

struct BotGameMoveRecord {
  int ply{0};
  std::string uci;
  std::string san;
  std::string fen_after;
};

struct BotGameRecord {
  std::string id;
  int bot_elo{1500};
  std::string player_color{"white"};
  std::string bot_color{"black"};
  std::string status{"active"};
  std::string result{"*"};
  std::string starting_fen;
  std::string current_fen;
  std::int64_t created_at{0};
  std::int64_t updated_at{0};
  bool show_eval_bar{false};
  std::optional<std::string> analysis_game_id;
  std::vector<BotGameMoveRecord> moves;
};

struct BotGameSummaryRecord {
  std::string id;
  int bot_elo{1500};
  std::string player_color{"white"};
  std::string bot_color{"black"};
  std::string status{"active"};
  std::string result{"*"};
  std::int64_t created_at{0};
  std::int64_t updated_at{0};
  int move_count{0};
  std::optional<std::string> analysis_game_id;
};


struct KnowledgeMaintenanceState {
  std::string profile_id;
  std::string graph_source_signature;
  std::int64_t last_full_maintenance_ms{0};
  std::int64_t updated_at_ms{0};
};

struct TrainingProgressRecord {
  std::string exercise_id;
  bool mastered{false};
  int success_streak{0};
  int success_count{0};
  int attempt_count{0};
  int best_depth{0};
  std::optional<std::int64_t> last_attempt_at;
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
  std::string player_profile_owner_id(const std::string& profile_id) const;
  std::vector<std::string> profile_game_ids(const std::string& profile_id) const;
  void merge_local_profile(const std::string& source_profile_id, const std::string& target_profile_id);

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
  std::optional<std::string> app_setting(const std::string& key) const { return setting(key); }

  std::string import_pgn(const std::string& profile_id, const ParsedGame& game);
  std::string import_fen(
      const std::string& profile_id,
      const std::string& fen,
      const std::string& display_name);
  std::vector<GameRecord> games(const std::string& profile_id) const;
  std::optional<std::string> latest_game_month_at_or_before(
      const std::string& profile_id, const std::string& maximum_month) const;
  std::vector<GameRecord> games_for_month(
      const std::string& profile_id, const std::string& month) const;
  std::vector<GameRecord> favorite_games() const;
  std::optional<GameRecord> game(const std::string& game_id) const;
  BotGameRecord create_bot_game(int bot_elo, const std::string& starting_fen, const std::string& player_color);
  std::optional<BotGameRecord> active_bot_game() const;
  std::optional<BotGameRecord> bot_game(const std::string& game_id) const;
  std::vector<BotGameSummaryRecord> bot_games() const;
  void append_bot_game_move(
      const std::string& game_id,
      int base_ply,
      const std::string& expected_fen_before,
      const BotGameMoveRecord& move);
  void finish_bot_game(
      const std::string& game_id,
      const std::string& status,
      const std::string& result);
  void delete_bot_game(const std::string& game_id);
  void set_bot_game_show_eval_bar(const std::string& game_id, bool enabled);
  void set_bot_game_analysis_game_id(
      const std::string& game_id, const std::string& analysis_game_id);

  std::vector<TrainingProgressRecord> training_progress() const;
  void put_training_progress(const TrainingProgressRecord& progress);

  // Lightweight rows for statistics, newest game first (by end/creation time).
  std::vector<GameStatRow> games_for_statistics(const std::string& profile_id) const;
  std::vector<GameMoveErrorRow> move_errors_for_statistics(
      const std::string& profile_id, int before_ply) const;
  std::vector<AccuracyGameRow> accuracy_games_for_statistics(
      const std::string& profile_id) const;
  std::vector<AccuracyMoveRow> accuracy_moves_for_statistics(
      const std::string& profile_id) const;
  // Existing complete shared analyses whose derived classifier contract is stale
  // or whose old move rows lack phase-accuracy weights. Raw engine slots are
  // reused; no Stockfish work is implied.
  std::vector<CachedAccuracyBackfillRow> statistics_games_missing_move_accuracy(
      const std::string& profile_id, int limit) const;
  std::vector<ProfileGameMetadataRow> profile_games_metadata(
      const std::string& profile_id) const;

  // Outcome + final ply per game, for the game-phase breakdown.
  std::vector<GamePhaseRow> games_for_phases(const std::string& profile_id) const;

  PlayerLearningStats player_learning_stats(const std::string& profile_id) const;

  std::optional<KnowledgeMaintenanceState> knowledge_maintenance_state(
      const std::string& profile_id) const;
  void set_knowledge_maintenance_state(
      const KnowledgeMaintenanceState& state);
  std::vector<PlayerProfileGameSourceRow> player_profile_game_sources(
      const std::string& profile_id) const;
  // Monotonic in-process generation for source data consumed by the shared
  // player statistics read model. It advances on relevant SQLite row changes
  // so StatisticsService can invalidate its transient read cache without a
  // second persisted cache or a full source scan.
  [[nodiscard]] std::uint64_t statistics_source_revision() const noexcept;
  std::optional<PlayerProfileGameSourceRow> player_profile_game_source(
      const std::string& profile_id, const std::string& game_id) const;
  std::vector<PlayerProfileMoveSourceRow> player_profile_move_sources(
      const std::string& game_id, const std::string& player_color) const;
  void sync_ai_profile_evidence_registry(const std::string& profile_id);
  std::vector<AiProfileEvidenceRegistryRow> ai_profile_evidence_registry(
      const std::string& profile_id) const;
  std::optional<std::string> ai_chess_profile_payload(
      const std::string& profile_id) const;
  void set_ai_chess_profile_payload(
      const std::string& profile_id, const std::string& payload_json);
  void record_ai_coach_skill_attempt(
      const std::string& profile_id, const std::string& motif_id,
      bool independent_success, int verified_weak_attempts,
      int guided_successes, int success_streak, int schedule_level,
      std::int64_t interval_seconds, std::int64_t next_practice_at,
      std::int64_t practiced_at);
  std::vector<AiCoachSkillProgressRow> ai_coach_skill_progress(
      const std::string& profile_id) const;
  CoachSessionRecord create_coach_session(
      const std::string& profile_id, std::int64_t now);
  CoachSessionRecord ensure_coach_session(
      const std::string& profile_id, const std::string& session_id,
      std::int64_t now);
  std::optional<CoachSessionRecord> coach_session(
      const std::string& profile_id, const std::string& session_id) const;
  std::vector<CoachSessionRecord> coach_sessions(
      const std::string& profile_id) const;
  std::vector<CoachSessionMessageRecord> coach_session_messages(
      const std::string& profile_id, const std::string& session_id) const;
  void rename_coach_session(
      const std::string& profile_id, const std::string& session_id,
      const std::string& name, std::int64_t updated_at);
  void delete_coach_session(
      const std::string& profile_id, const std::string& session_id);
  void touch_coach_session(
      const std::string& profile_id, const std::string& session_id,
      std::int64_t opened_at);
  void save_coach_session_state(
      const std::string& profile_id, const std::string& session_id,
      const std::string& compact_state_json, std::int64_t updated_at);
  void append_coach_session_message(
      const std::string& profile_id, const std::string& session_id,
      const std::string& role, const std::string& content,
      const std::string& payload_json, bool automatic_turn,
      std::int64_t created_at);

  // Freezes the latest already-known played-at timestamp on first Stage-3
  // synchronization. Older archive games discovered later stay historical;
  // only games played after this watermark are incremental profile learning.
  std::int64_t ensure_ai_profile_sampling_bootstrap_cutoff(
      const std::string& profile_id, std::int64_t latest_known_played_at);
  void update_ai_profile_sampling_state(
      const std::string& profile_id,
      const AiProfileSamplingState& state);
  std::optional<AiProfileSamplingState> ai_profile_sampling_state(
      const std::string& profile_id) const;
  void upsert_ai_profile_queue(
      const std::string& profile_id,
      const std::string& game_id,
      double priority,
      const std::string& reason,
      int pipeline_stage,
      bool historical_sample,
      bool interesting_selected,
      std::int64_t source_version);
  std::optional<AiProfileQueueRecord> next_ai_profile_queue(
      const std::string& profile_id) const;
  // Requeues only transient in-process work that cannot survive an app restart.
  // engine_pending remains selectable on its own and therefore needs no cold
  // recovery rewrite.
  void recover_ai_profile_queue_inflight(const std::string& profile_id);
  void set_ai_profile_queue_state(
      const std::string& profile_id,
      const std::string& game_id,
      const std::string& state,
      std::int64_t processed_version,
      bool increment_attempts = false,
      int pipeline_stage = -1);
  // Marks derived player-profile work stale after the authoritative shared
  // game-analysis cache changes. This never deletes analysis; it only asks the
  // profile service to re-read the newer (or user-deleted) cache state.
  void notify_ai_profile_analysis_changed(const std::string& game_id);
  AiProfileQueueProgress ai_profile_queue_progress(
      const std::string& profile_id) const;
  std::vector<std::string> ai_profile_processed_game_ids(
      const std::string& profile_id) const;

  // Opening classification. games_needing_opening returns (game_id, pgn) for
  // games not yet classified (opening_ply IS NULL); limit <= 0 returns all.
  // set_game_opening records the result: pass eco/name empty and ply 0 when no
  // named opening was found, so the game is not rescanned.
  std::vector<std::pair<std::string, std::string>> games_needing_opening(int limit) const;
  void set_game_opening(
      const std::string& game_id,
      const std::optional<std::string>& eco,
      const std::optional<std::string>& name,
      int ply);

  PersistedAnalysis prepare_analysis(
      const std::string& game_id,
      const std::string& config_hash,
      const std::string& engine_version,
      int total_plies,
      int depth,
      int multi_pv,
      int time_limit_seconds = 0,
      bool adaptive_early_stop = true);
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
  // Focused read model for move classification. Returns the engine results for
  // position slots [ply] and [ply + 1] without constructing game-wide analysis
  // summaries or category aggregates.
  std::pair<std::optional<AnalysisResult>, std::optional<AnalysisResult>>
  adjacent_analysis_results(
      const std::string& game_id,
      const std::string& config_hash,
      int ply) const;
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
  // Shared game-analysis cache lookup. Compatibility is monotonic: a saved
  // complete run may satisfy a weaker request, but a weaker run can never
  // replace a stronger compatible run. Threads/hash are execution resources
  // and do not lower chess-result quality; engine version, depth, MultiPV,
  // time budget and strict/adaptive semantics remain compatibility boundaries.
  std::optional<PersistedAnalysis> compatible_analysis(
      const std::string& game_id,
      const std::string& engine_version,
      const AppSettings& requested,
      int requested_ply = -1) const;

  // Keep only the authoritative saved analysis for a game.  Position-cache
  // entries are intentionally independent and survive this pruning.
  void prune_game_analyses_except(
      const std::string& game_id, const std::string& engine_version,
      const std::string& keep_config_hash);
  void delete_game_analyses(const std::string& game_id);

  std::optional<AnalysisResult> compatible_position_analysis(
      const std::string& position_fen,
      const std::string& engine_version,
      const AppSettings& requested) const;
  std::optional<AnalysisResult> best_position_checkpoint(
      const std::string& position_fen,
      const std::string& engine_version,
      int maximum_depth,
      int multi_pv) const;
  void persist_position_analysis(
      const std::string& position_fen,
      const std::string& engine_version,
      const AppSettings& settings,
      const AnalysisResult& result,
      std::int64_t analysis_timestamp);
  void clear_global_position_cache();

  // Lightweight bounded housekeeping for generated/cache-only data.
  // User games, favorites, and completed current analyses are never
  // removed here. The legacy downloads table is migration-only.
  void run_maintenance();

 private:
  void execute(const std::string& sql) const;
  int schema_version() const;
  std::optional<std::string> setting(const std::string& key) const;

  std::filesystem::path data_directory_;
  sqlite3* db_{nullptr};
  std::atomic_uint64_t statistics_source_revision_{1};
};

}  // namespace kchess
