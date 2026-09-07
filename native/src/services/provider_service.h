#pragma once

#include <atomic>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

#include "core/models.h"
#include "http/http_client.h"
#include "persistence/database.h"
#include "providers/game_provider.h"
#include "providers/provider_request_scheduler.h"
#include "services/profile_service.h"

namespace kchess {

// Owns online-provider transports, synchronization jobs and provider cache
// orchestration. Core only coordinates lifecycle between profiles/analysis.
class ProviderService {
 public:
  ProviderService(
      Database& database, ProfileService& profile_service,
      std::filesystem::path data_directory);
  ~ProviderService();

  ProviderService(const ProviderService&) = delete;
  ProviderService& operator=(const ProviderService&) = delete;

  std::string start_provider_profile_json(ProfileType type, const std::string& username);
  // Fetch a public player's profile + rating stats for a scouting comparison,
  // without creating a profile or writing any games to the database. The job
  // result is a ProviderOverview-shaped JSON for the target username.
  std::string start_scout_json(ProfileType type, const std::string& username);
  // Deep scouting report: fetches recent archives and aggregates the target's
  // win/draw/loss by colour, time control, termination and opening (from the
  // PGN's own ECO tags) in memory, without persisting anything.
  std::string start_scout_report_json(ProfileType type, const std::string& username);
  std::string start_provider_sync_json(
      const std::string& profile_id, int year, int month);
  std::string provider_job_status_json(const std::string& job_id);
  void cancel_provider_job(const std::string& job_id);
  std::string provider_overview_json(const std::string& profile_id);

  void cancel_jobs_for_other_profiles(const std::string& profile_id);
  void cancel_jobs_for_profile(const std::string& profile_id);

 private:
  struct ProviderJob {
    std::string id;
    std::string profile_id;
    std::shared_ptr<CancelToken> cancel{std::make_shared<CancelToken>()};
    std::atomic_bool finished{false};
    mutable std::mutex state_mutex;
    std::string state{"running"};
    std::string result_json;
    std::string error_kind;
    std::string error_message;
    std::thread worker;
  };

  std::unique_ptr<GameProvider> provider_for(ProfileType type);
  void run_provider_create(
      ProfileType type, std::string username,
      const std::shared_ptr<ProviderJob>& job) noexcept;
  void run_scout(
      ProfileType type, std::string username,
      const std::shared_ptr<ProviderJob>& job) noexcept;
  void run_scout_report(
      ProfileType type, std::string username,
      const std::shared_ptr<ProviderJob>& job) noexcept;
  void run_provider_sync(
      std::string profile_id, int year, int month,
      const std::shared_ptr<ProviderJob>& job) noexcept;
  void sync_provider_resources(
      const std::string& profile_id, GameProvider& provider, int year, int month,
      const std::shared_ptr<ProviderJob>& job);
  void finish_provider_job(
      const std::shared_ptr<ProviderJob>& job, std::string state,
      std::string result, std::string error_kind = {},
      std::string error_message = {}) noexcept;
  void cache_provider_avatar(
      const std::string& profile_id, const ProviderProfile& profile,
      const std::shared_ptr<CancelToken>& cancel) noexcept;

  Database& database_;
  ProfileService& profile_service_;
  std::filesystem::path data_directory_;
  std::unique_ptr<HttpClient> http_client_;
  ProviderRequestScheduler provider_scheduler_;
  mutable std::mutex provider_jobs_mutex_;
  std::unordered_map<std::string, std::shared_ptr<ProviderJob>> provider_jobs_;
  std::atomic_uint64_t next_provider_job_id_{1};
};

}  // namespace kchess
