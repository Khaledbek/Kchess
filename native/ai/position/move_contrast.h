#pragma once

#include <optional>

#include "../conversation/coach_session.h"
#include "../dto/coach_request.h"
#include "../dto/evidence.h"

namespace kchess::ai {

struct PositionFeatures;

// Factual contrast for a completed answer to an open board question. Optional
// objective fields come only from the matching completed shared-analysis record.
// Never starts a search or infers move quality from static feature proxies.
[[nodiscard]] std::optional<EvidenceItem> completed_move_contrast(
    const CoachRequest& request, const CoachSessionState* session,
    const PositionFeatures* current_features = nullptr,
    const EvidenceItem* completed_analysis = nullptr);

}  // namespace kchess::ai
