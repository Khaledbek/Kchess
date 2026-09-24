#pragma once

#include <string>
#include <vector>

#include "../dto/evidence.h"
#include "../dto/structured_coach_response.h"

namespace kchess::ai {

// Final native trust-boundary renderer for provider output. Provider prose may
// contain only opaque move/fact tokens. After validation succeeds, this class
// resolves those tokens from exact native candidate evidence and verifies that
// each token is actually authorized by the segment's selected Fact IDs (or by
// its typed candidate-bound claim). The provider never owns concrete move
// notation.
class VerifiedFactRenderer {
 public:
  [[nodiscard]] StructuredCoachContent render(
      const StructuredCoachContent& content,
      const std::vector<EvidenceItem>& evidence) const;
};

}  // namespace kchess::ai
