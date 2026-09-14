#pragma once

// -----------------------------------------------------------------------------
// Section: Background game analysis
// -----------------------------------------------------------------------------

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <set>
#include <string>
#include <thread>

#include "persistence/database.h"
#include "services/analysis_service.h"
#include "services/bot_service.h"

namespace kchess {

// Analyses the active profile's games one at a time while the app is open,
// newest first, so statistics can use the engine's verdicts for the whole
// library instead of only the games the user happened to open.
//
// It never competes with the user: while they analyse a game, browse
// refinements, explore a sideline or play a bot, the running game is stopped
// (its finished positions stay saved) and nothing new starts until they have
// been idle for a short while. It also back-fills per-move accuracy for games
// analysed before that was stored, which needs no engine at all.
class BackgroundAnalysis {
 public:
  BackgroundAnalysis(Database& database, AnalysisService& analysis, BotService& bot);
  ~BackgroundAnalysis();

  BackgroundAnalysis(const BackgroundAnalysis&) = delete;
  BackgroundAnalysis& operator=(const BackgroundAnalysis&) = delete;

  // Starts the worker loop once; the saved on/off setting decides whether it
  // does anything. Safe to call repeatedly.
  void start();
  // Stops the loop and the game it is analysing. Called before shutdown.
  void stop() noexcept;

  // Persists the user's choice; turning it off stops the current game.
  void set_enabled(bool enabled);
  bool enabled() const;

  // {enabled, state, analysedGames, totalGames}. state is one of disabled,
  // noProfile, running, paused, waiting, complete, unavailable.
  std::string status_json() const;

  // How long the user must leave analysis or a bot game alone before
  // background work resumes.
  static constexpr int kAnalysisCooldownSeconds = 20;
  static constexpr int kBotCooldownSeconds = 45;

 private:
  void loop() noexcept;
  void tick();
  void set_state(const char* state);

  Database& database_;
  AnalysisService& analysis_;
  BotService& bot_;

  std::thread worker_;
  std::atomic_bool stopping_{false};
  mutable std::mutex mutex_;
  std::condition_variable wake_;
  const char* state_{"waiting"};

  // This session only: games whose analysis failed, and games already
  // re-classified for per-move accuracy, so neither is retried in a loop.
  std::set<std::string> skipped_;
  std::set<std::string> refreshed_;
  std::string current_game_;
  bool paused_by_us_{false};
  int consecutive_start_failures_{0};
  std::int64_t retry_after_{0};
};

}  // namespace kchess
