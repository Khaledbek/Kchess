#pragma once

#include <string>
#include <vector>

namespace kchess {

struct AppliedMove {
  std::string uci;
  std::string san;
  std::string fen_after;
};

// Applies one legal UCI move to a FEN without mutating any stored game.
// Throws std::invalid_argument when the position or move is invalid/illegal.
AppliedMove apply_legal_uci_move(const std::string& fen, const std::string& uci);

// Every legal move in the position, each with its SAN and the FEN it reaches.
// Lets a caller resolve a board drag or a scripted SAN move without owning any
// chess rules itself. Throws std::invalid_argument on an invalid FEN.
std::vector<AppliedMove> legal_moves(const std::string& fen);

// Whether the side to move is in check. Combined with an empty legal_moves()
// this is what separates checkmate from stalemate.
bool in_check(const std::string& fen);

}  // namespace kchess
