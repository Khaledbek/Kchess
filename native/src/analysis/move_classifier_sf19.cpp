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
  // Severity and rank-quality labels use Stockfish's normalized WDL/expected
  // result whenever it is available. CP-derived decision value is only a
  // compatibility fallback for legacy or incomplete rows.
  if (valid_value(engine_expected)) return engine_expected;
  return accuracy_decision_value({
      .evaluation_cp = evaluation_cp,
      .mate_in = mate_in,
      .depth = 0,
  });
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

  bool equivalent_best = false;
  if (!input.played_is_best && mate_distance_equivalent(input.best_mate_in, input.played_mate_in)) {
    equivalent_best = within_value(value_loss, config.equivalent_value_loss)
        && within_cp(cp_loss, config.equivalent_cp_loss)
        && (value_loss.has_value() || cp_loss.has_value()
            || (input.best_mate_in.has_value() && input.played_mate_in.has_value()));
  }
  // If the service says the played move is the published rank-1 move, the
  // classifier must trust that same root snapshot. Cross-snapshot disagreement
  // is handled before severity classification and is never reinterpreted here.
  const bool trusted_engine_best = input.played_is_best;
  const bool candidate_best = trusted_engine_best || equivalent_best;

  bool second_equivalent = false;
  if (mate_distance_equivalent(input.best_mate_in, input.second_best_mate_in)) {
    second_equivalent = within_value(second_value_gap, config.equivalent_value_loss)
        && within_cp(second_cp_gap, config.equivalent_cp_loss)
        && (second_value_gap.has_value() || second_cp_gap.has_value());
  }

  const bool has_second_value = second_value_gap.has_value();
  const bool has_second_cp = second_cp_gap.has_value();
  const bool value_separates = has_second_value
      && *second_value_gap >= config.critical_value_gap;
  const bool cp_separates = has_second_cp
      && *second_cp_gap >= config.critical_cp_gap;
  const auto second_mate_distance_loss = mate_distance_loss(
      input.best_mate_in, input.second_best_mate_in);
  const bool mate_distance_separates = second_mate_distance_loss.has_value()
      && *second_mate_distance_loss >= 4;
  const bool numeric_cluster_separation = has_second_value && has_second_cp
      ? (value_separates && cp_separates)
      : (value_separates || cp_separates);
  const bool second_clearly_worse = !second_equivalent
      && (unique_forced_mate || mate_distance_separates
          || numeric_cluster_separation);

  if (candidate_best) {
    const bool direct_piece_sacrifice = input.sacrifice_against_best_defense
        && input.sacrifice_piece_value_cp >= config.brilliant_piece_min_cp
        && input.sacrifice_net_material_loss_cp
            >= config.brilliant_net_sacrifice_min_cp;
    const bool ignored_piece_exposure = input.material_exposure_present
        && input.material_exposure_piece_value_cp >= config.brilliant_piece_min_cp
        && input.material_exposure_net_loss_cp
            >= config.brilliant_net_sacrifice_min_cp;
    const bool real_material_concession = direct_piece_sacrifice
        || ignored_piece_exposure;
    const bool pv_compensation = input.played_material_fully_recovered
        || input.played_material_net_gain_after_pv_cp > 0
        || input.played_material_concession_cp
            <= input.played_material_recovered_cp;
    const bool sound_after_sacrifice = played_forces_mate
        || (valid_value(played_value) && *played_value >= config.brilliant_min_value);
    const bool compensated_concession = real_material_concession
        && (sound_after_sacrifice || pv_compensation);
    const bool already_decided = !played_forces_mate
        && valid_value(best_value)
        && *best_value >= config.brilliant_decided_ceiling;

    if (trusted_engine_best
        && wrapped.best_move_verified_after
        && !input.was_in_check_before_move
        && input.legal_move_count > 1
        && compensated_concession
        && sound_after_sacrifice
        && !already_decided) {
      return MoveCategory::brilliant;
    }

    if (trusted_engine_best && second_clearly_worse) {
      return MoveCategory::critical;
    }
    return MoveCategory::best;
  }

  // Mate state is categorical and overrides every numeric calibration.
  if (input.allowed_forced_mate) return MoveCategory::blunder;
  if (input.missed_forced_mate) return MoveCategory::miss;

  if (const auto mate_loss = mate_distance_loss(
          input.best_mate_in, input.played_mate_in);
      mate_loss.has_value()) {
    if (*mate_loss <= 1) return MoveCategory::best;
    if (*mate_loss <= 3) return MoveCategory::excellent;
    if (*mate_loss <= 6) return MoveCategory::good;
    return MoveCategory::okay;
  }

  const int sustained_material_loss = std::max(
      0, -input.played_final_material_delta_cp);
  const int best_net_material_gain = std::max(
      0, input.best_final_material_delta_cp);
  const int played_net_material_gain = std::max(
      0, input.played_final_material_delta_cp);
  const int material_opportunity_gap = std::max(
      0, best_net_material_gain - played_net_material_gain);

  const auto severity = classify_move_severity(
      best_value,
      played_value,
      input.best_evaluation_cp,
      input.played_evaluation_cp);
  if (severity == MoveSeverity::blunder) return MoveCategory::blunder;

  const bool excellent_quality = within_value(
          value_loss, config.excellent_value_loss)
      && within_cp(cp_loss, config.excellent_cp_loss);
  const bool good_quality = within_value(value_loss, config.good_value_loss)
      && within_cp(cp_loss, config.good_cp_loss);
  const bool okay_quality = within_value(value_loss, config.okay_value_loss)
      && within_cp(cp_loss, config.okay_cp_loss);

  // MultiPV rank is evidence of candidate quality, but not the label itself.
  // Equivalent alternatives were already promoted to Best. Any known top-five
  // alternative just outside the Best cluster may still be Excellent.
  const bool known_top_five = input.played_rank >= 2 && input.played_rank <= 5;
  if (known_top_five) {
    if (excellent_quality) return MoveCategory::excellent;
    if (good_quality) return MoveCategory::good;
    if (okay_quality) return MoveCategory::okay;
  }

  const bool had_real_advantage = best_forces_mate
      || (valid_value(best_value)
          ? *best_value >= config.miss_min_best_value
          : (input.best_evaluation_cp.has_value()
              && *input.best_evaluation_cp >= 100));
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

  const bool outside_top_five = input.played_rank >= 6;
  if (severity == MoveSeverity::mistake) return MoveCategory::mistake;
  if (severity == MoveSeverity::inaccuracy) return MoveCategory::inaccuracy;

  // Old/incomplete caches can lack a reliable rank. Keep light deviations
  // positive rather than inventing a severe label from one number.
  if (excellent_quality) return MoveCategory::excellent;
  if (good_quality) return MoveCategory::good;
  if (okay_quality) return MoveCategory::okay;

  // Outside-top-five is supporting rank evidence, not severity by itself.
  (void)outside_top_five;
  return MoveCategory::okay;
}

}  // namespace kchess
