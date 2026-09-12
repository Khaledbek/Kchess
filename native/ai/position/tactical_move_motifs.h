#pragma once

#include <vector>

#include "position.h"
#include "tactical_detector.h"

namespace kchess::ai::detail {

// -----------------------------------------------------------------------------
// Section: Legal-move advanced motifs
// -----------------------------------------------------------------------------

std::vector<TacticalMotif> detect_move_motifs(
    const Stockfish::Position& position);

}  // namespace kchess::ai::detail
