#include "analysis/move_classifier_sf19.h"

#include <algorithm>
#include <cmath>
#include <optional>

#include "analysis/accuracy.h"

namespace kchess {
namespace {

// -----------------------------------------------------------------------------
// Section: Stable SF19 evaluation scale
// -----------------------------------------------------------------------------

bool valid_value(const std::optional<double>& value) {
  return value.has_value() && std::isfinite(*value)
      && *value >= 0.0 && *value <= 1.0;
}

std::optional<double> stable_value(
    const std::optional<int>& evaluation_cp,
    const std::optional<int>& mate_in,
    const std::optional<double>& engine_expected) {
  // Accuracy V3 deliberately uses CP/mate rather than Stockfish WDL because it
  // stays continuous in already won/lost positions.  Reuse that same stable
  // value model for SF19 move labels.  WDL remains a last-resort fallback for
  // legacy/terminal rows where no CP/mate sample exists.
  const auto value = accuracy_decision_value({
      .evaluation_cp = evaluation_cp,
      .mate_in = mate_in,
      .depth = 0,
  });
  if (value.has_value()) return value;
  return valid_value(engine_expected) ? engine_expected : std::nullopt;
}

std::optional<double> nonnegative_value_gap(
    const std::optional<double>& better,
    const std::optional<double>& worse) {
  if (!valid_value(better) || !valid_value(worse)) return std::nullopt;
  return std::max(0.0, *better - *worse);
}

std::optional<int> nonnegative_cp_gap(
    const std::optional<int>& better,
    const std::optional<int>& worse) {
  if (!better.has_value() || !worse.has_value()) return std::nullopt;
  return std::max(0, *better - *worse);
}

bool same_mate_state(
    const std::optional<int>& best,
    const std::optional<int>& alternative) {
  if (!best.has_value() && !alternative.has_value()) return true;
  if (!best.has_value() || !alternative.has_value()) return false;
  if ((*best > 0) != (*alternative > 0)) return false;
  if ((*best < 0) != (*alternative < 0)) return false;
  // Two winning mates are strategically equivalent for Best-label purposes;
  // distance can still make rank 1 Critical below.
  return true;
}

bool within_value(
    const std::optional<double>& loss,
    const double limit) {
  return !loss.has_value() || *loss <= limit;
}

bool within_cp(
    const std::optional<int>& loss,
    const int limit) {
  return !loss.has_value() || *loss <= limit;
}

bool has_real_drop(
    const std::optional<double>& value_loss,
    const std::optional<int>& cp_loss,
    const double value_limit,
    const int cp_limit) {
  if (value_loss.has_value()) return *value_loss >= value_limit;
  return cp_loss.has_value() && *cp_loss >= cp_limit;
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Stockfish 19 move classification
// -----------------------------------------------------------------------------

MoveCategory classify_move_sf19(
    const MoveClassifierSf19Input& wrapped,
    const MoveClassifierSf19Config& config) {
  const auto& input = wrapped.common;

  // Exactly one legal move is not a decision. Forced intentionally has higher
  // priority than book/theory so the dedicated icon is deterministic.
  if (input.legal_move_count == 1) return MoveCategory::forced;
  if (input.theory) return MoveCategory::theory;

  const auto best_value = stable_value(
      input.best_evaluation_cp, input.best_mate_in, input.best_expected_score);
  const auto played_value = stable_value(
      input.played_evaluation_cp, input.played_mate_in, input.played_expected_score);
  const auto second_value = stable_value(
      input.second_best_evaluation_cp,
      input.second_best_mate_in,
      input.second_best_expected_score);

  const auto value_loss = nonnegative_value_gap(best_value, played_value);
  const auto cp_loss = nonnegative_cp_gap(
      input.best_evaluation_cp, input.played_evaluation_cp);
  const auto second_value_gap = nonnegative_value_gap(best_value, second_value);
  const auto second_cp_gap = nonnegative_cp_gap(
      input.best_evaluation_cp, input.second_best_evaluation_cp);

  const bool best_forces_mate = input.best_mate_in.has_value()
      && *input.best_mate_in > 0;
  const bool played_forces_mate = input.played_mate_in.has_value()
      && *input.played_mate_in > 0;
  const bool second_forces_mate = input.second_best_mate_in.has_value()
      && *input.second_best_mate_in > 0;
  const bool unique_forced_mate = best_forces_mate && !second_forces_mate;

  // The final Stockfish bestmove can never receive a negative label merely
  // because a separate resulting-position search differs numerically.
  bool equivalent_best = false;
  if (!input.played_is_best && same_mate_state(input.best_mate_in, input.played_mate_in)) {
    equivalent_best = within_value(value_loss, config.equivalent_value_loss)
        && within_cp(cp_loss, config.equivalent_cp_loss)
        && (value_loss.has_value() || cp_loss.has_value()
            || (input.best_mate_in.has_value() && input.played_mate_in.has_value()));
  }
  const bool candidate_best = input.played_is_best || equivalent_best;

  bool second_equivalent = false;
  if (same_mate_state(input.best_mate_in, input.second_best_mate_in)) {
    second_equivalent = within_value(second_value_gap, config.equivalent_value_loss)
        && within_cp(second_cp_gap, config.equivalent_cp_loss)
        && (second_value_gap.has_value() || second_cp_gap.has_value());
  }

  const bool second_clearly_worse = !second_equivalent
      && (unique_forced_mate
          || (second_value_gap.has_value()
              && *second_value_gap >= config.critical_value_gap)
          || (!second_value_gap.has_value()
              && second_cp_gap.has_value()
              && *second_cp_gap >= config.critical_cp_gap)
          || (input.only_move_tactical
              && second_value_gap.has_value()
              && *second_value_gap >= config.critical_value_gap * 0.60));

  if (candidate_best) {
    const bool verified_piece_sacrifice = input.sacrifice_against_best_defense
        && input.sacrifice_piece_value_cp >= config.brilliant_piece_min_cp
        && input.sacrifice_net_material_loss_cp
            >= config.brilliant_net_sacrifice_min_cp;
    const bool sound_after_sacrifice = played_forces_mate
        || (valid_value(played_value) && *played_value >= config.brilliant_min_value);
    const bool already_decided = !played_forces_mate
        && valid_value(best_value)
        && *best_value >= config.brilliant_decided_ceiling;

    if (input.played_is_best
        && !input.was_in_check_before_move
        && input.legal_move_count > 1
        && verified_piece_sacrifice
        && sound_after_sacrifice
        && !already_decided) {
      return MoveCategory::brilliant;
    }

    if (input.played_is_best && second_clearly_worse) {
      return MoveCategory::critical;
    }
    return MoveCategory::best;
  }

  // Mate state is categorical and overrides every numeric calibration.
  if (input.allowed_forced_mate) return MoveCategory::blunder;
  if (input.missed_forced_mate) return MoveCategory::miss;

  const int sustained_material_loss = std::max(
      0, -wrapped.played_final_material_delta_cp);
  const int best_net_material_gain = std::max(
      0, wrapped.best_final_material_delta_cp);
  const int played_net_material_gain = std::max(
      0, wrapped.played_final_material_delta_cp);
  const int material_opportunity_gap = std::max(
      0, best_net_material_gain - played_net_material_gain);

  const bool meaningful_drop = has_real_drop(
      value_loss, cp_loss, config.mistake_value_loss, config.mistake_cp_loss);
  const bool blunder_drop = has_real_drop(
      value_loss, cp_loss, config.blunder_material_value_loss, config.blunder_cp_loss);

  const bool material_blunder = sustained_material_loss
          >= config.blunder_material_loss_cp
      && blunder_drop;
  const bool catastrophic_swing = value_loss.has_value()
      && *value_loss >= config.catastrophic_value_loss;
  const bool winning_advantage_collapsed = valid_value(best_value)
      && valid_value(played_value)
      && *best_value >= config.winning_collapse_best_value
      && *played_value <= config.winning_collapse_played_value
      && value_loss.has_value()
      && *value_loss >= config.winning_collapse_loss;

  if (material_blunder || catastrophic_swing || winning_advantage_collapsed) {
    return MoveCategory::blunder;
  }

  // A settled pawn/piece loss plus a genuine evaluation deterioration is a
  // Mistake.  This uses final PV material, not the temporary maximum loss that
  // occurs in ordinary exchanges such as BxB QxB.
  const bool material_mistake = sustained_material_loss
          >= config.mistake_material_loss_cp
      && meaningful_drop;
  if (material_mistake) return MoveCategory::mistake;

  const bool excellent_quality = within_value(
          value_loss, config.excellent_value_loss)
      && within_cp(cp_loss, config.excellent_cp_loss);
  const bool good_quality = within_value(value_loss, config.good_value_loss)
      && within_cp(cp_loss, config.good_cp_loss);
  const bool okay_quality = within_value(value_loss, config.okay_value_loss)
      && within_cp(cp_loss, config.okay_cp_loss);

  // The explicit engine ranks are interpreted only when score loss is small.
  // This implements the KChess policy: rank 2/3/4 -> Excellent/Good/Okay.
  if (input.played_rank == 2 && excellent_quality) return MoveCategory::excellent;
  if (input.played_rank == 3 && good_quality) return MoveCategory::good;
  if (input.played_rank == 4 && okay_quality) return MoveCategory::okay;

  const bool had_real_advantage = best_forces_mate
      || (valid_value(best_value) && *best_value >= config.miss_min_best_value)
      || (input.best_evaluation_cp.has_value() && *input.best_evaluation_cp >= 100);
  const bool opportunity_lost = material_opportunity_gap
          >= config.miss_material_opportunity_cp
      || (value_loss.has_value() && *value_loss >= config.miss_value_loss)
      || (!value_loss.has_value()
          && cp_loss.has_value() && *cp_loss >= config.miss_cp_loss);

  // Miss is an opportunity loss without sustained self-damage. In already
  // won positions a large raw CP gap alone is not enough; the stable value or
  // concrete net material opportunity must also show a meaningful difference.
  if (had_real_advantage
      && opportunity_lost
      && sustained_material_loss < config.mistake_material_loss_cp) {
    return MoveCategory::miss;
  }

  const bool outside_top_four = input.played_rank >= 5;
  const bool positional_mistake = outside_top_four
      && value_loss.has_value()
      && *value_loss >= config.positional_mistake_value_loss;
  if (positional_mistake) return MoveCategory::mistake;

  // Old/incomplete caches can lack a reliable rank. Keep light deviations
  // positive rather than inventing a severe label from one number.
  if (excellent_quality) return MoveCategory::excellent;
  if (good_quality) return MoveCategory::good;
  if (okay_quality) return MoveCategory::okay;

  if (outside_top_four && meaningful_drop) return MoveCategory::mistake;
  return MoveCategory::okay;
}

}  // namespace kchess
