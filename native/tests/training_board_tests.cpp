// -----------------------------------------------------------------------------
// Section: Native training catalogue and ABI contract
// -----------------------------------------------------------------------------

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

#include "kchess/core_api.h"

namespace {

void expect(const bool condition, const std::string& message) {
  if (!condition) throw std::runtime_error(message);
}

std::string take_string(char* value) {
  expect(value != nullptr, "Expected native string result");
  std::string result(value);
  kc_string_free(value);
  return result;
}

void test_training_exports() {
  const auto directory =
      std::filesystem::temp_directory_path() / "kchess_training_tests";
  std::filesystem::remove_all(directory);
  std::filesystem::create_directories(directory);
  auto* core = kc_core_create(directory.string().c_str());
  expect(core != nullptr, "Core handle must be created");
  expect(kc_core_initialize(core) == KC_STATUS_OK, "Core must initialize");

  const auto overview = take_string(kc_training_overview_json(core));
  expect(
      overview.find("\"endgame_lucena\"") != std::string::npos,
      "Overview must expose the native catalogue");
  expect(
      overview.find("solution") == std::string::npos,
      "Solution moves must not cross into Flutter");

  const auto attempt =
      take_string(kc_start_training_attempt_json(core, "endgame_opposition"));
  expect(
      attempt.find("\"attemptId\":\"training-1\"") != std::string::npos,
      "Starting an exercise must create an opaque attempt");

  const auto wrong =
      take_string(kc_play_training_move_json(core, "training-1", "d5", "d4"));
  expect(
      wrong.find("\"accepted\":false") != std::string::npos,
      "Native training must reject a wrong move");

  kc_core_destroy(core);
  std::filesystem::remove_all(directory);
}

}  // namespace

int main() {
  try {
    test_training_exports();
    std::cout << "All KChess training tests passed.\n";
    return EXIT_SUCCESS;
  } catch (const std::exception& error) {
    std::cerr << "Native test failed: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
