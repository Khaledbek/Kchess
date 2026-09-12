#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/models.h"
#include "engine/chess_engine.h"

namespace kchess {

// -----------------------------------------------------------------------------
// Section: Classifier model
// -----------------------------------------------------------------------------

struct MoveClassifierConfig {
  // V12 keeps the V11 severity model but binds Best/alternative comparisons to
  // the same published rank-1 engine snapshot used by the board arrow. No
  // severe label is produced from a single evaluation-bar threshold alone.
  static constexpr int version = 12;

  // Engine-equivalent moves. Stockfish's final bestmove is always rank 1, but
  // an alternative with effectively the same score is also treated as Best.
  double equivalent_loss{0.006};
  int equivalent_cp_loss{12};

  // Positive-quality bands used when rank information is unavailable. When
  // ranks 2/3/4 are known, they map to Excellent/Good/Okay if the score remains
  // inside the light-deviation envelope.
  double excellent_loss{0.035};
  int excellent_cp_loss{70};
  double good_loss{0.065};
  int good_cp_loss{120};
  double okay_loss{0.10};
  int okay_cp_loss{180};

  // Critical/Great means the engine-best move is genuinely narrow: the second
  // choice must fall clearly behind in expected result, centipawns, or mate.
  double critical_gap{0.10};
  int critical_cp_gap{120};
  // Brilliant requires a verified non-pawn piece sacrifice against best
  // defence plus a sound resulting advantage. A pawn sacrifice alone can still
  // be Best/Critical, but never Brilliant.
  int brilliant_piece_min_cp{300};
  int brilliant_net_sacrifice_min_cp{201};
  double brilliant_min_best_score{0.60};
  double brilliant_decided_score_ceiling{0.97};
  int brilliant_min_best_cp{100};

  // Miss means a real opportunity was left on the board without the played
  // move itself dropping material. This covers examples such as +2 -> +1.
  double miss_loss{0.05};
  int miss_cp_loss{80};
  int miss_min_best_cp{50};
  int miss_material_opportunity_cp{100};

  // Mistake requires a meaningful evaluation deterioration plus independent
  // board/rank evidence (usually material loss or dropping outside top choices).
  int mistake_material_loss_cp{100};
  double mistake_loss{0.05};
  int mistake_cp_loss{90};

  // Blunder is reserved for newly hanging mate, losing more than ~2 pawns of
  // material with a real evaluation drop, or a catastrophic result-band flip.
  int blunder_material_loss_cp{201};
  double blunder_loss{0.10};
  int blunder_cp_loss{180};
  double blunder_outcome_loss{0.20};
  double blunder_severe_loss{0.30};

  // Compatibility aliases used by the adaptive pre-analysis uncertainty test.
  double brilliant_gap{0.10};
  double miss_unique_gap{0.10};
};

struct PositionEvaluation {
  std::optional<WdlScore> wdl;
  std::optional<int> evaluation_cp;
  std::optional<int> mate_in;
};

struct VerifiedSacrifice {
  bool verified{false};
  int offered_piece_value_cp{0};
  int net_material_loss_cp{0};
};

struct PvMaterialEvidence {
  int maximum_gain_cp{0};
  int maximum_loss_cp{0};
  int final_delta_cp{0};
};

struct MoveClassifierInput {
  bool theory{false};
  // Authoritative final Stockfish bestmove for this root.
  bool played_is_best{false};
  bool sacrifice_against_best_defense{false};
  bool only_move_tactical{false};
  bool missed_forced_mate{false};
  bool allowed_forced_mate{false};
  bool was_in_check_before_move{false};
  int legal_move_count{0};

  // 1..4 when the move is present in the classification MultiPV search. 5 means
  // "outside the top four". 0 means that no reliable rank was available.
  int played_rank{0};

  // Board-understanding evidence, measured in centipawn material units.
  int sacrifice_piece_value_cp{0};
  int sacrifice_net_material_loss_cp{0};
  int material_loss_cp{0};
  int best_material_gain_cp{0};
  int played_material_gain_cp{0};

  std::optional<double> best_expected_score;
  std::optional<double> played_expected_score;
  std::optional<double> second_best_expected_score;
  // Scores are always from the mover/root side perspective.
  std::optional<int> best_evaluation_cp;
  std::optional<int> played_evaluation_cp;
  std::optional<int> second_best_evaluation_cp;
  std::optional<int> best_mate_in;
  std::optional<int> played_mate_in;
  std::optional<int> second_best_mate_in;
};

struct PositionContext {
  int legal_move_count{0};
  bool in_check{false};
};

// -----------------------------------------------------------------------------
// Section: Evaluation and classification
// -----------------------------------------------------------------------------

std::optional<double> expected_score_side_to_move(const PositionEvaluation& evaluation);
std::optional<double> expected_score_mover_after_move(const PositionEvaluation& evaluation);
std::optional<double> expected_score_loss(
    const std::optional<double>& best, const std::optional<double>& played);
MoveCategory classify_move(
    const MoveClassifierInput& input,
    const MoveClassifierConfig& config = {});
std::string move_category_name(MoveCategory category);

// -----------------------------------------------------------------------------
// Section: Position evidence
// -----------------------------------------------------------------------------

bool root_move_is_tactical(const std::string& fen_before, const std::string& uci_move);
VerifiedSacrifice root_move_sacrifice_evidence(
    const std::string& fen_before,
    const std::vector<std::string>& principal_variation);
bool root_move_sacrifice_against_best_defense(
    const std::string& fen_before,
    const std::vector<std::string>& principal_variation,
    int minimum_material_loss_cp = 150);
PvMaterialEvidence pv_material_evidence(
    const std::string& fen_before,
    const std::vector<std::string>& principal_variation,
    int max_plies = 6);
bool material_sacrifice_in_pv(
    const std::string& fen_before,
    const std::vector<std::string>& principal_variation,
    int max_plies = 4);
PositionContext position_context(const std::string& fen);

}  // namespace kchess
