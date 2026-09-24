#pragma once

#include <optional>
#include <string>
#include <vector>

#include "../dto/evidence.h"
#include "../dto/structured_coach_response.h"

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Provider-response validation report
// -----------------------------------------------------------------------------

struct ResponseValidationReport {
  bool valid{true};
  std::vector<std::string> issues;
};

class ResponseValidator {
 public:
  [[nodiscard]] ResponseValidationReport validate(
      const StructuredCoachContent& content,
      const std::optional<std::string>& position_fen,
      const std::vector<EvidenceItem>& evidence,
      const std::vector<EvidenceItem>* supplied_evidence = nullptr,
      bool profile_requested = false,
      bool require_grounded_segments = false,
      const std::optional<ChessVerdictContract>* expected_verdict = nullptr,
      const std::optional<ChessVerdictReview>* expected_review = nullptr) const;

  // Removes only invalid optional board/move metadata from an already repaired
  // provider response. Profile grounding failures remain fatal because dropping
  // metadata cannot make unsupported personal prose truthful.
  [[nodiscard]] ResponseValidationReport sanitize_metadata(
      StructuredCoachContent& content,
      const std::optional<std::string>& position_fen,
      const std::vector<EvidenceItem>& evidence,
      const std::vector<EvidenceItem>* supplied_evidence = nullptr,
      bool profile_requested = false) const;
};

}  // namespace kchess::ai
