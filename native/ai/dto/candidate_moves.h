#pragma once

#include <optional>
#include <string>
#include <vector>

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Engine-neutral candidate input
// -----------------------------------------------------------------------------

struct CandidateLineInput {
  int rank{0};
  std::string move_uci;
  std::vector<std::string> pv_uci;
  std::optional<int> evaluation_cp;
  std::optional<int> mate_in;
  // Root-side expected score in [0,1], derived from engine WDL when available.
  std::optional<double> expected_score;
};

struct CandidateMoveSnapshot {
  std::vector<CandidateLineInput> root_lines;
  std::optional<CandidateLineInput> user_move_line;
};

// -----------------------------------------------------------------------------
// Section: Candidate move result
// -----------------------------------------------------------------------------

struct CandidateMove {
  std::string candidate_id;
  int rank{0};
  std::string move_uci;
  std::vector<std::string> pv_uci;
  std::optional<int> evaluation_cp;
  std::optional<int> mate_in;
  // Root-side expected score in [0,1], derived from engine WDL when available.
  std::optional<double> expected_score;
};

struct CandidateMoveSet {
  std::optional<CandidateMove> best;
  // For explicitly inverted root analyses (worst move / fastest loss), the
  // selected move is not a recommendation and must not be mislabeled as the
  // engine's "best" candidate. focus_kind is stable provider/debug metadata.
  std::optional<CandidateMove> focus;
  std::string focus_kind;
  std::vector<CandidateMove> alternatives;
  std::optional<CandidateMove> user_move;
  std::optional<std::string> critical_reply_uci;
};

}  // namespace kchess::ai
