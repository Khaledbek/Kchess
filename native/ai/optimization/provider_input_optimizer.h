#pragma once

#include <cstddef>
#include <vector>

#include "../coach_types.h"
#include "../dto/evidence.h"

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Provider input budget
// -----------------------------------------------------------------------------

[[nodiscard]] std::vector<EvidenceItem> optimize_provider_evidence(
    const std::vector<EvidenceItem>& evidence,
    ResponseDepth depth,
    std::size_t context_tokens);

}  // namespace kchess::ai
