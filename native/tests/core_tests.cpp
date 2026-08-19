#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "analysis/accuracy.h"
#include "analysis/move_classifier.h"
#include "chess/fen.h"
#include "chess/pgn.h"
#include "engine/chess_engine.h"
#include "kchess/core_api.h"
#include "persistence/database.h"
#include "theory/opening_theory_provider.h"
#include "theory/position_key.h"

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

std::string json_string_field(const std::string& json, const std::string& field) {
  const std::string marker = "\"" + field + "\":\"";
  const auto start = json.find(marker);
  expect(start != std::string::npos, "Expected JSON field: " + field);
  const auto value_start = start + marker.size();
  const auto end = json.find('"', value_start);
  expect(end != std::string::npos, "Expected JSON string terminator");
  return json.substr(value_start, end - value_start);
}

std::filesystem::path bundled_book_path() {
  return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path()
      / "flutter_app" / "assets" / "opening_book.kcb";
}

void test_smoke() {
  expect(kc_smoke_test(41) == 42, "FFI smoke result must be 42");
  expect(std::string(kc_core_version()).find("phase4") != std::string::npos,
         "Phase-4 version must be exposed through the C ABI");
}

void test_opening_book() {
  const auto path = bundled_book_path();
  expect(std::filesystem::exists(path), "Bundled KCB book must exist");
  const kchess::KcbOpeningTheoryProvider book(path);
  expect(book.metadata().format_version == 1 && book.metadata().entry_count > 923,
         "Expanded bundled KCB1 must contain more entries than the Phase-3 proof of concept");
  expect(book.metadata().builder_version == "kcb-builder-2",
         "Expanded Book must expose the multi-input builder version");
  expect(book.metadata().source == "lichess" && book.source_license() == "CC0-1.0",
         "Bundled opening book must retain CC0 provenance");
  const auto e4 = book.lookup(kchess::kStartFen, "e2e4");
  expect(e4.is_theory && e4.games >= 100, "Common e4 must be a local Theory move");
  expect(!book.lookup(kchess::kStartFen, "e1e2").is_theory,
         "Unknown/illegal moves must not become Theory");

  auto expect_theory_line = [&](const std::string& pgn, const std::string& name) {
    const auto parsed = kchess::parse_pgn(pgn);
    expect(parsed.valid, "Opening quality fixture must parse: " + name);
    for (const auto& move : parsed.game.moves) {
      expect(book.lookup(move.fen_before, move.uci).is_theory,
             name + " move must be present as Theory: " + move.san);
    }
  };
  expect_theory_line("1. e4 c5 *", "Sicilian Defense");
  expect_theory_line("1. d4 d5 *", "Queen's Pawn Game");
  expect_theory_line("1. d4 Nf6 *", "Indian Game");
  expect_theory_line("1. c4 e5 *", "English Opening");
  expect_theory_line(
      "1. e4 e5 2. Nf3 Nc6 3. Bb5 a6 4. Ba4 Nf6 5. O-O Be7 *",
      "Ruy Lopez");

  const auto transposition_a = kchess::parse_pgn("1. Nf3 d5 2. g3 Nf6 *");
  const auto transposition_b = kchess::parse_pgn("1. g3 Nf6 2. Nf3 d5 *");
  expect(transposition_a.valid && transposition_b.valid,
         "Transposition fixtures must parse");
  const auto& fen_a = transposition_a.game.moves.back().fen_after;
  const auto& fen_b = transposition_b.game.moves.back().fen_after;
  expect(kchess::stockfish_position_key(fen_a) == kchess::stockfish_position_key(fen_b),
         "Transposed move orders must produce the same Stockfish position key");
  const auto transposed_move_a = book.lookup(fen_a, "f1g2");
  const auto transposed_move_b = book.lookup(fen_b, "f1g2");
  expect(transposed_move_a.is_theory && transposed_move_b.is_theory
             && transposed_move_a.games == transposed_move_b.games,
         "Transposed positions must expose the same Theory move and aggregate counts");

  constexpr int kLookupIterations = 5'000;
  const auto lookup_started = std::chrono::steady_clock::now();
  for (int iteration = 0; iteration < kLookupIterations; ++iteration) {
    (void)book.lookup(kchess::kStartFen, iteration % 2 == 0 ? "e2e4" : "d2d4");
  }
  const auto lookup_elapsed = std::chrono::steady_clock::now() - lookup_started;
  expect(lookup_elapsed < std::chrono::milliseconds(kLookupIterations),
         "Average in-memory KCB lookup must remain below one millisecond");
  expect(kchess::stockfish_position_key(kchess::kStartFen) == 0x8f8f01d4562f59fbULL,
         "Native and Python Stockfish start-position keys must match");
  expect(kchess::stockfish_position_key(
             "4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1") == 0xdf7a0861474fbd85ULL,
         "Native and Python Stockfish en-passant keys must match");

  std::ifstream input(path, std::ios::binary);
  const std::vector<char> bytes{
      std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
  const auto directory = std::filesystem::temp_directory_path() / "kchess_kcb_tests";
  std::filesystem::remove_all(directory);
  std::filesystem::create_directories(directory);
  auto expect_invalid = [&](const std::string& name, std::vector<char> damaged) {
    const auto damaged_path = directory / name;
    std::ofstream output(damaged_path, std::ios::binary);
    output.write(damaged.data(), static_cast<std::streamsize>(damaged.size()));
    output.close();
    try {
      const kchess::KcbOpeningTheoryProvider ignored(damaged_path);
      (void)ignored;
      expect(false, "Damaged KCB must be rejected: " + name);
    } catch (const std::exception&) {
    }
  };
  auto bad_magic = bytes;
  bad_magic[0] = 'X';
  expect_invalid("bad-magic.kcb", std::move(bad_magic));
  auto bad_version = bytes;
  bad_version[4] = 2;
  expect_invalid("bad-version.kcb", std::move(bad_version));
  auto truncated = bytes;
  truncated.pop_back();
  expect_invalid("truncated.kcb", std::move(truncated));
  std::filesystem::remove_all(directory);
}

void test_classifier_and_accuracy() {
  using kchess::MoveCategory;
  using kchess::MoveClassifierInput;
  auto classified = [](MoveClassifierInput input) { return kchess::classify_move(input); };
  expect(classified({.theory = true, .played_is_best = true}) == MoveCategory::theory,
         "Theory must override Best");
  expect(classified({.played_is_best = true, .best_expected_score = .8,
                     .played_expected_score = .4}) == MoveCategory::best,
         "The engine best move must classify Best");
  expect(classified({.best_expected_score = .7, .played_expected_score = .68})
             == MoveCategory::excellent,
         "Small loss must classify Excellent");
  expect(classified({.best_expected_score = .7, .played_expected_score = .65})
             == MoveCategory::okay,
         "Moderate loss must classify Okay");
  expect(classified({.best_expected_score = .7, .played_expected_score = .60})
             == MoveCategory::mistake,
         "Significant loss must classify Mistake");
  expect(classified({.best_expected_score = .7, .played_expected_score = .40})
             == MoveCategory::blunder,
         "Large loss must classify Blunder");
  expect(classified({.best_expected_score = .80, .played_expected_score = .64})
             == MoveCategory::miss,
         "A missed clear chance must classify Miss before Mistake");
  expect(classified({.played_is_best = true, .only_move_tactical = true,
                     .legal_move_count = 12,
                     .best_expected_score = .80, .played_expected_score = .80,
                     .second_best_expected_score = .50}) == MoveCategory::brilliant,
         "A near-only tactical best move may classify Brilliant");
  expect(classified({.played_is_best = true, .best_expected_score = .80,
                     .legal_move_count = 12,
                     .played_expected_score = .80,
                     .second_best_expected_score = .50}) == MoveCategory::best,
         "A MultiPV gap without a tactical signal must remain Best");
  expect(classified({.allowed_forced_mate = true, .best_expected_score = .5,
                     .played_expected_score = .1}) == MoveCategory::blunder,
         "Allowing forced mate must classify Blunder");
  expect(classified({.missed_forced_mate = true, .best_expected_score = .8,
                     .played_expected_score = .7}) == MoveCategory::miss,
         "Missing forced mate without collapse must classify Miss");

  const kchess::PositionEvaluation white_wdl{
      .wdl = kchess::WdlScore{.wins = 700, .draws = 200, .losses = 100}};
  expect(std::abs(*kchess::expected_score_side_to_move(white_wdl) - .8) < 1e-9,
         "White/side-to-move WDL expected score must be normalized");
  expect(std::abs(*kchess::expected_score_mover_after_move(white_wdl) - .2) < 1e-9,
         "Post-move WDL must invert to the mover perspective, including Black moves");
  expect(*kchess::expected_score_side_to_move({.mate_in = 4}) == 1.0
             && *kchess::expected_score_mover_after_move({.mate_in = 4}) == 0.0,
         "Mate scores must use explicit perspective inversion");
  const auto cp = kchess::expected_score_side_to_move({.evaluation_cp = 200});
  expect(cp.has_value() && *cp > .5 && *cp < 1.0, "CP fallback must be bounded");

  const auto perfect = kchess::game_accuracy({{MoveCategory::best, 0.0},
                                               {MoveCategory::excellent, .005}});
  const auto small = kchess::game_accuracy({{MoveCategory::excellent, .02},
                                             {MoveCategory::okay, .04}});
  const auto blunder = kchess::game_accuracy({{MoveCategory::best, 0.0},
                                               {MoveCategory::blunder, .4}});
  expect(perfect.has_value() && *perfect > 98.0, "Perfect play must have high Accuracy");
  expect(small.has_value() && blunder.has_value() && *small > *blunder,
         "Several small losses must score above a game with a Blunder");
  const auto theory_only = kchess::game_accuracy({{MoveCategory::theory, std::nullopt}});
  expect(theory_only == 100.0, "All-Theory games use the documented 100 fallback");
  expect(!kchess::game_accuracy({}).has_value(), "Empty games must not divide by zero");
}

void test_fen_validation() {
  const auto start = kchess::validate_fen(kchess::kStartFen);
  expect(start.valid && start.normalized == kchess::kStartFen, "Start FEN must validate");
  expect(kchess::validate_fen("8/8/8/8/8/8/8/8 w - - 0 1").valid == false,
         "FEN without kings must fail");
  expect(kchess::validate_fen("4k3/8/8/8/8/8/8/4K3 x - - 0 1").valid == false,
         "Invalid active color must fail");
  expect(kchess::validate_fen("4k3/8/8/8/8/8/8/4K3 w K - 0 1").valid == false,
         "Impossible castling right must fail");
  expect(kchess::validate_fen("4k3/8/8/8/8/8/8/P3K3 w - - 0 1").valid == false,
         "Pawn on first rank must fail");
  expect(kchess::validate_fen("8/8/8/8/8/8/4k3/4K3 w - - 0 1").valid == false,
         "Adjacent kings must fail");
}

void test_pgn_parser() {
  const std::string pgn = R"pgn([Event "Parser"]
[Site "Local"]
[Date "2026.08.17"]
[Round "1"]
[White "Ada"]
[Black "Turing"]
[Result "*"]

1. e4 {king pawn} e5 $1 2. Nf3 (2. Bc4 Nc6) Nc6
3. Bb5 a6 4. Ba4 Nf6 5. O-O Be7 *)pgn";
  const auto parsed = kchess::parse_pgn(pgn);
  expect(parsed.valid, "Tagged PGN with comments, NAG and RAV must parse: " + parsed.error);
  expect(parsed.game.tags.at("White") == "Ada", "PGN tags must be preserved");
  expect(parsed.game.moves.size() == 10, "Only the PGN main line must become analyzed plies");
  expect(parsed.game.moves[0].san == "e4" && parsed.game.moves[0].uci == "e2e4",
         "SAN must resolve to UCI");
  expect(parsed.game.moves[8].uci == "e1g1", "Castling SAN must resolve correctly");
  expect(!parsed.game.moves[0].fen_before.empty()
             && !parsed.game.moves[0].fen_after.empty(),
         "Every ply must retain before/after FEN");
  expect(parsed.game.comments.size() == 1, "Brace comments must be retained");
  expect(parsed.game.nags.size() == 1, "NAGs must be retained");
  expect(parsed.game.variations.size() == 1, "Nested movetext must be retained as variation");

  const auto en_passant = kchess::parse_pgn(R"pgn([SetUp "1"]
[FEN "4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1"]
[Result "*"]

1. exd6 Kd7 *)pgn");
  expect(en_passant.valid && en_passant.game.moves[0].uci == "e5d6",
         "En-passant SAN must parse");

  const auto promotion = kchess::parse_pgn(R"pgn([SetUp "1"]
[FEN "4k3/P7/8/8/8/8/8/4K3 w - - 0 1"]
[Result "*"]

1. a8=Q+ *)pgn");
  expect(promotion.valid && promotion.game.moves[0].uci == "a7a8q",
         "Promotion with check suffix must parse");

  const auto mate = kchess::parse_pgn(
      "1. e4 e5 2. Bc4 Nc6 3. Qh5 Nf6 4. Qxf7# 1-0");
  expect(mate.valid && mate.game.moves.back().san == "Qxf7#",
         "Checkmate SAN must parse");

  const auto disambiguation = kchess::parse_pgn(R"pgn([SetUp "1"]
[FEN "4k3/8/8/8/8/8/3N3N/4K3 w - - 0 1"]

1. Ndf3 *)pgn");
  expect(disambiguation.valid && disambiguation.game.moves[0].uci == "d2f3",
         "Disambiguated SAN must parse");
  expect(!kchess::parse_pgn("1. e4 e5 2. Illegal *").valid,
         "Illegal SAN must fail with an import error");
}

kchess::AnalysisResult test_stockfish() {
  kchess::StockfishEngine engine;
  engine.start();
  expect(engine.is_ready(), "Stockfish must reach ready state");
  engine.new_game();
  auto result = engine.analyze({
      .fen = kchess::kStartFen,
      .depth = 4,
      .multi_pv = 3,
      .threads = 1,
      .hash_mb = 16,
  });
  expect(result.lines.size() == 3, "Stockfish MultiPV=3 must return three lines");
  expect(!result.best_move.empty() && result.reached_depth >= 4,
         "Stockfish must return a real best move and reached depth");
  expect(result.lines.front().evaluation_cp.has_value()
             || result.lines.front().mate_in.has_value(),
         "Stockfish must return centipawn or mate evaluation");
  expect(result.lines.front().wdl.has_value(), "Stockfish must return WDL data");
  engine.stop();
  expect(!engine.is_ready(), "Stockfish lifecycle stop must release the engine");
  return result;
}

void test_database_cache(const kchess::AnalysisResult& engine_result) {
  const auto directory =
      std::filesystem::temp_directory_path() / "kchess_phase3_database_tests";
  std::filesystem::remove_all(directory);
  std::filesystem::create_directories(directory);
  std::string game_id;
  {
    kchess::Database database(directory);
    database.open_and_migrate();
    const auto profile = database.create_profile(
        kchess::ProfileType::local_pgn_fen, "Local", std::nullopt,
        "profile_unknown.png");
    const auto parsed = kchess::parse_pgn("1. e4 e5 *");
    expect(parsed.valid, "Persistence fixture PGN must parse");
    game_id = database.import_pgn(profile.id, parsed.game);
    const auto run = database.prepare_analysis(
        game_id, "test-config", "Stockfish 18 test", 2, 4, 3);
    expect(run.status == "running" && run.completed_plies == 0,
           "New analysis run must start empty");
    database.persist_engine_result(game_id, "test-config", 0, engine_result, 1234);
    const auto partial = database.analysis(game_id, "test-config");
    expect(partial.has_value() && partial->completed_plies == 1
               && partial->lines.size() == 3,
           "Per-ply MultiPV results must be persisted incrementally");
    database.set_analysis_status(game_id, "test-config", "cancelled");
  }
  {
    kchess::Database database(directory);
    database.open_and_migrate();
    const auto resumed = database.prepare_analysis(
        game_id, "test-config", "Stockfish 18 test", 2, 4, 3);
    expect(resumed.status == "running" && resumed.completed_plies == 1,
           "A compatible interrupted analysis must resume after restart");
    database.persist_engine_result(game_id, "test-config", 1, engine_result, 1235);
    const std::vector<kchess::MoveClassificationRecord> classifications{
        {.ply = 0, .classification = kchess::MoveCategory::theory,
         .classifier_version = 1, .theory = {.is_theory = true, .games = 100,
          .white_wins = 40, .draws = 30, .black_wins = 30}},
        {.ply = 1, .classification = kchess::MoveCategory::best,
         .classifier_version = 1, .expected_score_before = .6,
         .expected_score_best = .6, .expected_score_played = .6,
         .expected_score_loss = 0.0, .recommended_move = "e7e5"},
    };
    database.persist_classifications(
        game_id, "test-config", classifications, 100.0, 100.0, 1, 1, "kcb1:test");
    expect(database.classification_is_current(
               game_id, "test-config", 1, 1, "kcb1:test"),
           "Classifier/Accuracy/Book cache versions must be independent of engine data");
    expect(!database.classification_is_current(
               game_id, "test-config", 1, 1, "kcb1:expanded"),
           "A changed Book must invalidate only classification compatibility");
    database.set_analysis_status(game_id, "test-config", "complete");
    const auto complete = database.analysis(game_id, "test-config", 0);
    expect(complete.has_value() && complete->status == "complete"
               && complete->completed_plies == 2 && complete->latest_ply == 0
               && complete->classification == kchess::MoveCategory::theory
               && complete->theory->games == 100,
           "Completed engine cache and rebuilt classification must survive restart");
  }
  std::filesystem::remove_all(directory);
}

void test_c_api_import_and_cancel() {
  const auto directory = std::filesystem::temp_directory_path() / "kchess_phase3_api_tests";
  std::filesystem::remove_all(directory);
  std::filesystem::create_directories(directory);
  std::filesystem::copy_file(
      bundled_book_path(), directory / "opening_book.kcb",
      std::filesystem::copy_options::overwrite_existing);
  const auto short_fixture = kchess::parse_pgn("1. e4 e5 2. Ke2 Ke7 *");
  const kchess::KcbOpeningTheoryProvider fixture_book(bundled_book_path());
  expect(short_fixture.valid && short_fixture.game.moves.size() == 4
             && !fixture_book.lookup(
                    short_fixture.game.moves.back().fen_before,
                    short_fixture.game.moves.back().uci).is_theory,
         "Production analysis fixture must end with a non-Book move");
  auto* core = kc_core_create(directory.string().c_str());
  expect(core != nullptr && kc_core_initialize(core) == KC_STATUS_OK,
         "C API core initialization must succeed");
  const auto profile = take_string(kc_create_profile_json(core, 2, "Local", ""));
  expect(profile.find("\"displayName\":\"Local\"") != std::string::npos,
         "Local profile must be returned");
  expect(kc_import_pgn_json(core, "1. e4 e5 2. Illegal *") == nullptr,
         "Invalid PGN must fail at the C boundary");
  const auto imported = take_string(kc_import_pgn_json(
      core,
      "[White \"Ada\"]\n[Black \"Turing\"]\n\n"
      "1. e4 e5 2. Ke2 Ke7 *"));
  const auto game_id = json_string_field(imported, "id");
  const auto detail = take_string(kc_game_json(core, game_id.c_str()));
  expect(detail.find("\"uci\":\"e2e4\"") != std::string::npos
             && detail.find("\"uci\":\"e1e2\"") != std::string::npos,
         "Imported game detail must expose native per-ply state");
  auto full_analysis = take_string(kc_start_analysis_json(core, game_id.c_str()));
  for (int attempt = 0;
       full_analysis.find("\"status\":\"running\"") != std::string::npos
           && attempt < 6000;
       ++attempt) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    full_analysis = take_string(kc_analysis_status_json(core, game_id.c_str()));
  }
  expect(full_analysis.find("\"status\":\"complete\"") != std::string::npos,
         "A complete short PGN must be analyzed by the production Stockfish job");
  expect(full_analysis.find("\"lines\":[{") != std::string::npos
             && !json_string_field(full_analysis, "bestMove").empty(),
         "Production analysis must expose persisted MultiPV and best move");
  const auto opening_move = take_string(
      kc_move_analysis_status_json(core, game_id.c_str(), 0));
  expect(opening_move.find("\"classification\":\"theory\"") != std::string::npos
             && opening_move.find("\"theory\":{") != std::string::npos,
         "Production analysis must recognize the known opening move offline");
  expect(full_analysis.find("\"classification\":\"theory\"") == std::string::npos
             && full_analysis.find("\"classification\":\"unknown\"") == std::string::npos
             && full_analysis.find("\"localAccuracy\":null") == std::string::npos,
         "The complete PGN must classify a non-Book move and calculate Accuracy");
  const auto fen_game = take_string(kc_import_fen_json(
      core, kchess::kStartFen, "Start position"));
  const auto fen_game_id = json_string_field(fen_game, "id");
  const auto started = take_string(kc_start_analysis_json(core, fen_game_id.c_str()));
  expect(started.find("\"status\":\"running\"") != std::string::npos,
         "Analysis start must be asynchronous");
  expect(kc_cancel_analysis(core, fen_game_id.c_str()) == KC_STATUS_OK,
         "Running Stockfish analysis must be cancellable");
  kc_core_destroy(core);

  core = kc_core_create(directory.string().c_str());
  expect(core != nullptr && kc_core_initialize(core) == KC_STATUS_OK,
         "Core must reopen after an interrupted engine job");
  const auto active = take_string(kc_active_profile_json(core));
  expect(active.find("\"displayName\":\"Local\"") != std::string::npos,
         "Active profile must survive restart");
  const auto games = take_string(kc_games_json(core));
  expect(games.find("\"isFixture\":true") == std::string::npos,
         "Phase-1 fixture games must not be recreated");
  const auto cached = take_string(kc_start_analysis_json(core, game_id.c_str()));
  expect(cached.find("\"status\":\"complete\"") != std::string::npos,
         "Compatible complete analysis must load from cache after restart");
  kc_core_destroy(core);
  std::filesystem::remove_all(directory);
}

}  // namespace

int main() {
  try {
    test_smoke();
    test_opening_book();
    test_classifier_and_accuracy();
    test_fen_validation();
    test_pgn_parser();
    const auto engine_result = test_stockfish();
    test_database_cache(engine_result);
    test_c_api_import_and_cancel();
    std::cout << "All KChess phase-3.1 native tests passed.\n";
    return EXIT_SUCCESS;
  } catch (const std::exception& error) {
    std::cerr << "Native test failed: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
