#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

#include "ai/coach_orchestrator.h"
#include "persistence/database.h"
#include "services/analysis_service.h"

namespace kchess {
struct CoachHintCache;

// -----------------------------------------------------------------------------
// Section: Native coach integration service
// -----------------------------------------------------------------------------

class CoachService {
 public:
  CoachService(Database& database, AnalysisService& analysis_service);
  ~CoachService();

  CoachService(const CoachService&) = delete;
  CoachService& operator=(const CoachService&) = delete;

  [[nodiscard]] std::string ask_json(const std::string& request_json) const;
  [[nodiscard]] std::string context_json(const std::string& request_json) const;
  [[nodiscard]] std::string automatic_json(const std::string& request_json) const;

  [[nodiscard]] std::string start_ask_json(const std::string& request_json);
  [[nodiscard]] std::string start_automatic_json(const std::string& request_json);
  [[nodiscard]] std::string start_hint_json(const std::string& request_json);
  [[nodiscard]] std::string job_status_json(const std::string& job_id);
  void cancel_job(const std::string& job_id);

 private:
  enum class JobKind { ask, automatic, hint };

  struct CoachJob {
    std::string id;
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
      std::string request_json, JobKind kind);
  [[nodiscard]] std::string automatic_json_impl(
      const std::string& request_json, const std::atomic_bool* cancelled) const;
  void cancel_superseded_automatic_jobs();
  void run_job(
      std::string request_json, JobKind kind,
      const std::shared_ptr<CoachJob>& job) noexcept;

  Database& database_;
  AnalysisService& analysis_service_;
  std::shared_ptr<CoachHintCache> hint_cache_;
  ai::CoachOrchestrator orchestrator_;
  mutable std::mutex execution_mutex_;
  mutable std::mutex jobs_mutex_;
  std::unordered_map<std::string, std::shared_ptr<CoachJob>> jobs_;
  std::atomic_uint64_t next_job_id_{1};
};

}  // namespace kchess
