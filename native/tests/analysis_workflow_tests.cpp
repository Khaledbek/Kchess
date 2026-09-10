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

}  // namespace

int main() {
  const auto directory = std::filesystem::temp_directory_path()
      / ("kchess_analysis_workflow_" + std::to_string(
          std::chrono::steady_clock::now().time_since_epoch().count()));
  kc_core_handle core = nullptr;
  try {
    std::filesystem::create_directories(directory);
    const auto encoded = directory.u8string();
    const std::string path(encoded.begin(), encoded.end());
    core = kc_core_create(path.c_str());
    expect(core != nullptr, "core handle created");
    expect(kc_core_initialize(core) == KC_STATUS_OK, "core initialized");
    take_json(kc_create_profile_json(core, 2, "BlackSide", ""), core);
    expect(kc_set_engine_settings(core, 1, 2, 0) == KC_STATUS_OK,
           "short variation settings accepted");
    expect(kc_set_sideline_engine_settings(core, 7, 4, 1, 64) == KC_STATUS_OK,
           "sideline engine settings accepted");
    const auto saved_settings = take_json(kc_app_settings_json(core), core);
    expect(saved_settings.at("sidelineDepth") == 7
               && saved_settings.at("sidelineMultiPv") == 4
               && saved_settings.at("sidelineThreads") == 1
               && saved_settings.at("sidelineHashMb") == 64,
           "last sideline engine settings are persisted independently");
    const auto pgn_game = take_json(
        kc_import_pgn_json(
            core,
            "[White \"WhiteSide\"]\n[Black \"BlackSide\"]\n[Result \"*\"]\n\n"
            "1. e4 e5 2. Nf3 Nc6 *"),
        core);
    const auto pgn_game_id = pgn_game.at("id").get<std::string>();
    const auto original_detail = take_json(
        kc_game_json(core, pgn_game_id.c_str()), core);
    expect(original_detail.at("startingPosition").at("pieces").size() == 64,
           "game detail exposes a native 64-square board DTO");
    expect(original_detail.at("startingPosition").at("sideToMove") == "white",
           "game detail exposes native side-to-move");
    const auto queried_games = take_json(
        kc_games_query_json(
            core,
            R"({"search":"white","color":"black","sort":"oldest","timeControls":[]})"),
        core);
    expect(queried_games.size() == 1 && queried_games.front().at("id") == pgn_game_id,
           "search, profile color and sorting are owned by the native game query");

    const auto rejoin = take_json(
        kc_resolve_board_move_json(
            core, pgn_game_id.c_str(),
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
            "e2", "e4", 0),
        core);
    expect(rejoin.at("uci") == "e2e4" && rejoin.at("mainLinePly") == 0,
           "native board resolution rejoins the matching PGN ply");

    const auto promotion_options = take_json(
        kc_board_promotion_options_json(
            core, "4k3/P7/8/8/8/8/8/4K3 w - - 0 1", "a7", "a8"),
        core);
    expect(promotion_options.size() == 4
               && promotion_options.at(0) == "q"
               && promotion_options.at(1) == "r"
               && promotion_options.at(2) == "b"
               && promotion_options.at(3) == "n",
           "native board resolution exposes every legal promotion choice");

    auto* missing_promotion = kc_resolve_board_move_json(
        core, pgn_game_id.c_str(),
        "4k3/P7/8/8/8/8/8/4K3 w - - 0 1", "a7", "a8", 0);
    expect(missing_promotion == nullptr,
           "promotion is never silently defaulted to a queen");

    const auto promotion = take_json(
        kc_resolve_board_move_json(
            core, pgn_game_id.c_str(),
            "4k3/P7/8/8/8/8/8/4K3 w - - 0 1", "a7", "a8n", 0),
        core);
    expect(promotion.at("uci") == "a7a8n" && promotion.at("san") == "a8=N",
           "native board resolution applies the selected underpromotion");
    const auto free_promotion = take_json(
        kc_resolve_free_board_move_json(
            core, "4k3/P7/8/8/8/8/8/4K3 w - - 0 1", "a7", "a8r"),
        core);
    expect(free_promotion.at("uci") == "a7a8r"
               && free_promotion.at("positionAfter").at("sideToMove") == "black",
           "temporary free-board play applies the selected promotion too");

    constexpr auto start_fen =
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    auto variation = take_json(
        kc_start_variation_analysis_json(core, start_fen, "g1f3"), core);
    expect(variation.at("playedSan") == "Nf3", "legal UCI move converted to SAN");
    expect(variation.at("fen").get<std::string>().find("5N2") != std::string::npos,
           "temporary FEN contains the played knight move");
    expect(variation.at("position").at("pieces").size() == 64
               && variation.at("position").at("sideToMove") == "black",
           "variation exposes its native presentation-ready board DTO");
    const auto job_id = variation.at("jobId").get<std::string>();
    for (int attempt = 0;
         variation.at("status") == "running" && attempt < 500; ++attempt) {
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
      variation = take_json(
          kc_variation_analysis_status_json(core, job_id.c_str()), core);
    }
    expect(variation.at("status") == "complete", "temporary variation completed");
    expect(!variation.at("bestMove").get<std::string>().empty(),
           "temporary variation exposes a best move");
    expect(!variation.at("lines").empty() && variation.at("lines").size() <= 2,
           "temporary variation exposes configured MultiPV lines");
    expect(variation.at("lines").front().at("evaluationBarWhitePermille").is_null(),
           "SF18 keeps the legacy evaluation-bar projection");
    expect(!variation.at("classification").is_null(),
           "completed variation exposes its move classification");
    expect(take_json(kc_game_json(core, pgn_game_id.c_str()), core) == original_detail,
           "temporary analysis leaves the original PGN and game moves unchanged");

    auto analysis = take_json(kc_start_analysis_json(core, pgn_game_id.c_str()), core);
    for (int attempt = 0;
         analysis.at("status") == "running" && attempt < 500; ++attempt) {
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
      analysis = take_json(kc_analysis_status_json(core, pgn_game_id.c_str()), core);
    }
    expect(analysis.at("status") == "complete", "main analysis completed once");
    expect(analysis.at("summary").at("profileSide") == "black",
           "active profile matching Black is mapped to the Black summary");
    expect(!analysis.at("summary").at("white").at("localAccuracy").is_null()
               && !analysis.at("summary").at("black").at("localAccuracy").is_null(),
           "White and Black accuracy are persisted independently");
    const auto cached = take_json(kc_start_analysis_json(core, pgn_game_id.c_str()), core);
    expect(cached.at("status") == "complete" && cached.at("progress") == 1.0,
           "second open returns compatible complete cache immediately");

    char* illegal = kc_start_variation_analysis_json(core, start_fen, "g1g4");
    expect(illegal == nullptr, "illegal board move rejected by native chess rules");

    // SF19 sideline regression: consecutive variation jobs reuse the same
    // engine/TT, but the public live snapshot must never leak from the previous
    // root position. Finished SF19 lines must also come from one exact MultiPV
    // iteration so their depths/ranks describe one coherent position result.
    expect(kc_set_engine_id(core, "stockfish19") == KC_STATUS_OK,
           "Stockfish 19 selected for sideline regression");
    expect(kc_set_sideline_engine_settings(core, 5, 2, 1, 64) == KC_STATUS_OK,
           "short SF19 sideline settings accepted");

    const auto sf19_fen_game = take_json(
        kc_import_fen_json(core, start_fen, "SF19 eval-bar regression"), core);
    const auto sf19_fen_game_id = sf19_fen_game.at("id").get<std::string>();
    auto sf19_main = take_json(
        kc_start_analysis_json(core, sf19_fen_game_id.c_str()), core);
    for (int attempt = 0;
         sf19_main.at("status") == "running" && attempt < 500; ++attempt) {
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
      sf19_main = take_json(
          kc_analysis_status_json(core, sf19_fen_game_id.c_str()), core);
    }
    expect(sf19_main.at("status") == "complete" && !sf19_main.at("lines").empty(),
           "SF19 FEN main-line analysis completed for eval-bar regression");
    expect(sf19_main.at("analyzedFen") == start_fen,
           "SF19 main-line snapshot is explicitly bound to its analyzed FEN");
    expect(sf19_main.at("bestMove") == sf19_main.at("lines").front().at("moves").front(),
           "SF19 main-line best move agrees with coherent rank 1");
    const auto& sf19_main_line = sf19_main.at("lines").front();
    expect(!sf19_main_line.at("evaluationBarWhitePermille").is_null(),
           "SF19 main-line publishes its native WDL evaluation-bar projection");

    auto sf19_variation = take_json(
        kc_start_variation_analysis_json(core, start_fen, "e2e4"), core);
    const auto sf19_job_id = sf19_variation.at("jobId").get<std::string>();
    for (int attempt = 0;
         sf19_variation.at("status") == "running" && attempt < 500; ++attempt) {
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
      sf19_variation = take_json(
          kc_variation_analysis_status_json(core, sf19_job_id.c_str()), core);
    }
    expect(sf19_variation.at("status") == "complete",
           "SF19 temporary variation completed");
    expect(!sf19_variation.at("bestMove").get<std::string>().empty(),
           "SF19 temporary variation exposes a position-local best move");
    expect(sf19_variation.at("engineVersion").get<std::string>().rfind("Stockfish 19", 0) == 0,
           "SF19 sideline identifies the engine that produced its arrow data");
    const auto& sf19_lines = sf19_variation.at("lines");
    expect(!sf19_lines.empty() && sf19_lines.size() <= 2,
           "SF19 sideline hides internal classification-only MultiPV lines");
    const int sf19_depth = sf19_lines.front().at("depth").get<int>();
    bool coherent_sf19_lines = true;
    for (std::size_t index = 0; index < sf19_lines.size(); ++index) {
      coherent_sf19_lines = coherent_sf19_lines
          && sf19_lines[index].at("depth").get<int>() == sf19_depth
          && sf19_lines[index].at("rank").get<int>() == static_cast<int>(index) + 1;
    }
    expect(coherent_sf19_lines,
           "SF19 sideline publishes one exact coherent MultiPV iteration");
    expect(sf19_variation.at("bestMove") == sf19_lines.front().at("moves").front(),
           "SF19 sideline best move agrees with coherent rank 1");
    const auto& sf19_principal = sf19_lines.front();
    expect(!sf19_principal.at("evaluationBarWhitePermille").is_null(),
           "SF19 sideline publishes its native WDL evaluation-bar projection");
    const auto& sf19_wdl = sf19_principal.at("wdl");
    const int sf19_wins = sf19_wdl.at("wins").get<int>();
    const int sf19_draws = sf19_wdl.at("draws").get<int>();
    const int sf19_losses = sf19_wdl.at("losses").get<int>();
    const int sf19_total = sf19_wins + sf19_draws + sf19_losses;
    const int expected_sf19_bar = static_cast<int>(
        (1000LL * (2LL * sf19_wins + sf19_draws) + sf19_total)
        / (2LL * sf19_total));
    expect(sf19_principal.at("evaluationBarWhitePermille").get<int>()
               == expected_sf19_bar,
           "SF19 evaluation bar follows White-perspective WDL expected score");

    constexpr auto mate_setup_fen =
        "7k/5Q2/6K1/8/8/8/8/8 w - - 0 1";
    auto sf19_mate = take_json(
        kc_start_variation_analysis_json(core, mate_setup_fen, "f7f8"), core);
    // Qf8 is mate, so the resulting position has no engine best move. If the
    // reused SF19 adapter leaks the previous root snapshot, this immediate
    // response can incorrectly contain the old sideline best move.
    expect(sf19_mate.at("bestMove").get<std::string>().empty(),
           "new SF19 sideline never starts with the previous position best move");
    const auto sf19_mate_job_id = sf19_mate.at("jobId").get<std::string>();
    for (int attempt = 0;
         sf19_mate.at("status") == "running" && attempt < 500; ++attempt) {
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
      sf19_mate = take_json(
          kc_variation_analysis_status_json(core, sf19_mate_job_id.c_str()), core);
    }
    expect(sf19_mate.at("status") == "complete",
           "terminal SF19 sideline completes normally");
    expect(sf19_mate.at("bestMove").get<std::string>().empty(),
           "terminal SF19 sideline keeps an empty best move");

    kc_core_destroy(core);
    core = nullptr;
    std::filesystem::remove_all(directory);
    std::cout << "All " << assertions
              << " focused analysis-workflow assertions passed.\n";
    return EXIT_SUCCESS;
  } catch (const std::exception& error) {
    if (core != nullptr) kc_core_destroy(core);
    std::filesystem::remove_all(directory);
    std::cerr << "Analysis-workflow test failed: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
