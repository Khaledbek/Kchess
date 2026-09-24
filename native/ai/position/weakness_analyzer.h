#pragma once

#include <string>
#include <vector>

#include "../dto/evidence.h"
#include "position_features.h"

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Weakness DTOs
// -----------------------------------------------------------------------------

enum class WeaknessKind {
  weak_king,
  backward_pawn,
  isolated_pawn,
  doubled_pawn,
  weak_square,
  weak_color_complex,
  loose_piece,
  bad_bishop,
  bad_knight,
  unprotected_pawn,
  weak_back_rank,
  overloaded_defender,
  lack_of_space,
  development_deficit,
};

struct PositionWeakness {
  WeaknessKind kind{WeaknessKind::weak_square};
  int square{-1};
  int count{1};
  double severity{0.0};
  double confidence{1.0};
  std::string detail;
};

struct SideWeaknesses {
  std::vector<PositionWeakness> items;
};

struct PositionWeaknesses {
  SideWeaknesses white;
  SideWeaknesses black;
};

// -----------------------------------------------------------------------------
// Section: Feature interpretation
// -----------------------------------------------------------------------------

class WeaknessAnalyzer {
 public:
  [[nodiscard]] PositionWeaknesses analyze(
      const PositionFeatures& features) const;
  [[nodiscard]] EvidenceItem evidence(const PositionFeatures& features) const;
  [[nodiscard]] EvidenceItem evidence(const PositionWeaknesses& weaknesses) const;
};

}  // namespace kchess::ai
