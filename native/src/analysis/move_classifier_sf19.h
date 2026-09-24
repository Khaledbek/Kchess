#pragma once

#include "analysis/move_classifier.h"

namespace kchess {

// -----------------------------------------------------------------------------
// Section: Stockfish 19 classifier model
// -----------------------------------------------------------------------------

// Stockfish 19 keeps its own positive-label/rank calibration, but negative
// Mistake/Blunder severity is shared with SF18: normalized WDL/expected-score
// loss first, CP only as a compatibility fallback when outcome data is absent.
struct MoveClassifierSf19Config {
  static constexpr int version = 1910;

  // Equivalent alternatives are also Best. A published SF19 rank-1 move is
  // authoritative inside its own exact root snapshot. Independent post-move
  // verification may mark the combined evidence unstable, but must never turn
  // that same published rank-1 move into a negative classification.
  double equivalent_value_loss{0.015};
  int equivalent_cp_loss{20};

  // SF19 cross-snapshot stability thresholds. These belong to the service-level
  // coherence guard, not to severity. A contradiction means "inconclusive /
  // verify again", never "the engine's own published best move was a blunder".
  int bestmove_verification_depth_slack{2};
  int bestmove_hard_min_after_depth{6};
  double bestmove_contradiction_value_loss{0.08};
  int bestmove_contradiction_cp_loss{120};
  double bestmove_hard_contradiction_value_loss{0.18};
  int bestmove_hard_contradiction_cp_loss{250};

  // Positive quality bands are cluster thresholds, not fixed rank labels.
  // Any MultiPV alternative inside the equivalent band is Best; alternatives
  // just outside it can still be Excellent regardless of whether they are
  // rank 2, 3, 4, or 5.
  double excellent_value_loss{0.08};
  int excellent_cp_loss{90};
  double good_value_loss{0.13};
  int good_cp_loss{140};
  double okay_value_loss{0.19};
  int okay_cp_loss{210};

  // Critical/Great: rank 1 is isolated from the next quality cluster. When both
  // normalized value and CP exist, both must confirm the gap.
  double critical_value_gap{0.04};
  int critical_cp_gap{50};

  // Brilliant: a real non-pawn material concession is accepted or consciously
  // left available, at least ~2 pawns of net material are at stake, and the
  // independently confirmed move keeps/creates a sound result.
  int brilliant_piece_min_cp{300};
  int brilliant_net_sacrifice_min_cp{201};
  double brilliant_min_value{0.62};
  double brilliant_decided_ceiling{0.96};
  int brilliant_confirmation_depth_slack{4};
  double brilliant_confirmation_value_loss{0.055};
  int brilliant_confirmation_cp_loss{90};

  // Miss: a concrete advantage/opportunity was available, but the played move
  // did not materially damage itself enough to be a Mistake/Blunder.
  double miss_value_loss{0.055};
  int miss_cp_loss{100};
  double miss_min_best_value{0.58};
  int miss_material_opportunity_cp{100};

  // Material remains supporting evidence for distinguishing a missed
  // opportunity from self-inflicted damage. Negative severity itself is shared
  // with SF18 and is driven by normalized expected-score loss first.
  int mistake_material_loss_cp{100};
};

struct MoveClassifierSf19Input {
  MoveAnalysisFacts common;

  // Brilliant is intentionally stricter than plain Best. The independent
  // resulting-position analysis must confirm that the claimed best sacrifice
  // remains sound. Missing/unverified evidence therefore falls back to Best or
  // Critical rather than inventing a Brilliant label.
  bool best_move_verified_after{false};

};

MoveCategory classify_move_sf19(
    const MoveClassifierSf19Input& input,
    const MoveClassifierSf19Config& config = {});

}  // namespace kchess
