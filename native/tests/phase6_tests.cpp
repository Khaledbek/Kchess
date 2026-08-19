#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

#include "engine/chess_engine.h"
#include "persistence/database.h"
#include "sqlite3.h"

namespace {

using namespace kchess;
int assertions = 0;

void expect(const bool condition, const char* message) {
  ++assertions;
  if (!condition) throw std::runtime_error(message);
}

int row_count(const std::filesystem::path& database_path, const char* table) {
  sqlite3* database = nullptr;
  if (sqlite3_open(database_path.string().c_str(), &database) != SQLITE_OK) {
    throw std::runtime_error("could not inspect phase-6 database");
  }
  const std::string sql = std::string("SELECT count(*) FROM ") + table + ';';
  sqlite3_stmt* statement = nullptr;
  sqlite3_prepare_v2(database, sql.c_str(), -1, &statement, nullptr);
  const int result = sqlite3_step(statement) == SQLITE_ROW
      ? sqlite3_column_int(statement, 0) : -1;
  sqlite3_finalize(statement);
  sqlite3_close(database);
  return result;
}

void test_settings_and_profile_delete(const std::filesystem::path& directory) {
  std::string first_id;
  std::string second_id;
  std::string game_id;
  {
    Database database(directory);
    database.open_and_migrate();
    const auto first = database.create_profile(
        ProfileType::local_pgn_fen, "First", std::nullopt, "profile_unknown.png");
    first_id = first.id;
    database.set_engine_settings(24, 5, 7);
    database.set_setting("showBoardArrows", "false");
    const auto saved = database.settings();
    expect(saved.depth == 24, "depth saved");
    expect(saved.multi_pv == 5, "MultiPV saved");
    expect(saved.time_limit_seconds == 7, "time limit saved");
    expect(!saved.show_board_arrows, "arrow visibility saved separately");

    ParsedGame game;
    game.initial_fen = "start";
    game.raw_pgn = "[Result \"*\"]\n\n*";
    game.moves.push_back({
        .ply_index = 0,
        .move_number = 1,
        .side_to_move = "white",
        .san = "e4",
        .uci = "e2e4",
        .fen_before = "before",
        .fen_after = "after",
    });
    game_id = database.import_pgn(first.id, game);
    database.set_favorite(first.id, game_id, true);
    database.set_downloaded(first.id, game_id, true);
    database.put_provider_cache(
        first.id, "stats", "[]", {.fetched_at = 1, .expires_at = 2});
    database.set_provider_sync_state(
        first.id, ProviderType::chess_com, "idle", {}, 0);
    database.prepare_analysis(game_id, "strong", "Stockfish test", 1, 24, 5, 7);
    database.set_analysis_status(game_id, "strong", "complete");

    const auto compatible = database.compatible_analysis(
        game_id, "Stockfish test",
        AppSettings{.depth = 18, .multi_pv = 3, .time_limit_seconds = 5});
    expect(compatible.has_value(), "stronger cached analysis is reusable");
    const auto unlimited = database.compatible_analysis(
        game_id, "Stockfish test",
        AppSettings{.depth = 18, .multi_pv = 3, .time_limit_seconds = 0});
    expect(!unlimited.has_value(), "time-limited result is not unlimited-compatible");
    expect(!database.compatible_analysis(
                game_id, "Stockfish test",
                AppSettings{.depth = 25, .multi_pv = 3, .time_limit_seconds = 5})
                .has_value(),
           "insufficient cached depth is rejected");
    expect(!database.compatible_analysis(
                game_id, "Stockfish test",
                AppSettings{.depth = 18, .multi_pv = 6, .time_limit_seconds = 5})
                .has_value(),
           "insufficient cached MultiPV is rejected");
    expect(!database.compatible_analysis(
                game_id, "Stockfish test",
                AppSettings{.depth = 18, .multi_pv = 3, .time_limit_seconds = 8})
                .has_value(),
           "insufficient cached time budget is rejected");

  }
  {
    Database reopened(directory);
    reopened.open_and_migrate();
    expect(reopened.active_profile()->id == first_id, "last active profile survives restart");
    const auto loaded = reopened.settings();
    expect(loaded.depth == 24, "depth survives restart");
    expect(loaded.multi_pv == 5, "MultiPV survives restart");
    expect(loaded.time_limit_seconds == 7, "time limit survives restart");
    expect(!loaded.show_board_arrows, "show arrows survives restart");

    const auto second = reopened.create_profile(
        ProfileType::local_pgn_fen, "Second", std::nullopt, "profile_unknown.png");
    second_id = second.id;
    reopened.delete_profile(first_id);
    expect(!reopened.profile(first_id).has_value(), "inactive profile deleted");
    expect(reopened.active_profile()->id == second.id, "active profile remains selected");
  }
  {
    Database reopened(directory);
    reopened.open_and_migrate();
    expect(reopened.active_profile()->id == second_id, "replacement survives restart");

    const auto third = reopened.create_profile(
        ProfileType::chess_com, "Third", "third", "provider_chesscom_fallback.png");
    reopened.set_active_profile(second_id);
    const auto replacement = reopened.delete_profile(second_id);
    expect(replacement.has_value() && replacement->id == third.id,
           "deleting active profile selects another profile");
    expect(reopened.delete_profile(third.id) == std::nullopt,
           "deleting last profile clears active profile");
    expect(!reopened.active_profile().has_value(), "last active profile reference cleared");
  }

  const auto database_path = directory / "kchess.sqlite3";
  expect(row_count(database_path, "profiles") == 0, "profiles removed");
  expect(row_count(database_path, "games") == 0, "profile games cascaded");
  expect(row_count(database_path, "game_moves") == 0, "game moves cascaded");
  expect(row_count(database_path, "favorites") == 0, "favorites cascaded");
  expect(row_count(database_path, "downloads") == 0, "downloads cascaded");
  expect(row_count(database_path, "analysis_runs") == 0, "analysis runs cascaded");
  expect(row_count(database_path, "provider_stats_cache") == 0, "provider cache cascaded");
  expect(row_count(database_path, "provider_sync_state") == 0, "sync state cascaded");
  expect(row_count(database_path, "engine_settings") == 0, "engine settings cascaded");
}

void test_windows_engine(const std::filesystem::path& test_directory) {
  const auto missing = test_directory / "missing_stockfish_assets";
  StockfishEngine invalid(missing);
  bool controlled_error = false;
  try {
    invalid.validate_available();
  } catch (const std::exception&) {
    controlled_error = true;
  }
  expect(controlled_error, "invalid Stockfish asset path is a controlled error");

  auto asset_directory = std::filesystem::path(KCHESS_STOCKFISH_ASSET_DIR);
#if defined(_WIN32)
  auto unicode_name = std::filesystem::path(L"kchess_\u00DCnicode_assets_");
  unicode_name += std::to_wstring(
      std::chrono::steady_clock::now().time_since_epoch().count());
  const auto unicode_directory = test_directory / unicode_name;
  std::filesystem::create_directories(unicode_directory);
  std::filesystem::create_hard_link(
      asset_directory / "nn-c288c895ea92.nnue",
      unicode_directory / "nn-c288c895ea92.nnue");
  std::filesystem::create_hard_link(
      asset_directory / "nn-37f18f62d772.nnue",
      unicode_directory / "nn-37f18f62d772.nnue");
  asset_directory = unicode_directory;
#endif
  StockfishEngine engine(asset_directory);
  engine.validate_available();
  engine.start();
  expect(engine.is_ready(), "official Stockfish engine starts");
  engine.new_game();
  const auto started = std::chrono::steady_clock::now();
  const auto result = engine.analyze({
      .fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
      .depth = 2,
      .multi_pv = 2,
      .threads = 1,
      .hash_mb = 16,
      .time_limit_seconds = 1,
  });
  const auto elapsed = std::chrono::steady_clock::now() - started;
  expect(!result.best_move.empty(), "engine returns a best move");
  expect(result.reached_depth >= 2, "configured depth reaches engine adapter");
  expect(!result.lines.empty() && result.lines.size() <= 2,
         "configured MultiPV reaches engine adapter");
  expect(elapsed < std::chrono::seconds(5), "short engine smoke remains bounded");
  engine.stop();
#if defined(_WIN32)
  std::filesystem::remove_all(unicode_directory);
#endif
}

}  // namespace

int main() {
  const auto directory = std::filesystem::temp_directory_path()
      / ("kchess_phase6_tests_" + std::to_string(
          std::chrono::steady_clock::now().time_since_epoch().count()));
  try {
    std::filesystem::create_directories(directory);
    test_settings_and_profile_delete(directory);
    test_windows_engine(directory);
    std::filesystem::remove_all(directory);
    std::cout << "All " << assertions << " KChess phase-6 assertions passed.\n";
    return EXIT_SUCCESS;
  } catch (const std::exception& error) {
    std::filesystem::remove_all(directory);
    std::cerr << "Phase-6 test failed: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
