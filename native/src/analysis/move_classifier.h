#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/models.h"
#include "engine/chess_engine.h"

namespace kchess {

struct MoveClassifierConfig {
  static constexpr int version = 6;
  double best_loss{0.005};
  double excellent_loss{0.03};
  double okay_loss{0.08};
  // Brilliant is deliberately rare: a near-best move outside theory, from a
  // non-forced/non-check position, with a verified material sacrifice and
  // tactical justification (forced mate or a uniquely strong best line).
  double brilliant_gap{0.15};
  double brilliant_min_best_score{0.60};
  double critical_gap{0.18};
  double critical_min_best_score{0.45};
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
  bool material_sacrifice{false};
  bool only_move_tactical{false};
  bool missed_forced_mate{false};
  bool allowed_forced_mate{false};
  bool was_in_check_before_move{false};
  bool forces_nontrivial_mate{false};
  int legal_move_count{0};
  std::optional<double> best_expected_score;
  std::optional<double> played_expected_score;
  std::optional<double> second_best_expected_score;
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
bool material_sacrifice_in_pv(
    const std::string& fen_before,
    const std::vector<std::string>& principal_variation,
    int max_plies = 4);
PositionContext position_context(const std::string& fen);

}  // namespace kchess
