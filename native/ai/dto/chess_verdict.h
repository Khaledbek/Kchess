#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Native chess verdict contract
// -----------------------------------------------------------------------------

// These labels are native judgments. The remote provider may explain them but
// must never choose, soften or reverse them from conversation tone alone.
enum class MoveVerdict {
  unknown,
  best,
  excellent,
  good,
  playable,
  inaccuracy,
  mistake,
  blunder,
  losing,
};

enum class PositionVerdict {
  unknown,
  white_winning,
  white_clearly_better,
  white_slightly_better,
  equal,
  black_slightly_better,
  black_clearly_better,
  black_winning,
};

inline constexpr std::string_view move_verdict_name(const MoveVerdict verdict) {
  switch (verdict) {
    case MoveVerdict::best: return "best";
    case MoveVerdict::excellent: return "excellent";
    case MoveVerdict::good: return "good";
    case MoveVerdict::playable: return "playable";
    case MoveVerdict::inaccuracy: return "inaccuracy";
    case MoveVerdict::mistake: return "mistake";
    case MoveVerdict::blunder: return "blunder";
    case MoveVerdict::losing: return "losing";
    case MoveVerdict::unknown: return "unknown";
  }
  return "unknown";
}

inline constexpr std::string_view position_verdict_name(
    const PositionVerdict verdict) {
  switch (verdict) {
    case PositionVerdict::white_winning: return "white_winning";
    case PositionVerdict::white_clearly_better: return "white_clearly_better";
    case PositionVerdict::white_slightly_better: return "white_slightly_better";
    case PositionVerdict::equal: return "equal";
    case PositionVerdict::black_slightly_better: return "black_slightly_better";
    case PositionVerdict::black_clearly_better: return "black_clearly_better";
    case PositionVerdict::black_winning: return "black_winning";
    case PositionVerdict::unknown: return "unknown";
  }
  return "unknown";
}

struct ChessVerdictContract {
  // True only when native engine/classification evidence is sufficient for a
  // binding judgment. An absent/unknown verdict is not permission to guess.
  bool authoritative{false};

  // current_position when the evaluation describes the root position;
  // after_evaluated_move when it describes the selected/played move result.
  std::string position_scope{"current_position"};
  PositionVerdict position_verdict{PositionVerdict::unknown};
  MoveVerdict move_verdict{MoveVerdict::unknown};
  std::optional<std::string> evaluated_move_uci;

  // White-perspective values mirror provider-visible engine candidate facts.
  std::optional<int> white_evaluation_cp;
  std::optional<int> white_mate_in;

  // Side-to-move loss against the native best candidate when both are known.
  std::optional<int> move_loss_cp;
  std::optional<double> move_loss_expected_score;

  // Stable native provenance label for diagnostics/provider policy.
  std::string basis;
};

// -----------------------------------------------------------------------------
// Section: Native verdict challenge/review contract
// -----------------------------------------------------------------------------

enum class VerdictReviewOutcome {
  none,
  unchanged,
  changed,
  inconclusive,
};

inline constexpr std::string_view verdict_review_outcome_name(
    const VerdictReviewOutcome outcome) {
  switch (outcome) {
    case VerdictReviewOutcome::unchanged: return "unchanged";
    case VerdictReviewOutcome::changed: return "changed";
    case VerdictReviewOutcome::inconclusive: return "inconclusive";
    case VerdictReviewOutcome::none: return "none";
  }
  return "none";
}

struct ChessVerdictChallenge {
  ChessVerdictContract previous_verdict;
  std::optional<std::string> challenged_move_uci;
  std::optional<std::string> analysis_fen;
  std::string trigger{"user_objection"};
};

struct ChessVerdictReview {
  bool active{false};
  ChessVerdictContract previous_verdict;
  VerdictReviewOutcome outcome{VerdictReviewOutcome::none};
  std::optional<std::string> challenged_move_uci;
  std::string trigger;
};

}  // namespace kchess::ai
