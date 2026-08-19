#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "engine/chess_engine.h"
#include "persistence/database.h"
#include "theory/opening_theory_provider.h"

namespace kchess {

// Owns all Stockfish-backed analysis work and analysis-specific persistence.
// Core only orchestrates public calls; game and variation analysis lifecycle,
// cache reuse, classification rebuilds and cancellation live here.
class AnalysisService {
 public:
  AnalysisService(Database& database, OpeningTheoryProvider& opening_theory);
  ~AnalysisService();

  AnalysisService(const AnalysisService&) = delete;
  AnalysisService& operator=(const AnalysisService&) = delete;

  void set_opening_theory_provider(OpeningTheoryProvider& opening_theory) noexcept;

  std::string start_analysis_json(const std::string& game_id);
  std::string analysis_status_json(const std::string& game_id);
  std::string move_analysis_status_json(const std::string& game_id, int ply);
  void cancel_analysis(const std::string& game_id);
  void clear_engine_cache();
  void cancel_jobs_for_games(const std::vector<std::string>& game_ids);

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
    std::atomic_int completed_moves{0};
    std::shared_ptr<StockfishEngine> engine;
    std::string config_hash;
    std::thread worker;
  };

  struct VariationJob {
    std::string id;
    std::string played_move;
    std::string played_san;
    std::string fen;
    std::atomic_bool finished{false};
    mutable std::mutex state_mutex;
    std::string status{"running"};
    std::string error;
    AnalysisResult result;
    std::shared_ptr<StockfishEngine> engine;
    std::thread worker;
  };

  AnalysisRequest analysis_request(
      const std::string& fen, const AppSettings& settings) const;
  std::string analysis_config_hash(const AppSettings& settings) const;
  std::optional<PersistedAnalysis> reusable_analysis(
      const std::string& game_id,
      const AppSettings& settings,
      int requested_ply = -1) const;
  std::string analysis_json(
      const std::string& game_id, const PersistedAnalysis& analysis,
      const AnalysisJob* live_job = nullptr) const;
  static const char* job_state_name(AnalysisJobState state) noexcept;
  static AnalysisJobState persisted_job_state(const PersistedAnalysis& analysis) noexcept;
  void rebuild_classification(
      const std::string& game_id,
      const std::string& config_hash,
      bool force = false,
      int through_ply = -1);
  void run_analysis(
      const std::string& game_id,
      const std::string& config_hash,
      AppSettings settings,
      std::vector<std::string> positions,
      std::vector<int> completed_position_slots,
      const std::shared_ptr<AnalysisJob>& job) noexcept;
  void reap_finished_variation_jobs();
  std::string start_variation_job_json(
      const std::string& fen,
      const std::string& uci,
      AppSettings settings);

  Database& database_;
  OpeningTheoryProvider* opening_theory_;

  mutable std::mutex jobs_mutex_;
  std::unordered_map<std::string, std::shared_ptr<AnalysisJob>> jobs_;

  mutable std::mutex variation_jobs_mutex_;
  std::unordered_map<std::string, std::shared_ptr<VariationJob>> variation_jobs_;
  std::atomic_uint64_t next_variation_job_id_{1};
};

}  // namespace kchess
