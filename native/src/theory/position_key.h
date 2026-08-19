#pragma once

#include <cstdint>
#include <string>

namespace kchess {

// Stockfish 18 Zobrist key with FEN clocks canonicalized out of the identity.
std::uint64_t stockfish_position_key(const std::string& fen);

// KCB1's stable UCI move encoding (from, to and optional promotion).
std::uint16_t encode_book_move(const std::string& uci_move);
std::string decode_book_move(std::uint16_t encoded_move);

}  // namespace kchess
