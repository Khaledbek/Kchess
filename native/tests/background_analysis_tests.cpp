// -----------------------------------------------------------------------------
// Section: Background analysis and accuracy statistics over the C ABI
// -----------------------------------------------------------------------------

#include <chrono>
#include <filesystem>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

#include <nlohmann/json.hpp>

#include "kchess/core_api.h"

namespace {

using nlohmann::json;
using namespace std::chrono_literals;

void expect(const bool condition, const std::string& message) {
  if (!condition) throw std::runtime_error(message);
}

json take_json(char* value, kc_core_handle core) {
  if (value == nullptr) {
    const auto* error = kc_core_last_error(core);
    throw std::runtime_error(std::string("native call failed: ") + (error ? error : ""));
  }
  const json parsed = json::parse(value);
  kc_string_free(value);
  return parsed;
}

json status(kc_core_handle core) {
  return take_json(kc_background_analysis_status_json(core), core);
}

// Polls until the predicate holds or the deadline passes; returns the last status.
json wait_for(kc_core_handle core, const std::function<bool(const json&)>& done,
              std::chrono::seconds timeout) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  json current = status(core);
  while (!done(current) && std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(250ms);
    current = status(core);
  }
  return current;
}

constexpr const char* kFirstGame =
    "[White \"Tester\"]\n[Black \"Opponent\"]\n[Result \"1-0\"]\n\n"
    "1. e4 e5 2. Nf3 Nc6 3. Bc4 Nf6 4. Ng5 d5 5. exd5 Nxd5 6. Nxf7 Kxf7 "
    "7. Qf3+ Ke6 8. Nc3 Nb4 9. Qe4 c6 10. a3 Na6 11. d4 Nc7 12. Bf4 Kf7 "
    "13. Bxe5 Be6 14. O-O-O Qd7 1-0";
constexpr const char* kSecondGame =
    "[White \"Opponent\"]\n[Black \"Tester\"]\n[Result \"0-1\"]\n\n"
    "1. d4 d5 2. c4 e6 3. Nc3 Nf6 4. Bg5 Be7 5. e3 O-O 6. Nf3 h6 7. Bh4 b6 "
    "8. cxd5 Nxd5 9. Bxe7 Qxe7 10. Nxd5 exd5 0-1";

}  // namespace

int main() {
  const auto directory = std::filesystem::temp_directory_path() /
      ("kchess_background_analysis_" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  kc_core_handle core = nullptr;
  try {
    std::filesystem::create_directories(directory);
    const auto encoded = directory.u8string();
    core = kc_core_create(std::string(encoded.begin(), encoded.end()).c_str());
    expect(kc_core_initialize(core) == KC_STATUS_OK, "core initialized");
    take_json(kc_create_profile_json(core, 2, "Tester", ""), core);
    // Shallow searches keep the test quick; the pipeline is the same.
    expect(kc_set_analysis_depth_range(core, 6, 8) == KC_STATUS_OK, "depth range set");

    const auto first = take_json(kc_import_pgn_json(core, kFirstGame), core);
    const auto second = take_json(kc_import_pgn_json(core, kSecondGame), core);

    // Before anything runs: nothing analysed, nothing to average.
    auto accuracy = take_json(kc_statistics_accuracy_json(core, "all"), core);
    expect(accuracy.at("analysedGames") == 0 && accuracy.at("averageAccuracy").is_null(),
        "No analysed games, no accuracy");
    auto initial = status(core);
    expect(initial.at("enabled") == true, "Background analysis is on by default");
    expect(initial.at("totalGames") == 2 && initial.at("analysedGames") == 0,
        "Progress counts the profile's games");

    expect(kc_start_background_analysis(core) == KC_STATUS_OK, "background started");

    // Turning it off stops work and says so.
    expect(kc_set_background_analysis_enabled(core, 0) == KC_STATUS_OK, "disabled");
    auto off = wait_for(core, [](const json& s) { return s.at("state") == "disabled"; }, 10s);
    expect(off.at("enabled") == false && off.at("state") == "disabled",
        "A disabled queue reports it");
    expect(kc_set_background_analysis_enabled(core, 1) == KC_STATUS_OK, "re-enabled");

    // It starts on its own and yields as soon as the user analyses a game.
    const auto running = wait_for(core, [](const json& s) { return s.at("state") == "running"; }, 30s);
    expect(running.at("state") == "running", "The queue picks a game up");
    const auto first_id = first.at("id").get<std::string>();
    take_json(kc_start_analysis_json(core, first_id.c_str()), core);
    const auto paused = wait_for(core, [](const json& s) { return s.at("state") == "paused"; }, 10s);
    expect(paused.at("state") == "paused", "The user's own analysis pauses the queue");

    // Once the user is idle again it finishes the whole library.
    const auto finished = wait_for(
        core, [](const json& s) { return s.at("analysedGames") == s.at("totalGames"); }, 300s);
    expect(finished.at("analysedGames") == 2, "Every game ends up analysed");
    const auto complete = wait_for(core, [](const json& s) { return s.at("state") == "complete"; }, 20s);
    expect(complete.at("state") == "complete", "Nothing left reports complete");

    // Opening a game the queue analysed reuses its result instead of
    // analysing it again: the analysis screen gets a complete run at once.
    const auto second_id = second.at("id").get<std::string>();
    const auto reopened = take_json(kc_start_analysis_json(core, second_id.c_str()), core);
    expect(reopened.at("status") == "complete",
        "A background-analysed game opens with its analysis already complete");

    accuracy = take_json(kc_statistics_accuracy_json(core, "all"), core);
    expect(accuracy.at("analysedGames") == 2, "Both games count");
    const double average = accuracy.at("averageAccuracy").get<double>();
    expect(average >= 0.0 && average <= 100.0, "Average accuracy is a percentage");
    expect(accuracy.at("byColor").at("white").at("games") == 1 &&
               accuracy.at("byColor").at("black").at("games") == 1,
        "Colours come from the profile's side in each game");
    const auto& opening = accuracy.at("byPhase").at(0);
    expect(opening.at("phase") == "opening" && opening.at("games") == 2,
        "Both games reached the opening with per-move accuracy stored");
    expect(accuracy.at("byPhase").at(1).at("games") == 1,
        "Only the longer game reached the middlegame");
    expect(accuracy.at("timeline").size() == 2, "One chart point per game");
    expect(accuracy.at("trend").at("verdict") == "insufficient" &&
               accuracy.at("trend").at("gamesNeeded") == 8,
        "Two games are too few to call a trend");

    kc_core_destroy(core);
    core = nullptr;
    std::filesystem::remove_all(directory);
  } catch (const std::exception& error) {
    if (core != nullptr) kc_core_destroy(core);
    std::cerr << "background analysis tests failed: " << error.what() << std::endl;
    return 1;
  }
  std::cout << "background analysis tests passed" << std::endl;
  return 0;
}
