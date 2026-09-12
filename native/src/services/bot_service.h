#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <string>
#include <thread>
#include <unordered_map>

#include "engine/bot_move_selector.h"
#include "engine/chess_engine.h"
#include "persistence/database.h"

namespace kchess {

// -----------------------------------------------------------------------------
// Section: Local Stockfish 18 bot jobs
// -----------------------------------------------------------------------------

class BotService {
 public:
  explicit BotService(Database& database);
  ~BotService();

  BotService(const BotService&) = delete;
  BotService& operator=(const BotService&) = delete;

  std::string create_game_json(int requested_elo);
  std::string active_game_json() const;
  std::string game_json(const std::string& game_id) const;
  std::string games_json() const;
  std::string analysis_pgn(const std::string& game_id) const;
  std::string record_game_move_json(
      const std::string& game_id,
      const std::string& expected_fen_before,
      const std::string& uci);
  std::string record_game_move_from_ply_json(
      const std::string& game_id,
      int base_ply,
      const std::string& expected_fen_before,
      const std::string& uci);
  void resign_game(const std::string& game_id);
  void abort_game(const std::string& game_id);
  void delete_game(const std::string& game_id);
  void set_game_show_eval_bar(const std::string& game_id, bool enabled);

  std::string start_move_json(const std::string& fen, int requested_elo);
  std::string move_status_json(const std::string& job_id) const;
  void cancel_move(const std::string& job_id);

 private:
  enum class JobState {
    queued,
    running,
    cancelling,
    cancelled,
    completed,
    failed,
  };

  struct EngineLease {
    std::shared_ptr<ChessEngine> engine;
    std::uint64_t session_id{0};
    bool warm{false};
  };

  struct BotJob {
    std::string id;
    std::string fen;
    int requested_elo{kBotEloMinimum};
    BotDifficultyProfile profile;
    BotMovePlan plan;
    std::atomic_bool cancel_requested{false};
    std::atomic_bool finished{false};
    std::atomic<JobState> state{JobState::queued};
    mutable std::mutex result_mutex;
    std::string error;
    AnalysisResult analysis;
    BotMoveChoice choice;
    std::string selected_san;
    std::string selected_fen_after;
    std::string selected_position_after_json;
    std::optional<int> selected_evaluation_cp;
    std::optional<int> selected_mate_in;
    std::optional<WdlScore> selected_wdl;
    bool terminal{false};
    bool checkmate{false};
    std::string result{"*"};
    std::string engine_version;
    std::uint64_t engine_session_id{0};
    bool reused_warm_engine{false};
    std::thread worker;
  };

  static const char* state_name(JobState state) noexcept;
  std::string session_json(const BotGameRecord& game) const;
  std::string job_json(const BotJob& job) const;
  void run_job(const std::shared_ptr<BotJob>& job) noexcept;
  EngineLease acquire_engine();
  void reap_finished_jobs();
  void stop_all_jobs() noexcept;
  double next_random_unit();

  Database& database_;

  mutable std::mutex jobs_mutex_;
  std::unordered_map<std::string, std::shared_ptr<BotJob>> jobs_;
  std::atomic_uint64_t next_job_id_{1};

  // Dedicated engine: bot play is permanently Stockfish 18 and does not read
  // the global analysis-engine setting. Reusing it preserves TT/NNUE state
  // between moves without creating one engine instance per Elo level.
  mutable std::mutex engine_mutex_;
  std::shared_ptr<ChessEngine> engine_;
  std::uint64_t engine_session_id_{0};
  std::uint64_t next_engine_session_id_{1};
  std::uint64_t engine_jobs_started_{0};

  std::mutex random_mutex_;
  std::mt19937_64 random_;
};

}  // namespace kchess
