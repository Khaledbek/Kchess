#include "tactical_detector.h"

#include <algorithm>
#include <deque>
#include <initializer_list>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include "bitboard.h"
#include "chess/fen.h"
#include "engine/stockfish_runtime.h"
#include "position.h"
#include "tactical_line_motifs.h"
#include "tactical_move_motifs.h"
#include "tactical_pattern_motifs.h"

namespace kchess::ai {
namespace {

// -----------------------------------------------------------------------------
// Section: Attack helpers
// -----------------------------------------------------------------------------

Stockfish::Bitboard square_bit(Stockfish::Square square) {
  return Stockfish::Bitboard{1} << static_cast<unsigned>(square);
}

bool meaningful_target(Stockfish::Piece piece) {
  return piece != Stockfish::NO_PIECE &&
         Stockfish::type_of(piece) != Stockfish::PAWN;
}

std::vector<int> attacked_targets(const Stockfish::Position& position,
                                  Stockfish::Square source,
                                  Stockfish::Color attacker) {
  std::vector<int> targets;
  const auto source_bit = square_bit(source);
  for (Stockfish::Square target = Stockfish::SQ_A1;
       target <= Stockfish::SQ_H8; ++target) {
    const auto piece = position.piece_on(target);
    if (!meaningful_target(piece) || Stockfish::color_of(piece) == attacker) {
      continue;
    }
    if (position.attackers_to(target) & source_bit) {
      targets.push_back(static_cast<int>(target));
    }
  }
  return targets;
}

bool contains_type(const Stockfish::Position& position,
                   const std::vector<int>& squares,
                   Stockfish::PieceType type) {
  return std::any_of(squares.begin(), squares.end(), [&](int square) {
    return Stockfish::type_of(
               position.piece_on(static_cast<Stockfish::Square>(square))) == type;
  });
}

void detect_multi_attacks(const Stockfish::Position& position,
                          Stockfish::Color attacker,
                          std::vector<TacticalMotif>& motifs) {
  for (Stockfish::Square source = Stockfish::SQ_A1;
       source <= Stockfish::SQ_H8; ++source) {
    const auto piece = position.piece_on(source);
    if (piece == Stockfish::NO_PIECE || Stockfish::color_of(piece) != attacker) {
      continue;
    }

    auto targets = attacked_targets(position, source, attacker);
    if (targets.size() < 2) continue;
    const auto type = Stockfish::type_of(piece);
    const bool fork_piece = type == Stockfish::KNIGHT || type == Stockfish::PAWN;
    const bool royal = fork_piece && contains_type(position, targets, Stockfish::KING) &&
                       contains_type(position, targets, Stockfish::QUEEN);

    TacticalMotifKind kind = TacticalMotifKind::double_attack;
    double confidence = 0.9;
    if (royal) {
      kind = TacticalMotifKind::royal_fork;
      confidence = 1.0;
    } else if (fork_piece) {
      kind = TacticalMotifKind::fork;
      confidence = 0.95;
    }

    motifs.push_back({
        .kind = kind,
        .by_white = attacker == Stockfish::WHITE,
        .attacker_squares = {static_cast<int>(source)},
        .target_squares = std::move(targets),
        .confidence = confidence,
    });
  }
}

Stockfish::Square king_square(const Stockfish::Position& position,
                              Stockfish::Color color) {
  const auto king = Stockfish::make_piece(color, Stockfish::KING);
  for (Stockfish::Square square = Stockfish::SQ_A1;
       square <= Stockfish::SQ_H8; ++square) {
    if (position.piece_on(square) == king) return square;
  }
  return Stockfish::SQ_NONE;
}

void detect_double_check(const Stockfish::Position& position,
                         std::vector<TacticalMotif>& motifs) {
  const auto checkers = position.checkers();
  if (Stockfish::popcount(checkers) < 2) return;

  std::vector<int> attacker_squares;
  for (Stockfish::Square square = Stockfish::SQ_A1;
       square <= Stockfish::SQ_H8; ++square) {
    if (checkers & square_bit(square)) {
      attacker_squares.push_back(static_cast<int>(square));
    }
  }
  const auto victim = position.side_to_move();
  const auto king = king_square(position, victim);
  motifs.push_back({
      .kind = TacticalMotifKind::double_check,
      .by_white = victim == Stockfish::BLACK,
      .attacker_squares = std::move(attacker_squares),
      .target_squares = king == Stockfish::SQ_NONE
                            ? std::vector<int>{}
                            : std::vector<int>{static_cast<int>(king)},
      .confidence = 1.0,
  });
}

// -----------------------------------------------------------------------------
// Section: Evidence serialization
// -----------------------------------------------------------------------------

const char* motif_name(TacticalMotifKind kind) {
  switch (kind) {
    case TacticalMotifKind::fork: return "fork";
    case TacticalMotifKind::royal_fork: return "royal_fork";
    case TacticalMotifKind::double_attack: return "double_attack";
    case TacticalMotifKind::pin: return "pin";
    case TacticalMotifKind::skewer: return "skewer";
    case TacticalMotifKind::discovered_attack: return "discovered_attack";
    case TacticalMotifKind::double_check: return "double_check";
    case TacticalMotifKind::deflection: return "deflection";
    case TacticalMotifKind::decoy: return "decoy";
    case TacticalMotifKind::clearance: return "clearance";
    case TacticalMotifKind::clearance_sacrifice: return "clearance_sacrifice";
    case TacticalMotifKind::removal_of_defender: return "removal_of_defender";
    case TacticalMotifKind::overloading: return "overloading";
    case TacticalMotifKind::interference: return "interference";
    case TacticalMotifKind::zwischenzug: return "zwischenzug";
    case TacticalMotifKind::desperado: return "desperado";
    case TacticalMotifKind::x_ray: return "x_ray";
    case TacticalMotifKind::battery: return "battery";
    case TacticalMotifKind::back_rank: return "back_rank";
    case TacticalMotifKind::greek_gift: return "greek_gift";
    case TacticalMotifKind::smothered_mate: return "smothered_mate";
  }
  return "double_attack";
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Public detector
// -----------------------------------------------------------------------------

TacticalAnalysis TacticalDetector::analyze(const std::string& fen) const {
  const auto validation = kchess::validate_fen(fen);
  if (!validation.valid) throw std::invalid_argument(validation.error);

  kchess::initialize_stockfish_runtime();
  std::deque<Stockfish::StateInfo> states(1);
  Stockfish::Position position;
  position.set(validation.normalized, false, &states.back());

  TacticalAnalysis analysis;
  for (const auto color : {Stockfish::WHITE, Stockfish::BLACK}) {
    detect_multi_attacks(position, color, analysis.motifs);
    auto line_motifs = detail::detect_line_motifs(position, color);
    analysis.motifs.insert(analysis.motifs.end(), line_motifs.begin(),
                           line_motifs.end());
  }
  detect_double_check(position, analysis.motifs);

  auto patterns = detail::detect_pattern_motifs(position);
  analysis.motifs.insert(analysis.motifs.end(), patterns.begin(), patterns.end());
  auto move_motifs = detail::detect_move_motifs(position);
  analysis.motifs.insert(analysis.motifs.end(), move_motifs.begin(),
                         move_motifs.end());

  const auto less = [](const TacticalMotif& left, const TacticalMotif& right) {
    if (left.kind != right.kind) return left.kind < right.kind;
    if (left.by_white != right.by_white) return left.by_white < right.by_white;
    if (left.attacker_squares != right.attacker_squares) {
      return left.attacker_squares < right.attacker_squares;
    }
    if (left.target_squares != right.target_squares) {
      return left.target_squares < right.target_squares;
    }
    if (left.blocker_square != right.blocker_square) {
      return left.blocker_square < right.blocker_square;
    }
    if (left.trigger_from != right.trigger_from) {
      return left.trigger_from < right.trigger_from;
    }
    return left.trigger_to < right.trigger_to;
  };
  std::sort(analysis.motifs.begin(), analysis.motifs.end(), less);
  analysis.motifs.erase(
      std::unique(analysis.motifs.begin(), analysis.motifs.end(),
                  [&](const TacticalMotif& left, const TacticalMotif& right) {
                    return !less(left, right) && !less(right, left);
                  }),
      analysis.motifs.end());
  return analysis;
}

EvidenceItem TacticalDetector::evidence(const std::string& fen) const {
  return evidence(analyze(fen));
}

EvidenceItem TacticalDetector::evidence(const TacticalAnalysis& analysis) const {
  nlohmann::json motifs = nlohmann::json::array();
  for (const auto& motif : analysis.motifs) {
    motifs.push_back({
        {"kind", motif_name(motif.kind)},
        {"by", motif.by_white ? "white" : "black"},
        {"attackers", motif.attacker_squares},
        {"targets", motif.target_squares},
        {"blocker", motif.blocker_square},
        {"trigger_from", motif.trigger_from},
        {"trigger_to", motif.trigger_to},
        {"confidence", motif.confidence},
    });
  }
  return {
      .id = "position.tactics.v2",
      .kind = EvidenceKind::tactical_motifs,
      .payload = nlohmann::json{{"version", 2}, {"motifs", motifs}}.dump(),
      .confidence = 1.0,
  };
}

}  // namespace kchess::ai
