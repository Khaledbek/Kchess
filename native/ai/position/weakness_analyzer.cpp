#include "weakness_analyzer.h"

#include <algorithm>
#include <utility>

#include <nlohmann/json.hpp>

namespace kchess::ai {
namespace {

// -----------------------------------------------------------------------------
// Section: Scoring helpers
// -----------------------------------------------------------------------------

double clamp_score(double value) {
  return std::clamp(value, 0.0, 1.0);
}

void add_count_weakness(SideWeaknesses& out, WeaknessKind kind, int count,
                        double base, double confidence) {
  if (count <= 0) return;
  out.items.push_back({
      .kind = kind,
      .count = count,
      .severity = clamp_score(base + 0.08 * static_cast<double>(count - 1)),
      .confidence = confidence,
  });
}

void add_square_weaknesses(SideWeaknesses& out, WeaknessKind kind,
                           const std::vector<int>& squares, double severity,
                           double confidence) {
  for (const int square : squares) {
    out.items.push_back({
        .kind = kind,
        .square = square,
        .severity = severity,
        .confidence = confidence,
    });
  }
}

bool light_square(int square) {
  const int file = square % 8;
  const int rank = square / 8;
  return ((file + rank) & 1) != 0;
}

int weak_squares_on_color(const StrategicSideFeatures& strategic, bool light) {
  return static_cast<int>(std::count_if(
      strategic.weak_square_candidates.begin(),
      strategic.weak_square_candidates.end(),
      [light](int square) { return light_square(square) == light; }));
}

void add_color_complex_weaknesses(const StrategicSideFeatures& strategic,
                                  SideWeaknesses& out) {
  const int light_weak = weak_squares_on_color(strategic, true);
  const int dark_weak = weak_squares_on_color(strategic, false);

  if (strategic.light_square_bishops == 0 && light_weak >= 2) {
    out.items.push_back({
        .kind = WeaknessKind::weak_color_complex,
        .count = light_weak,
        .severity = clamp_score(0.42 + 0.07 * light_weak),
        .confidence = 0.68,
        .detail = "light",
    });
  }
  if (strategic.dark_square_bishops == 0 && dark_weak >= 2) {
    out.items.push_back({
        .kind = WeaknessKind::weak_color_complex,
        .count = dark_weak,
        .severity = clamp_score(0.42 + 0.07 * dark_weak),
        .confidence = 0.68,
        .detail = "dark",
    });
  }
}

// -----------------------------------------------------------------------------
// Section: Side interpretation
// -----------------------------------------------------------------------------

SideWeaknesses analyze_side(const SidePositionFeatures& side,
                            const SidePositionFeatures& opponent) {
  SideWeaknesses out;
  const auto& strategic = side.strategic;
  const auto& facts = side.weakness_facts;

  add_count_weakness(out, WeaknessKind::isolated_pawn,
                     strategic.pawns.isolated_pawns, 0.42, 0.96);
  add_count_weakness(out, WeaknessKind::doubled_pawn,
                     strategic.pawns.doubled_pawns, 0.40, 0.96);

  add_square_weaknesses(out, WeaknessKind::backward_pawn,
                        facts.backward_pawn_candidates, 0.48, 0.76);
  add_square_weaknesses(out, WeaknessKind::weak_square,
                        strategic.weak_square_candidates, 0.46, 0.74);
  add_square_weaknesses(out, WeaknessKind::bad_knight,
                        strategic.bad_knight_candidates, 0.45, 0.72);
  add_square_weaknesses(out, WeaknessKind::bad_bishop,
                        strategic.bad_bishop_candidates, 0.45, 0.72);
  add_square_weaknesses(out, WeaknessKind::loose_piece,
                        facts.loose_piece_candidates, 0.62, 0.90);
  add_square_weaknesses(out, WeaknessKind::unprotected_pawn,
                        facts.unprotected_pawn_candidates, 0.38, 0.94);
  add_square_weaknesses(out, WeaknessKind::overloaded_defender,
                        facts.overloaded_defender_candidates, 0.64, 0.72);

  const auto& king = strategic.king;
  const bool king_weak = king.exposed_files >= 2 ||
                         king.attacked_zone_squares >= 3 ||
                         (king.enemy_attackers >= 2 && king.pawn_shield <= 1);
  if (king_weak) {
    const double severity = 0.38 + 0.10 * king.exposed_files +
                            0.05 * king.attacked_zone_squares +
                            0.07 * king.enemy_attackers -
                            0.04 * king.pawn_shield;
    out.items.push_back({
        .kind = WeaknessKind::weak_king,
        .square = king.king_square,
        .severity = clamp_score(severity),
        .confidence = 0.80,
    });
  }

  if (facts.weak_back_rank_candidate) {
    out.items.push_back({
        .kind = WeaknessKind::weak_back_rank,
        .square = king.king_square,
        .severity = 0.70,
        .confidence = 0.78,
    });
  }

  if (opponent.space_squares - side.space_squares >= 6) {
    out.items.push_back({
        .kind = WeaknessKind::lack_of_space,
        .count = opponent.space_squares - side.space_squares,
        .severity = clamp_score(
            0.40 + 0.03 * (opponent.space_squares - side.space_squares)),
        .confidence = 0.78,
    });
  }

  if (opponent.minor_pieces_off_home - side.minor_pieces_off_home >= 2) {
    out.items.push_back({
        .kind = WeaknessKind::development_deficit,
        .count = opponent.minor_pieces_off_home - side.minor_pieces_off_home,
        .severity = 0.58,
        .confidence = 0.90,
    });
  }

  add_color_complex_weaknesses(strategic, out);
  return out;
}

// -----------------------------------------------------------------------------
// Section: Evidence serialization
// -----------------------------------------------------------------------------

const char* weakness_name(WeaknessKind kind) {
  switch (kind) {
    case WeaknessKind::weak_king: return "weak_king";
    case WeaknessKind::backward_pawn: return "backward_pawn";
    case WeaknessKind::isolated_pawn: return "isolated_pawn";
    case WeaknessKind::doubled_pawn: return "doubled_pawn";
    case WeaknessKind::weak_square: return "weak_square";
    case WeaknessKind::weak_color_complex: return "weak_color_complex";
    case WeaknessKind::loose_piece: return "loose_piece";
    case WeaknessKind::bad_bishop: return "bad_bishop";
    case WeaknessKind::bad_knight: return "bad_knight";
    case WeaknessKind::unprotected_pawn: return "unprotected_pawn";
    case WeaknessKind::weak_back_rank: return "weak_back_rank";
    case WeaknessKind::overloaded_defender: return "overloaded_defender";
    case WeaknessKind::lack_of_space: return "lack_of_space";
    case WeaknessKind::development_deficit: return "development_deficit";
  }
  return "unknown";
}

nlohmann::json side_json(const SideWeaknesses& side) {
  nlohmann::json result = nlohmann::json::array();
  for (const auto& weakness : side.items) {
    nlohmann::json item{
        {"type", weakness_name(weakness.kind)},
        {"count", weakness.count},
        {"severity", weakness.severity},
        {"confidence", weakness.confidence},
    };
    if (weakness.square >= 0) item["square"] = weakness.square;
    if (!weakness.detail.empty()) item["detail"] = weakness.detail;
    result.push_back(std::move(item));
  }
  return result;
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Public analysis
// -----------------------------------------------------------------------------

PositionWeaknesses WeaknessAnalyzer::analyze(
    const PositionFeatures& features) const {
  return {
      .white = analyze_side(features.white, features.black),
      .black = analyze_side(features.black, features.white),
  };
}

EvidenceItem WeaknessAnalyzer::evidence(const PositionFeatures& features) const {
  return evidence(analyze(features));
}

EvidenceItem WeaknessAnalyzer::evidence(
    const PositionWeaknesses& weaknesses) const {
  const nlohmann::json payload{
      {"version", 1},
      {"white", side_json(weaknesses.white)},
      {"black", side_json(weaknesses.black)},
  };
  return {
      .id = "position.weaknesses.v1",
      .kind = EvidenceKind::position_weaknesses,
      .payload = payload.dump(),
      .confidence = 0.82,
  };
}

}  // namespace kchess::ai
