#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/models.h"
#include "engine/chess_engine.h"

namespace kchess {

struct MoveClassifierConfig {
  static constexpr int version = 9;
  double excellent_loss{0.03};
  int excellent_cp_loss{50};
  double okay_loss{0.08};
  int okay_cp_loss{150};
  // Brilliant is deliberately rare: it must be Stockfish rank 1, start a
  // concrete sacrifice that survives the opponent's best root reply, and be
  // uniquely justified. Great/Critical is the non-sacrificial only-move tier.
  double brilliant_gap{0.10};
  int brilliant_cp_gap{120};
  double brilliant_min_best_score{0.50};
  double critical_gap{0.18};
  int critical_cp_gap{160};
  double critical_min_best_score{0.40};
  double miss_best_score{0.70};
  double miss_played_ceiling{0.55};
  double miss_loss{0.15};
  double blunder_min_best_score{0.35};
  double blunder_severe_loss{0.30};
  double blunder_outcome_loss{0.20};
};

struct PositionEvaluation {
  std::optional<WdlScore> wdl;
  std::optional<int> evaluation_cp;
  std::optional<int> mate_in;
};

struct MoveClassifierInput {
  bool theory{false};
  bool played_is_best{false};
  bool sacrifice_against_best_defense{false};
  bool only_move_tactical{false};
  bool missed_forced_mate{false};
  bool allowed_forced_mate{false};
  bool was_in_check_before_move{false};
  int legal_move_count{0};
  std::optional<double> best_expected_score;
  std::optional<double> played_expected_score;
  std::optional<double> second_best_expected_score;
  // Centipawn scores are always from the mover/root side perspective. They
  // complement WDL in positions where win/draw/loss probabilities saturate.
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

std::optional<double> expected_score_side_to_move(const PositionEvaluation& evaluation);
std::optional<double> expected_score_mover_after_move(const PositionEvaluation& evaluation);
std::optional<double> expected_score_loss(
    const std::optional<double>& best, const std::optional<double>& played);
MoveCategory classify_move(
    const MoveClassifierInput& input,
    const MoveClassifierConfig& config = {});
std::string move_category_name(MoveCategory category);
bool root_move_sacrifice_against_best_defense(
    const std::string& fen_before,
    const std::vector<std::string>& principal_variation,
    int minimum_material_loss_cp = 150);
bool material_sacrifice_in_pv(
    const std::string& fen_before,
    const std::vector<std::string>& principal_variation,
    int max_plies = 4);
PositionContext position_context(const std::string& fen);

}  // namespace kchess
