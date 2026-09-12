#pragma once

#include <string>
#include <vector>

#include "../dto/evidence.h"
#include "position_structure_features.h"
#include "position_weakness_facts.h"

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Position feature DTOs
// -----------------------------------------------------------------------------

struct SidePositionFeatures {
  int material_cp{0};
  int minor_pieces_off_home{0};
  int mobility_squares{0};
  int activity_squares{0};
  int space_squares{0};
  int defended_pieces{0};
  std::vector<int> semi_open_files;
  StrategicSideFeatures strategic;
  PositionWeaknessFacts weakness_facts;
};

struct PositionFeatures {
  bool white_to_move{true};
  int side_to_move_legal_moves{0};
  std::vector<int> open_files;
  SidePositionFeatures white;
  SidePositionFeatures black;
};

// -----------------------------------------------------------------------------
// Section: Deterministic extractor
// -----------------------------------------------------------------------------

class PositionFeatureExtractor {
 public:
  [[nodiscard]] PositionFeatures extract(const std::string& fen) const;
  [[nodiscard]] EvidenceItem evidence(const std::string& fen) const;
  [[nodiscard]] EvidenceItem evidence(const PositionFeatures& features) const;
};

}  // namespace kchess::ai
