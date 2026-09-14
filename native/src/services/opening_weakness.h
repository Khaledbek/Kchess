#pragma once

// -----------------------------------------------------------------------------
// Section: Opening weakness detection (pure; no database or JSON)
// -----------------------------------------------------------------------------

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "services/statistics_domain.h"

namespace kchess::statistics {

// Base opening family: everything before the first ':' or ',' (trimmed).
// "Scandinavian Defense: Mieses-Kotroc Variation" -> "Scandinavian Defense".
std::string opening_family(const std::string& name);

// The widest opening phase any game can have; see opening_phase_end_ply.
constexpr int kOpeningPhaseMaxPly = 40;

// Last ply (exclusive) that still counts as the opening for a game whose named
// line ends at opening_ply: the named moves plus ten more moves for each side,
// which is the ground a full opening drill covers, bounded to plies 20..40 so
// an unnamed or very long line is still judged on a sensible stretch.
int opening_phase_end_ply(std::optional<int> opening_ply);

// One move of the profile's own that the engine flagged in the opening phase.
struct OpeningMoveError {
  int ply{0};                 // 0-based, 0 is White's first move
  std::string category;       // mistake | blunder | miss
  std::string san;            // what was played
  std::string position;       // placement, side, castling, en passant before it
  std::string recommended;    // the engine's move in notation; may be empty
};

// Everything one game in a named opening says about the profile.
struct OpeningGameEvidence {
  std::string eco;
  std::string name;           // full opening name
  std::string color;          // white | black
  std::string outcome;        // win | loss | draw | unknown
  bool analysed{false};       // a finished engine analysis exists
  std::vector<OpeningMoveError> errors;  // only the profile's own moves
};

// The same slip in the same position, made in more than one game.
struct RecurringOpeningMistake {
  std::string position;
  std::string san;
  std::string recommended;
  std::string category;       // the worst category it was given
  int move_number{1};
  std::string side;           // white | black
  int count{0};
};

struct OpeningWeakness {
  std::string level;          // variation | family
  std::string name;           // the variation, or the family for a family entry
  std::string family;
  std::string eco;
  std::string color;
  Tally tally;
  int analysed_games{0};
  int games_with_errors{0};   // analysed games with at least one opening error
  int errors{0};
  int blunders{0};
  bool poor_results{false};
  bool frequent_errors{false};
  std::optional<RecurringOpeningMistake> recurring;
  double severity{0.0};
};

// Openings the profile keeps losing or keeps misplaying, worst first.
//
// A line is weak when any of these holds:
//  - poor results: at least 4 decided games, 3 losses, and 60% of them lost;
//  - a recurring mistake: the same flagged move in the same position twice;
//  - frequent errors: opening errors in two thirds of at least 3 analysed
//    games, while losing at least half.
// Frequent errors alone are not enough: nearly every amateur game has one.
// Variations are reported first; a family appears only when it qualifies as a
// whole but none of its variations does (the losses are spread across lines).
std::vector<OpeningWeakness> find_opening_weaknesses(
    const std::vector<OpeningGameEvidence>& games, std::size_t limit);

}  // namespace kchess::statistics
