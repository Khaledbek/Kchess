#pragma once

#include <map>
#include <string>
#include <vector>

#include "engine/chess_engine.h"

namespace kchess {

// -----------------------------------------------------------------------------
// Section: Stockfish 19 result coherence
// -----------------------------------------------------------------------------

bool usable_stockfish19_engine_move(const std::string& move);

// Reconciles SF19's final bestmove callback with the latest MultiPV callbacks.
// The returned list keeps at most the current requested line count and, whenever
// a PV for final_best_move was observed, guarantees:
//   result[0].best_move() == final_best_move && result[0].rank == 1.
std::vector<EngineLine> coherent_stockfish19_ranked_lines(
    const std::map<int, EngineLine>& ranked_lines,
    const std::map<std::string, EngineLine>& latest_lines_by_move,
    const std::string& final_best_move);

}  // namespace kchess
