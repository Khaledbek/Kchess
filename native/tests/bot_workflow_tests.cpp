#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

#include <nlohmann/json.hpp>

#include "kchess/core_api.h"

namespace {

// -----------------------------------------------------------------------------
// Section: Persisted Stockfish 18 bot game integration
// -----------------------------------------------------------------------------

int assertions = 0;

void expect(const bool condition, const char* message) {
  ++assertions;
  if (!condition) throw std::runtime_error(message);
}

nlohmann::json take_json(char* value, const kc_core_handle core) {
  if (value == nullptr) throw std::runtime_error(kc_core_last_error(core));
  const std::string text(value);
  kc_string_free(value);
  return nlohmann::json::parse(text);
}

nlohmann::json wait_for_job(
    const kc_core_handle core, nlohmann::json job) {
  const auto job_id = job.at("jobId").get<std::string>();
  for (int attempt = 0;
       (job.at("status") == "queued" || job.at("status") == "running")
           && attempt < 1000;
       ++attempt) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    job = take_json(kc_bot_move_status_json(core, job_id.c_str()), core);
  }
  return job;
}

kc_core_handle open_core(const std::string& path) {
  auto* core = kc_core_create(path.c_str());
  if (core == nullptr) throw std::runtime_error("core handle creation failed");
  if (kc_core_initialize(core) != KC_STATUS_OK) {
    const std::string error = kc_core_last_error(core);
    kc_core_destroy(core);
    throw std::runtime_error("core initialization failed: " + error);
  }
  return core;
}

}  // namespace

int main() {
  const auto directory = std::filesystem::temp_directory_path()
      / ("kchess_bot_workflow_" + std::to_string(
          std::chrono::steady_clock::now().time_since_epoch().count()));
  kc_core_handle core = nullptr;
  try {
    std::filesystem::create_directories(directory);
    const auto encoded = directory.u8string();
    const std::string path(encoded.begin(), encoded.end());
    core = open_core(path);
    expect(core != nullptr, "core initialized");
    expect(kc_abi_version() == 7, "promotion choice update uses ABI version 7");

    constexpr auto start_fen =
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    const auto game = take_json(kc_create_bot_game_json(core, 1500), core);
    const auto game_id = game.at("gameId").get<std::string>();
    expect(game.at("playerColor") == "white" && game.at("botColor") == "black",
           "persisted bot game starts with the player as White");
    expect(game.at("botElo") == 1500 && game.at("status") == "active",
           "requested Elo and active status are persisted");
    expect(game.at("showEvaluationBar") == false,
           "bot eval bar is off by default and belongs to the game session");
    const auto global_settings_before = take_json(kc_app_settings_json(core), core);
    const auto global_eval_before = global_settings_before.at("showEvaluationBar");
    expect(kc_set_bot_game_show_eval_bar(core, game_id.c_str(), 1) == KC_STATUS_OK,
           "bot game eval bar can be enabled without touching global settings");
    const auto global_settings_after = take_json(kc_app_settings_json(core), core);
    expect(global_settings_after.at("showEvaluationBar") == global_eval_before,
           "bot-game eval preference is isolated from global analysis settings");
    const auto game_with_eval = take_json(kc_bot_game_json(core, game_id.c_str()), core);
    expect(game_with_eval.at("showEvaluationBar") == true,
           "bot game eval bar preference is persisted on the session");
    expect(game.at("position").at("fen") == start_fen
               && game.at("positions").size() == 1,
           "new persisted bot game starts from native chess state");

    const auto active = take_json(kc_active_bot_game_json(core), core);
    expect(active.at("gameId") == game_id,
           "active bot game can be queried for resume");

    char* duplicate = kc_create_bot_game_json(core, 1600);
    expect(duplicate == nullptr,
           "a second unfinished bot game cannot hide the resumable game");

    const auto human = take_json(
        kc_record_bot_game_move_json(core, game_id.c_str(), start_fen, "e2e4"),
        core);
    expect(human.at("uci") == "e2e4" && human.at("san") == "e4",
           "human move is validated and persisted natively");
    const auto black_fen = human.at("fenAfter").get<std::string>();
    expect(human.at("positionAfter").at("sideToMove") == "black",
           "persisted human move returns the next board position");

    const auto before_restart = take_json(kc_bot_game_json(core, game_id.c_str()), core);
    expect(before_restart.at("moves").size() == 1
               && before_restart.at("positions").size() == 2,
           "persisted session contains move history and every board position");

    kc_core_destroy(core);
    core = nullptr;
    core = open_core(path);
    const auto resumed = take_json(kc_active_bot_game_json(core), core);
    expect(resumed.at("gameId") == game_id
               && resumed.at("position").at("fen") == black_fen,
           "unfinished bot game survives a complete core restart");
    expect(resumed.at("moves").at(0).at("san") == "e4",
           "resumed game preserves SAN move history");
    expect(resumed.at("showEvaluationBar") == true,
           "bot-only eval bar preference survives a complete core restart");

    auto job = wait_for_job(
        core, take_json(kc_start_bot_move_json(core, black_fen.c_str(), 3200), core));
    expect(job.at("status") == "complete", "bot move job completed after resume");
    expect(job.at("engineId") == "stockfish18",
           "resumed bot game remains hard-locked to Stockfish 18");
    expect(job.at("move") == job.at("bestMove") && job.at("selectedRank") == 1,
           "3200 Elo always returns the final Stockfish 18 best move");
    expect(job.at("engineVersion").get<std::string>().rfind("Stockfish 18", 0) == 0,
           "completed resumed job identifies Stockfish 18 runtime");
    const auto warm_engine_session = job.at("engineSessionId").get<std::uint64_t>();
    expect(warm_engine_session > 0 && job.at("reusedWarmEngine") == false,
           "first move after core startup creates exactly one cold bot engine session");
    expect(job.at("evaluationFen") == black_fen,
           "bot evaluation is bound to the exact FEN analysed by Stockfish 18");
    expect(!job.at("evaluationCp").is_null() || !job.at("mateIn").is_null(),
           "completed Stockfish 18 job exposes a score for the bot-game eval bar");

    const auto bot_move = job.at("move").get<std::string>();
    const auto recorded_bot = take_json(
        kc_record_bot_game_move_json(
            core, game_id.c_str(), black_fen.c_str(), bot_move.c_str()),
        core);
    expect(recorded_bot.at("uci") == bot_move,
           "selected Stockfish move is persisted against the exact expected FEN");

    char* stale = kc_record_bot_game_move_json(
        core, game_id.c_str(), black_fen.c_str(), "g8f6");
    expect(stale == nullptr,
           "stale position cannot append a move to a resumed bot game");

    const auto branch = take_json(
        kc_record_bot_game_move_from_ply_json(
            core, game_id.c_str(), 1, black_fen.c_str(), "a7a6"),
        core);
    expect(branch.at("uci") == "a7a6" && branch.at("mainLinePly") == 2,
           "a move from an earlier bot-game ply replaces the old continuation");
    const auto branched_game = take_json(kc_bot_game_json(core, game_id.c_str()), core);
    expect(branched_game.at("moves").size() == 2
               && branched_game.at("moves").at(0).at("uci") == "e2e4"
               && branched_game.at("moves").at(1).at("uci") == "a7a6",
           "branching preserves the prefix and truncates the previous future line");
    expect(branched_game.at("position").at("fen") == branch.at("fenAfter"),
           "branched continuation becomes the persisted live position");

    expect(kc_resign_bot_game(core, game_id.c_str()) == KC_STATUS_OK,
           "player can resign the active bot game");
    const auto resigned = take_json(kc_bot_game_json(core, game_id.c_str()), core);
    expect(resigned.at("status") == "resigned" && resigned.at("result") == "0-1",
           "resignation is retained as a loss for the later bot history");
    const auto history_after_resign = take_json(kc_bot_games_json(core), core);
    expect(history_after_resign.size() == 1
               && history_after_resign.at(0).at("gameId") == game_id,
           "bot history lists the retained game newest first");
    expect(history_after_resign.at(0).at("outcome") == "loss"
               && history_after_resign.at(0).at("moveCount") == 2,
           "bot history exposes player-relative outcome and lightweight move count");

    const auto analysis_game = take_json(
        kc_bot_game_analysis_game_json(core, game_id.c_str()), core);
    const auto analysis_game_id = analysis_game.at("id").get<std::string>();
    expect(analysis_game.at("kind") == "pgn"
               && analysis_game.at("result") == "0-1"
               && analysis_game.at("moves").size() == 2,
           "finished bot game materializes as a normal local PGN analysis game");
    const auto analysis_game_again = take_json(
        kc_bot_game_analysis_game_json(core, game_id.c_str()), core);
    expect(analysis_game_again.at("id") == analysis_game_id,
           "reopening bot analysis reuses the same materialized game instead of duplicating it");
    const auto history_linked = take_json(kc_bot_games_json(core), core);
    expect(history_linked.at(0).at("analysisGameId") == analysis_game_id,
           "bot history persists the analysis-game link");

    const auto no_active = take_json(kc_active_bot_game_json(core), core);
    expect(no_active.is_null(), "resigned game is no longer resumable");

    const auto disposable = take_json(kc_create_bot_game_json(core, 100), core);
    const auto disposable_id = disposable.at("gameId").get<std::string>();
    expect(disposable.at("botElo") == 100,
           "new game can be created after the previous game is finished");
    const auto history_with_active = take_json(kc_bot_games_json(core), core);
    expect(history_with_active.size() == 2
               && history_with_active.at(0).at("status") == "active",
           "active bot game is visible in history and sorted above older games");
    expect(kc_abort_bot_game(core, disposable_id.c_str()) == KC_STATUS_OK,
           "active bot game can be aborted");
    const auto history_after_abort = take_json(kc_bot_games_json(core), core);
    expect(history_after_abort.size() == 1,
           "aborted bot game is removed from history rather than archived");
    char* deleted = kc_bot_game_json(core, disposable_id.c_str());
    expect(deleted == nullptr,
           "aborted bot game is deleted instead of becoming history");
    expect(take_json(kc_active_bot_game_json(core), core).is_null(),
           "aborted bot game is not resumable");

    expect(kc_delete_bot_game(core, game_id.c_str()) == KC_STATUS_OK,
           "completed bot games can be deleted explicitly from the history log");
    expect(take_json(kc_bot_games_json(core), core).empty(),
           "deleting a history entry removes only the bot-log record");
    const auto retained_analysis = take_json(kc_game_json(core, analysis_game_id.c_str()), core);
    expect(retained_analysis.at("id") == analysis_game_id,
           "deleting bot history keeps an already materialized analysis PGN");

    // Analysis-board bot continuations are intentionally ephemeral. They use
    // the free-board resolver plus Stockfish 18 jobs and must never create a
    // persisted bot-game row.
    const auto history_before_temporary = take_json(kc_bot_games_json(core), core);
    auto temporary_fen = std::string(start_fen);
    const auto temporary_human = take_json(
        kc_resolve_free_board_move_json(core, temporary_fen.c_str(), "e2", "e4"), core);
    temporary_fen = temporary_human.at("fenAfter").get<std::string>();
    expect(temporary_human.at("terminal") == false
               && temporary_human.at("checkmate") == false
               && temporary_human.at("result") == "*",
           "free-board move reports a non-terminal temporary position");
    const auto temporary_bot = wait_for_job(
        core, take_json(kc_start_bot_move_json(core, temporary_fen.c_str(), 800), core));
    expect(temporary_bot.at("status") == "complete"
               && temporary_bot.at("engineId") == "stockfish18",
           "temporary continuation uses the shared Stockfish 18 bot job");
    expect(temporary_bot.at("engineSessionId") == warm_engine_session
               && temporary_bot.at("reusedWarmEngine") == true,
           "consecutive bot jobs reuse the same warm Stockfish 18 engine and TT session");
    expect(take_json(kc_bot_games_json(core), core).size()
               == history_before_temporary.size()
               && take_json(kc_active_bot_game_json(core), core).is_null(),
           "free-board temporary continuation creates no persisted or resumable bot game");

    auto free_mate_fen = std::string(start_fen);
    nlohmann::json free_mate;
    for (const auto& [source, target] : std::vector<std::pair<const char*, const char*>>{
             {"f2", "f3"}, {"e7", "e5"}, {"g2", "g4"}, {"d8", "h4"}}) {
      free_mate = take_json(
          kc_resolve_free_board_move_json(
              core, free_mate_fen.c_str(), source, target),
          core);
      free_mate_fen = free_mate.at("fenAfter").get<std::string>();
    }
    expect(free_mate.at("terminal") == true
               && free_mate.at("checkmate") == true
               && free_mate.at("result") == "0-1",
           "free-board temporary continuation reports checkmate without persistence");
    const auto terminal_job = wait_for_job(
        core, take_json(kc_start_bot_move_json(core, free_mate_fen.c_str(), 3200), core));
    expect(terminal_job.at("terminal") == true
               && terminal_job.at("checkmate") == true
               && terminal_job.at("result") == "0-1"
               && terminal_job.at("move").get<std::string>().empty(),
           "Stockfish 18 bot job reports an already terminal temporary FEN");

    const auto mate_game = take_json(kc_create_bot_game_json(core, 1200), core);
    const auto mate_id = mate_game.at("gameId").get<std::string>();
    auto mate_fen = std::string(start_fen);
    for (const auto* move : {"f2f3", "e7e5", "g2g4", "d8h4"}) {
      const auto applied = take_json(
          kc_record_bot_game_move_json(core, mate_id.c_str(), mate_fen.c_str(), move), core);
      mate_fen = applied.at("fenAfter").get<std::string>();
    }
    const auto mated = take_json(kc_bot_game_json(core, mate_id.c_str()), core);
    expect(mated.at("status") == "complete"
               && mated.at("checkmate") == true
               && mated.at("result") == "0-1",
           "natural checkmate closes the persisted game and records its result");
    expect(take_json(kc_active_bot_game_json(core), core).is_null(),
           "naturally completed game does not block starting another game");

    auto beginner = wait_for_job(
        core, take_json(kc_start_bot_move_json(core, start_fen, 100), core));
    expect(beginner.at("requestedCandidateLines").get<int>() <= 32
               && beginner.at("scoutDepth").get<int>() <= 8
               && beginner.at("searchDepth").get<int>() <= 8,
           "100 Elo uses a shallow probability-targeted scout instead of deep 32-PV analysis");
    expect(beginner.at("status") == "complete"
               && beginner.at("searchedCandidateLines").get<int>() >= 1
               && beginner.at("targetRank").get<int>() >= 1,
           "low-Elo probability-first bot selection remains operational");

    char* invalid = kc_create_bot_game_json(core, 1550);
    expect(invalid == nullptr, "invalid non-100 Elo remains rejected by native policy");

    kc_core_destroy(core);
    core = nullptr;
    std::filesystem::remove_all(directory);
    std::cout << "Bot workflow tests passed (" << assertions << " assertions).\n";
    return EXIT_SUCCESS;
  } catch (const std::exception& error) {
    if (core != nullptr) kc_core_destroy(core);
    std::filesystem::remove_all(directory);
    std::cerr << "Bot workflow test failed after " << assertions
              << " assertions: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
