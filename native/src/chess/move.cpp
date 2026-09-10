// -----------------------------------------------------------------------------
// Section: Legal move resolution and terminal position detection
// -----------------------------------------------------------------------------

#include "chess/move.h"

#include <deque>
#include <optional>
#include <stdexcept>
#include <string>

#include "chess/fen.h"
#include "engine/stockfish_runtime.h"
#include "movegen.h"
#include "position.h"
#include "types.h"
#include "uci.h"

namespace kchess {
namespace {

char san_piece(const Stockfish::PieceType type) {
  switch (type) {
    case Stockfish::KNIGHT: return 'N';
    case Stockfish::BISHOP: return 'B';
    case Stockfish::ROOK: return 'R';
    case Stockfish::QUEEN: return 'Q';
    case Stockfish::KING: return 'K';
    default: return '\0';
  }
}

std::string move_san(const Stockfish::Position& position, const Stockfish::Move move) {
  std::string result;
  if (move.type_of() == Stockfish::CASTLING) {
    result = move.to_sq() > move.from_sq() ? "O-O" : "O-O-O";
  } else {
    const auto piece_type = Stockfish::type_of(position.moved_piece(move));
    const bool capture = position.capture(move);
    if (piece_type == Stockfish::PAWN) {
      if (capture) result.push_back(static_cast<char>('a' + Stockfish::file_of(move.from_sq())));
    } else {
      result.push_back(san_piece(piece_type));
      bool same_file = false;
      bool same_rank = false;
      bool ambiguous = false;
      for (const auto candidate : Stockfish::MoveList<Stockfish::LEGAL>(position)) {
        if (candidate == move || candidate.to_sq() != move.to_sq()
            || Stockfish::type_of(position.moved_piece(candidate)) != piece_type) {
          continue;
        }
        ambiguous = true;
        same_file = same_file || Stockfish::file_of(candidate.from_sq()) == Stockfish::file_of(move.from_sq());
        same_rank = same_rank || Stockfish::rank_of(candidate.from_sq()) == Stockfish::rank_of(move.from_sq());
      }
      if (ambiguous) {
        if (!same_file) result.push_back(static_cast<char>('a' + Stockfish::file_of(move.from_sq())));
        else if (!same_rank) result.push_back(static_cast<char>('1' + Stockfish::rank_of(move.from_sq())));
        else {
          result.push_back(static_cast<char>('a' + Stockfish::file_of(move.from_sq())));
          result.push_back(static_cast<char>('1' + Stockfish::rank_of(move.from_sq())));
        }
      }
    }
    if (capture) result.push_back('x');
    result += Stockfish::UCIEngine::square(move.to_sq());
    if (move.type_of() == Stockfish::PROMOTION) {
      result.push_back('=');
      result.push_back(san_piece(move.promotion_type()));
    }
  }

  std::deque<Stockfish::StateInfo> states(1);
  Stockfish::Position after;
  after.set(position.fen(), false, &states.back());
  states.emplace_back();
  after.do_move(move, states.back(), nullptr);
  if (after.checkers()) {
    result.push_back(Stockfish::MoveList<Stockfish::LEGAL>(after).size() == 0 ? '#' : '+');
  }
  return result;
}

}  // namespace

AppliedMove apply_legal_uci_move(const std::string& fen, const std::string& uci) {
  const auto validation = validate_fen(fen);
  if (!validation.valid) throw std::invalid_argument(validation.error);
  initialize_stockfish_runtime();
  std::deque<Stockfish::StateInfo> states(1);
  Stockfish::Position position;
  position.set(validation.normalized, false, &states.back());
  const auto normalized_uci = uci;
  auto move = Stockfish::UCIEngine::to_move(position, normalized_uci);
  if (move == Stockfish::Move::none()) throw std::invalid_argument("Illegal chess move");
  const auto san = move_san(position, move);
  states.emplace_back();
  position.do_move(move, states.back(), nullptr);
  return {
      .uci = Stockfish::UCIEngine::move(move, false),
      .san = san,
      .fen_after = position.fen(),
  };
}

std::optional<AppliedMove> find_legal_san_move(
    const std::string& fen, const std::string& san) {
  const auto validation = validate_fen(fen);
  if (!validation.valid) throw std::invalid_argument(validation.error);
  initialize_stockfish_runtime();
  std::deque<Stockfish::StateInfo> states(1);
  Stockfish::Position position;
  position.set(validation.normalized, false, &states.back());

  std::optional<AppliedMove> result;
  for (const auto move : Stockfish::MoveList<Stockfish::LEGAL>(position)) {
    if (move_san(position, move) != san) continue;
    if (result.has_value()) return std::nullopt;
    std::deque<Stockfish::StateInfo> after_states(1);
    Stockfish::Position after;
    after.set(position.fen(), false, &after_states.back());
    after_states.emplace_back();
    after.do_move(move, after_states.back(), nullptr);
    result = AppliedMove{
        .uci = Stockfish::UCIEngine::move(move, false),
        .san = san,
        .fen_after = after.fen(),
    };
  }
  return result;
}

std::vector<std::string> legal_promotion_choices(
    const std::string& fen,
    const std::string& source,
    const std::string& target) {
  if (source.size() != 2 || target.size() != 2) {
    throw std::invalid_argument("Board squares must use algebraic coordinates");
  }
  const auto validation = validate_fen(fen);
  if (!validation.valid) throw std::invalid_argument(validation.error);
  initialize_stockfish_runtime();
  std::deque<Stockfish::StateInfo> states(1);
  Stockfish::Position position;
  position.set(validation.normalized, false, &states.back());

  if (Stockfish::UCIEngine::to_move(position, source + target) != Stockfish::Move::none()) {
    return {};
  }

  std::vector<std::string> result;
  for (const char suffix : {'q', 'r', 'b', 'n'}) {
    const auto candidate = source + target + suffix;
    if (Stockfish::UCIEngine::to_move(position, candidate) != Stockfish::Move::none()) {
      result.emplace_back(1, suffix);
    }
  }
  return result;
}


PositionOutcome position_outcome(const std::string& fen) {
  const auto validation = validate_fen(fen);
  if (!validation.valid) throw std::invalid_argument(validation.error);
  initialize_stockfish_runtime();
  std::deque<Stockfish::StateInfo> states(1);
  Stockfish::Position position;
  position.set(validation.normalized, false, &states.back());

  if (position.is_draw(0)) {
    return {.terminal = true, .checkmate = false, .result = "1/2-1/2"};
  }
  if (Stockfish::MoveList<Stockfish::LEGAL>(position).size() != 0) {
    return {};
  }
  if (!position.checkers()) {
    return {.terminal = true, .checkmate = false, .result = "1/2-1/2"};
  }
  return {
      .terminal = true,
      .checkmate = true,
      .result = position.side_to_move() == Stockfish::WHITE ? "0-1" : "1-0",
  };
}

}  // namespace kchess
