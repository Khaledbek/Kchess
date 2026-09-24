#pragma once

#include <optional>

#include "../dto/candidate_moves.h"
#include "../dto/chess_verdict.h"
#include "../dto/coach_request.h"
#include "../dto/query_plan.h"

namespace kchess::ai {

// Builds one native verdict from already verified candidate/classification data.
// It performs no engine work and never infers a chess judgment from user prose.
[[nodiscard]] std::optional<ChessVerdictContract> build_chess_verdict(
    const CoachRequest& request,
    const QueryPlan& plan,
    const std::optional<CandidateMoveSet>& candidates);

}  // namespace kchess::ai
