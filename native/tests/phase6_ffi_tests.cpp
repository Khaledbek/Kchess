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
      / ("kchess_phase6_ffi_" + std::to_string(
          std::chrono::steady_clock::now().time_since_epoch().count()));
  kc_core_handle core = nullptr;
  try {
    std::filesystem::create_directories(directory);
    const auto encoded = directory.u8string();
    const std::string path(encoded.begin(), encoded.end());
    core = kc_core_create(path.c_str());
    expect(core != nullptr, "DLL core handle created");
    expect(kc_core_initialize(core) == KC_STATUS_OK, "DLL core initialized");

    const auto first = take_json(
        kc_create_profile_json(core, 2, "First", ""), core);
    const auto second = take_json(
        kc_create_profile_json(core, 2, "Second", ""), core);
    expect(kc_delete_profile(core, first.at("id").get<std::string>().c_str()) == KC_STATUS_OK,
           "non-active profile deleted through C ABI");
    expect(take_json(kc_active_profile_json(core), core).at("id") == second.at("id"),
           "active profile remains after non-active delete");

    expect(kc_set_engine_settings(core, 2, 2, 1) == KC_STATUS_OK,
           "engine settings accepted through C ABI");
    expect(kc_set_engine_settings(core, 0, 2, 1) == KC_STATUS_INVALID_ARGUMENT,
           "depth lower bound enforced");
    expect(kc_set_engine_settings(core, 65, 2, 1) == KC_STATUS_INVALID_ARGUMENT,
           "depth upper bound enforced");
    expect(kc_set_engine_settings(core, 2, 0, 1) == KC_STATUS_INVALID_ARGUMENT,
           "MultiPV lower bound enforced");
    expect(kc_set_engine_settings(core, 2, 9, 1) == KC_STATUS_INVALID_ARGUMENT,
           "MultiPV upper bound enforced");
    expect(kc_set_engine_settings(core, 2, 2, -1) == KC_STATUS_INVALID_ARGUMENT,
           "time limit lower bound enforced");
    expect(kc_set_engine_settings(core, 2, 2, 61) == KC_STATUS_INVALID_ARGUMENT,
           "time limit upper bound enforced");
    expect(kc_set_show_board_arrows(core, 0) == KC_STATUS_OK,
           "arrow setting accepted through C ABI");
    const auto settings = take_json(kc_app_settings_json(core), core);
    expect(settings.at("depth") == 2 && settings.at("multiPv") == 2,
           "depth and MultiPV round-trip through DLL");
    expect(settings.at("timeLimitSeconds") == 1 && !settings.at("showBoardArrows"),
           "time limit and arrow state round-trip through DLL");

    const auto game = take_json(
        kc_import_fen_json(core, "8/8/8/8/8/4k3/8/4K3 w - - 0 1", "Smoke"), core);
    const auto game_id = game.at("id").get<std::string>();
    auto analysis = take_json(kc_start_analysis_json(core, game_id.c_str()), core);
    for (int attempt = 0; analysis.at("status") == "running" && attempt < 1500; ++attempt) {
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
      analysis = take_json(kc_analysis_status_json(core, game_id.c_str()), core);
    }
    if (analysis.at("status") != "complete") {
      throw std::runtime_error("short DLL analysis failed: " + analysis.dump());
    }
    ++assertions;
    expect(!analysis.at("bestMove").get<std::string>().empty(),
           "DLL analysis returns UCI best move");
    expect(analysis.at("lines").size() <= 2, "DLL analysis uses configured MultiPV");

    expect(kc_delete_profile(core, second.at("id").get<std::string>().c_str()) == KC_STATUS_OK,
           "active last profile deleted through C ABI");
    expect(take_json(kc_profiles_json(core), core).empty(), "last profile removal leaves no rows");
    expect(take_json(kc_active_profile_json(core), core).is_null(),
           "last profile removal clears persisted active selection");

    kc_core_destroy(core);
    core = nullptr;
    std::filesystem::remove_all(directory);
    std::cout << "All " << assertions << " KChess phase-6 DLL assertions passed.\n";
    return EXIT_SUCCESS;
  } catch (const std::exception& error) {
    if (core != nullptr) kc_core_destroy(core);
    std::filesystem::remove_all(directory);
    std::cerr << "Phase-6 DLL test failed: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
