#pragma once

#include <string>
#include <vector>

#include "../dto/evidence.h"
#include "../dto/structured_coach_response.h"

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Deterministic chess-claim validation
// -----------------------------------------------------------------------------

struct ChessValidationResult {
  bool valid{true};
  std::string error_code;
};

class ChessValidator {
 public:
  [[nodiscard]] ChessValidationResult validate_claim(
      const CoachClaim& claim,
      const std::string& fen,
      const std::vector<EvidenceItem>& evidence) const;

  [[nodiscard]] ChessValidationResult validate_recommendation(
      const CoachRecommendation& recommendation,
      const std::string& fen,
      const std::vector<EvidenceItem>& evidence) const;
};

}  // namespace kchess::ai
