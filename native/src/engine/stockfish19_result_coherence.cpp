#include "engine/stockfish19_result_coherence.h"

#include <algorithm>

namespace kchess {

bool usable_stockfish19_engine_move(const std::string& move) {
  return !move.empty() && move != "(none)" && move != "0000";
}

Stockfish19ExactSnapshotAccumulator::Stockfish19ExactSnapshotAccumulator(
    const std::size_t requested_line_count)
    : requested_line_count_(std::max<std::size_t>(1, requested_line_count)) {
  candidate_.reserve(requested_line_count_);
}

void Stockfish19ExactSnapshotAccumulator::observe(
    const EngineLine& line, const bool exact) {
  if (line.depth <= 0 || line.rank <= 0
      || static_cast<std::size_t>(line.rank) > requested_line_count_) {
    return;
  }

  // Upstream SF19 emits one MultiPV report as rank 1, 2, ... N. Use that
  // sequence as the iteration boundary instead of grouping only by depth;
  // aspiration retries can emit multiple reports at the same depth.
  if (line.rank == 1) {
    reset_candidate();
    candidate_depth_ = line.depth;
  }

  const auto& move = line.best_move();
  const bool duplicate_move = std::any_of(
      candidate_.begin(), candidate_.end(), [&](const EngineLine& previous) {
        return previous.best_move() == move;
      });
  if (!exact || candidate_depth_ != line.depth || line.rank != next_rank_
      || !usable_stockfish19_engine_move(move) || duplicate_move) {
    reset_candidate();
    return;
  }

  candidate_.push_back(line);
  ++next_rank_;
  if (candidate_.size() != requested_line_count_) return;

  if (candidate_depth_ >= latest_depth_) {
    latest_complete_ = candidate_;
    latest_depth_ = candidate_depth_;
  }
  reset_candidate();
}

const std::vector<EngineLine>&
Stockfish19ExactSnapshotAccumulator::latest_complete() const noexcept {
  return latest_complete_;
}

int Stockfish19ExactSnapshotAccumulator::latest_depth() const noexcept {
  return latest_depth_;
}

void Stockfish19ExactSnapshotAccumulator::reset_candidate() noexcept {
  candidate_.clear();
  candidate_depth_ = 0;
  next_rank_ = 1;
}


}  // namespace kchess
