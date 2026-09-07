#include "analysis/accuracy.h"

#include <algorithm>
#include <cmath>

namespace kchess {
namespace {

bool valid_config(const AccuracyConfig& config) {
  return std::isfinite(config.cp_probability_scale)
      && config.cp_probability_scale > 0.0
      && std::isfinite(config.curve_scale)
      && config.curve_scale > 0.0
      && std::isfinite(config.curve_decay)
      && config.curve_decay >= 0.0
      && std::isfinite(config.curve_offset)
      && std::isfinite(config.minimum_decision_weight)
      && config.minimum_decision_weight >= 0.0
      && config.minimum_decision_weight <= 1.0
      && std::isfinite(config.forced_move_weight)
      && config.forced_move_weight >= 0.0
      && config.forced_move_weight <= 1.0
      && std::isfinite(config.full_weight_value_gap)
      && config.full_weight_value_gap > 0.0
      && config.full_weight_cp_gap > 0
      && std::isfinite(config.decided_value_threshold)
      && config.decided_value_threshold >= 0.5
      && config.decided_value_threshold <= 1.0
      && std::isfinite(config.decided_easy_weight_factor)
      && config.decided_easy_weight_factor >= 0.0
      && config.decided_easy_weight_factor <= 1.0
      && config.deeper_result_margin >= 0
      && std::isfinite(config.instability_value_tolerance)
      && config.instability_value_tolerance >= 0.0
      && std::isfinite(config.mate_distance_step)
      && config.mate_distance_step >= 0.0
      && config.mate_distance_cap > 0;
}

std::optional<double> resolved_played_value(
    const AccuracyMove& move, const AccuracyConfig& config) {
  const auto root = accuracy_decision_value(move.played_root, config);
  const auto after = accuracy_decision_value(move.played_after, config);
  if (!root.has_value()) return after;
  if (!after.has_value()) return root;

  const int root_depth = std::max(0, move.played_root.depth);
  const int after_depth = std::max(0, move.played_after.depth);

  // A materially deeper result is allowed to correct the shallower view in
  // either direction.  This is important when deeper search proves a draw or
  // discovers that an apparent mistake was actually sound.
  if (after_depth >= root_depth + config.deeper_result_margin) return after;
  if (root_depth >= after_depth + config.deeper_result_margin) return root;

  // At comparable depth, do not let an optimistic independent re-search erase
  // a warning already visible in the root comparison.  Conversely, if the
  // resulting-position search sees the move significantly worse, use that as
  // a horizon/stability guard.  Tiny differences are averaged as engine noise.
  if (*after + config.instability_value_tolerance < *root) return after;
  if (*root + config.instability_value_tolerance < *after) return root;
  return (*root + *after) / 2.0;
}

std::optional<int> nonnegative_cp_gap(
    const AccuracyEvaluation& best, const AccuracyEvaluation& alternative) {
  if (!best.evaluation_cp.has_value() || !alternative.evaluation_cp.has_value()) {
    return std::nullopt;
  }
  return std::max(0, *best.evaluation_cp - *alternative.evaluation_cp);
}

std::optional<double> move_regret(
    const AccuracyMove& move, const AccuracyConfig& config) {
  const auto best = accuracy_decision_value(move.best, config);
  if (!best.has_value()) return std::nullopt;

  // If Stockfish's rank-1 root move was actually played, the decision itself
  // is lossless at the analyzed depth.  An independent post-move search can
  // legitimately report a different absolute evaluation and must not punish
  // the engine-best choice for search noise.
  if (move.played_is_best) return 0.0;

  const auto played = resolved_played_value(move, config);
  if (!played.has_value()) return std::nullopt;
  return std::clamp(*best - *played, 0.0, 1.0);
}

}  // namespace

std::optional<double> accuracy_decision_value(
    const AccuracyEvaluation& evaluation, const AccuracyConfig& config) {
  if (!valid_config(config)) return std::nullopt;

  if (evaluation.mate_in.has_value()) {
    const int mate = *evaluation.mate_in;
    if (mate == 0) return 0.5;  // Caller should resolve terminal perspective.
    const int distance = std::min(std::abs(mate), config.mate_distance_cap);
    const double distance_penalty = config.mate_distance_step
        * static_cast<double>(std::max(0, distance - 1));
    if (mate > 0) return std::clamp(1.0 - distance_penalty, 0.5, 1.0);
    return std::clamp(distance_penalty, 0.0, 0.5);
  }

  if (!evaluation.evaluation_cp.has_value()) return std::nullopt;
  const double cp = static_cast<double>(*evaluation.evaluation_cp);
  // Avoid exp overflow for deliberately malformed/extreme test data.
  const double exponent = std::clamp(-cp / config.cp_probability_scale, -60.0, 60.0);
  return std::clamp(1.0 / (1.0 + std::exp(exponent)), 0.0, 1.0);
}

std::optional<double> move_accuracy(
    const AccuracyMove& move, const AccuracyConfig& config) {
  if (!valid_config(config)) return std::nullopt;
  const auto regret = move_regret(move, config);
  if (!regret.has_value()) return std::nullopt;

  const double value_percent_loss = 100.0 * *regret;
  const double raw = config.curve_scale
      * std::exp(-config.curve_decay * value_percent_loss)
      + config.curve_offset;
  return std::clamp(raw, 0.0, 100.0);
}

double move_accuracy_weight(
    const AccuracyMove& move, const AccuracyConfig& config) {
  if (!valid_config(config)) return 0.0;
  if (move.legal_move_count <= 1) return config.forced_move_weight;

  const auto best = accuracy_decision_value(move.best, config);
  const auto second = accuracy_decision_value(move.second_best, config);
  if (!best.has_value() || !second.has_value()) {
    return config.minimum_decision_weight;
  }

  const double value_gap = std::clamp(*best - *second, 0.0, 1.0);
  double difficulty = std::clamp(
      value_gap / config.full_weight_value_gap, 0.0, 1.0);
  if (const auto cp_gap = nonnegative_cp_gap(move.best, move.second_best);
      cp_gap.has_value()) {
    difficulty = std::max(
        difficulty,
        std::clamp(
            static_cast<double>(*cp_gap) / config.full_weight_cp_gap,
            0.0,
            1.0));
  }
  if (move.best.mate_in.has_value() && *move.best.mate_in > 0
      && move.second_best.mate_in.has_value() && *move.second_best.mate_in > 0) {
    const int mate_distance_advantage = std::max(
        0, *move.second_best.mate_in - *move.best.mate_in);
    difficulty = std::max(
        difficulty,
        std::clamp(static_cast<double>(mate_distance_advantage) / 4.0, 0.0, 1.0));
  }

  // If only the best line preserves/delivers mate, the conversion is a real
  // decision even when the position arose from an opponent blunder.  Finding
  // that precise continuation therefore receives full weight.
  if (move.best.mate_in.has_value() && *move.best.mate_in > 0
      && (!move.second_best.mate_in.has_value()
          || *move.second_best.mate_in <= 0)) {
    difficulty = 1.0;
  }

  double weight = std::clamp(
      config.minimum_decision_weight
          + (1.0 - config.minimum_decision_weight) * difficulty,
      0.0,
      1.0);

  // Once the result is already close to decided and several alternatives keep
  // essentially the same value, converting the opponent's earlier blunder is
  // an easy task. Such moves still receive a score, but they contribute very
  // little to the game total. A unique continuation escapes this reduction via
  // its large difficulty value (and a unique mating line is forced to 1 above).
  const bool decided = *best >= config.decided_value_threshold
      || *best <= 1.0 - config.decided_value_threshold;
  if (decided && difficulty < 0.25) {
    weight *= config.decided_easy_weight_factor;
  }
  return std::clamp(weight, 0.0, 1.0);
}

std::optional<double> game_accuracy(
    const std::vector<AccuracyMove>& moves, const AccuracyConfig& config) {
  if (!valid_config(config)) return std::nullopt;

  double weighted_accuracy_total = 0.0;
  double weighted_reciprocal_total = 0.0;
  double total_weight = 0.0;
  bool has_zero_accuracy = false;
  int theory = 0;

  for (const auto& move : moves) {
    if (move.theory) {
      ++theory;
      continue;
    }

    const auto accuracy = move_accuracy(move, config);
    if (!accuracy.has_value() || !std::isfinite(*accuracy)) continue;
    const double weight = move_accuracy_weight(move, config);
    if (!std::isfinite(weight) || weight <= 0.0) continue;

    weighted_accuracy_total += weight * *accuracy;
    total_weight += weight;
    if (*accuracy <= 0.0) {
      has_zero_accuracy = true;
    } else {
      weighted_reciprocal_total += weight / *accuracy;
    }
  }

  if (total_weight > 0.0) {
    const double arithmetic_mean = weighted_accuracy_total / total_weight;
    // Retain V2's anti-dilution harmonic component, but now every term carries
    // the importance of the decision.  Easy 100% moves therefore cannot wash
    // out one difficult mistake merely because there were many of them.
    const double harmonic_mean = has_zero_accuracy
        ? 0.0
        : total_weight / weighted_reciprocal_total;
    return std::clamp((arithmetic_mean + harmonic_mean) / 2.0, 0.0, 100.0);
  }
  if (theory > 0) return 100.0;
  return std::nullopt;
}

}  // namespace kchess
