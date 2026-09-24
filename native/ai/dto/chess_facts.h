#pragma once

#include <optional>
#include <string>
#include <vector>

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Provider-neutral verified chess facts
// -----------------------------------------------------------------------------

// ChessFact is a native truth-unit. Providers may later reference fact_id, but
// they must never manufacture or mutate the chess payload behind that ID.
enum class ChessFactKind {
  candidate_move,
  candidate_rank,
  evaluation_cp,
  mate_distance,
  expected_score,
  principal_variation,
  critical_reply,
  analysis_focus,
};

struct ChessFact {
  // Stable within the evidence contract, for example
  // fact.candidate.best.move or fact.candidate.worst.evaluation_cp.
  std::string fact_id;
  ChessFactKind kind{ChessFactKind::candidate_move};

  // Candidate identity is the authority boundary used by the Coach. Concrete
  // move notation remains native data associated with this identity.
  std::string candidate_id;
  std::string move_uci;
  std::vector<std::string> pv_uci;

  std::optional<int> integer_value;
  std::optional<double> decimal_value;
  std::string text_value;
};

}  // namespace kchess::ai
