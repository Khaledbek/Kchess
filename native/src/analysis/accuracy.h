#pragma once

#include <optional>
#include <vector>

namespace kchess {

// A single engine evaluation from the perspective of the player who is about
// to make the move being scored.  Centipawns therefore use "positive is good
// for the mover" on both root and post-move samples.
struct AccuracyEvaluation {
  std::optional<int> evaluation_cp;
  std::optional<int> mate_in;
  int depth{0};
};

struct AccuracyConfig {
  // V3 is intentionally independent from move classification.  It scores the
  // regret of a decision from CP/mate data and weights the game average by how
  // demanding the choice was.  Stockfish WDL is deliberately not an input,
  // because WDL saturates in clearly won/lost positions and made many moves
  // look like 100% even when they lost substantial centipawns.
  static constexpr int version = 3;

  // Smooth CP -> decision-value conversion.  This is a continuous quality
  // scale, not a literal claim about real-world win probability.
  double cp_probability_scale{220.0};

  // The same well-behaved move-accuracy curve used by V2, now fed by the
  // continuous V3 decision-value loss instead of Stockfish WDL loss.
  double curve_scale{103.1668100711649};
  double curve_decay{0.04354415386753951};
  double curve_offset{-3.166924740191411};

  // Decision weighting.  Equivalent alternatives are deliberately cheap so a
  // sequence of obvious conversions after an opponent blunder cannot inflate
  // the game score.  Unique/critical choices count much more.
  double minimum_decision_weight{0.04};
  double forced_move_weight{0.01};
  double full_weight_value_gap{0.12};
  int full_weight_cp_gap{180};
  double decided_value_threshold{0.90};
  double decided_easy_weight_factor{0.20};

  // Root restricted-search and post-move analysis are two independent views
  // of the played move.  A clearly deeper post-move search is trusted; at
  // similar depth a materially worse second view is used as a stability guard.
  int deeper_result_margin{4};
  double instability_value_tolerance{0.015};

  // Mate distance remains meaningful without turning M2 vs M3 into a huge
  // accuracy swing.  Positive mate is near 1, negative mate near 0.
  double mate_distance_step{0.0015};
  int mate_distance_cap{40};
};

struct AccuracyMove {
  bool theory{false};
  bool played_is_best{false};
  int legal_move_count{0};

  AccuracyEvaluation best;
  AccuracyEvaluation played_root;
  AccuracyEvaluation played_after;
  AccuracyEvaluation second_best;
};

// Returns a continuous 0..1 decision value for one engine evaluation.
std::optional<double> accuracy_decision_value(
    const AccuracyEvaluation& evaluation,
    const AccuracyConfig& config = {});

// Per-move accuracy and importance are exposed separately so tests and future
// UI can inspect the model without depending on classification labels.
std::optional<double> move_accuracy(
    const AccuracyMove& move,
    const AccuracyConfig& config = {});
double move_accuracy_weight(
    const AccuracyMove& move,
    const AccuracyConfig& config = {});

std::optional<double> game_accuracy(
    const std::vector<AccuracyMove>& moves,
    const AccuracyConfig& config = {});

}  // namespace kchess
