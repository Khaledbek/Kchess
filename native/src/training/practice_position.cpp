// -----------------------------------------------------------------------------
// Section: Bounded native placement generation and legality checks
// -----------------------------------------------------------------------------
#include "training/practice_position.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <random>
#include <stdexcept>
#include "chess/fen.h"
#include "chess/move.h"
#include "engine/stockfish_runtime.h"
#include "position.h"
#include "movegen.h"

namespace kchess {
namespace {
std::string fen_for(const std::array<char, 64>& board, char side) {
  std::string fen;
  for (int rank = 7; rank >= 0; --rank) {
    int empty = 0;
    for (int file = 0; file < 8; ++file) {
      const char piece = board[rank * 8 + file];
      if (!piece) { ++empty; continue; }
      if (empty) { fen += std::to_string(empty); empty = 0; }
      fen += piece;
    }
    if (empty) fen += std::to_string(empty);
    if (rank) fen += '/';
  }
  return fen + " " + side + " - - 0 1";
}
bool usable(const std::string& fen) {
  const auto validation = validate_fen(fen);
  if (!validation.valid) return false;
  std::deque<Stockfish::StateInfo> states(1);
  Stockfish::Position position;
  position.set(validation.normalized, false, &states.back());
  if (position.checkers()) return false;
  const auto moves = Stockfish::MoveList<Stockfish::LEGAL>(position);
  if (moves.size() == 0) return false;
  for (auto move : moves) if (position.capture(move)) return false;
  return true;
}
}

int practice_move_budget(const std::string& drill, int level) {
  if (level < 1 || level > 3) throw std::invalid_argument("Invalid drill level");
  if (drill != "endgame_kq_vs_k" && drill != "endgame_kr_vs_k" &&
      drill != "endgame_q_vs_r") throw std::invalid_argument("Unknown drill");
  return drill == "endgame_q_vs_r" ? (level == 3 ? 45 : (level + 1) * 10) : level * 10;
}

std::string generate_practice_position(const std::string& drill, int level) {
  practice_move_budget(drill, level);
  initialize_stockfish_runtime();
  std::mt19937 random(std::random_device{}());
  std::array<int,64> squares;
  for (int i = 0; i < 64; ++i) squares[i] = i;
  for (int attempt = 0; attempt < 2000; ++attempt) {
    std::shuffle(squares.begin(), squares.end(), random);
    const int bk = squares[0], wk = squares[1];
    const int edge = std::min({bk % 8, 7 - bk % 8, bk / 8, 7 - bk / 8});
    const int gap = std::max(std::abs(bk % 8 - wk % 8), std::abs(bk / 8 - wk / 8));
    if ((level == 1 && edge != 0) || (level == 2 && (edge < 1 || edge > 2)) ||
        (level == 3 && edge < 2) ||
        gap < (level == 3 ? (drill == "endgame_q_vs_r" ? 3 : 4) : 2)) continue;
    std::array<char,64> board{};
    board[bk] = 'k'; board[wk] = 'K';
    board[squares[2]] = drill == "endgame_kr_vs_k" ? 'R' : 'Q';
    if (drill == "endgame_q_vs_r") board[squares[3]] = 'r';
    const auto fen = fen_for(board, 'w');
    if (usable(fen) && usable(fen_for(board, 'b'))) return fen;
  }
  throw std::runtime_error("Cannot generate a playable drill position");
}

// Conservative dead-position detection: do not label KNN vs K or opposing
// minor pieces a dead draw merely because mate cannot be forced.
bool practice_dead_position(const std::string& fen) {
  const auto placement = fen.substr(0, fen.find(' '));
  int minors = 0, knights = 0, bishop_color = -1, square = 0;
  bool same_color = true;
  for (char c : placement) {
    if (c == '/') continue;
    if (c >= '1' && c <= '8') { square += c - '0'; continue; }
    if (c == 'P' || c == 'p' || c == 'R' || c == 'r' || c == 'Q' || c == 'q') return false;
    if (c == 'N' || c == 'n') { ++minors; ++knights; }
    if (c == 'B' || c == 'b') {
      ++minors;
      const int color = (square / 8 + square % 8) % 2;
      if (bishop_color >= 0 && bishop_color != color) same_color = false;
      bishop_color = color;
    }
    ++square;
  }
  return minors <= 1 || (knights == 0 && same_color);
}

}  // namespace kchess
