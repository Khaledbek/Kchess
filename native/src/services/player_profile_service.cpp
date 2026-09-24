#include "services/player_profile_service.h"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "ai/profile/adaptive_profile_analyzer.h"
#include "ai/profile/chess_profile.h"
#include "ai/profile/game_relevance_scorer.h"
#include "ai/profile/profile_evidence_adapter.h"
#include "ai/profile/profile_analysis_funnel.h"
#include "ai/profile/profile_updater.h"
#include "ai/profile/sample_builder.h"
#include "diagnostics/logger.h"
#include "knowledge/knowledge_runtime.h"
#include "persistence/sqlite_write_priority.h"
#include "services/provider_service.h"

namespace kchess {
namespace {

std::int64_t unix_time_seconds() {
  return std::chrono::duration_cast<std::chrono::seconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

std::int64_t unix_time_milliseconds() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

// Native mutations and AnalysisService terminal events explicitly request a
// resync. This low-frequency pass is only a safety net for missed/external
// changes; it must not turn the expensive all-game source projection into a
// polling loop.
constexpr std::int64_t kProfileSafetyResyncSeconds = 300;
// Give the first interactive frame and initial Flutter/FFI reads a quiet window
// before profile maintenance starts competing for CPU and SQLite. Native wake
// events are retained during the grace period and are processed afterwards.
constexpr std::int64_t kProfileStartupGraceMs = 4000;


std::string graph_source_signature(
    const std::vector<PlayerProfileGameSourceRow>& sources) {
  std::uint64_t hash = 1469598103934665603ULL;
  auto mix = [&](const std::string_view value) {
    for (const unsigned char byte : value) {
      hash ^= static_cast<std::uint64_t>(byte);
      hash *= 1099511628211ULL;
    }
    hash ^= 0xffU;
    hash *= 1099511628211ULL;
  };
  for (const auto& source : sources) {
    mix(source.game_id);
    mix(std::to_string(source.source_version));
    mix(std::to_string(source.move_count));
    mix(std::to_string(source.played_at));
    mix(source.opening_eco);
    mix(source.opening_name);
    mix(source.result);
    mix(source.provider_outcome);
    mix(source.player_color);
    mix(source.time_control_type);
  }
  std::ostringstream out;
  out << "fnv1a64:" << std::hex << std::setfill('0') << std::setw(16) << hash;
  return out.str();
}

ai::ProfileGameObservation observation_from_source(
    const PlayerProfileGameSourceRow& row,
    const std::vector<PlayerProfileMoveSourceRow>& moves = {}) {
  ai::ProfileGameObservation observation;
  observation.game_id = row.game_id;
  observation.played_at = row.played_at;
  observation.source_version = row.source_version;
  observation.time_control_type = row.time_control_type;
  observation.result = row.result;
  observation.provider_outcome = row.provider_outcome;
  observation.player_color = row.player_color;
  observation.player_rating = row.player_rating;
  observation.opponent_rating = row.opponent_rating;
  observation.accuracy = row.accuracy;
  observation.opening_eco = row.opening_eco;
  observation.opening_name = row.opening_name;
  observation.move_count = row.move_count;
  observation.analysis_complete = row.analysis_complete;
  observation.classifications = ai::ProfileClassificationCounts{
      .theory = row.theory,
      .brilliant = row.brilliant,
      .critical = row.critical,
      .best = row.best,
      .excellent = row.excellent,
      .miss = row.miss,
      .mistake = row.mistake,
      .blunder = row.blunder,
      .analyzed_moves = row.analyzed_moves,
  };
  observation.phases = {
      ai::ProfilePhaseCounts{
          .phase = ai::ProfilePhase::opening,
          .analyzed_moves = row.opening_analyzed_moves,
          .miss = row.opening_miss,
          .mistake = row.opening_mistake,
          .blunder = row.opening_blunder,
      },
      ai::ProfilePhaseCounts{
          .phase = ai::ProfilePhase::middlegame,
          .analyzed_moves = row.middlegame_analyzed_moves,
          .miss = row.middlegame_miss,
          .mistake = row.middlegame_mistake,
          .blunder = row.middlegame_blunder,
      },
      ai::ProfilePhaseCounts{
          .phase = ai::ProfilePhase::endgame,
          .analyzed_moves = row.endgame_analyzed_moves,
          .miss = row.endgame_miss,
          .mistake = row.endgame_mistake,
          .blunder = row.endgame_blunder,
      },
  };
  observation.average_expected_score_loss = row.average_expected_score_loss;
  observation.maximum_expected_score_loss = row.maximum_expected_score_loss;
  observation.moves.reserve(moves.size());
  for (const auto& move : moves) {
    observation.moves.push_back(ai::ProfileMoveObservation{
        .ply = move.ply,
        .classification = move.classification,
        .expected_score_loss = move.expected_score_loss,
        .theory = move.theory,
        .fen_before = move.fen_before,
        .uci = move.uci,
    });
  }
  return observation;
}

std::optional<int> profile_rating(
    const Profile& profile,
    const std::vector<ai::ProfileGameEvidence>& games) {
  if (profile.fide) return profile.fide;
  long long total = 0;
  int count = 0;
  for (const auto& game : games) {
    if (!game.player_rating) continue;
    total += *game.player_rating;
    ++count;
  }
  return count > 0 ? std::optional<int>(static_cast<int>(total / count)) : std::nullopt;
}

std::string job_state(const std::string& payload) {
  try {
    const auto value = nlohmann::json::parse(payload);
    return value.value("jobState", value.value("status", "running"));
  } catch (...) {
    return "failed";
  }
}

double effective_relevance(const double raw, const std::string& queue_reason) {
  if (queue_reason == "new_game" || queue_reason == "open_hypothesis" ||
      queue_reason == "knowledge_gap" ||
      queue_reason == "current_missing_evidence" || queue_reason == "initial_sample") {
    return std::max(raw, 0.62);
  }
  return raw;
}

}  // namespace

PlayerProfileService::PlayerProfileService(
    Database& database,
    AnalysisService& analysis_service,
    ProviderService& provider_service,
    knowledge::KnowledgeRuntime& knowledge_runtime)
    : database_(database),
      analysis_service_(analysis_service),
      provider_service_(provider_service),
      knowledge_runtime_(knowledge_runtime) {
  analysis_service_.set_profile_analysis_observer(
      [this](const std::string& game_id) {
        (void)game_id;
        // Main-line analysis completion/cancellation is a native wake event.
        // Persisted queue/cache state remains authoritative; the callback only
        // avoids waiting for UI polling or the periodic background timeout.
        resync_requested_ = true;
        profile_refresh_requested_ = true;
        wake_cv_.notify_all();
      });
}

PlayerProfileService::~PlayerProfileService() {
  // Unregister while this object is still alive. AnalysisService serializes
  // observer invocation, so no callback can outlive PlayerProfileService.
  analysis_service_.set_profile_analysis_observer({});
  stop_ = true;
  pause_engine_work();
  wake_cv_.notify_all();
  if (worker_.joinable()) worker_.join();
}

void PlayerProfileService::start() {
  if (worker_.joinable()) return;

  stop_ = false;
  startup_grace_until_ms_.store(
      unix_time_milliseconds() + kProfileStartupGraceMs,
      std::memory_order_relaxed);
  set_worker_activity(
      "startup_grace", "deferred", "protect_initial_interactive_startup");
  worker_ = std::thread([this] { run(); });
}

void PlayerProfileService::notify_profile_changed() {
  resync_requested_ = true;
  profile_refresh_requested_ = true;
  wake_cv_.notify_all();
}

void PlayerProfileService::pause_engine_work() {
  pause_requested_ = true;
  const auto game_id = current_game();
  if (game_id) {
    try {
      analysis_service_.cancel_analysis(*game_id);
    } catch (...) {
    }
  }
  wake_cv_.notify_all();
}

void PlayerProfileService::set_current_game(
    const std::optional<std::string>& game_id) {
  std::lock_guard lock(current_mutex_);
  current_game_id_ = game_id;
}

std::optional<std::string> PlayerProfileService::current_game() const {
  std::lock_guard lock(current_mutex_);
  return current_game_id_;
}

void PlayerProfileService::set_worker_activity(
    std::string operation, std::string phase, std::string detail,
    const std::string& profile_id, const std::size_t completed,
    const std::size_t total) {
  const auto timestamp = unix_time_milliseconds();
  std::lock_guard lock(worker_activity_mutex_);
  const bool new_operation = !worker_activity_.active ||
      worker_activity_.operation != operation ||
      worker_activity_.profile_id != profile_id;
  if (new_operation) {
    worker_activity_.started_at_ms = timestamp;
    worker_activity_.phase_started_at_ms = timestamp;
  } else if (worker_activity_.phase != phase) {
    worker_activity_.phase_started_at_ms = timestamp;
  }
  worker_activity_.active = true;
  worker_activity_.operation = std::move(operation);
  worker_activity_.phase = std::move(phase);
  worker_activity_.detail = std::move(detail);
  worker_activity_.profile_id = profile_id;
  worker_activity_.completed = completed;
  worker_activity_.total = total;
  worker_activity_.updated_at_ms = timestamp;
}

void PlayerProfileService::clear_worker_activity() noexcept {
  std::lock_guard lock(worker_activity_mutex_);
  worker_activity_.active = false;
  worker_activity_.operation = "idle";
  worker_activity_.phase = "idle";
  worker_activity_.detail = "no_profile_background_work";
  worker_activity_.profile_id.clear();
  worker_activity_.completed = 0;
  worker_activity_.total = 0;
  worker_activity_.phase_started_at_ms = 0;
  worker_activity_.updated_at_ms = unix_time_milliseconds();
}

std::string PlayerProfileService::worker_activity_json() const {
  const auto timestamp = unix_time_milliseconds();
  std::lock_guard lock(worker_activity_mutex_);
  const auto elapsed = worker_activity_.active && worker_activity_.started_at_ms > 0
      ? std::max<std::int64_t>(0, timestamp - worker_activity_.started_at_ms)
      : 0;
  const auto phase_elapsed =
      worker_activity_.active && worker_activity_.phase_started_at_ms > 0
      ? std::max<std::int64_t>(0, timestamp - worker_activity_.phase_started_at_ms)
      : 0;
  return nlohmann::json({
      {"active", worker_activity_.active},
      {"operation", worker_activity_.operation},
      {"phase", worker_activity_.phase},
      {"detail", worker_activity_.detail},
      {"workClass", worker_activity_.active ? "background_profile_maintenance" : "idle"},
      {"profileId", worker_activity_.profile_id},
      {"completed", worker_activity_.completed},
      {"total", worker_activity_.total},
      {"progressKnown", worker_activity_.total > 0},
      {"startedAtMs", worker_activity_.started_at_ms},
      {"phaseStartedAtMs", worker_activity_.phase_started_at_ms},
      {"elapsedMs", elapsed},
      {"phaseElapsedMs", phase_elapsed},
      {"updatedAtMs", worker_activity_.updated_at_ms},
  }).dump();
}

std::string PlayerProfileService::snapshot_json() {
  const auto profile = database_.active_profile();
  if (!profile) return "null";
  const auto profile_id = database_.player_profile_owner_id(profile->id);

  // UI reads stay cheap and never rebuild the learned model. They do, however,
  // overlay fresh queue counters plus live progress from the one shared
  // AnalysisService job that profile maintenance started. This keeps the
  // progress bar moving while a long game is being analysed instead of only
  // jumping when an entire relevant game reaches `done`.
  const auto payload = database_.ai_chess_profile_payload(profile_id);
  if (!payload) return "null";

  try {
    auto value = nlohmann::json::parse(*payload);
    auto& background = value["background"];
    if (!background.is_object()) background = nlohmann::json::object();
    background.erase("knownGames");
    background.erase("processedGames");
    background.erase("fastProbePositions");
    background.erase("verificationProbePositions");
    background.erase("deepProbePositions");

    const auto queue_progress = database_.ai_profile_queue_progress(profile_id);
    const auto sampling_state = database_.ai_profile_sampling_state(profile_id);
    const auto history_progress =
        provider_service_.player_profile_history_progress(profile_id);
    background["totalGames"] = queue_progress.total;
    background["historyAccounts"] = history_progress.account_count;
    background["historyDiscoveredAccounts"] =
        history_progress.discovered_account_count;
    background["historyAvailableMonths"] = history_progress.available_month_count;
    background["historySyncedMonths"] = history_progress.synced_month_count;
    background["historyPendingMonths"] = history_progress.pending_month_count;
    background["historyComplete"] = history_progress.complete;
    background["indexedGames"] = queue_progress.total;
    background["historicalSampleGames"] = queue_progress.historical_sample;
    const int sample_budget = sampling_state
        ? sampling_state->recommended_games
        : queue_progress.historical_sample;
    background["historicalSampleBudget"] = sample_budget;
    background["samplingProgress"] = sample_budget > 0
        ? std::clamp(
              static_cast<double>(queue_progress.historical_sample) /
                  static_cast<double>(sample_budget),
              0.0, 1.0)
        : 0.0;
    if (sampling_state) {
      background["samplingEligibleGames"] = sampling_state->eligible_games;
      background["samplingExcludedGames"] = sampling_state->excluded_games;
      background["samplingMinimumGames"] = sampling_state->minimum_games;
      background["samplingMaximumGames"] = sampling_state->maximum_games;
      background["samplingRequiredStrata"] = sampling_state->required_strata;
      background["samplingCoveredStrata"] = sampling_state->covered_strata;
      background["samplingCoverage"] = sampling_state->covered_population_share;
      background["samplingDiversity"] = sampling_state->diversity_score;
      background["samplingCapped"] = sampling_state->capped;
    }
    background["interestingGames"] = queue_progress.interesting;
    background["enginePromotedGames"] = queue_progress.engine_promoted_games;
    background["relevantGames"] = queue_progress.relevant;
    background["resolvedRelevantGames"] = queue_progress.resolved_relevant;
    background["queuedGames"] = queue_progress.queued;
    background["enginePendingGames"] = queue_progress.engine_pending;

    double current_progress = 0.0;
    bool has_current_profile_game = false;
    bool has_live_profile_game = false;
    if (const auto game_id = current_game()) {
      background["currentGameId"] = *game_id;
      try {
        const auto live = nlohmann::json::parse(
            analysis_service_.analysis_status_json(*game_id));
        const int completed = live.value("completedPlies", 0);
        const int total = live.value("totalPlies", 0);
        const int live_depth = live.value("liveDepth", 0);
        const int target_depth = live.value(
            "summary", nlohmann::json::object()).value("engineDepth", 0);
        current_progress = live.value("progress", 0.0);

        // Whole-ply progress can remain visually frozen for several seconds on
        // a difficult position. Blend the current search depth into the
        // unfinished ply only for presentation; persisted completion semantics
        // remain unchanged and still come exclusively from AnalysisService.
        if (total > 0 && completed < total && live_depth > 0 && target_depth > 0) {
          const double depth_fraction = std::clamp(
              static_cast<double>(live_depth) / static_cast<double>(target_depth),
              0.0, 0.99);
          current_progress = std::clamp(
              (static_cast<double>(completed) + depth_fraction) /
                  static_cast<double>(total),
              0.0, 0.999);
        }

        background["currentGameProgress"] = current_progress;
        background["currentGameCompletedPlies"] = completed;
        background["currentGameTotalPlies"] = total;
        background["currentGamePly"] = live.value("currentPly", -1);
        background["currentGameDepth"] = live_depth;
        background["currentGameTargetDepth"] = target_depth;
        const auto live_state = live.value("jobState", "running");
        background["currentGameJobState"] = live_state;
        has_current_profile_game = true;
        has_live_profile_game = live_state == "queued" || live_state == "running" ||
            live_state == "cancelling";
      } catch (...) {
        // The shared job may have completed between current_game() and this
        // snapshot. Queue counters below remain authoritative in that race.
      }
    } else {
      background.erase("currentGameId");
      background.erase("currentGameProgress");
      background.erase("currentGameCompletedPlies");
      background.erase("currentGameTotalPlies");
      background.erase("currentGamePly");
      background.erase("currentGameDepth");
      background.erase("currentGameTargetDepth");
      background.erase("currentGameJobState");
    }

    const int history_total_units =
        history_progress.account_count + history_progress.available_month_count;
    const int history_done_units = history_progress.discovered_account_count +
        history_progress.synced_month_count;
    const double history_fraction = history_total_units > 0
        ? std::clamp(
              static_cast<double>(history_done_units) /
                  static_cast<double>(history_total_units),
              0.0, 1.0)
        : 1.0;
    background["historyProgress"] = history_fraction;

    double relevant_done = static_cast<double>(queue_progress.resolved_relevant);
    if (has_current_profile_game && current_progress > 0.0) {
      relevant_done += current_progress;
    }
    const double relevant_fraction = queue_progress.relevant > 0
        ? std::clamp(
              relevant_done / static_cast<double>(queue_progress.relevant),
              0.0, 1.0)
        : 1.0;
    background["relevantProgress"] = relevant_fraction;

    const int total_progress_units = history_total_units + queue_progress.relevant;
    const double completed_progress_units =
        static_cast<double>(history_done_units) + relevant_done;
    double overall_progress = 0.0;
    if (total_progress_units > 0) {
      overall_progress = std::clamp(
          completed_progress_units / static_cast<double>(total_progress_units),
          0.0, 1.0);
    } else if (queue_progress.total > 0 && history_progress.complete) {
      overall_progress = 1.0;
    }
    background["progressCompletedUnits"] = completed_progress_units;
    background["progressTotalUnits"] = total_progress_units;
    background["overallProgress"] = overall_progress;

    if (!history_progress.complete) {
      background["status"] = "syncing_history";
    } else if (queue_progress.total == 0) {
      background["status"] = "empty";
    } else if (has_live_profile_game || queue_progress.processing > 0) {
      background["status"] = "analyzing";
    } else if (queue_progress.resolved_relevant >= queue_progress.relevant) {
      background["status"] = "complete";
    } else {
      background["status"] = "queued";
    }
    background["initialPreparationComplete"] =
        history_progress.complete && queue_progress.total > 0 &&
        !has_live_profile_game && queue_progress.processing == 0 &&
        queue_progress.resolved_relevant >= queue_progress.relevant;
    return value.dump();
  } catch (...) {
    return *payload;
  }
}

std::string PlayerProfileService::diagnostics_json() {
  const auto profile = database_.active_profile();
  if (!profile) return "null";
  const auto profile_id = database_.player_profile_owner_id(profile->id);
  const auto queue_progress = database_.ai_profile_queue_progress(profile_id);
  const auto sampling_state = database_.ai_profile_sampling_state(profile_id);
  const auto history_progress =
      provider_service_.player_profile_history_progress(profile_id);

  nlohmann::json background = {
      {"profileId", profile_id},
      {"totalGames", queue_progress.total},
      {"indexedGames", queue_progress.total},
      {"historicalSampleGames", queue_progress.historical_sample},
      {"interestingGames", queue_progress.interesting},
      {"enginePromotedGames", queue_progress.engine_promoted_games},
      {"relevantGames", queue_progress.relevant},
      {"resolvedRelevantGames", queue_progress.resolved_relevant},
      {"queuedGames", queue_progress.queued},
      {"enginePendingGames", queue_progress.engine_pending},
      {"processingGames", queue_progress.processing},
      {"historyAccounts", history_progress.account_count},
      {"historyDiscoveredAccounts", history_progress.discovered_account_count},
      {"historyAvailableMonths", history_progress.available_month_count},
      {"historySyncedMonths", history_progress.synced_month_count},
      {"historyPendingMonths", history_progress.pending_month_count},
      {"historyComplete", history_progress.complete},
      {"analysisServiceBusy", analysis_service_.has_active_work()},
      {"samplingMinimumPlies", ai::kProfileSamplingMinimumPlies},
      {"profileEnginePausedForForeground", pause_requested_.load()},
      {"profileRefreshPending", profile_refresh_requested_.load()},
      {"queueResyncPending", resync_requested_.load()},
      {"sourceSweeps", {
          {"count", source_sweep_count_.load(std::memory_order_relaxed)},
          {"rows", source_sweep_rows_.load(std::memory_order_relaxed)},
          {"lastDurationMs", source_sweep_last_ms_.load(std::memory_order_relaxed)},
          {"totalDurationMs", source_sweep_total_ms_.load(std::memory_order_relaxed)},
      }},
      {"sourceLookups", {
          {"count", source_lookup_count_.load(std::memory_order_relaxed)},
          {"hits", source_lookup_hits_.load(std::memory_order_relaxed)},
          {"lastDurationMs", source_lookup_last_ms_.load(std::memory_order_relaxed)},
          {"totalDurationMs", source_lookup_total_ms_.load(std::memory_order_relaxed)},
      }},
      {"refreshSourceSnapshotHits",
       refresh_source_snapshot_hits_.load(std::memory_order_relaxed)},
      {"safetyResyncSeconds", kProfileSafetyResyncSeconds},
  };
  const auto grace_until = startup_grace_until_ms_.load(std::memory_order_relaxed);
  background["startupGraceMs"] = kProfileStartupGraceMs;
  background["startupGraceRemainingMs"] = std::max<std::int64_t>(
      0, grace_until - unix_time_milliseconds());
  const auto worker_payload = nlohmann::json::parse(
      worker_activity_json(), nullptr, false);
  if (!worker_payload.is_discarded() && worker_payload.is_object()) {
    background["workerActivity"] = worker_payload;
  }

  if (sampling_state) {
    background["samplingBootstrapCutoffPlayedAt"] =
        sampling_state->bootstrap_cutoff_played_at;
    background["samplingTotalGames"] = sampling_state->total_games;
    background["samplingEligibleGames"] = sampling_state->eligible_games;
    background["samplingExcludedGames"] = sampling_state->excluded_games;
    background["samplingMinimumGames"] = sampling_state->minimum_games;
    background["historicalSampleBudget"] = sampling_state->recommended_games;
    background["samplingMaximumGames"] = sampling_state->maximum_games;
    background["samplingSelectedGames"] = sampling_state->selected_games;
    background["samplingRequiredStrata"] = sampling_state->required_strata;
    background["samplingCoveredStrata"] = sampling_state->covered_strata;
    background["samplingCoverage"] = sampling_state->covered_population_share;
    background["samplingDiversity"] = sampling_state->diversity_score;
    background["samplingCapped"] = sampling_state->capped;
    background["samplingUpdatedAt"] = sampling_state->updated_at;
  } else {
    background["historicalSampleBudget"] = queue_progress.historical_sample;
    background["samplingMaximumGames"] =
        static_cast<int>(ai::ProfileSamplingPolicy{}.maximum_games);
    background["samplingCapped"] = false;
  }

  bool has_live_profile_game = false;
  if (const auto game_id = current_game()) {
    background["currentGameId"] = *game_id;
    try {
      const auto live = nlohmann::json::parse(
          analysis_service_.analysis_status_json(*game_id));
      background["currentGameProgress"] = live.value("progress", 0.0);
      background["currentGameCompletedPlies"] =
          live.value("completedPlies", 0);
      background["currentGameTotalPlies"] = live.value("totalPlies", 0);
      background["currentGamePly"] = live.value("currentPly", -1);
      background["currentGameDepth"] = live.value("liveDepth", 0);
      background["currentGameTargetDepth"] =
          live.value("summary", nlohmann::json::object())
              .value("engineDepth", 0);
      const auto state = live.value("jobState", "running");
      background["currentGameJobState"] = state;
      has_live_profile_game =
          state == "queued" || state == "running" || state == "cancelling";
    } catch (...) {
      has_live_profile_game = analysis_service_.has_active_work();
    }
  }

  if (!history_progress.complete) {
    background["status"] = "syncing_history";
    background["activeOperation"] = "provider_history_sync";
  } else if (queue_progress.total == 0) {
    background["status"] = "empty";
    background["activeOperation"] = "metadata_index";
  } else if (has_live_profile_game || queue_progress.processing > 0) {
    background["status"] = "analyzing";
    background["activeOperation"] = "profile_engine_analysis";
  } else if (pause_requested_.load() && analysis_service_.has_active_work()) {
    background["status"] = "waiting";
    background["activeOperation"] = "waiting_for_foreground_analysis";
  } else if (profile_refresh_requested_.load()) {
    background["status"] = "refresh_pending";
    background["activeOperation"] = "profile_knowledge_refresh";
  } else if (queue_progress.resolved_relevant >= queue_progress.relevant) {
    background["status"] = "complete";
    background["activeOperation"] = "idle";
  } else {
    background["status"] = "queued";
    background["activeOperation"] = "profile_queue";
  }
  if (!worker_payload.is_discarded() && worker_payload.is_object() &&
      worker_payload.value("active", false)) {
    background["activeOperation"] = worker_payload.value("operation", "unknown");
    background["activePhase"] = worker_payload.value("phase", "unknown");
    background["activeDetail"] = worker_payload.value("detail", "unknown");
  }
  background["initialPreparationComplete"] =
      history_progress.complete && queue_progress.total > 0 &&
      !has_live_profile_game && queue_progress.processing == 0 &&
      queue_progress.resolved_relevant >= queue_progress.relevant;

  return background.dump();
}

void PlayerProfileService::run() noexcept {
  while (!stop_) {
    const auto grace_until =
        startup_grace_until_ms_.load(std::memory_order_relaxed);
    const auto current_ms = unix_time_milliseconds();
    if (grace_until > current_ms) {
      set_worker_activity(
          "startup_grace", "deferred", "protect_initial_interactive_startup",
          {}, static_cast<std::size_t>(kProfileStartupGraceMs -
              std::max<std::int64_t>(0, grace_until - current_ms)),
          static_cast<std::size_t>(kProfileStartupGraceMs));
      std::unique_lock lock(wake_mutex_);
      wake_cv_.wait_for(
          lock,
          std::chrono::milliseconds(
              std::min<std::int64_t>(500, grace_until - current_ms)));
      continue;
    }
    if (grace_until != 0) {
      startup_grace_until_ms_.store(0, std::memory_order_relaxed);
      clear_worker_activity();
    }

    bool did_work = false;
    try {
      did_work = process_once();
    } catch (const std::exception& error) {
      clear_worker_activity();
      diagnostics::warning("player_profile", std::string("background error: ") + error.what());
    } catch (...) {
      clear_worker_activity();
      diagnostics::warning("player_profile", "unknown background error");
    }

    std::unique_lock lock(wake_mutex_);
    // Any native state transition may wake profile maintenance immediately. A
    // predicate that only watched `stop_` would consume notify_all() calls and
    // still sleep until the timeout, defeating the AnalysisService hand-off.
    // Spurious wakes are harmless because the persistent queue is authoritative.
    wake_cv_.wait_for(
        lock,
        did_work ? std::chrono::milliseconds(250) : std::chrono::milliseconds(1500));
  }
  clear_worker_activity();
}

bool PlayerProfileService::refresh_cached_statistics_accuracy_if_due(
    const std::string& profile_id, const std::int64_t now_seconds) {
  if (statistics_accuracy_profile_id_ != profile_id) {
    statistics_accuracy_profile_id_ = profile_id;
    last_statistics_accuracy_check_at_ = 0;
  }
  // An empty cache needs only a low-frequency check; while old rows remain,
  // the worker continues one already-computed game per pass.
  if (now_seconds - last_statistics_accuracy_check_at_ < 30 ||
      analysis_service_.has_active_work()) return false;
  const bool refreshed =
      analysis_service_.refresh_cached_accuracy_for_statistics(profile_id, 1) > 0;
  last_statistics_accuracy_check_at_ = refreshed ? 0 : now_seconds;
  return refreshed;
}

bool PlayerProfileService::process_once() {
  const auto profile = database_.active_profile();
  if (!profile) {
    clear_worker_activity();
    return false;
  }
  const auto profile_id = database_.player_profile_owner_id(profile->id);
  const auto now = unix_time_seconds();
  if (database_.app_setting("backgroundAnalysisEnabled").value_or("true") != "true") {
    // This only derives Statistics from saved game analyses. The disabled
    // profile setting still prevents all background engine and queue work.
    clear_worker_activity();
    return refresh_cached_statistics_accuracy_if_due(profile->id, now);
  }

  const bool owner_changed = synced_profile_id_ != profile_id;
  const bool first_owner_activation = owner_changed && synced_profile_id_.empty();
  bool restored_complete_startup_state = false;
  if (owner_changed) {
    set_worker_activity(
        "profile_startup", "recover_inflight", "recover_stale_in_process_queue_claims",
        profile_id);
    // `processing` is an in-process claim, not durable engine work. A previous
    // app instance cannot still own it, so recover it immediately when this
    // persisted profile owner becomes active instead of presenting a frozen
    // queue for the stale timeout window.
    database_.recover_ai_profile_queue_inflight(profile_id);

    // A completed persistent queue is already the authoritative restart point.
    // Do not immediately rescan thousands of games and rebuild derived
    // knowledge merely because the process restarted. Real provider imports,
    // analysis invalidations and the five-minute safety pass still request the
    // same existing sync path.
    if (first_owner_activation) {
      const auto queue_progress = database_.ai_profile_queue_progress(profile_id);
      const auto history_progress =
          provider_service_.player_profile_history_progress(profile_id);
      restored_complete_startup_state =
          history_progress.complete && queue_progress.total > 0 &&
          database_.ai_chess_profile_payload(profile_id).has_value() &&
          queue_progress.processing == 0 && queue_progress.queued == 0 &&
          queue_progress.engine_pending == 0 &&
          queue_progress.resolved_relevant >= queue_progress.relevant;
      if (restored_complete_startup_state) {
        resync_requested_ = false;
        profile_refresh_requested_ = false;
        synced_profile_id_ = profile_id;
        last_sync_at_ = now;
        set_worker_activity(
            "profile_startup", "resume_complete_state",
            "reuse_persisted_complete_profile_without_startup_rescan", profile_id,
            static_cast<std::size_t>(queue_progress.resolved_relevant),
            static_cast<std::size_t>(queue_progress.relevant));
      }
    }
  }

  const bool explicit_resync = resync_requested_.exchange(false);
  if (explicit_resync || (owner_changed && !restored_complete_startup_state) ||
      now - last_sync_at_ >= kProfileSafetyResyncSeconds) {
    set_worker_activity(
        "profile_queue_sync", "starting", explicit_resync
            ? "native_state_change_requested_resync"
            : owner_changed ? "profile_owner_requires_initial_sync"
                            : "low_frequency_safety_resync",
        profile_id);
    sync_queue(profile_id, now);
    // Queue synchronization is scheduler-critical; the full learned-profile /
    // graph materialization is not. Never make queue consumption wait behind
    // a large graph rebuild or a recoverable profile-refresh failure.
    profile_refresh_requested_ = true;
    synced_profile_id_ = profile_id;
    last_sync_at_ = now;
  }

  // Historical provider discovery is part of native profile maintenance, not
  // a Games-screen side effect. Fetch at most one missing month per pass so
  // large accounts resume incrementally from persisted provider-month caches.
  // AnalysisService may keep working in parallel; provider I/O never requires
  // Flutter navigation or a UI-owned provider job.
  if (profile->type != ProfileType::local_pgn_fen &&
      (last_history_backfill_at_ == 0 ||
       now - last_history_backfill_at_ >= history_backfill_interval_seconds_)) {
    last_history_backfill_at_ = now;
    set_worker_activity(
        "provider_history_sync", "check_next_month",
        "check_or_import_one_missing_provider_month", profile_id);
    const bool imported_history =
        provider_service_.backfill_player_profile_history_once(profile_id);
    // Continue a large archive quickly while there is known work. Once caught
    // up (or temporarily unavailable), reduce polling so offline/provider
    // failures cannot turn into a tight request loop.
    history_backfill_interval_seconds_ = imported_history ? 1 : 30;
    if (imported_history) {
      set_worker_activity(
          "profile_queue_sync", "after_history_import",
          "resync_queue_after_provider_history_import", profile_id);
      sync_queue(profile_id, now);
      profile_refresh_requested_ = true;
      synced_profile_id_ = profile_id;
      last_sync_at_ = now;
      return true;
    }
  }

  // Foreground analysis explicitly pauses profile engine work. Keep the pause
  // while any AnalysisService job is active; once foreground work reaches a
  // terminal state the native observer wakes us, we release the pause, and the
  // persistent queue resumes on its own. The extra idle pass closes the small
  // hand-off race between Core::pause_engine_work() and foreground job start.
  if (pause_requested_.load()) {
    if (analysis_service_.has_active_work()) {
      set_worker_activity(
          "waiting", "foreground_analysis",
          "profile_background_yields_to_foreground_analysis", profile_id);
      return false;
    }
    pause_requested_ = false;
    clear_worker_activity();
    return false;
  }

  // Keep ownership of the profile-triggered shared-analysis job until it has
  // actually stopped. Without this guard the queue could move on immediately,
  // clear currentGameId and make the UI look frozen at the previous whole-game
  // count even though Stockfish was still working in AnalysisService.
  if (const auto active_game = current_game()) {
    bool profile_job_active = false;
    try {
      const auto state = job_state(analysis_service_.analysis_status_json(*active_game));
      profile_job_active = state == "queued" || state == "running" ||
          state == "cancelling";
    } catch (...) {
      profile_job_active = analysis_service_.has_active_work();
    }
    if (profile_job_active) {
      set_worker_activity(
          "profile_engine_analysis", "waiting_for_terminal",
          "shared_analysis_service_is_processing_profile_game", profile_id);
      database_.set_ai_profile_queue_state(
          profile_id, *active_game, "engine_pending", -1);
      // If a model refresh is pending, use time in which Stockfish is already
      // working instead of delaying the start of the next queue item.
      refresh_profile_if_requested(profile_id, now);
      return false;
    }
    set_current_game(std::nullopt);
  }

  set_worker_activity(
      "profile_queue", "select_next", "select_next_persistent_profile_queue_item",
      profile_id);
  const auto queue = database_.next_ai_profile_queue(profile_id);
  if (!queue) {
    refresh_profile_if_requested(profile_id, now, true);
    // Old complete game analyses can predate persisted per-move accuracy.
    // Fill one cached game per idle pass, after profile queue work, using
    // AnalysisService's classification path without starting Stockfish.
    if (refresh_cached_statistics_accuracy_if_due(profile->id, now)) {
      return true;
    }
    if (!profile_refresh_requested_.load()) clear_worker_activity();
    return false;
  }

  set_worker_activity(
      "profile_queue", "load_source", "load_selected_game_source_only",
      profile_id);
  const auto source = load_profile_game_source(profile_id, queue->game_id);
  if (!source) {
    // A deleted/moved game can race a previously selected queue row. Let the
    // authoritative queue/source sync reconcile it instead of sweeping the
    // entire library merely to discover that one row disappeared.
    resync_requested_ = true;
    return true;
  }

  // AnalysisService marks the persistent queue stale whenever the shared
  // analysis cache changes. If the queue still carries the previous evidence
  // generation, re-run relevance/evidence synchronization immediately rather
  // than interpreting the game with an old processed_version. This is what
  // makes a later deeper foreground analysis automatically refresh the profile
  // without starting Stockfish again from profile maintenance.
  if (queue->source_version != source->source_version ||
      (queue->processed_version >= 0 &&
       queue->processed_version != source->source_version)) {
    sync_queue(profile_id, now);
    profile_refresh_requested_ = true;
    last_sync_at_ = now;
    return true;
  }

  // Metadata-only games were already indexed by sync_queue and should not
  // consume an engine slot even if a stale queue row becomes visible here.
  // Keep the terminal state distinct from `done`: metadata indexing is not
  // engine analysis and must never make the UI claim that all games were
  // analyzed.
  if (queue->reason == "metadata_only") {
    database_.set_ai_profile_queue_state(
        profile_id, source->game_id, "indexed", source->source_version);
    profile_refresh_requested_ = true;
    return true;
  }

  // Foreground analysis/bot/sideline work always wins. The staged profile
  // probe is started only when the shared AnalysisService is otherwise idle.
  if (analysis_service_.has_active_work()) {
    set_worker_activity(
        "waiting", "analysis_service_busy",
        "profile_engine_work_deferred_for_existing_analysis", profile_id);
    database_.set_ai_profile_queue_state(
        profile_id, source->game_id, "engine_pending", -1);
    return false;
  }
  set_worker_activity(
      "profile_engine_analysis", "start_or_reuse_shared_analysis",
      "process_relevant_profile_game_through_shared_analysis_cache", profile_id);
  return run_engine_request(profile_id, *queue, *source, now);
}

void PlayerProfileService::sync_queue(
    const std::string& profile_id, const std::int64_t now_seconds) {
  set_worker_activity(
      "profile_queue_sync", "load_sources",
      "load_all_profile_game_sources_for_low_frequency_queue_sync", profile_id);
  auto sources = load_profile_game_sources(profile_id);
  std::unordered_map<std::string, const PlayerProfileGameSourceRow*> source_by_id;
  source_by_id.reserve(sources.size());
  for (const auto& source : sources) source_by_id[source.game_id] = &source;

  set_worker_activity(
      "profile_queue_sync", "load_metadata",
      "load_payload_free_profile_game_metadata", profile_id, 0, sources.size());
  const auto metadata_rows = database_.profile_games_metadata(profile_id);
  std::vector<ai::ProfileGameMetadata> metadata;
  metadata.reserve(metadata_rows.size());
  for (const auto& row : metadata_rows) {
    metadata.push_back(ai::ProfileGameMetadata{
        .game_id = row.game_id,
        .played_at = row.played_at,
        .player_color = row.player_color,
        .outcome = row.provider_outcome,
        .time_control = row.time_control_type,
        .opening_eco = row.opening_eco,
        .opening_name = row.opening_name,
        .termination_type = row.termination_type,
        .player_rating = row.player_rating,
        .opponent_rating = row.opponent_rating,
        .plies = row.plies,
        .sample_eligible = row.plies >= ai::kProfileSamplingMinimumPlies,
        .has_complete_analysis = row.has_complete_analysis,
        .accuracy = row.accuracy,
        .miss_count = row.miss_count,
        .mistake_count = row.mistake_count,
        .blunder_count = row.blunder_count,
        .evidence_level = row.has_complete_analysis
            ? ai::ProfileEvidenceLevel::analyzed_summary
            : ai::ProfileEvidenceLevel::metadata_only,
    });
  }

  std::vector<ai::PlayerPattern> open_hypothesis_patterns;
  ai::ProfileRelevanceContext relevance_context;
  if (const auto payload = database_.ai_chess_profile_payload(profile_id)) {
    if (const auto learned = ai::chess_profile_from_json(*payload)) {
      for (const auto& hypothesis : learned->hypotheses) {
        if (hypothesis.status == "confirmed") continue;
        const auto pattern = std::find_if(
            learned->patterns.begin(), learned->patterns.end(),
            [&](const ai::PlayerPattern& value) {
              return value.id == hypothesis.pattern_id;
            });
        if (pattern == learned->patterns.end()) continue;
        open_hypothesis_patterns.push_back(*pattern);
        if (pattern->time_control != "all") {
          relevance_context.priority_time_controls.push_back(pattern->time_control);
        }
        if (pattern->type == "weakness") relevance_context.prioritize_losses = true;
        if (pattern->phase == "endgame") relevance_context.prioritize_endgame_length = true;
      }
    }
  }

  set_worker_activity(
      "profile_queue_sync", "active_learning_hints",
      "read_existing_knowledge_gaps_for_queue_priority_only", profile_id);
  // Knowledge gaps steer the existing nine-stage funnel only. The shared
  // KnowledgeRuntime owns graph/quality/conflict state; this service remains
  // the sole owner of engine scheduling, retries and resume state.
  try {
    const auto plan = knowledge_runtime_.active_learning_plan(
        profile_id, now_seconds * 1000);
    relevance_context.priority_game_ids.insert(
        relevance_context.priority_game_ids.end(),
        plan.priority_game_ids.begin(), plan.priority_game_ids.end());
    relevance_context.priority_openings.insert(
        relevance_context.priority_openings.end(),
        plan.priority_openings.begin(), plan.priority_openings.end());
    relevance_context.priority_time_controls.insert(
        relevance_context.priority_time_controls.end(),
        plan.priority_time_controls.begin(), plan.priority_time_controls.end());
    relevance_context.prioritize_losses =
        relevance_context.prioritize_losses || plan.prioritize_losses;
    relevance_context.prioritize_endgame_length =
        relevance_context.prioritize_endgame_length ||
        plan.prioritize_endgame_length;
  } catch (const std::exception& error) {
    diagnostics::warning(
        "player_profile",
        std::string("knowledge-gap refresh deferred: ") + error.what());
  } catch (...) {
    diagnostics::warning(
        "player_profile", "knowledge-gap refresh deferred");
  }

  set_worker_activity(
      "profile_queue_sync", "sampling_and_ranking",
      "rank_games_and_select_adaptive_historical_sample", profile_id, 0,
      metadata.size());
  const auto ranked = ai::rank_profile_games_by_relevance(
      metadata, now_seconds, relevance_context);

  // Stage 3 is the hard historical gate before any interesting-game / move
  // candidate work. Build the statistics-derived eligible population, then let
  // the adaptive hierarchical sampler choose its dynamic recommendation (500
  // remains only the hard ceiling). Only genuinely post-bootstrap games are
  // outside this initial-history ceiling so the learned profile can continue
  // adapting without treating recently imported archive history as new work.
  const auto sampling_population = ai::build_profile_sampling_population(
      metadata, now_seconds);
  const auto historical_sample = ai::select_profile_historical_sample(
      sampling_population, ranked);
  std::set<std::string> historical_sample_ids;
  for (const auto& entry : historical_sample.games) {
    historical_sample_ids.insert(entry.game_id);
  }

  // Stage 1 is a fast first impression *inside* the representative historical
  // bootstrap, not another route around Stage 3. This guarantees that initial
  // preparation can never exceed the Stage-3 hard cap merely because the first
  // twenty games were selected independently.
  std::vector<ai::ProfileGameMetadata> historical_sample_metadata;
  historical_sample_metadata.reserve(historical_sample.games.size());
  for (const auto& game : metadata) {
    if (historical_sample_ids.contains(game.game_id)) {
      historical_sample_metadata.push_back(game);
    }
  }
  const auto initial_sample = ai::build_initial_profile_sample(
      historical_sample_metadata, now_seconds, 20);
  std::set<std::string> initial_ids;
  for (const auto& entry : initial_sample) initial_ids.insert(entry.game_id);

  // Freeze the historical boundary from the newest game already known when the
  // bootstrap population first exists. Provider archive months discovered later
  // but played before this watermark stay historical and therefore cannot evade
  // the adaptive Stage-3 cap. Only games actually played after the watermark are
  // incremental learning outside the bootstrap sample.
  std::int64_t latest_known_played_at = 0;
  for (const auto& game : metadata) {
    latest_known_played_at = std::max(latest_known_played_at, game.played_at);
  }
  const std::int64_t bootstrap_cutoff =
      database_.ensure_ai_profile_sampling_bootstrap_cutoff(
          profile_id, latest_known_played_at);

  database_.update_ai_profile_sampling_state(
      profile_id,
      AiProfileSamplingState{
          .bootstrap_cutoff_played_at = bootstrap_cutoff,
          .total_games = static_cast<int>(historical_sample.requirement.total_games),
          .eligible_games = static_cast<int>(historical_sample.requirement.eligible_games),
          .excluded_games = static_cast<int>(historical_sample.requirement.excluded_games),
          .minimum_games = static_cast<int>(historical_sample.requirement.minimum_games),
          .recommended_games = static_cast<int>(historical_sample.requirement.recommended_games),
          .maximum_games = static_cast<int>(historical_sample.requirement.maximum_games),
          .selected_games = static_cast<int>(historical_sample.games.size()),
          .required_strata = static_cast<int>(historical_sample.requirement.required_strata),
          .covered_strata = static_cast<int>(historical_sample.covered_strata),
          .diversity_score = historical_sample.requirement.diversity_score,
          .covered_population_share = historical_sample.covered_population_share,
          .capped = historical_sample.requirement.capped,
          .updated_at = now_seconds,
      });

  const auto is_incremental_game = [&](const ai::ProfileGameMetadata& game) {
    return game.sample_eligible && bootstrap_cutoff > 0 &&
        game.played_at > bootstrap_cutoff;
  };

  std::vector<ai::ProfileGameMetadata> stage4_metadata;
  stage4_metadata.reserve(historical_sample.games.size() + 32);
  std::set<std::string> stage4_ids = historical_sample_ids;
  for (const auto& game : metadata) {
    if (is_incremental_game(game)) stage4_ids.insert(game.game_id);
  }
  for (const auto& game : metadata) {
    if (stage4_ids.contains(game.game_id)) stage4_metadata.push_back(game);
  }

  std::vector<ai::ProfileGameRelevance> stage4_ranked;
  stage4_ranked.reserve(stage4_metadata.size());
  for (const auto& item : ranked) {
    if (stage4_ids.contains(item.game_id)) stage4_ranked.push_back(item);
  }
  const auto interesting = ai::select_interesting_profile_games(
      stage4_metadata, stage4_ranked, relevance_context);
  std::unordered_map<std::string, ai::InterestingProfileGame> interesting_by_id;
  for (const auto& game : interesting) interesting_by_id.emplace(game.game_id, game);

  std::unordered_map<std::string, double> relevance_by_id;
  relevance_by_id.reserve(ranked.size());
  for (const auto& game : ranked) relevance_by_id[game.game_id] = game.score;

  set_worker_activity(
      "profile_queue_sync", "persist_queue",
      "update_existing_persistent_profile_queue_rows", profile_id, 0,
      metadata.size());
  std::size_t queue_index = 0;
  for (const auto& game : metadata) {
    ++queue_index;
    if (queue_index == metadata.size() || queue_index % 256 == 0) {
      set_worker_activity(
          "profile_queue_sync", "persist_queue",
          "update_existing_persistent_profile_queue_rows", profile_id,
          queue_index, metadata.size());
    }
    const auto source_it = source_by_id.find(game.game_id);
    if (source_it == source_by_id.end() || source_it->second == nullptr) continue;
    const auto& source = *source_it->second;
    const auto age_seconds = game.played_at > 0 && now_seconds > game.played_at
        ? now_seconds - game.played_at
        : 0;
    const bool new_game = is_incremental_game(game);
    const bool current_missing_evidence =
        game.played_at > 0 && age_seconds <= 180LL * 86400LL &&
        !source.analysis_complete && source.analyzed_moves == 0;

    bool hypothesis_match = false;
    const auto source_evidence = ai::ProfileEvidenceAdapter{}.adapt(
        observation_from_source(source));
    for (const auto& pattern : open_hypothesis_patterns) {
      const bool time_control_matches =
          pattern.time_control == "all" ||
          pattern.time_control == ai::profile_time_control_id(source_evidence.time_control);
      if (!time_control_matches) continue;
      if (pattern.phase == "all") {
        hypothesis_match = true;
        break;
      }
      for (const auto& phase : source_evidence.phases) {
        if (ai::profile_phase_id(phase.phase) == pattern.phase) {
          hypothesis_match = true;
          break;
        }
      }
      if (hypothesis_match) break;
    }

    const bool knowledge_gap_match = std::find(
        relevance_context.priority_game_ids.begin(),
        relevance_context.priority_game_ids.end(), game.game_id) !=
        relevance_context.priority_game_ids.end();
    const bool initial = initial_ids.contains(game.game_id);
    const bool historical_sampled = historical_sample_ids.contains(game.game_id);
    const bool stage4_eligible = stage4_ids.contains(game.game_id);
    const bool is_interesting = stage4_eligible &&
        interesting_by_id.contains(game.game_id);
    double priority = relevance_by_id.contains(game.game_id)
        ? relevance_by_id[game.game_id]
        : 0.0;
    std::string reason = "metadata_only";
    if (is_interesting) {
      reason = "interesting_game";
      priority += 0.35;
    }
    if (current_missing_evidence && is_interesting) {
      reason = "current_missing_evidence";
      priority += 0.60;
    }
    if (knowledge_gap_match && stage4_eligible) {
      reason = "knowledge_gap";
      priority += 1.25;
    }
    if (hypothesis_match && stage4_eligible) {
      reason = "open_hypothesis";
      priority += 1.50;
    }
    if (initial) {
      reason = "initial_sample";
      priority += 1.00;
    }
    if (new_game && (is_interesting || initial || hypothesis_match)) {
      reason = "new_game";
      priority += 2.00;
    }

    const int pipeline_stage = ai::profile_pipeline_stage_number(
        reason != "metadata_only"
            ? ai::ProfilePipelineStage::interesting_game
            : historical_sampled
                ? ai::ProfilePipelineStage::representative_sample
                : ai::ProfilePipelineStage::metadata_and_relevance);
    database_.upsert_ai_profile_queue(
        profile_id, game.game_id, priority, reason, pipeline_stage,
        historical_sampled, is_interesting, source.source_version);

    // The all-game metadata sweep itself is the complete work for ordinary
    // low-relevance history. Mark it `indexed`, not `done`, so persistence and
    // UI can distinguish cheap library coverage from relevant evidence work.
    if (reason == "metadata_only") {
      database_.set_ai_profile_queue_state(
          profile_id, game.game_id, "indexed", source.source_version);
    }
  }

  // The same authoritative projection is also the input of the derived model
  // refresh. Keep it only until that refresh consumes it; this is a one-shot
  // in-process hand-off, not a second persistent cache or source of truth.
  refresh_source_snapshot_profile_id_ = profile_id;
  refresh_source_snapshot_ = std::move(sources);
  refresh_source_snapshot_ready_ = true;
}

void PlayerProfileService::refresh_profile_if_requested(
    const std::string& profile_id,
    const std::int64_t now_seconds,
    const bool force) noexcept {
  if (!profile_refresh_requested_.load()) return;
  if (!force && last_profile_refresh_at_ > 0 &&
      now_seconds - last_profile_refresh_at_ < 5) {
    return;
  }
  if (!profile_refresh_requested_.exchange(false)) return;

  try {
    set_worker_activity(
        "profile_refresh", "starting",
        "rebuild_derived_profile_from_existing_authoritative_sources", profile_id);
    refresh_profile(profile_id, now_seconds);
    last_profile_refresh_at_ = unix_time_seconds();
    clear_worker_activity();
  } catch (const std::exception& error) {
    clear_worker_activity();
    // Derived profile/graph materialization is recoverable and must never
    // strand the authoritative persistent queue before its next item.
    profile_refresh_requested_ = true;
    diagnostics::warning(
        "player_profile",
        std::string("profile refresh deferred: ") + error.what());
  } catch (...) {
    clear_worker_activity();
    profile_refresh_requested_ = true;
    diagnostics::warning(
        "player_profile", "profile refresh deferred: unknown error");
  }
}

std::vector<PlayerProfileGameSourceRow> PlayerProfileService::load_profile_game_sources(
    const std::string& profile_id) {
  const auto started = std::chrono::steady_clock::now();
  auto rows = database_.player_profile_game_sources(profile_id);
  const auto elapsed = static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - started)
          .count());
  source_sweep_count_.fetch_add(1, std::memory_order_relaxed);
  source_sweep_rows_.fetch_add(rows.size(), std::memory_order_relaxed);
  source_sweep_total_ms_.fetch_add(elapsed, std::memory_order_relaxed);
  source_sweep_last_ms_.store(elapsed, std::memory_order_relaxed);
  return rows;
}

std::optional<PlayerProfileGameSourceRow> PlayerProfileService::load_profile_game_source(
    const std::string& profile_id, const std::string& game_id) {
  const auto started = std::chrono::steady_clock::now();
  auto row = database_.player_profile_game_source(profile_id, game_id);
  const auto elapsed = static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - started)
          .count());
  source_lookup_count_.fetch_add(1, std::memory_order_relaxed);
  if (row) source_lookup_hits_.fetch_add(1, std::memory_order_relaxed);
  source_lookup_total_ms_.fetch_add(elapsed, std::memory_order_relaxed);
  source_lookup_last_ms_.store(elapsed, std::memory_order_relaxed);
  return row;
}

void PlayerProfileService::refresh_profile(
    const std::string& profile_id, const std::int64_t now_seconds) {
  std::lock_guard refresh_lock(refresh_mutex_);
  const auto profile = database_.profile(profile_id);
  if (!profile) return;

  set_worker_activity(
      "profile_refresh", "load_sources",
      "load_or_reuse_profile_game_source_snapshot", profile_id);
  std::vector<PlayerProfileGameSourceRow> sources;
  if (refresh_source_snapshot_ready_ &&
      refresh_source_snapshot_profile_id_ == profile_id) {
    sources = std::move(refresh_source_snapshot_);
    refresh_source_snapshot_.clear();
    refresh_source_snapshot_profile_id_.clear();
    refresh_source_snapshot_ready_ = false;
    refresh_source_snapshot_hits_.fetch_add(1, std::memory_order_relaxed);
  } else {
    sources = load_profile_game_sources(profile_id);
  }

  set_worker_activity(
      "profile_refresh", "build_evidence_overview",
      "adapt_profile_game_sources_to_compact_evidence", profile_id, 0,
      sources.size());
  std::vector<ai::ProfileGameEvidence> overview;
  overview.reserve(sources.size());
  std::size_t overview_index = 0;
  for (const auto& source : sources) {
    auto observation = observation_from_source(source);
    overview.push_back(ai::ProfileEvidenceAdapter{}.adapt(observation));
    ++overview_index;
    if (overview_index == sources.size() || overview_index % 512 == 0) {
      set_worker_activity(
          "profile_refresh", "build_evidence_overview",
          "adapt_profile_game_sources_to_compact_evidence", profile_id,
          overview_index, sources.size());
    }
  }
  const auto sample = ai::InitialSampleBuilder{}.build(overview, now_seconds);
  std::set<std::string> model_ids(sample.game_ids.begin(), sample.game_ids.end());
  const auto processed = database_.ai_profile_processed_game_ids(profile_id);
  model_ids.insert(processed.begin(), processed.end());

  // Detailed move examples are bounded even when thousands of historical games
  // have already contributed aggregate evidence.
  std::set<std::string> detail_ids(sample.game_ids.begin(), sample.game_ids.end());
  for (std::size_t i = 0; i < processed.size() && i < 80; ++i) {
    detail_ids.insert(processed[i]);
  }

  std::vector<ai::ProfileGameEvidence> model_games;
  model_games.reserve(model_ids.size());
  for (const auto& source : sources) {
    if (!model_ids.contains(source.game_id)) continue;
    std::vector<PlayerProfileMoveSourceRow> moves;
    if (detail_ids.contains(source.game_id) && source.analysis_complete) {
      moves = database_.player_profile_move_sources(source.game_id, source.player_color);
    }
    auto observation = observation_from_source(source, moves);
    model_games.push_back(ai::ProfileEvidenceAdapter{}.adapt(observation));
  }

  const auto queue_progress = database_.ai_profile_queue_progress(profile_id);
  const auto sampling_state = database_.ai_profile_sampling_state(profile_id);
  const auto history_progress =
      provider_service_.player_profile_history_progress(profile_id);
  ai::ProfileBackgroundProgress background;
  background.total_games = queue_progress.total;
  background.history_accounts = history_progress.account_count;
  background.history_discovered_accounts = history_progress.discovered_account_count;
  background.history_available_months = history_progress.available_month_count;
  background.history_synced_months = history_progress.synced_month_count;
  background.history_pending_months = history_progress.pending_month_count;
  background.history_complete = history_progress.complete;
  // Queue synchronization is the metadata sweep: every current queue row was
  // indexed even when the game was promoted to a later evidence stage.
  background.indexed_games = queue_progress.total;
  background.historical_sample_games = queue_progress.historical_sample;
  background.historical_sample_budget = sampling_state
      ? sampling_state->recommended_games
      : queue_progress.historical_sample;
  if (sampling_state) {
    background.sampling_eligible_games = sampling_state->eligible_games;
    background.sampling_excluded_games = sampling_state->excluded_games;
    background.sampling_minimum_games = sampling_state->minimum_games;
    background.sampling_maximum_games = sampling_state->maximum_games;
    background.sampling_required_strata = sampling_state->required_strata;
    background.sampling_covered_strata = sampling_state->covered_strata;
    background.sampling_coverage = sampling_state->covered_population_share;
    background.sampling_diversity = sampling_state->diversity_score;
    background.sampling_capped = sampling_state->capped;
  }
  background.interesting_games = queue_progress.interesting;
  background.engine_promoted_games = queue_progress.engine_promoted_games;
  background.relevant_games = queue_progress.relevant;
  background.resolved_relevant_games = queue_progress.resolved_relevant;
  background.reused_analysis_games = static_cast<int>(std::count_if(
      sources.begin(), sources.end(),
      [](const PlayerProfileGameSourceRow& source) { return source.analysis_complete; }));
  background.queued_games = queue_progress.queued;
  background.engine_pending_games = queue_progress.engine_pending;
  background.current_game_id = current_game();
  background.updated_at = now_seconds;
  if (!history_progress.complete) background.status = "syncing_history";
  else if (queue_progress.total == 0) background.status = "empty";
  else if (background.current_game_id || queue_progress.processing > 0) background.status = "analyzing";
  else if (queue_progress.resolved_relevant >= queue_progress.relevant) background.status = "complete";
  else background.status = "queued";
  background.initial_preparation_complete =
      history_progress.complete && queue_progress.total > 0 &&
      !background.current_game_id.has_value() && queue_progress.processing == 0 &&
      queue_progress.resolved_relevant >= queue_progress.relevant;

  std::optional<ai::ChessProfile> previous;
  if (const auto payload = database_.ai_chess_profile_payload(profile_id)) {
    previous = ai::chess_profile_from_json(*payload);
  }
  auto learned = ai::ChessProfileUpdater{}.update_from_evidence(
      profile_id, profile_rating(*profile, model_games), model_games,
      now_seconds, background, previous, overview);
  set_worker_activity(
      "profile_refresh", "persist_learned_profile",
      "persist_updated_chess_profile_payload", profile_id);
  database_.set_ai_chess_profile_payload(profile_id, ai::chess_profile_json(learned));

  // Keep a payload-free source registry beside the learned model. The graph is
  // rebuilt from those stable locators plus the compact learned model; it never
  // copies PGNs, engine lines or analysis payloads.
  set_worker_activity(
      "profile_refresh", "sync_evidence_registry",
      "sync_payload_free_profile_evidence_locators", profile_id);
  database_.sync_ai_profile_evidence_registry(profile_id);
  // Update 123: the general KnowledgeRuntime replaces the legacy profile-only
  // graph as the active retrieval layer. It projects the learned profile plus
  // existing statistics/games/analysis sources, refreshes quality/conflicts,
  // materializes semantic chunks, and updates active-learning gaps.
  // Derived knowledge is background-only work. Never begin the expensive graph
  // projection while foreground analysis is active or already waiting for the
  // shared SQLite writer. Requeue the refresh instead; the worker will resume it
  // after foreground work releases priority.
  const auto write_priority = persistence::sqlite_write_priority_gate().snapshot();
  if (write_priority.foreground_sessions != 0 ||
      write_priority.foreground_waiters != 0) {
    profile_refresh_requested_ = true;
    set_worker_activity(
        "profile_refresh", "knowledge_refresh_deferred",
        "foreground_analysis_has_priority_over_derived_knowledge", profile_id);
    return;
  }
  set_worker_activity(
      "profile_refresh", "knowledge_refresh",
      "project_changed_profile_sources_into_shared_knowledge_runtime", profile_id);
  (void)knowledge_runtime_.refresh_active_profile(
      now_seconds * 1000, graph_source_signature(sources));
}

bool PlayerProfileService::run_engine_request(
    const std::string& profile_id,
    const AiProfileQueueRecord& queue,
    const PlayerProfileGameSourceRow& source,
    const std::int64_t now_seconds) {
  database_.set_ai_profile_queue_state(
      profile_id, source.game_id, "processing", -1, false,
      ai::profile_pipeline_stage_number(ai::ProfilePipelineStage::interesting_game));
  set_current_game(source.game_id);

  const auto finish_done = [&](const std::int64_t version) {
    database_.set_ai_profile_queue_state(
        profile_id, source.game_id, "done", version);
  };
  const auto requeue = [&]() {
    // A failed orchestration attempt is never evidence resolution. Keep the
    // row resumable and let next_ai_profile_queue apply bounded retry backoff;
    // marking repeated failures `done` would permanently hide missing relevant
    // evidence behind a false-complete profile.
    database_.set_ai_profile_queue_state(
        profile_id, source.game_id, "queued", -1, true);
  };

  try {
    auto observation = observation_from_source(source);
    const auto evidence = ai::ProfileEvidenceAdapter{}.adapt(observation);
    const double relevance = effective_relevance(
        ai::GameRelevanceScorer{}.score(evidence, now_seconds).score, queue.reason);
    const int age_bucket = ai::profile_age_bucket(source.played_at, now_seconds);
    const bool error_signal = source.miss + source.mistake + source.blunder > 0 ||
        (source.maximum_expected_score_loss.has_value() &&
         *source.maximum_expected_score_loss >= 0.08);
    const bool accuracy_outlier = source.accuracy.has_value() &&
        (*source.accuracy <= 68.0 || *source.accuracy >= 94.0);
    const ai::InterestingProfileGame game{
        .game_id = source.game_id,
        .relevance = std::clamp(relevance, 0.0, 1.0),
        .age_bucket = age_bucket,
        .recent = age_bucket <= 1,
        .error_signal = error_signal,
        .accuracy_outlier = accuracy_outlier,
        .existing_analysis = source.analysis_complete,
        .profile_promoted = queue.reason == "open_hypothesis" ||
            queue.reason == "knowledge_gap" ||
            queue.reason == "initial_sample" || queue.reason == "new_game",
    };

    std::vector<ai::ProfilePositionSignal> signals;
    const auto persisted_moves = database_.player_profile_move_sources(
        source.game_id, source.player_color);
    signals.reserve(persisted_moves.size() + static_cast<std::size_t>(source.move_count));
    std::set<int> persisted_ply;
    for (const auto& row : persisted_moves) {
      persisted_ply.insert(row.ply);
      signals.push_back(ai::ProfilePositionSignal{
          .ply = row.ply,
          .classification = row.classification,
          .expected_score_loss = row.expected_score_loss,
          .theory = row.theory,
          .fen_before = row.fen_before,
          .uci = row.uci,
      });
    }

    // When a game has no local move analysis yet, feed the raw player moves to
    // Stage 5 only as sparse-scout candidates. No chess interpretation happens
    // here and there is never a whole-game fallback.
    if (const auto stored_game = database_.game(source.game_id)) {
      for (const auto& move : stored_game->moves) {
        if (source.player_color != "unknown" && move.side_to_move != source.player_color) continue;
        if (persisted_ply.contains(move.ply_index)) continue;
        signals.push_back(ai::ProfilePositionSignal{
            .ply = move.ply_index,
            .classification = {},
            .expected_score_loss = std::nullopt,
            .theory = false,
            .fen_before = move.fen_before,
            .uci = move.uci,
        });
      }
    }
    std::stable_sort(
        signals.begin(), signals.end(),
        [](const ai::ProfilePositionSignal& left, const ai::ProfilePositionSignal& right) {
          return left.ply < right.ply;
        });

    const auto candidates = ai::select_profile_position_candidates(game, signals);
    database_.set_ai_profile_queue_state(
        profile_id, source.game_id, "processing", -1, false,
        ai::profile_pipeline_stage_number(ai::ProfilePipelineStage::position_candidates));
    const auto evidence_decisions = ai::filter_existing_profile_evidence(candidates);
    database_.set_ai_profile_queue_state(
        profile_id, source.game_id, "processing", -1, false,
        ai::profile_pipeline_stage_number(ai::ProfilePipelineStage::existing_evidence));
    const auto fast_requests = ai::plan_fast_profile_probes(
        evidence_decisions, game.relevance);

    // If existing KChess evidence already settles every candidate, the game is
    // complete for profile purposes without starting Stockfish.
    if (fast_requests.empty()) {
      finish_done(source.source_version);
      set_current_game(std::nullopt);
      profile_refresh_requested_ = true;
      return true;
    }

    // Profile maintenance is a client of the same persisted game-analysis
    // cache as the Analysis UI.  The staged funnel still decides whether this
    // game deserves engine work; once promoted, it requests the cheapest full
    // game quality that can satisfy the current fast stage.  If the user has
    // already saved an equal-or-better analysis, AnalysisService reuses it.
    // Otherwise the background worker upgrades the authoritative
    // analysis_runs/move_analysis cache.  No second profile-owned engine result
    // is written here.
    int requested_depth = AppSettings::min_depth;
    int requested_multi_pv = 1;
    int requested_threads = 1;
    int requested_hash_mb = 1024;
    for (const auto& request : fast_requests) {
      requested_depth = std::max(requested_depth, request.budget.hard_depth);
      requested_multi_pv = std::max(requested_multi_pv, request.budget.multi_pv);
      requested_threads = std::max(requested_threads, request.budget.threads);
      requested_hash_mb = std::max(requested_hash_mb, request.budget.hash_mb);
    }

    database_.set_ai_profile_queue_state(
        profile_id, source.game_id, "processing", -1, false,
        ai::profile_pipeline_stage_number(ai::ProfilePipelineStage::fast_probe));
    const auto request_result = analysis_service_.ensure_shared_profile_analysis(
        source.game_id, requested_depth, requested_multi_pv,
        requested_threads, requested_hash_mb);
    const bool cache_ready = request_result ==
        AnalysisService::SharedProfileAnalysisRequestResult::cache_ready;
    const bool profile_job_started = request_result ==
        AnalysisService::SharedProfileAnalysisRequestResult::started;
    if (cache_ready) {
      // `cache_ready` is terminal for this queue generation: the requested
      // equal-or-better authoritative full analysis already exists and
      // AnalysisService has rebuilt its classifications. Re-queueing here
      // selects the same high-priority game forever without changing any
      // evidence, which is the exact 0/N profile-preparation stall.
      finish_done(source.source_version);
      set_current_game(std::nullopt);
      profile_refresh_requested_ = true;
      return true;
    }

    database_.set_ai_profile_queue_state(
        profile_id, source.game_id, "engine_pending", -1, false,
        ai::profile_pipeline_stage_number(ai::ProfilePipelineStage::fast_probe));
    if (!profile_job_started) {
      // `deferred` means foreground work won the engine slot. Do not expose a
      // phantom currentGameId for a profile job that does not exist.
      set_current_game(std::nullopt);
    }
    // When AnalysisService actually started shared background work, keep
    // currentGameId until its terminal observer wakes this worker. On the next
    // pass the completed shared run is re-read and this queue generation is
    // resolved without duplicate Stockfish work.
    return true;
  } catch (const std::exception& error) {
    diagnostics::warning(
        "player_profile", "staged profile analysis failed game=" + source.game_id +
            " error=" + error.what());
    requeue();
  } catch (...) {
    diagnostics::warning(
        "player_profile", "staged profile analysis failed game=" + source.game_id);
    requeue();
  }

  set_current_game(std::nullopt);
  return true;
}

}  // namespace kchess
