#pragma once

#include <vector>

#include "position.h"
#include "tactical_detector.h"

namespace kchess::ai::detail {

// -----------------------------------------------------------------------------
// Section: Static advanced motifs
// -----------------------------------------------------------------------------

std::vector<TacticalMotif> detect_pattern_motifs(
    const Stockfish::Position& position);

}  // namespace kchess::ai::detail
