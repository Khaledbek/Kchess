#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <vector>

#include "engine/chess_engine.h"

namespace kchess {

// -----------------------------------------------------------------------------
// Section: Stockfish 19 result coherence
// -----------------------------------------------------------------------------

bool usable_stockfish19_engine_move(const std::string& move);


class Stockfish19ExactSnapshotAccumulator {
 public:
  explicit Stockfish19ExactSnapshotAccumulator(std::size_t requested_line_count);

  void observe(const EngineLine& line, bool exact);
  const std::vector<EngineLine>& latest_complete() const noexcept;
  int latest_depth() const noexcept;

 private:
  void reset_candidate() noexcept;

  std::size_t requested_line_count_{1};
  std::vector<EngineLine> candidate_;
  int candidate_depth_{0};
  int next_rank_{1};
  std::vector<EngineLine> latest_complete_;
  int latest_depth_{0};
};

// Reconciles SF19's final bestmove callback with the latest MultiPV callbacks.
// The returned list keeps at most the current requested line count and, whenever
// a PV for final_best_move was observed, guarantees:
//   result[0].best_move() == final_best_move && result[0].rank == 1.
std::vector<EngineLine> coherent_stockfish19_ranked_lines(
    const std::map<int, EngineLine>& ranked_lines,
    const std::map<std::string, EngineLine>& latest_lines_by_move,
    const std::string& final_best_move);

}  // namespace kchess
