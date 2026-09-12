#pragma once

#include <vector>

#include "position.h"
#include "tactical_detector.h"

namespace kchess::ai::detail {

// -----------------------------------------------------------------------------
// Section: Sliding-piece motif extraction
// -----------------------------------------------------------------------------

[[nodiscard]] std::vector<TacticalMotif> detect_line_motifs(
    const Stockfish::Position& position, Stockfish::Color attacker);

}  // namespace kchess::ai::detail
