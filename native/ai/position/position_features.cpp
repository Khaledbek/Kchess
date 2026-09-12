#include "position_features.h"

#include <deque>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include "bitboard.h"
#include "chess/fen.h"
#include "engine/stockfish_runtime.h"
#include "movegen.h"
#include "position.h"

namespace kchess::ai {
namespace {

// -----------------------------------------------------------------------------
// Section: Board metrics
// -----------------------------------------------------------------------------

int material_cp(const Stockfish::Position& position, Stockfish::Color color) {
  return 100 * position.count<Stockfish::PAWN>(color)
      + 320 * position.count<Stockfish::KNIGHT>(color)
      + 330 * position.count<Stockfish::BISHOP>(color)
      + 500 * position.count<Stockfish::ROOK>(color)
      + 900 * position.count<Stockfish::QUEEN>(color);
}

bool home_minor(const Stockfish::Square square, const Stockfish::Piece piece) {
  const bool white = Stockfish::color_of(piece) == Stockfish::WHITE;
  if (Stockfish::type_of(piece) == Stockfish::KNIGHT) {
    return white ? square == Stockfish::SQ_B1 || square == Stockfish::SQ_G1
                 : square == Stockfish::SQ_B8 || square == Stockfish::SQ_G8;
  }
  if (Stockfish::type_of(piece) == Stockfish::BISHOP) {
    return white ? square == Stockfish::SQ_C1 || square == Stockfish::SQ_F1
                 : square == Stockfish::SQ_C8 || square == Stockfish::SQ_F8;
  }
  return false;
}

Stockfish::Bitboard all_attacks(const Stockfish::Position& position,
                                Stockfish::Color color) {
  return position.attacks_by<Stockfish::PAWN>(color)
      | position.attacks_by<Stockfish::KNIGHT>(color)
      | position.attacks_by<Stockfish::BISHOP>(color)
      | position.attacks_by<Stockfish::ROOK>(color)
      | position.attacks_by<Stockfish::QUEEN>(color)
      | position.attacks_by<Stockfish::KING>(color);
}

Stockfish::Bitboard active_attacks(const Stockfish::Position& position,
                                   Stockfish::Color color) {
  return position.attacks_by<Stockfish::KNIGHT>(color)
      | position.attacks_by<Stockfish::BISHOP>(color)
      | position.attacks_by<Stockfish::ROOK>(color)
      | position.attacks_by<Stockfish::QUEEN>(color);
}

int minor_pieces_off_home(const Stockfish::Position& position,
                          Stockfish::Color color) {
  int count = 0;
  for (Stockfish::Square square = Stockfish::SQ_A1;
       square <= Stockfish::SQ_H8; ++square) {
    const auto piece = position.piece_on(square);
    if (piece == Stockfish::NO_PIECE || Stockfish::color_of(piece) != color) {
      continue;
    }
    const auto type = Stockfish::type_of(piece);
    if ((type == Stockfish::KNIGHT || type == Stockfish::BISHOP)
        && !home_minor(square, piece)) {
      ++count;
    }
  }
  return count;
}

int defended_pieces(const Stockfish::Position& position,
                    Stockfish::Color color) {
  int count = 0;
  const auto own = position.pieces(color);
  for (Stockfish::Square square = Stockfish::SQ_A1;
       square <= Stockfish::SQ_H8; ++square) {
    const auto piece = position.piece_on(square);
    if (piece == Stockfish::NO_PIECE || Stockfish::color_of(piece) != color
        || Stockfish::type_of(piece) == Stockfish::KING) {
      continue;
    }
    if (position.attackers_to(square) & own) ++count;
  }
  return count;
}

Stockfish::Bitboard enemy_half(Stockfish::Color color) {
  if (color == Stockfish::WHITE) {
    return Stockfish::Rank5BB | Stockfish::Rank6BB
        | Stockfish::Rank7BB | Stockfish::Rank8BB;
  }
  return Stockfish::Rank1BB | Stockfish::Rank2BB
      | Stockfish::Rank3BB | Stockfish::Rank4BB;
}

std::vector<int> semi_open_files(const Stockfish::Position& position,
                                 Stockfish::Color color) {
  std::vector<int> files;
  const auto own_pawns = position.pieces(color, Stockfish::PAWN);
  const auto enemy_pawns = position.pieces(~color, Stockfish::PAWN);
  for (int file = 0; file < 8; ++file) {
    const auto mask = Stockfish::file_bb(static_cast<Stockfish::File>(file));
    if (!(own_pawns & mask) && (enemy_pawns & mask)) files.push_back(file);
  }
  return files;
}

std::vector<int> open_files(const Stockfish::Position& position) {
  std::vector<int> files;
  const auto pawns = position.pieces(Stockfish::PAWN);
  for (int file = 0; file < 8; ++file) {
    const auto mask = Stockfish::file_bb(static_cast<Stockfish::File>(file));
    if (!(pawns & mask)) files.push_back(file);
  }
  return files;
}

SidePositionFeatures side_features(const Stockfish::Position& position,
                                   Stockfish::Color color) {
  const auto own = position.pieces(color);
  const auto attacks = all_attacks(position, color) & ~own;
  const auto activity = active_attacks(position, color) & ~own;
  return {
      .material_cp = material_cp(position, color),
      .minor_pieces_off_home = minor_pieces_off_home(position, color),
      .mobility_squares = Stockfish::popcount(attacks),
      .activity_squares = Stockfish::popcount(activity),
      .space_squares = Stockfish::popcount(attacks & enemy_half(color)),
      .defended_pieces = defended_pieces(position, color),
      .semi_open_files = semi_open_files(position, color),
      .strategic = extract_strategic_features(position, color),
      .weakness_facts = extract_position_weakness_facts(position, color),
  };
}

nlohmann::json side_json(const SidePositionFeatures& side) {
  const auto& strategic = side.strategic;
  return {
      {"materialCp", side.material_cp},
      {"minorPiecesOffHome", side.minor_pieces_off_home},
      {"mobilitySquares", side.mobility_squares},
      {"activitySquares", side.activity_squares},
      {"spaceSquares", side.space_squares},
      {"defendedPieces", side.defended_pieces},
      {"semiOpenFiles", side.semi_open_files},
      {"pawnStructure", {
          {"isolated", strategic.pawns.isolated_pawns},
          {"doubled", strategic.pawns.doubled_pawns},
          {"connected", strategic.pawns.connected_pawns},
          {"passedSquares", strategic.pawns.passed_pawn_squares},
      }},
      {"weakSquareCandidates", strategic.weak_square_candidates},
      {"outpostCandidates", strategic.outpost_candidates},
      {"badPieceCandidates", strategic.bad_piece_candidates},
      {"badKnightCandidates", strategic.bad_knight_candidates},
      {"badBishopCandidates", strategic.bad_bishop_candidates},
      {"kingSafety", {
          {"kingSquare", strategic.king.king_square},
          {"pawnShield", strategic.king.pawn_shield},
          {"exposedFiles", strategic.king.exposed_files},
          {"attackedZoneSquares", strategic.king.attacked_zone_squares},
          {"enemyAttackers", strategic.king.enemy_attackers},
      }},
      {"colorComplex", {
          {"lightSquarePawns", strategic.light_square_pawns},
          {"darkSquarePawns", strategic.dark_square_pawns},
          {"lightSquareBishops", strategic.light_square_bishops},
          {"darkSquareBishops", strategic.dark_square_bishops},
      }},
      {"weaknessFacts", {
          {"backwardPawnCandidates", side.weakness_facts.backward_pawn_candidates},
          {"loosePieceCandidates", side.weakness_facts.loose_piece_candidates},
          {"unprotectedPawnCandidates",
           side.weakness_facts.unprotected_pawn_candidates},
          {"overloadedDefenderCandidates",
           side.weakness_facts.overloaded_defender_candidates},
          {"weakBackRankCandidate", side.weakness_facts.weak_back_rank_candidate},
      }},
  };
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Public extraction
// -----------------------------------------------------------------------------

PositionFeatures PositionFeatureExtractor::extract(const std::string& fen) const {
  const auto validation = kchess::validate_fen(fen);
  if (!validation.valid) throw std::invalid_argument(validation.error);

  kchess::initialize_stockfish_runtime();
  std::deque<Stockfish::StateInfo> states(1);
  Stockfish::Position position;
  position.set(validation.normalized, false, &states.back());

  return {
      .white_to_move = position.side_to_move() == Stockfish::WHITE,
      .side_to_move_legal_moves = static_cast<int>(
          Stockfish::MoveList<Stockfish::LEGAL>(position).size()),
      .open_files = open_files(position),
      .white = side_features(position, Stockfish::WHITE),
      .black = side_features(position, Stockfish::BLACK),
  };
}

EvidenceItem PositionFeatureExtractor::evidence(const std::string& fen) const {
  return evidence(extract(fen));
}

EvidenceItem PositionFeatureExtractor::evidence(
    const PositionFeatures& features) const {
  nlohmann::json payload{
      {"version", 3},
      {"sideToMove", features.white_to_move ? "white" : "black"},
      {"sideToMoveLegalMoves", features.side_to_move_legal_moves},
      {"openFiles", features.open_files},
      {"white", side_json(features.white)},
      {"black", side_json(features.black)},
  };
  return {
      .id = "position.features.v1",
      .kind = EvidenceKind::position_features,
      .payload = payload.dump(),
      .confidence = 1.0,
  };
}

}  // namespace kchess::ai
