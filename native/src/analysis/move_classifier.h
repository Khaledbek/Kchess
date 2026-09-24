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
  // V13 binds Best/alternative comparisons to the published rank-1 snapshot
  // and moves negative severity to the shared outcome-first contract below.
  // No severe label is produced from transient material or CP noise alone.
  static constexpr int version = 18;

  // Engine-equivalent moves. A completed Stockfish final bestmove is the
  // recommendation authority even when the last captured MultiPV ordering is
  // different; score-equivalent alternatives may also classify as Best.
  double equivalent_loss{0.012};
  int equivalent_cp_loss{20};

  // Positive-quality cluster bands. Rank is supporting evidence only: any known
  // top-five alternative inside a band receives that band, while score-equivalent
  // alternatives are promoted to Best regardless of exact rank.
  double excellent_loss{0.035};
  int excellent_cp_loss{70};
  double good_loss{0.065};
  int good_cp_loss{120};
  double okay_loss{0.10};
  int okay_cp_loss{180};

  // Critical/Great means rank 1 is genuinely isolated from the next cluster.
  // When both WDL/expected value and CP are available, both must confirm the
  // separation; a raw score gap in an already saturated position is not enough.
  double critical_gap{0.04};
  int critical_cp_gap{50};
  // Brilliant requires a real non-pawn material concession plus a sound
  // Best/Near-Best result. The concession may be the moved piece itself or a
  // different own piece that the move consciously leaves profitably capturable.
  // A pawn-only concession can still be Best/Critical, but never Brilliant.
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

  // Material evidence is semantic only (Miss vs self-inflicted damage).
  // Severity itself is decided by the shared WDL/expected-score loss contract
  // below, with CP used only when normalized outcome data is unavailable.
  int mistake_material_loss_cp{100};

  // Compatibility aliases used by the adaptive pre-analysis uncertainty test.
  double brilliant_gap{0.10};
};


// Shared negative-severity contract for SF18 and SF19. Stockfish does not
// produce Mistake/Blunder labels; KChess derives them from decision quality.
// WDL/expected score is authoritative whenever available. Centipawns are a
// compatibility fallback for old cache rows or incomplete engine samples only.
enum class MoveSeverity {
  none,
  inaccuracy,
  mistake,
  blunder,
};

struct MoveSeverityConfig {
  // Moderate objective deterioration is an Inaccuracy. Mistake and Blunder
  // are intentionally farther apart so Okay/Inaccuracy/Mistake are not
  // collapsed into one threshold.
  double inaccuracy_expected_loss{0.08};
  double mistake_expected_loss{0.15};
  double blunder_expected_loss{0.25};
  double result_band_blunder_loss{0.18};
  double result_band_best_floor{0.60};
  double result_band_played_ceiling{0.45};
  double collapse_best_floor{0.45};
  double collapse_played_ceiling{0.15};
  int fallback_inaccuracy_cp_loss{180};
  int fallback_mistake_cp_loss{260};
  int fallback_blunder_cp_loss{400};
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
  // Largest temporary material swing in either direction, measured from the
  // root mover's material balance before the PV starts.
  int maximum_gain_cp{0};
  int maximum_loss_cp{0};
  int final_delta_cp{0};

  // Recovery after the deepest material concession in the inspected PV.  A
  // queen sacrifice that reaches -900 and later returns to 0 records 900cp of
  // recovered material.  If the continuation finishes at +300 it records
  // 1200cp, because the mover both recovered the concession and emerged ahead.
  int recovered_from_maximum_loss_cp{0};
  int final_net_loss_cp{0};
  int final_net_gain_cp{0};
  int maximum_loss_ply{0};
  bool fully_recovered{false};
};

// Immediate material exposure created or consciously left available by the
// root move.  This is deliberately broader than VerifiedSacrifice: the
// opponent may profitably capture a different piece than the one that moved.
// Update 3 may later combine this with deeper PV recovery/compensation.
struct MaterialExposureEvidence {
  bool present{false};
  bool immediately_unprotected{false};
  bool other_piece_than_mover{false};
  bool piece_was_already_on_square{false};
  int exposed_piece_value_cp{0};
  int estimated_net_loss_cp{0};
  std::string opponent_capture_uci;
};

// Shared, engine-neutral facts for one completed move decision.  SF18 and SF19
// consume the same record; classifier-specific policy belongs in the classifier
// config, not in a second facts model.  Later tactical/material passes extend
// this structure instead of adding parallel ad-hoc inputs.
struct MoveAnalysisFacts {
  std::string played_move_uci;
  std::string best_move_uci;

  bool theory{false};
  // Authoritative final Stockfish bestmove for this root.
  bool played_is_best{false};
  bool sacrifice_against_best_defense{false};
  bool missed_forced_mate{false};
  bool allowed_forced_mate{false};
  bool was_in_check_before_move{false};
  int legal_move_count{0};

  // 1..5 when the move is present in the classification MultiPV search. 6 means
  // "outside the top five". 0 means that no reliable rank was available.
  int played_rank{0};

  // Board-understanding evidence, measured in centipawn material units.
  int sacrifice_piece_value_cp{0};
  int sacrifice_net_material_loss_cp{0};
  int material_loss_cp{0};
  int best_material_gain_cp{0};
  int played_material_gain_cp{0};
  // Net material at the end of the analyzed continuation, from the mover's
  // perspective.  Keep this distinct from transient capture swings.
  int best_final_material_delta_cp{0};
  int played_final_material_delta_cp{0};

  // Multi-ply material-concession/recovery facts. These describe the whole
  // best-defense continuation rather than only the piece on the destination
  // square. They are intentionally policy-free; Update 4 decides whether the
  // combination is worthy of the Brilliant label.
  int played_material_concession_cp{0};
  int played_material_recovered_cp{0};
  int played_material_net_loss_after_pv_cp{0};
  int played_material_net_gain_after_pv_cp{0};
  bool played_material_fully_recovered{false};

  // Root-position tactical exposure. Unlike the legacy direct-sacrifice fields
  // above, these facts also cover a move that ignores a profitable capture of
  // another own piece (for example a bishop left en prise while attacking).
  bool material_exposure_present{false};
  bool material_exposure_unprotected{false};
  bool material_exposure_other_piece{false};
  bool material_exposure_preexisting_piece{false};
  int material_exposure_piece_value_cp{0};
  int material_exposure_net_loss_cp{0};

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

// Mate distance is categorical evidence, not a centipawn surrogate. A positive
// value means the mover can force mate; a negative value means the mover is
// being mated. The loss is measured only when both candidates preserve the
// same mate result.
std::optional<int> mate_distance_loss(
    const std::optional<int>& best_mate_in,
    const std::optional<int>& played_mate_in);
bool mate_distance_equivalent(
    const std::optional<int>& best_mate_in,
    const std::optional<int>& played_mate_in,
    int tolerance = 1);
MoveSeverity classify_move_severity(
    const std::optional<double>& best_expected,
    const std::optional<double>& played_expected,
    const std::optional<int>& best_evaluation_cp,
    const std::optional<int>& played_evaluation_cp,
    const MoveSeverityConfig& config = {});
MoveCategory classify_move(
    const MoveAnalysisFacts& input,
    const MoveClassifierConfig& config = {});
std::string move_category_name(MoveCategory category);

// -----------------------------------------------------------------------------
// Section: Position evidence
// -----------------------------------------------------------------------------

bool root_move_is_tactical(const std::string& fen_before, const std::string& uci_move);
VerifiedSacrifice root_move_sacrifice_evidence(
    const std::string& fen_before,
    const std::vector<std::string>& principal_variation);
MaterialExposureEvidence root_move_material_exposure_evidence(
    const std::string& fen_before,
    const std::string& uci_move);
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
