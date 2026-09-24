#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "engine/chess_engine.h"
#include "persistence/database.h"
#include "persistence/sqlite_write_priority.h"
#include "theory/opening_theory_provider.h"

namespace kchess {

// Owns all Stockfish-backed analysis work and analysis-specific persistence.
// Core only orchestrates public calls; game and variation analysis lifecycle,
// cache reuse, classification rebuilds and cancellation live here.
class AnalysisService {
 public:
  enum class SharedProfileAnalysisRequestResult {
    cache_ready,
    started,
    deferred,
  };

  AnalysisService(Database& database, OpeningTheoryProvider& opening_theory);
  ~AnalysisService();

  AnalysisService(const AnalysisService&) = delete;
  AnalysisService& operator=(const AnalysisService&) = delete;

  void set_opening_theory_provider(OpeningTheoryProvider& opening_theory) noexcept;

  std::string start_analysis_json(const std::string& game_id);

  // Background profile maintenance is a second client of the same persisted
  // game-analysis cache used by the Analysis UI. It has an internal, non-user-
  // configurable engine budget (including engine identity/resources) and never
  // inherits mutable Analysis-screen settings. It still owns no separate
  // Stockfish result store.
  // The result distinguishes an already reusable cache, a newly started
  // background job, and foreground contention. Profile orchestration must not
  // claim ownership of a game when the request was merely deferred.
  SharedProfileAnalysisRequestResult ensure_shared_profile_analysis(
      const std::string& game_id,
      int requested_depth,
      int requested_multi_pv = 1,
      int requested_threads = 1,
      int requested_hash_mb = 1024);

  // PlayerProfileService installs a lightweight wake observer. AnalysisService
  // invokes it after a main-line analysis reaches any terminal state so the
  // persistent profile queue can resume without UI polling/navigation.
  void set_profile_analysis_observer(
      std::function<void(const std::string& game_id)> observer);

  // Shared analysis-cache contract used by every KChess consumer that needs
  // persisted game analysis. A completed run at equal-or-higher quality is
  // authoritative and must be reused instead of starting weaker duplicate
  // Stockfish work. The caller may request one move snapshot via requested_ply.
  [[nodiscard]] std::optional<PersistedAnalysis> shared_cached_analysis(
      const std::string& game_id,
      const AppSettings& requested,
      int requested_ply = -1) const;

  // Fill missing per-move accuracy or classification in completed runs by reclassifying
  // their saved engine slots. This starts no Stockfish job and writes only to
  // the same authoritative analysis_runs/move_analysis cache.
  int refresh_cached_accuracy_for_statistics(
      const std::string& profile_id, int maximum_games);

  std::string analysis_status_json(const std::string& game_id);
  std::string move_analysis_status_json(const std::string& game_id, int ply);
  [[nodiscard]] std::string performance_diagnostics_json() const;
  std::string start_move_refinement_json(const std::string& game_id, int ply);
  void cancel_analysis(const std::string& game_id);
  void delete_analysis(const std::string& game_id);
  void clear_engine_cache();
  void cancel_jobs_for_games(const std::vector<std::string>& game_ids);
  [[nodiscard]] bool has_active_work() const;
  void prepare_for_engine_change() noexcept;

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
  std::string coach_hint_json(const std::string& fen);
  // Re-evaluate one explicitly challenged legal root move against the fresh
  // native best root move. This is foreground Coach evidence only.
  std::string coach_move_review_json(
      const std::string& fen, const std::string& move_uci);
  // Evaluate the complete legal root move set and return the objectively worst
  // candidates for the side to move. When fastest_loss is true, an available
  // forced loss is ordered by shortest mate distance before ordinary eval loss.
  // This is ephemeral coach evidence and is never persisted as game analysis.
  std::string coach_extreme_move_json(
      const std::string& fen, bool fastest_loss);

 private:
  enum class AnalysisJobState {
    queued,
    running,
    cancelling,
    cancelled,
    completed,
    failed,
  };

  struct AnalysisJob {
    std::atomic_bool cancel_requested{false};
    std::atomic_bool finished{false};
    std::atomic<AnalysisJobState> state{AnalysisJobState::queued};
    std::atomic_int current_position_slot{-1};
    std::atomic_int requested_position_slot{-1};
    std::atomic_uint64_t target_generation{0};
    std::atomic_int completed_moves{0};
    std::shared_ptr<ChessEngine> engine;
    std::shared_ptr<persistence::ForegroundSqlitePriorityLease> foreground_sqlite_priority;
    std::string config_hash;
    // During maximum-depth refinement, a move keeps the last published
    // shallow classification only until both deeper adjacent position slots
    // are available. The deeper per-move classification then replaces it
    // immediately; the whole game need not wait for the queue to finish.
    std::string published_classification_config_hash;
    std::thread worker;
  };

  struct VariationJob {
    std::string id;
    std::string played_move;
    std::string played_san;
    std::string fen_before;
    std::string fen;
    std::atomic_bool finished{false};
    std::atomic_bool cancel_requested{false};
    mutable std::mutex state_mutex;
    std::string status{"running"};
    std::string error;
    AnalysisResult result;
    std::optional<MoveCategory> classification;
    bool classification_unstable{false};
    std::string classification_stability_reason;
    // Native presentation provenance for the best-move arrow on fen.  The
    // arrow is published only after the complete after-position search has
    // finished, so the next sideline move can reuse this exact result as its
    // BEFORE-position classification root.
    std::string arrow_config_hash;
    int arrow_requested_depth{0};
    int arrow_requested_multi_pv{0};
    bool arrow_snapshot_complete{false};
    std::string classification_snapshot_id;
    std::string classification_rank1_move;
    bool played_move_matches_published_rank1{false};
    bool classification_presentation_coherent{true};
    std::string classification_presentation_reason;
    int visible_multi_pv{1};
    std::atomic_bool expose_live_result{true};
    std::shared_ptr<ChessEngine> engine;
    std::thread worker;
  };

  // Snapshot the complete budget used by the pre-analysis pass.  The minimum
  // depth is the pre-analysis ceiling; the normal depth remains reserved for
  // live refinement.  Other user-controlled engine resources remain fully
  // configurable and are intentionally preserved.
  AppSettings preanalysis_budget_settings() const;
  AnalysisRequest analysis_request(
      const std::string& fen, const AppSettings& settings) const;
  AnalysisRequest preanalysis_request(
      const std::string& fen, const AppSettings& budget) const;
  std::string analysis_config_hash(const AppSettings& settings) const;
  std::string start_analysis_with_settings_json(
      const std::string& game_id,
      const AppSettings& settings,
      bool background_profile_request);
  std::string analysis_json(
      const std::string& game_id, const PersistedAnalysis& analysis,
      const AnalysisJob* live_job = nullptr) const;
  PersistedAnalysis progressive_live_classification_snapshot(
      const std::string& game_id,
      int ply,
      PersistedAnalysis analysis,
      const AnalysisJob* live_job) const;
  static const char* job_state_name(AnalysisJobState state) noexcept;
  static AnalysisJobState persisted_job_state(const PersistedAnalysis& analysis) noexcept;
  std::string update_classification_for_ply(
      const GameRecord& game,
      const std::string& game_id,
      const std::string& config_hash,
      const std::string& engine_version,
      int requested_depth,
      int time_limit_seconds,
      int ply);
  void rebuild_classification(
      const std::string& game_id,
      const std::string& config_hash,
      bool force = false);
  void run_analysis(
      const std::string& game_id,
      const std::string& config_hash,
      AppSettings settings,
      std::vector<std::string> positions,
      std::vector<int> completed_position_slots,
      const std::shared_ptr<AnalysisJob>& job,
      int preferred_slot = -1) noexcept;
  void run_refinement_queue(
      const std::string& game_id,
      const std::string& config_hash,
      AppSettings settings,
      std::vector<std::string> positions,
      std::vector<int> completed_position_slots,
      const std::shared_ptr<AnalysisJob>& job) noexcept;
  void notify_profile_analysis_observer(const std::string& game_id) noexcept;
  void reap_finished_variation_jobs();
  std::shared_ptr<ChessEngine> acquire_variation_engine(const std::string& engine_id);
  void stop_all_variation_jobs(bool stop_engine) noexcept;
  void stop_all_mainline_analysis_jobs() noexcept;
  std::string start_variation_job_json(
      const std::string& fen,
      const std::string& uci,
      AppSettings settings);

  Database& database_;
  mutable std::mutex profile_analysis_observer_mutex_;
  std::function<void(const std::string& game_id)> profile_analysis_observer_;
  OpeningTheoryProvider* opening_theory_;

  mutable std::mutex jobs_mutex_;
  mutable std::mutex statistics_accuracy_refresh_mutex_;
  std::unordered_set<std::string> statistics_accuracy_failed_runs_;
  std::unordered_map<std::string, std::shared_ptr<AnalysisJob>> jobs_;

  mutable std::mutex refinement_jobs_mutex_;
  std::unordered_map<std::string, std::shared_ptr<AnalysisJob>> refinement_jobs_;

  mutable std::mutex variation_jobs_mutex_;
  std::unordered_map<std::string, std::shared_ptr<VariationJob>> variation_jobs_;
  // Ephemeral sideline-only position cache. It is never persisted into a game
  // analysis row and is cleared when the user leaves variation mode.
  std::unordered_map<std::string, AnalysisResult> variation_position_results_;
  std::shared_ptr<ChessEngine> variation_engine_;
  std::atomic_uint64_t next_variation_job_id_{1};

  // Lock-free developer telemetry for the existing classification path. These
  // counters are diagnostic only and never participate in scheduling or cache
  // decisions.
  std::atomic_uint64_t classification_requests_{0};
  std::atomic_uint64_t classification_current_hits_{0};
  std::atomic_uint64_t classification_rebuilds_{0};
  std::atomic_uint64_t classification_incremental_updates_{0};
  std::atomic_uint64_t classification_moves_processed_{0};
  std::atomic_uint64_t classification_sqlite_reads_{0};
  std::atomic_uint64_t classification_total_ms_{0};
  std::atomic_uint64_t classification_full_rebuild_total_ms_{0};
  std::atomic_uint64_t classification_incremental_total_ms_{0};
  std::atomic_uint64_t classification_last_ms_{0};
};

}  // namespace kchess
