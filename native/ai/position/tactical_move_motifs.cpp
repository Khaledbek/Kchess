#include "tactical_move_motifs.h"

#include <algorithm>
#include <cmath>
#include <deque>
#include <utility>

#include "bitboard.h"
#include "movegen.h"

namespace kchess::ai::detail {
namespace {

// -----------------------------------------------------------------------------
// Section: Shared helpers
// -----------------------------------------------------------------------------

Stockfish::Bitboard square_bit(Stockfish::Square square) {
  return Stockfish::Bitboard{1} << static_cast<unsigned>(square);
}

int piece_value(Stockfish::Piece piece) {
  if (piece == Stockfish::NO_PIECE) return 0;
  switch (Stockfish::type_of(piece)) {
    case Stockfish::PAWN: return 100;
    case Stockfish::KNIGHT: return 320;
    case Stockfish::BISHOP: return 330;
    case Stockfish::ROOK: return 500;
    case Stockfish::QUEEN: return 900;
    case Stockfish::KING: return 20000;
    default: return 0;
  }
}

bool slider(Stockfish::Piece piece) {
  if (piece == Stockfish::NO_PIECE) return false;
  const auto type = Stockfish::type_of(piece);
  return type == Stockfish::BISHOP || type == Stockfish::ROOK ||
         type == Stockfish::QUEEN;
}

bool valuable_target(Stockfish::Piece piece) {
  return piece != Stockfish::NO_PIECE &&
         Stockfish::type_of(piece) != Stockfish::PAWN &&
         Stockfish::type_of(piece) != Stockfish::KING;
}

bool has_legal_capture(const Stockfish::Position& position) {
  for (const auto move : Stockfish::MoveList<Stockfish::LEGAL>(position)) {
    if (position.capture(move)) return true;
  }
  return false;
}

bool checkmate(const Stockfish::Position& position) {
  return position.checkers() &&
         Stockfish::MoveList<Stockfish::LEGAL>(position).size() == 0;
}

bool has_mating_move(const Stockfish::Position& position) {
  for (const auto move : Stockfish::MoveList<Stockfish::LEGAL>(position)) {
    std::deque<Stockfish::StateInfo> states(1);
    Stockfish::Position after;
    after.set(position.fen(), false, &states.back());
    states.emplace_back();
    after.do_move(move, states.back(), nullptr);
    if (checkmate(after)) return true;
  }
  return false;
}

bool smothered(const Stockfish::Position& position,
               Stockfish::Color victim) {
  Stockfish::Square king = Stockfish::SQ_NONE;
  const auto king_piece = Stockfish::make_piece(victim, Stockfish::KING);
  for (Stockfish::Square square = Stockfish::SQ_A1;
       square <= Stockfish::SQ_H8; ++square) {
    if (position.piece_on(square) == king_piece) {
      king = square;
      break;
    }
  }
  if (king == Stockfish::SQ_NONE) return false;

  const int file = static_cast<int>(Stockfish::file_of(king));
  const int rank = static_cast<int>(Stockfish::rank_of(king));
  int neighbors = 0;
  int own_occupied = 0;
  for (int df = -1; df <= 1; ++df) {
    for (int dr = -1; dr <= 1; ++dr) {
      if (df == 0 && dr == 0) continue;
      const int f = file + df;
      const int r = rank + dr;
      if (f < 0 || f >= 8 || r < 0 || r >= 8) continue;
      ++neighbors;
      const auto piece = position.piece_on(static_cast<Stockfish::Square>(r * 8 + f));
      if (piece != Stockfish::NO_PIECE && Stockfish::color_of(piece) == victim) {
        ++own_occupied;
      }
    }
  }
  return neighbors >= 3 && own_occupied == neighbors;
}

std::vector<int> defended_targets(const Stockfish::Position& position,
                                  Stockfish::Square defender,
                                  Stockfish::Color defender_color,
                                  bool unique_only = false) {
  std::vector<int> result;
  const auto enemy = position.pieces(~defender_color);
  for (Stockfish::Square target = Stockfish::SQ_A1;
       target <= Stockfish::SQ_H8; ++target) {
    const auto piece = position.piece_on(target);
    if (!valuable_target(piece) || Stockfish::color_of(piece) != defender_color) {
      continue;
    }
    const auto attackers = position.attackers_to(target);
    const auto defenders = attackers & position.pieces(defender_color);
    if (!(defenders & square_bit(defender)) || !(attackers & enemy)) continue;
    if (unique_only && Stockfish::popcount(defenders) != 1) continue;
    result.push_back(static_cast<int>(target));
  }
  return result;
}

// -----------------------------------------------------------------------------
// Section: Clearance and defender motifs
// -----------------------------------------------------------------------------


bool square_between(Stockfish::Square middle, Stockfish::Square first,
                    Stockfish::Square second) {
  const int mf = static_cast<int>(Stockfish::file_of(middle));
  const int mr = static_cast<int>(Stockfish::rank_of(middle));
  const int ff = static_cast<int>(Stockfish::file_of(first));
  const int fr = static_cast<int>(Stockfish::rank_of(first));
  const int sf = static_cast<int>(Stockfish::file_of(second));
  const int sr = static_cast<int>(Stockfish::rank_of(second));
  const int df = sf - ff;
  const int dr = sr - fr;
  if (df != 0 && dr != 0 && std::abs(df) != std::abs(dr)) return false;
  if (df == 0 && mf != ff) return false;
  if (dr == 0 && mr != fr) return false;
  if (df != 0 && dr != 0 && std::abs(mf - ff) != std::abs(mr - fr)) {
    return false;
  }
  return mf >= std::min(ff, sf) && mf <= std::max(ff, sf) &&
         mr >= std::min(fr, sr) && mr <= std::max(fr, sr) &&
         middle != first && middle != second;
}

bool revealed_slider_attack(const Stockfish::Position& before,
                            const Stockfish::Position& after,
                            Stockfish::Color actor,
                            Stockfish::Square vacated,
                            int& rear_square,
                            int& target_square) {
  for (Stockfish::Square target = Stockfish::SQ_A1;
       target <= Stockfish::SQ_H8; ++target) {
    const auto target_piece = before.piece_on(target);
    if (!valuable_target(target_piece) || Stockfish::color_of(target_piece) == actor) {
      continue;
    }
    const auto before_attackers = before.attackers_to(target) & before.pieces(actor);
    const auto after_attackers = after.attackers_to(target) & after.pieces(actor);
    if (before_attackers == after_attackers) continue;

    for (Stockfish::Square attacker = Stockfish::SQ_A1;
         attacker <= Stockfish::SQ_H8; ++attacker) {
      if (!(after_attackers & square_bit(attacker)) ||
          (before_attackers & square_bit(attacker))) {
        continue;
      }
      if (!slider(after.piece_on(attacker)) ||
          !square_between(vacated, attacker, target)) {
        continue;
      }
      rear_square = static_cast<int>(attacker);
      target_square = static_cast<int>(target);
      return true;
    }
  }
  return false;
}

void detect_clearance(const Stockfish::Position& before,
                      const Stockfish::Position& after,
                      Stockfish::Move move,
                      Stockfish::Color actor,
                      std::vector<TacticalMotif>& motifs) {
  int rear = -1;
  int target = -1;
  if (!revealed_slider_attack(before, after, actor, move.from_sq(), rear, target)) {
    return;
  }
  const auto moved_piece = before.moved_piece(move);
  const auto captured_piece = before.piece_on(move.to_sq());
  const bool offered =
      (after.attackers_to(move.to_sq()) & after.pieces(~actor)) != 0;
  const bool sacrifice = offered &&
      (!before.capture(move) ||
       piece_value(moved_piece) > piece_value(captured_piece) + 100);
  motifs.push_back({
      .kind = sacrifice ? TacticalMotifKind::clearance_sacrifice
                        : TacticalMotifKind::clearance,
      .by_white = actor == Stockfish::WHITE,
      .attacker_squares = {rear, static_cast<int>(move.from_sq())},
      .target_squares = {target},
      .blocker_square = static_cast<int>(move.from_sq()),
      .trigger_from = static_cast<int>(move.from_sq()),
      .trigger_to = static_cast<int>(move.to_sq()),
      .confidence = sacrifice ? 0.76 : 0.92,
  });
}

void detect_defender_move(const Stockfish::Position& before,
                          const Stockfish::Position& after,
                          Stockfish::Move move,
                          Stockfish::Color actor,
                          std::vector<TacticalMotif>& motifs) {
  const auto enemy = ~actor;
  const auto victim = before.piece_on(move.to_sq());
  if (before.capture(move) && victim != Stockfish::NO_PIECE &&
      Stockfish::color_of(victim) == enemy) {
    auto targets = defended_targets(before, move.to_sq(), enemy);
    if (!targets.empty()) {
      motifs.push_back({
          .kind = TacticalMotifKind::removal_of_defender,
          .by_white = actor == Stockfish::WHITE,
          .attacker_squares = {static_cast<int>(move.from_sq())},
          .target_squares = std::move(targets),
          .blocker_square = static_cast<int>(move.to_sq()),
          .trigger_from = static_cast<int>(move.from_sq()),
          .trigger_to = static_cast<int>(move.to_sq()),
          .confidence = 0.94,
      });
    }
    return;
  }

  const auto moved_bit = square_bit(move.to_sq());
  for (Stockfish::Square defender = Stockfish::SQ_A1;
       defender <= Stockfish::SQ_H8; ++defender) {
    const auto piece = after.piece_on(defender);
    if (piece == Stockfish::NO_PIECE || Stockfish::color_of(piece) != enemy ||
        Stockfish::type_of(piece) == Stockfish::KING) {
      continue;
    }
    if (!(after.attackers_to(defender) & moved_bit)) continue;
    auto targets = defended_targets(before, defender, enemy, true);
    if (targets.empty()) continue;
    motifs.push_back({
        .kind = TacticalMotifKind::deflection,
        .by_white = actor == Stockfish::WHITE,
        .attacker_squares = {static_cast<int>(move.to_sq())},
        .target_squares = std::move(targets),
        .blocker_square = static_cast<int>(defender),
        .trigger_from = static_cast<int>(move.from_sq()),
        .trigger_to = static_cast<int>(move.to_sq()),
        .confidence = after.checkers() ? 0.78 : 0.64,
    });
    break;
  }
}

// -----------------------------------------------------------------------------
// Section: Interference, zwischenzug and desperado
// -----------------------------------------------------------------------------

void detect_interference(const Stockfish::Position& before,
                         const Stockfish::Position& after,
                         Stockfish::Move move,
                         Stockfish::Color actor,
                         std::vector<TacticalMotif>& motifs) {
  if (before.capture(move)) return;
  const auto enemy = ~actor;
  for (Stockfish::Square target = Stockfish::SQ_A1;
       target <= Stockfish::SQ_H8; ++target) {
    const auto piece = before.piece_on(target);
    if (!valuable_target(piece) || Stockfish::color_of(piece) != enemy) continue;
    if (!(before.attackers_to(target) & before.pieces(actor))) continue;

    const auto before_defenders = before.attackers_to(target) & before.pieces(enemy);
    const auto after_defenders = after.attackers_to(target) & after.pieces(enemy);
    const auto lost = before_defenders & ~after_defenders;
    if (!lost) continue;

    for (Stockfish::Square defender = Stockfish::SQ_A1;
         defender <= Stockfish::SQ_H8; ++defender) {
      if ((lost & square_bit(defender)) && slider(before.piece_on(defender))) {
        motifs.push_back({
            .kind = TacticalMotifKind::interference,
            .by_white = actor == Stockfish::WHITE,
            .attacker_squares = {static_cast<int>(move.from_sq())},
            .target_squares = {static_cast<int>(target)},
            .blocker_square = static_cast<int>(move.to_sq()),
            .trigger_from = static_cast<int>(move.from_sq()),
            .trigger_to = static_cast<int>(move.to_sq()),
            .confidence = 0.84,
        });
        return;
      }
    }
  }
}

void detect_zwischenzug(const Stockfish::Position& before,
                        const Stockfish::Position& after,
                        Stockfish::Move move,
                        Stockfish::Color actor,
                        bool capture_available,
                        std::vector<TacticalMotif>& motifs) {
  if (!capture_available || before.capture(move) || !after.checkers()) return;
  motifs.push_back({
      .kind = TacticalMotifKind::zwischenzug,
      .by_white = actor == Stockfish::WHITE,
      .attacker_squares = {static_cast<int>(move.from_sq())},
      .target_squares = {},
      .trigger_from = static_cast<int>(move.from_sq()),
      .trigger_to = static_cast<int>(move.to_sq()),
      .confidence = 0.62,
  });
}

void detect_desperado(const Stockfish::Position& before,
                      const Stockfish::Position& after,
                      Stockfish::Move move,
                      Stockfish::Color actor,
                      std::vector<TacticalMotif>& motifs) {
  const auto source = move.from_sq();
  if (!(before.attackers_to(source) & before.pieces(~actor))) return;
  const auto moving = before.moved_piece(move);
  const auto captured = before.piece_on(move.to_sq());
  const bool worthwhile_capture = before.capture(move) &&
                                  piece_value(captured) >= piece_value(moving);
  if (!worthwhile_capture && !after.checkers()) return;
  motifs.push_back({
      .kind = TacticalMotifKind::desperado,
      .by_white = actor == Stockfish::WHITE,
      .attacker_squares = {static_cast<int>(source)},
      .target_squares = before.capture(move)
                            ? std::vector<int>{static_cast<int>(move.to_sq())}
                            : std::vector<int>{},
      .trigger_from = static_cast<int>(source),
      .trigger_to = static_cast<int>(move.to_sq()),
      .confidence = worthwhile_capture ? 0.78 : 0.68,
  });
}

// -----------------------------------------------------------------------------
// Section: Named mating patterns
// -----------------------------------------------------------------------------

void detect_greek_gift(const Stockfish::Position& before,
                       const Stockfish::Position& after,
                       Stockfish::Move move,
                       Stockfish::Color actor,
                       std::vector<TacticalMotif>& motifs) {
  if (Stockfish::type_of(before.moved_piece(move)) != Stockfish::BISHOP ||
      !before.capture(move) || !after.checkers()) {
    return;
  }
  const int file = static_cast<int>(Stockfish::file_of(move.to_sq()));
  const int rank = static_cast<int>(Stockfish::rank_of(move.to_sq()));
  if (file != 7 || (rank != 1 && rank != 6)) return;
  const auto captured = before.piece_on(move.to_sq());
  if (Stockfish::type_of(captured) != Stockfish::PAWN) return;

  motifs.push_back({
      .kind = TacticalMotifKind::greek_gift,
      .by_white = actor == Stockfish::WHITE,
      .attacker_squares = {static_cast<int>(move.from_sq())},
      .target_squares = {static_cast<int>(move.to_sq())},
      .trigger_from = static_cast<int>(move.from_sq()),
      .trigger_to = static_cast<int>(move.to_sq()),
      .confidence = 0.9,
  });
}

void detect_smothered_mate(const Stockfish::Position& before,
                           const Stockfish::Position& after,
                           Stockfish::Move move,
                           Stockfish::Color actor,
                           std::vector<TacticalMotif>& motifs) {
  if (Stockfish::type_of(before.moved_piece(move)) != Stockfish::KNIGHT ||
      !checkmate(after) || !smothered(after, ~actor)) {
    return;
  }
  motifs.push_back({
      .kind = TacticalMotifKind::smothered_mate,
      .by_white = actor == Stockfish::WHITE,
      .attacker_squares = {static_cast<int>(move.from_sq())},
      .target_squares = {static_cast<int>(move.to_sq())},
      .trigger_from = static_cast<int>(move.from_sq()),
      .trigger_to = static_cast<int>(move.to_sq()),
      .confidence = 1.0,
  });
}

void detect_decoy(const Stockfish::Position& before,
                  const Stockfish::Position& after,
                  Stockfish::Move move,
                  Stockfish::Color actor,
                  std::vector<TacticalMotif>& motifs) {
  if (!after.checkers() || piece_value(before.moved_piece(move)) > 500) return;

  for (const auto reply : Stockfish::MoveList<Stockfish::LEGAL>(after)) {
    if (reply.to_sq() != move.to_sq() ||
        Stockfish::type_of(after.moved_piece(reply)) != Stockfish::KING ||
        !after.capture(reply)) {
      continue;
    }
    std::deque<Stockfish::StateInfo> states(1);
    Stockfish::Position after_reply;
    after_reply.set(after.fen(), false, &states.back());
    states.emplace_back();
    after_reply.do_move(reply, states.back(), nullptr);
    if (!has_mating_move(after_reply)) continue;

    motifs.push_back({
        .kind = TacticalMotifKind::decoy,
        .by_white = actor == Stockfish::WHITE,
        .attacker_squares = {static_cast<int>(move.from_sq())},
        .target_squares = {static_cast<int>(move.to_sq())},
        .trigger_from = static_cast<int>(move.from_sq()),
        .trigger_to = static_cast<int>(move.to_sq()),
        .confidence = 0.95,
    });
    return;
  }
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Public legal-move detector
// -----------------------------------------------------------------------------

std::vector<TacticalMotif> detect_move_motifs(
    const Stockfish::Position& position) {
  std::vector<TacticalMotif> motifs;
  const auto actor = position.side_to_move();
  const bool capture_available = has_legal_capture(position);

  for (const auto move : Stockfish::MoveList<Stockfish::LEGAL>(position)) {
    std::deque<Stockfish::StateInfo> states(1);
    Stockfish::Position after;
    after.set(position.fen(), false, &states.back());
    states.emplace_back();
    after.do_move(move, states.back(), nullptr);

    detect_clearance(position, after, move, actor, motifs);
    detect_defender_move(position, after, move, actor, motifs);
    detect_interference(position, after, move, actor, motifs);
    detect_zwischenzug(position, after, move, actor, capture_available, motifs);
    detect_desperado(position, after, move, actor, motifs);
    detect_greek_gift(position, after, move, actor, motifs);
    detect_smothered_mate(position, after, move, actor, motifs);
    detect_decoy(position, after, move, actor, motifs);
  }
  return motifs;
}

}  // namespace kchess::ai::detail
