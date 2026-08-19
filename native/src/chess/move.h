#pragma once

#include <string>

namespace kchess {

struct AppliedMove {
  std::string uci;
  std::string san;
  std::string fen_after;
};

// Applies one legal UCI move to a FEN without mutating any stored game.
// Throws std::invalid_argument when the position or move is invalid/illegal.
AppliedMove apply_legal_uci_move(const std::string& fen, const std::string& uci);

}  // namespace kchess
