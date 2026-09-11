#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

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

// Mirrors OpeningRepertoireLibrary in flutter_app/lib/features/training/data/.
// Unlike the endgame exercises these start from the initial position and run
// White-move-first regardless of which side the user trains, because the lab
// hands the odd plies to the computer when the line is a Black repertoire.
struct OpeningLine {
  const char* id;
  std::vector<std::string> moves;
};

const std::vector<OpeningLine>& seeded_openings() {
  static const std::vector<OpeningLine> lines{
      {"opening_italian_giuoco_piano",
       {"e4", "e5", "Nf3", "Nc6", "Bc4", "Bc5", "c3", "Nf6", "d3", "d6", "O-O", "O-O"}},
      {"opening_caro_kann_advance",
       {"e4", "c6", "d4", "d5", "e5", "Bf5", "Nf3", "e6", "Be2", "c5", "Be3", "Nd7"}},
      {"opening_qgd_exchange",
       {"d4", "d5", "c4", "e6", "Nc3", "Nf6", "cxd5", "exd5", "Bg5", "Be7", "e3", "c6"}},
  };
  return lines;
}

void test_seeded_openings_are_playable() {
  const std::string start =
      "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
  for (const auto& line : seeded_openings()) {
    std::string fen = start;
    for (std::size_t ply = 0; ply < line.moves.size(); ++ply) {
      const auto moves = kchess::legal_moves(fen);
      const auto match = std::find_if(
          moves.begin(), moves.end(),
          [&](const kchess::AppliedMove& move) { return move.san == line.moves[ply]; });
      expect(match != moves.end(),
             std::string(line.id) + " ply " + std::to_string(ply) + " ("
                 + line.moves[ply] + ") is not legal in " + fen);
      // The lab picks the move by SAN, so a duplicate SAN would make the
      // target move ambiguous and the drill unwinnable.
      expect(std::count_if(moves.begin(), moves.end(),
                           [&](const kchess::AppliedMove& move) {
                             return move.san == line.moves[ply];
                           }) == 1,
             std::string(line.id) + " ply " + std::to_string(ply) + " ("
                 + line.moves[ply] + ") is ambiguous");
      fen = match->fen_after;
    }
    // The line has to leave a real position behind: the lab shows the final
    // board under the completion overlay.
    expect(!kchess::legal_moves(fen).empty(),
           std::string(line.id) + " must end on a playable position");
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

// Every study shipped in flutter_app/assets/endgame_studies.json comes from
// Kling & Horwitz (1851). They are transcribed data, so the one thing that can
// silently ruin them is a malformed or already-finished position: the trainer
// would open on a board with nothing to play. Validate the shipped file itself
// rather than a copy, so the asset is the single source of truth.
void test_shipped_studies_are_playable() {
  const auto asset = std::filesystem::path(__FILE__)
                         .parent_path()
                         .parent_path()
                         .parent_path()
                     / "flutter_app" / "assets" / "endgame_studies.json";
  std::ifstream input(asset);
  expect(input.good(), "Study asset must exist at " + asset.string());
  const auto payload = nlohmann::json::parse(input);

  std::size_t studies = 0;
  std::vector<std::string> seen_ids;
  for (const auto& section : payload.at("sections")) {
    const auto section_id = section.at("id").get<std::string>();
    for (const auto& study : section.at("studies")) {
      const auto id = study.at("id").get<std::string>();
      const auto fen = study.at("fen").get<std::string>();
      const auto solver = study.at("solverColor").get<std::string>();
      ++studies;

      // 1. Legal, parseable position.
      std::vector<kchess::AppliedMove> moves;
      try {
        moves = kchess::legal_moves(fen);
      } catch (const std::exception& error) {
        expect(false, id + " has an invalid FEN (" + fen + "): " + error.what());
      }

      // 2. Something to actually play.
      expect(!moves.empty(), id + " starts with no legal move: " + fen);

      // 3. The solver must be the side to move, or the study opens on the
      //    opponent's turn and the trainer would move for them.
      const auto board = kchess::position_view_json(fen);
      const std::string expected = "\"sideToMove\":\"" + solver + "\"";
      expect(board.find(expected) != std::string::npos,
             id + " expects " + solver + " to move but the FEN disagrees: " + fen);

      expect(std::find(seen_ids.begin(), seen_ids.end(), id) == seen_ids.end(),
             "Duplicate study id " + id);
      seen_ids.push_back(id);
      expect(id.rfind(section_id, 0) == 0,
             id + " does not belong to section " + section_id);
    }
  }
  expect(studies > 0, "The study asset must not be empty");
  std::cout << "  validated " << studies << " shipped studies" << '\n';
}

// The screenshot bug: a lone white king facing a black pawn was reported as a
// dead draw because the check only looked at the solver's own pieces.
void test_insufficient_material() {
  const auto drawn = [](const char* fen) {
    return kchess::insufficient_mating_material(fen);
  };

  // Nobody can mate.
  expect(drawn("8/8/4k3/8/8/4K3/8/8 w - - 0 1"), "K vs K is a dead draw");
  expect(drawn("8/8/4k3/8/8/4K3/8/5B2 w - - 0 1"), "K+B vs K is a dead draw");
  expect(drawn("8/8/4k3/8/8/4K3/8/5N2 w - - 0 1"), "K+N vs K is a dead draw");
  expect(drawn("8/5b2/4k3/8/8/4K3/8/5B2 w - - 0 1"),
         "K+B vs K+B is a dead draw");

  // Someone can still mate — these must keep playing.
  expect(!drawn("8/1p6/1k6/8/8/2K5/8/8 w - - 0 1"),
         "A lone king facing a pawn is not a draw");
  expect(!drawn("8/8/4k3/8/8/4K3/8/5R2 w - - 0 1"), "A rook can mate");
  expect(!drawn("8/8/4k3/8/8/4K3/8/5Q2 w - - 0 1"), "A queen can mate");
  expect(!drawn("8/8/4k3/8/8/4K3/8/4BB2 w - - 0 1"), "Two bishops can mate");
  expect(!drawn("8/8/4k3/8/8/4K3/8/4BN2 w - - 0 1"),
         "Bishop and knight can mate");
  expect(!drawn("8/8/4k3/8/8/4K3/8/4bn2 w - - 0 1"),
         "The rule is side-agnostic");
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
    test_seeded_openings_are_playable();
    test_seeded_drills_are_playable();
    test_terminal_positions();
    test_insufficient_material();
    test_shipped_studies_are_playable();
    test_legal_moves_contract();
    test_c_exports();
    std::cout << "All KChess training board tests passed.\n";
    return EXIT_SUCCESS;
  } catch (const std::exception& error) {
    std::cerr << "Native test failed: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
