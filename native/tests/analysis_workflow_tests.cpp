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
    const auto pgn_game = take_json(
        kc_import_pgn_json(
            core,
            "[White \"WhiteSide\"]\n[Black \"BlackSide\"]\n[Result \"*\"]\n\n"
            "1. e4 e5 2. Nf3 Nc6 *"),
        core);
    const auto pgn_game_id = pgn_game.at("id").get<std::string>();
    const auto original_detail = take_json(
        kc_game_json(core, pgn_game_id.c_str()), core);

    constexpr auto start_fen =
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    auto variation = take_json(
        kc_start_variation_analysis_json(core, start_fen, "g1f3"), core);
    expect(variation.at("playedSan") == "Nf3", "legal UCI move converted to SAN");
    expect(variation.at("fen").get<std::string>().find("5N2") != std::string::npos,
           "temporary FEN contains the played knight move");
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
