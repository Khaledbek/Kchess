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
      const std::vector<EvidenceItem>& evidence) const;

  // Removes only invalid structured board/move metadata from an already
  // repaired provider response. The natural-language answer is left intact.
  [[nodiscard]] ResponseValidationReport sanitize_metadata(
      StructuredCoachContent& content,
      const std::optional<std::string>& position_fen,
      const std::vector<EvidenceItem>& evidence) const;
};

}  // namespace kchess::ai
