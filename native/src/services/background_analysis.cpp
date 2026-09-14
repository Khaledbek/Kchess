// -----------------------------------------------------------------------------
// Section: Background game analysis
// -----------------------------------------------------------------------------

#include "services/background_analysis.h"

#include <algorithm>
#include <chrono>
#include <nlohmann/json.hpp>

#include "diagnostics/logger.h"

namespace kchess {
namespace {

constexpr const char* kEnabledSetting = "backgroundAnalysisEnabled";
constexpr auto kTick = std::chrono::seconds(2);
// A game the engine cannot start (no engine binary, for instance) is retried
// after this long rather than on every tick.
constexpr int kUnavailableBackoffSeconds = 60;
constexpr int kMaxConsecutiveStartFailures = 3;

std::int64_t now_seconds() {
  return std::chrono::duration_cast<std::chrono::seconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

}  // namespace

BackgroundAnalysis::BackgroundAnalysis(
    Database& database, AnalysisService& analysis, BotService& bot)
    : database_(database), analysis_(analysis), bot_(bot) {}

BackgroundAnalysis::~BackgroundAnalysis() { stop(); }

void BackgroundAnalysis::start() {
  std::lock_guard lock(mutex_);
  if (worker_.joinable()) return;
  stopping_ = false;
  worker_ = std::thread([this] { loop(); });
}

void BackgroundAnalysis::stop() noexcept {
  {
    std::lock_guard lock(mutex_);
    if (!worker_.joinable()) return;
    stopping_ = true;
  }
  wake_.notify_all();
  analysis_.cancel_background_analysis();
  worker_.join();
}

void BackgroundAnalysis::set_enabled(const bool enabled) {
  database_.set_setting(kEnabledSetting, enabled ? "true" : "false");
  if (!enabled) analysis_.cancel_background_analysis();
  wake_.notify_all();
}

bool BackgroundAnalysis::enabled() const {
  // On unless the user turned it off.
  return database_.app_setting(kEnabledSetting).value_or("true") == "true";
}

std::string BackgroundAnalysis::status_json() const {
  const auto profile = database_.active_profile();
  BackgroundAnalysisProgress progress;
  if (profile.has_value()) progress = database_.background_analysis_progress(profile->id);
  std::string state;
  {
    std::lock_guard lock(mutex_);
    state = state_;
  }
  if (!enabled()) state = "disabled";
  return nlohmann::json{{"enabled", enabled()},
                        {"state", state},
                        {"analysedGames", progress.analysed},
                        {"totalGames", progress.total}}
      .dump();
}

void BackgroundAnalysis::set_state(const char* state) {
  std::lock_guard lock(mutex_);
  state_ = state;
}

void BackgroundAnalysis::loop() noexcept {
  diagnostics::info("background", "Background analysis started");
  while (!stopping_) {
    try {
      tick();
    } catch (const std::exception& error) {
      diagnostics::error("background", std::string("tick failed: ") + error.what());
    } catch (...) {
      diagnostics::error("background", "tick failed");
    }
    std::unique_lock lock(mutex_);
    wake_.wait_for(lock, kTick, [this] { return stopping_.load(); });
  }
  diagnostics::info("background", "Background analysis stopped");
}

void BackgroundAnalysis::tick() {
  if (!enabled()) {
    analysis_.cancel_background_analysis();
    if (!current_game_.empty()) paused_by_us_ = true;
    set_state("disabled");
    return;
  }
  const auto profile = database_.active_profile();
  if (!profile.has_value()) {
    set_state("noProfile");
    return;
  }

  // The user's own work always wins.
  const auto now = now_seconds();
  const bool user_busy = analysis_.foreground_analysis_running() ||
      now - analysis_.last_foreground_activity() < kAnalysisCooldownSeconds ||
      now - bot_.last_activity() < kBotCooldownSeconds;
  if (user_busy) {
    analysis_.cancel_background_analysis();
    // Whoever stopped the game (this loop, or the analysis service when the
    // user started their own work), it was a pause, not a failure.
    if (!current_game_.empty()) paused_by_us_ = true;
    set_state("paused");
    return;
  }

  if (analysis_.background_analysis_running()) {
    set_state("running");
    return;
  }

  // The last game ended. If it did not finish and nobody stopped it, it
  // failed: leave it alone for the rest of the session.
  if (!current_game_.empty()) {
    const auto awaiting = database_.games_awaiting_analysis(profile->id, 1000000);
    const bool still_awaiting =
        std::find(awaiting.begin(), awaiting.end(), current_game_) != awaiting.end();
    if (still_awaiting && !paused_by_us_) {
      diagnostics::info("background", "game=" + current_game_ + " skipped after failure");
      skipped_.insert(current_game_);
    }
    current_game_.clear();
    paused_by_us_ = false;
  }

  // Cheap first: games analysed before per-move accuracy was stored.
  for (const auto& game_id : database_.games_missing_move_accuracy(profile->id, 8)) {
    if (!refreshed_.insert(game_id).second) continue;
    try {
      analysis_.refresh_classification(game_id);
    } catch (const std::exception& error) {
      diagnostics::error("background", "game=" + game_id + " refresh failed: " + error.what());
    }
    if (stopping_) return;
  }

  if (now < retry_after_) {
    set_state("unavailable");
    return;
  }

  std::string next;
  for (const auto& game_id : database_.games_awaiting_analysis(profile->id, 64)) {
    if (!skipped_.contains(game_id)) {
      next = game_id;
      break;
    }
  }
  if (next.empty()) {
    set_state("complete");
    return;
  }

  try {
    analysis_.start_background_analysis(next);
    current_game_ = next;
    consecutive_start_failures_ = 0;
    set_state("running");
  } catch (const std::exception& error) {
    diagnostics::error("background", "game=" + next + " could not start: " + error.what());
    skipped_.insert(next);
    // Several games in a row failing to start is the engine, not the games.
    if (++consecutive_start_failures_ >= kMaxConsecutiveStartFailures) {
      skipped_.clear();
      consecutive_start_failures_ = 0;
      retry_after_ = now + kUnavailableBackoffSeconds;
      set_state("unavailable");
    }
  }
}

}  // namespace kchess
