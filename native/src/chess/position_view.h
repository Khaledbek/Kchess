#pragma once

#include <string>

namespace kchess {

// Produces the normalized, presentation-ready board DTO consumed by Flutter.
// FEN parsing and side-to-move interpretation deliberately stay in C++.
std::string position_view_json(const std::string& fen);

// Compares the four rule-relevant FEN position fields (placement, side,
// castling and en-passant), ignoring move clocks.
bool same_chess_position(const std::string& left, const std::string& right);

// Returns true for White and false for Black; invalid FEN throws.
bool white_to_move(const std::string& fen);

// Whether neither side has enough material to force mate, so the game is a
// dead draw. Deliberately side-agnostic: a lone king facing a pawn is not a
// draw just because the king's owner has nothing.
bool insufficient_mating_material(const std::string& fen);

}  // namespace kchess
