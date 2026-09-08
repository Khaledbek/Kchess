#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "chess/move.h"
#include "chess/position_view.h"
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

struct Exercise {
  const char* id;
  const char* fen;
  std::vector<std::string> line;
};

// Mirrors TrainingLibrary in flutter_app/lib/features/training/data/. Each line
// alternates solver move / opponent reply from the exercise's starting FEN,
// which is exactly how the training player replays it: it looks the SAN up in
// legal_moves() and follows fen_after. Replaying them here against the real
// move generator is what proves the hand-authored SAN is legal and unambiguous.
const std::vector<Exercise>& seeded_endgames() {
  static const std::vector<Exercise> exercises{
      {"endgame_opposition", "8/4k3/8/3K4/4P3/8/8/8 w - - 0 1",
       {"Ke5", "Kd7", "Kf6", "Kd6", "e5+", "Kd5", "e6", "Kd6", "e7", "Kd7", "Kf7"}},
      {"endgame_lucena", "3K4/3P1k2/8/8/8/8/8/r3R3 w - - 0 1",
       {"Re4", "Ra2", "Kc7", "Rc2+", "Kb6", "Rb2+", "Kc6", "Rc2+", "Kb5", "Rb2+", "Rb4"}},
      {"endgame_philidor", "8/4k3/r7/3KP3/8/8/8/7R b - - 0 1",
       {"Rb6", "e6", "Rb1", "Ke5", "Re1+"}},
  };
  return exercises;
}

void test_seeded_lines_are_playable() {
  for (const auto& exercise : seeded_endgames()) {
    std::string fen = exercise.fen;
    for (std::size_t ply = 0; ply < exercise.line.size(); ++ply) {
      const auto moves = kchess::legal_moves(fen);
      expect(!moves.empty(),
             std::string(exercise.id) + " has no legal move at ply " + std::to_string(ply));
      const auto match = std::find_if(
          moves.begin(), moves.end(),
          [&](const kchess::AppliedMove& move) { return move.san == exercise.line[ply]; });
      expect(match != moves.end(),
             std::string(exercise.id) + " ply " + std::to_string(ply) + " ("
                 + exercise.line[ply] + ") is not legal in " + fen);
      // SAN must be unique, or the player could not tell which move was meant.
      expect(std::count_if(moves.begin(), moves.end(),
                           [&](const kchess::AppliedMove& move) {
                             return move.san == exercise.line[ply];
                           }) == 1,
             std::string(exercise.id) + " ply " + std::to_string(ply) + " ("
                 + exercise.line[ply] + ") is ambiguous");
      fen = match->fen_after;
    }
  }
}

// The three seeded drills are played out against the engine rather than
// replayed from a script, so what has to hold is that each start position is
// legal, playable, and belongs to the side the drill expects to move.
void test_seeded_drills_are_playable() {
  struct Drill {
    const char* id;
    const char* fen;
  };
  const std::vector<Drill> drills{
      {"endgame_kq_vs_k", "4k3/8/8/8/8/8/8/4KQ2 w - - 0 1"},
      {"endgame_kr_vs_k", "4k3/8/8/8/8/8/8/4KR2 w - - 0 1"},
      {"endgame_q_vs_r", "8/8/8/8/8/2k5/1r6/2K1Q3 w - - 0 1"},
  };
  for (const auto& drill : drills) {
    const auto moves = kchess::legal_moves(drill.fen);
    expect(!moves.empty(), std::string(drill.id) + " must have legal moves");
    expect(!kchess::in_check(drill.fen),
           std::string(drill.id) + " must not start with the attacker in check");
  }
}

// Checkmate and stalemate are indistinguishable from an empty move list alone,
// which is exactly the bug that would let a drill celebrate a stalemate.
void test_terminal_positions() {
  // Back-rank mate: black king h8, white rook a8, black to move.
  const std::string mate = "R6k/6pp/8/8/8/8/8/6K1 b - - 0 1";
  expect(kchess::legal_moves(mate).empty(), "Checkmate must leave no legal move");
  expect(kchess::in_check(mate), "Checkmate must report check");

  // Stalemate: black king a8 boxed in by queen c7, black to move, not in check.
  const std::string stalemate = "k7/2Q5/8/8/8/8/8/6K1 b - - 0 1";
  expect(kchess::legal_moves(stalemate).empty(),
         "Stalemate must leave no legal move");
  expect(!kchess::in_check(stalemate), "Stalemate must not report check");

  // And the drill start positions are neither.
  expect(!kchess::legal_moves("4k3/8/8/8/8/8/8/4KQ2 w - - 0 1").empty(),
         "A playable position must offer moves");
}

void test_legal_moves_contract() {
  const auto moves = kchess::legal_moves("8/4k3/8/3K4/4P3/8/8/8 w - - 0 1");
  expect(std::any_of(moves.begin(), moves.end(),
                     [](const kchess::AppliedMove& move) { return move.uci == "d5e5"; }),
         "legal_moves must offer the opposition move d5e5");
  expect(std::none_of(moves.begin(), moves.end(),
                      [](const kchess::AppliedMove& move) {
                        return move.uci.rfind("d5a8", 0) == 0;
                      }),
         "legal_moves must not offer an illegal king move");
  // Every option must carry a position the caller can render next.
  expect(std::none_of(moves.begin(), moves.end(),
                      [](const kchess::AppliedMove& move) {
                        return move.san.empty() || move.fen_after.empty();
                      }),
         "Every legal move needs a SAN and a resulting FEN");

  bool threw = false;
  try {
    kchess::legal_moves("not a fen");
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  expect(threw, "legal_moves must reject an invalid FEN");
}

void test_c_exports() {
  const auto directory =
      std::filesystem::temp_directory_path() / "kchess_training_board_tests";
  std::filesystem::remove_all(directory);
  std::filesystem::create_directories(directory);
  auto* core = kc_core_create(directory.string().c_str());
  expect(core != nullptr, "Core handle must be created");

  // Both exports are gameless, so neither needs an initialized library.
  const auto board =
      take_string(kc_board_position_json(core, "8/4k3/8/3K4/4P3/8/8/8 w - - 0 1"));
  expect(board.find("\"sideToMove\":\"white\"") != std::string::npos,
         "kc_board_position_json must report the side to move");
  expect(board.find("\"pieces\":") != std::string::npos,
         "kc_board_position_json must return the piece list");

  // The status the drill player branches on must cross the ABI.
  expect(board.find("\"status\":\"playable\"") != std::string::npos,
         "kc_board_position_json must report a playable status");
  const auto mated = take_string(
      kc_board_position_json(core, "R6k/6pp/8/8/8/8/8/6K1 b - - 0 1"));
  expect(mated.find("\"status\":\"checkmate\"") != std::string::npos
             && mated.find("\"inCheck\":true") != std::string::npos,
         "kc_board_position_json must report checkmate");
  const auto stalemated = take_string(
      kc_board_position_json(core, "k7/2Q5/8/8/8/8/8/6K1 b - - 0 1"));
  expect(stalemated.find("\"status\":\"stalemate\"") != std::string::npos
             && stalemated.find("\"legalMoveCount\":0") != std::string::npos,
         "kc_board_position_json must report stalemate");

  const auto listed =
      take_string(kc_board_legal_moves_json(core, "8/4k3/8/3K4/4P3/8/8/8 w - - 0 1"));
  expect(listed.find("\"san\":\"Ke5\"") != std::string::npos,
         "kc_board_legal_moves_json must list the opposition move");
  expect(kc_board_legal_moves_json(core, "not a fen") == nullptr,
         "An invalid FEN must fail at the C boundary");
  expect(kc_board_position_json(core, nullptr) == nullptr,
         "A null FEN must fail at the C boundary");

  kc_core_destroy(core);
  std::filesystem::remove_all(directory);
}

}  // namespace

int main() {
  try {
    test_seeded_lines_are_playable();
    test_seeded_drills_are_playable();
    test_terminal_positions();
    test_legal_moves_contract();
    test_c_exports();
    std::cout << "All KChess training board tests passed.\n";
    return EXIT_SUCCESS;
  } catch (const std::exception& error) {
    std::cerr << "Native test failed: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
