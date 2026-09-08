#pragma once

#include "analysis/move_classifier.h"

namespace kchess {

// -----------------------------------------------------------------------------
// Section: Stockfish 19 classifier model
// -----------------------------------------------------------------------------

// Stockfish 19 uses a separately calibrated WDL model and its score distribution
// differs enough from Stockfish 18 that KChess must not reuse V11's WDL-driven
// severity gates verbatim.  SF19 classification therefore uses a stable
// CP/mate decision-value scale plus board evidence, while SF18 keeps the
// existing MoveClassifierConfig V11 unchanged.
struct MoveClassifierSf19Config {
  static constexpr int version = 1902;

  // Equivalent alternatives are also Best.  The final Stockfish bestmove is
  // always authoritative; these limits only promote a non-rank-1 alternative.
  double equivalent_value_loss{0.015};
  int equivalent_cp_loss{20};

  // Rank-aware light-deviation bands. Rank 2/3/4 maps to
  // Excellent/Good/Okay only while the actual decision loss is small.
  double excellent_value_loss{0.08};
  int excellent_cp_loss{90};
  double good_value_loss{0.13};
  int good_cp_loss{140};
  double okay_value_loss{0.19};
  int okay_cp_loss{210};

  // Critical/Great: rank 1 is meaningfully better than rank 2.
  double critical_value_gap{0.10};
  int critical_cp_gap{160};

  // Brilliant: a real non-pawn piece is offered to the best defence, at least
  // ~2 pawns of net material are given, and the move keeps/creates an advantage.
  int brilliant_piece_min_cp{300};
  int brilliant_net_sacrifice_min_cp{201};
  double brilliant_min_value{0.62};
  double brilliant_decided_ceiling{0.96};

  // Miss: a concrete advantage/opportunity was available, but the played move
  // did not materially damage itself enough to be a Mistake/Blunder.
  double miss_value_loss{0.055};
  int miss_cp_loss{100};
  double miss_min_best_value{0.58};
  int miss_material_opportunity_cp{100};

  // Mistake requires either sustained material damage plus a real score loss,
  // or a clearly inferior outside-top-four decision.  Temporary exchanges in a
  // PV are not treated as material loss.
  int mistake_material_loss_cp{100};
  double mistake_value_loss{0.055};
  int mistake_cp_loss{100};
  double positional_mistake_value_loss{0.075};

  // Blunder: newly hanging mate, sustained loss of >2 pawns with a meaningful
  // score deterioration, or a catastrophic result swing even if the short PV
  // has not yet resolved the material consequence.
  int blunder_material_loss_cp{201};
  double blunder_material_value_loss{0.075};
  int blunder_cp_loss{180};
  double catastrophic_value_loss{0.32};
  double winning_collapse_best_value{0.80};
  double winning_collapse_played_value{0.65};
  double winning_collapse_loss{0.18};
};

struct MoveClassifierSf19Input {
  MoveClassifierInput common;

  // Net material at the end of the analyzed PV, from the mover's perspective.
  // These are deliberately separate from V11's maximum transient PV loss: an
  // exchange such as BxB QxB must not look like a dropped bishop just because
  // the intermediate half-move temporarily removed one piece.
  int played_final_material_delta_cp{0};
  int best_final_material_delta_cp{0};
};

MoveCategory classify_move_sf19(
    const MoveClassifierSf19Input& input,
    const MoveClassifierSf19Config& config = {});

}  // namespace kchess
