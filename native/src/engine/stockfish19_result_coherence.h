#pragma once

#include <cstddef>
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



}  // namespace kchess
