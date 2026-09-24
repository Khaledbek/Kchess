#pragma once

#include <cstdint>
#include <string>


namespace kchess {

inline constexpr std::uint32_t kBookMoveEncodingVersion = 1;
inline constexpr std::uint64_t kPositionKeyFingerprintSeed = 14695981039346656037ULL;

// Stockfish 18 Zobrist key with FEN clocks canonicalized out of the identity.
std::uint64_t stockfish_position_key(const std::string& fen);

// Stable fingerprint over an ordered set of canonical position keys. This is
// used only to prove that independently built opening assets share the same
// Stockfish position-key universe; it is not a chess-position identity itself.
std::uint64_t extend_position_key_fingerprint(
    std::uint64_t fingerprint, std::uint64_t position_key) noexcept;

// Shared KCB1/KCL1 stable UCI move encoding (from, to and optional promotion).
std::uint16_t encode_book_move(const std::string& uci_move);
std::string decode_book_move(std::uint16_t encoded_move);

}  // namespace kchess
