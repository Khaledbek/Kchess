// -----------------------------------------------------------------------------
// Section: Native move application and terminal state
// -----------------------------------------------------------------------------

#pragma once

#include <optional>
#include <string>
#include <vector>

namespace kchess {

struct AppliedMove {
  std::string uci;
  std::string san;
  std::string fen_after;
};

struct PositionOutcome {
  bool terminal{false};
  bool checkmate{false};
  std::string result{"*"};
};

// Applies one legal UCI move to a FEN without mutating any stored game.
// Throws std::invalid_argument when the position or move is invalid/illegal.
AppliedMove apply_legal_uci_move(const std::string& fen, const std::string& uci);

// Resolves a SAN token against all legal moves in the supplied position.
// Returns null when the token is not legal or is ambiguous in that position.
std::optional<AppliedMove> find_legal_san_move(
    const std::string& fen, const std::string& san);

// Returns legal promotion suffixes (q/r/b/n) for a source/target pair.
// An empty vector means the move does not require a promotion choice.
std::vector<std::string> legal_promotion_choices(
    const std::string& fen,
    const std::string& source,
    const std::string& target);

// Detects terminal no-move states and the fifty-move rule from a standalone FEN.
// Repetition claims require full history and are intentionally not inferred here.
PositionOutcome position_outcome(const std::string& fen);

}  // namespace kchess
