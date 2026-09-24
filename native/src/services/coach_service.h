#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>

#include "ai/coach_orchestrator.h"
#include "ai/models/portable_small_models.h"
#include "persistence/database.h"
#include "services/analysis_service.h"

namespace kchess {
struct CoachHintCache;
namespace knowledge { class KnowledgeRuntime; }

// -----------------------------------------------------------------------------
// Section: Native coach integration service
// -----------------------------------------------------------------------------

class CoachService {
 public:
  CoachService(Database& database, AnalysisService& analysis_service,
               knowledge::KnowledgeRuntime& knowledge_runtime,
               std::filesystem::path small_model_root = {});
  ~CoachService();

  CoachService(const CoachService&) = delete;
  CoachService& operator=(const CoachService&) = delete;

  [[nodiscard]] std::string ask_json(const std::string& request_json);
  [[nodiscard]] std::string context_json(const std::string& request_json) const;
  [[nodiscard]] std::string automatic_json(const std::string& request_json) const;

  [[nodiscard]] std::string start_ask_json(const std::string& request_json);
  [[nodiscard]] std::string start_automatic_json(const std::string& request_json);
  [[nodiscard]] std::string start_hint_json(const std::string& request_json);
  [[nodiscard]] std::string job_status_json(const std::string& job_id);
  [[nodiscard]] std::string performance_diagnostics_json() const;
  void cancel_job(const std::string& job_id);

 private:
  enum class JobKind { ask, automatic, hint };

  struct CoachJob {
    std::string id;
    std::string session_id;
    JobKind kind{JobKind::ask};
    std::atomic_bool finished{false};
    std::atomic_bool cancelled{false};
    mutable std::mutex state_mutex;
    std::string state{"running"};
    std::string result_json;
    std::string error_message;
    std::thread worker;
  };

  [[nodiscard]] std::string start_job(
      std::string request_json, JobKind kind, std::string session_id = {});
  [[nodiscard]] std::string ask_json_impl(
      const std::string& request_json, const std::atomic_bool* cancelled);
  [[nodiscard]] std::string automatic_json_impl(
      const std::string& request_json, const std::atomic_bool* cancelled) const;
  void cancel_superseded_automatic_jobs(const std::string& session_id);
  void cancel_automatic_jobs_for_foreground();
  [[nodiscard]] bool acquire_execution_slot(
      JobKind kind, const std::atomic_bool* cancelled = nullptr) const;
  void release_execution_slot() const;
  [[nodiscard]] std::optional<std::int64_t> seconds_since_last_automatic(
      const std::string& key, std::int64_t now_seconds) const;
  void record_automatic_delivery(
      const std::string& key, std::int64_t now_seconds) const;
  void run_job(
      std::string request_json, JobKind kind,
      const std::shared_ptr<CoachJob>& job) noexcept;

  struct PerformanceTotals {
    std::uint64_t requests{0};
    std::uint64_t automatic_requests{0};
    std::uint64_t accepted_requests{0};
    std::uint64_t validation_failures{0};
    std::uint64_t repaired_responses{0};
    std::uint64_t provider_errors{0};
    std::uint64_t provider_calls{0};
    std::uint64_t total_ms{0};
    std::uint64_t session_ms{0};
    std::uint64_t route_ms{0};
    std::uint64_t planning_ms{0};
    std::uint64_t context_ms{0};
    std::uint64_t retrieval_ms{0};
    std::uint64_t position_analysis_ms{0};
    std::uint64_t practicality_ms{0};
    std::uint64_t teaching_plan_ms{0};
    std::uint64_t position_cache_requests{0};
    std::uint64_t position_cache_any_hits{0};
    std::uint64_t position_cache_full_hits{0};
    std::uint64_t response_cache_requests{0};
    std::uint64_t response_cache_hits{0};
    std::uint64_t provider_evidence_input_items{0};
    std::uint64_t provider_evidence_selected_items{0};
    std::uint64_t provider_exact_duplicates_dropped{0};
    std::uint64_t provider_estimated_input_tokens{0};
    std::uint64_t provider_estimated_selected_tokens{0};
    std::uint64_t provider_request_ms{0};
    std::uint64_t response_cache_key_ms{0};
    std::uint64_t provider_ms{0};
    std::uint64_t validation_ms{0};
    std::uint64_t repair_provider_ms{0};
  };

  void record_performance_trace(const ai::CoachPipelineTrace& trace);

  Database& database_;
  AnalysisService& analysis_service_;
  std::shared_ptr<CoachHintCache> hint_cache_;
  ai::LoadedSmallModelSuite small_models_;
  ai::CoachOrchestrator orchestrator_;
  mutable std::mutex execution_state_mutex_;
  mutable std::condition_variable execution_cv_;
  mutable bool execution_active_{false};
  mutable JobKind execution_active_kind_{JobKind::ask};
  mutable std::uint64_t foreground_waiters_{0};
  mutable std::uint64_t automatic_waiters_{0};
  mutable std::atomic_uint64_t automatic_preemptions_{0};
  mutable std::atomic_uint64_t foreground_wait_count_{0};
  mutable std::atomic_uint64_t foreground_wait_total_ms_{0};
  mutable std::atomic_uint64_t foreground_wait_last_ms_{0};
  mutable std::mutex automatic_history_mutex_;
  mutable std::unordered_map<std::string, std::int64_t> last_automatic_at_;
  mutable std::mutex performance_mutex_;
  PerformanceTotals performance_totals_;
  std::deque<ai::CoachPipelineTrace> recent_performance_;
  mutable std::mutex jobs_mutex_;
  std::unordered_map<std::string, std::shared_ptr<CoachJob>> jobs_;
  std::atomic_uint64_t next_job_id_{1};
};

}  // namespace kchess
