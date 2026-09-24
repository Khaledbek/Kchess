#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

#include "engine/stockfish19_result_coherence.h"

namespace {

// -----------------------------------------------------------------------------
// Section: Stockfish 19 result-coherence regressions
// -----------------------------------------------------------------------------

using kchess::EngineLine;

int assertions = 0;

void expect(const bool condition, const std::string& message) {
  ++assertions;
  if (!condition) throw std::runtime_error(message);
}

EngineLine line(const int rank, const int depth, const int cp, const std::string& move) {
  EngineLine result;
  result.rank = rank;
  result.depth = depth;
  result.evaluation_cp = cp;
  result.moves = {move};
  return result;
}

}  // namespace

int main() {
  try {
    // Regression model for the supplied PGN: the board arrow renders rank 1,
    // while SF19's final bestmove can arrive after a late MultiPV rank reshuffle.
    // KChess must never classify that visible engine recommendation as Okay or
    // Excellent merely because bestmove and the captured rank slots disagree.
    {
      kchess::Stockfish19ExactSnapshotAccumulator snapshots(2);
      snapshots.observe(line(1, 16, 20, "e2e4"), true);
      snapshots.observe(line(2, 16, 15, "d2d4"), true);
      snapshots.observe(line(1, 17, 28, "d2d4"), true);

      expect(snapshots.latest_depth() == 16,
             "incomplete newer depth does not replace complete preparation snapshot");
      expect(snapshots.latest_complete().front().best_move() == "e2e4",
             "preparation snapshot keeps rank 1 from one complete iteration");

      snapshots.observe(line(2, 17, 24, "e2e4"), true);
      expect(snapshots.latest_depth() == 17,
             "complete exact newer iteration replaces older preparation snapshot");
      expect(snapshots.latest_complete().front().best_move() == "d2d4",
             "new exact rank 1 becomes preparation best move");
    }

    {
      kchess::Stockfish19ExactSnapshotAccumulator snapshots(2);
      snapshots.observe(line(1, 18, 40, "c2c4"), true);
      snapshots.observe(line(2, 18, 35, "g1f3"), true);
      expect(snapshots.latest_depth() == 18, "exact iteration is accepted");

      snapshots.observe(line(1, 19, 55, "d2d4"), false);
      snapshots.observe(line(2, 19, 45, "c2c4"), true);
      expect(snapshots.latest_depth() == 18,
             "inexact newer MultiPV report cannot replace exact preparation snapshot");

      snapshots.observe(line(1, 20, 60, "g1f3"), true);
      snapshots.observe(line(1, 20, 62, "d2d4"), true);
      snapshots.observe(line(2, 20, 52, "g1f3"), true);
      expect(snapshots.latest_depth() == 20,
             "a restarted same-depth report is captured as its own iteration");
      expect(snapshots.latest_complete().front().best_move() == "d2d4",
             "ranks from separate same-depth reports are never mixed");
    }

    std::cout << "SF19 result coherence tests passed (" << assertions
              << " assertions).\n";
    return EXIT_SUCCESS;
  } catch (const std::exception& error) {
    std::cerr << "SF19 result coherence test failed after " << assertions
              << " assertions: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
