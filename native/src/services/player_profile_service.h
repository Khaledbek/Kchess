#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "persistence/database.h"
#include "services/analysis_service.h"

namespace kchess {

class ProviderService;
namespace knowledge {
class KnowledgeRuntime;
}

// -----------------------------------------------------------------------------
// Section: Persistent player-profile orchestration
// -----------------------------------------------------------------------------

class PlayerProfileService {
 public:
  PlayerProfileService(
      Database& database, AnalysisService& analysis_service,
      ProviderService& provider_service, knowledge::KnowledgeRuntime& knowledge_runtime);
  ~PlayerProfileService();

  PlayerProfileService(const PlayerProfileService&) = delete;
  PlayerProfileService& operator=(const PlayerProfileService&) = delete;

  void start();
  void notify_profile_changed();
  void pause_engine_work();

  [[nodiscard]] std::string snapshot_json();
  [[nodiscard]] std::string diagnostics_json();

 private:
  struct WorkerActivity {
    bool active{false};
    std::string operation{"idle"};
    std::string phase{"idle"};
    std::string detail;
    std::string profile_id;
    std::size_t completed{0};
    std::size_t total{0};
    std::int64_t started_at_ms{0};
    std::int64_t phase_started_at_ms{0};
    std::int64_t updated_at_ms{0};
  };

  void run() noexcept;
  bool process_once();
  bool refresh_cached_statistics_accuracy_if_due(
      const std::string& profile_id, std::int64_t now_seconds);
  void sync_queue(const std::string& profile_id, std::int64_t now_seconds);
  void refresh_profile(const std::string& profile_id, std::int64_t now_seconds);
  void refresh_profile_if_requested(
      const std::string& profile_id, std::int64_t now_seconds, bool force = false) noexcept;
  [[nodiscard]] std::vector<PlayerProfileGameSourceRow> load_profile_game_sources(
      const std::string& profile_id);
  [[nodiscard]] std::optional<PlayerProfileGameSourceRow> load_profile_game_source(
      const std::string& profile_id, const std::string& game_id);
  bool run_engine_request(
      const std::string& profile_id,
      const AiProfileQueueRecord& queue,
      const PlayerProfileGameSourceRow& source,
      std::int64_t now_seconds);
  void set_current_game(const std::optional<std::string>& game_id);
  [[nodiscard]] std::optional<std::string> current_game() const;
  void set_worker_activity(
      std::string operation, std::string phase, std::string detail,
      const std::string& profile_id = {}, std::size_t completed = 0,
      std::size_t total = 0);
  void clear_worker_activity() noexcept;
  [[nodiscard]] std::string worker_activity_json() const;

  Database& database_;
  AnalysisService& analysis_service_;
  ProviderService& provider_service_;
  knowledge::KnowledgeRuntime& knowledge_runtime_;
  std::atomic_bool stop_{false};
  std::atomic_bool pause_requested_{false};
  std::atomic_bool resync_requested_{false};
  std::atomic_bool profile_refresh_requested_{false};
  std::atomic_int64_t startup_grace_until_ms_{0};
  std::thread worker_;
  mutable std::mutex wake_mutex_;
  std::condition_variable wake_cv_;
  mutable std::mutex refresh_mutex_;
  mutable std::mutex current_mutex_;
  std::optional<std::string> current_game_id_;
  mutable std::mutex worker_activity_mutex_;
  WorkerActivity worker_activity_;
  std::string synced_profile_id_;
  std::int64_t last_sync_at_{0};
  std::int64_t last_profile_refresh_at_{0};
  std::int64_t last_history_backfill_at_{0};
  std::int64_t last_statistics_accuracy_check_at_{0};
  std::string statistics_accuracy_profile_id_;
  int history_backfill_interval_seconds_{0};
  std::string refresh_source_snapshot_profile_id_;
  std::vector<PlayerProfileGameSourceRow> refresh_source_snapshot_;
  bool refresh_source_snapshot_ready_{false};
  std::atomic_uint64_t source_sweep_count_{0};
  std::atomic_uint64_t source_sweep_rows_{0};
  std::atomic_uint64_t source_sweep_total_ms_{0};
  std::atomic_uint64_t source_sweep_last_ms_{0};
  std::atomic_uint64_t source_lookup_count_{0};
  std::atomic_uint64_t source_lookup_hits_{0};
  std::atomic_uint64_t source_lookup_total_ms_{0};
  std::atomic_uint64_t source_lookup_last_ms_{0};
  std::atomic_uint64_t refresh_source_snapshot_hits_{0};
};

}  // namespace kchess
