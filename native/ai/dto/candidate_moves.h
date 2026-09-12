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
};

struct CandidateMoveSnapshot {
  std::vector<CandidateLineInput> root_lines;
  std::optional<CandidateLineInput> user_move_line;
};

// -----------------------------------------------------------------------------
// Section: Candidate move result
// -----------------------------------------------------------------------------

struct CandidateMove {
  int rank{0};
  std::string move_uci;
  std::vector<std::string> pv_uci;
  std::optional<int> evaluation_cp;
  std::optional<int> mate_in;
};

struct CandidateMoveSet {
  std::optional<CandidateMove> best;
  std::vector<CandidateMove> alternatives;
  std::optional<CandidateMove> user_move;
  std::optional<std::string> critical_reply_uci;
};

}  // namespace kchess::ai
