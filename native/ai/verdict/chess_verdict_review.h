#pragma once

#include <optional>

#include "../dto/chess_verdict.h"
#include "../dto/coach_request.h"

namespace kchess::ai {

struct CoachSessionState;

[[nodiscard]] std::optional<ChessVerdictChallenge> resolve_verdict_challenge(
    const CoachRequest& request, const CoachSessionState* session);

[[nodiscard]] std::optional<ChessVerdictReview> review_chess_verdict(
    const std::optional<ChessVerdictChallenge>& challenge,
    const std::optional<ChessVerdictContract>& current_verdict);

}  // namespace kchess::ai
