#include "practicality_metrics.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace kchess::ai::detail {
namespace {

// -----------------------------------------------------------------------------
// Section: Normalization helpers
// -----------------------------------------------------------------------------

double clamp01(double value) {
  return std::clamp(value, 0.0, 1.0);
}

std::optional<int> evaluation_gap_cp(const CandidateMove& best,
                                     const CandidateMove& other) {
  if (best.evaluation_cp && other.evaluation_cp) {
    return std::abs(*best.evaluation_cp - *other.evaluation_cp);
  }
  if (best.mate_in && other.mate_in) {
    return std::min(1000, 50 * std::abs(*best.mate_in - *other.mate_in));
  }
  if (best.mate_in || other.mate_in) return 1000;
  return std::nullopt;
}

std::vector<const CandidateMove*> ranked_candidates(
    const CandidateMoveSet& candidates) {
  std::vector<const CandidateMove*> lines;
  if (candidates.best) lines.push_back(&*candidates.best);
  for (const auto& alternative : candidates.alternatives) {
    lines.push_back(&alternative);
  }
  return lines;
}

std::set<std::string> critical_replies(const CandidateMoveSet& candidates) {
  std::set<std::string> replies;
  for (const auto* line : ranked_candidates(candidates)) {
    if (line->pv_uci.size() >= 2 && line->pv_uci.front() == line->move_uci) {
      replies.insert(line->pv_uci[1]);
    }
  }
  return replies;
}

std::optional<double> king_exposure(const PositionFeatures& features) {
  const auto& side = features.white_to_move ? features.white : features.black;
  const auto& king = side.strategic.king;
  if (king.king_square < 0) return std::nullopt;

  return clamp01(0.35 * clamp01(king.exposed_files / 3.0) +
                 0.35 * clamp01(king.attacked_zone_squares / 9.0) +
                 0.20 * clamp01(king.enemy_attackers / 6.0) +
                 0.10 * clamp01((3.0 - king.pawn_shield) / 3.0));
}

std::optional<double> tactical_density(const TacticalAnalysis& tactics) {
  if (tactics.motifs.empty()) return 0.0;
  double confidence_sum = 0.0;
  for (const auto& motif : tactics.motifs) {
    confidence_sum += clamp01(motif.confidence);
  }
  return clamp01(confidence_sum / 6.0);
}

struct WeightedScore {
  double value{0.0};
  double weight{0.0};

  void add(double metric, double metric_weight) {
    add(std::optional<double>(metric), metric_weight);
  }

  void add(const std::optional<double>& metric, double metric_weight) {
    if (!metric) return;
    value += clamp01(*metric) * metric_weight;
    weight += metric_weight;
  }

  [[nodiscard]] std::optional<double> average() const {
    if (weight <= 0.0) return std::nullopt;
    return clamp01(value / weight);
  }
};

// -----------------------------------------------------------------------------
// Section: Raw objective metrics
// -----------------------------------------------------------------------------

PracticalityMetrics build_metrics(
    const CandidateMoveSet& candidates,
    const std::optional<PositionFeatures>& features,
    const std::optional<TacticalAnalysis>& tactics) {
  PracticalityMetrics metrics;
  if (!candidates.best) return metrics;

  const auto& best = *candidates.best;
  int known_gaps = 0;
  int large_gaps = 0;
  int max_gap = 0;
  std::optional<int> nearest_gap;
  for (const auto& alternative : candidates.alternatives) {
    const auto gap = evaluation_gap_cp(best, alternative);
    if (!gap) continue;
    ++known_gaps;
    max_gap = std::max(max_gap, *gap);
    nearest_gap = nearest_gap ? std::min(*nearest_gap, *gap) : *gap;
    if (*gap >= 120) ++large_gaps;
  }

  metrics.evaluation_loss_if_inaccurate_cp = nearest_gap;
  if (known_gaps > 0) {
    metrics.only_move_density =
        static_cast<double>(large_gaps) / static_cast<double>(known_gaps);
    metrics.evaluation_volatility = clamp01(max_gap / 300.0);
  }

  const auto replies = critical_replies(candidates);
  metrics.critical_reply_count = static_cast<int>(replies.size());
  metrics.forced_line_length = static_cast<int>(best.pv_uci.size());

  if (features) {
    const double legal_moves = clamp01(features->side_to_move_legal_moves / 40.0);
    const double reply_branches = clamp01(replies.size() / 4.0);
    metrics.branching_complexity =
        clamp01(0.70 * legal_moves + 0.30 * reply_branches);
    metrics.king_exposure = king_exposure(*features);
  } else if (!replies.empty()) {
    metrics.branching_complexity = clamp01(replies.size() / 4.0);
  }

  if (tactics) metrics.tactical_density = tactical_density(*tactics);

  WeightedScore calculation;
  calculation.add(clamp01(metrics.forced_line_length / 10.0), 0.55);
  calculation.add(metrics.only_move_density, 0.25);
  calculation.add(metrics.tactical_density, 0.20);
  metrics.calculation_depth_required = calculation.average();

  WeightedScore instability;
  instability.add(metrics.evaluation_volatility, 0.45);
  instability.add(metrics.tactical_density, 0.30);
  instability.add(metrics.king_exposure, 0.25);
  if (instability.weight > 0.0) {
    metrics.position_stability = 1.0 - *instability.average();
  }
  return metrics;
}

// -----------------------------------------------------------------------------
// Section: Aggregate objective scores
// -----------------------------------------------------------------------------

PracticalityAssessment aggregate(PracticalityMetrics metrics) {
  PracticalityAssessment result;
  result.metrics = std::move(metrics);

  const std::optional<double> evaluation_risk =
      result.metrics.evaluation_loss_if_inaccurate_cp
          ? std::optional<double>(clamp01(
                *result.metrics.evaluation_loss_if_inaccurate_cp / 250.0))
          : std::nullopt;

  WeightedScore difficulty;
  difficulty.add(result.metrics.only_move_density, 0.20);
  difficulty.add(result.metrics.evaluation_volatility, 0.20);
  difficulty.add(result.metrics.branching_complexity, 0.15);
  difficulty.add(result.metrics.calculation_depth_required, 0.25);
  difficulty.add(result.metrics.tactical_density, 0.20);
  result.difficulty = difficulty.average();

  WeightedScore risk;
  risk.add(evaluation_risk, 0.30);
  risk.add(result.metrics.evaluation_volatility, 0.25);
  risk.add(result.metrics.king_exposure, 0.25);
  risk.add(result.metrics.tactical_density, 0.20);
  result.risk = risk.average();

  WeightedScore unforgiving;
  unforgiving.add(evaluation_risk, 0.60);
  unforgiving.add(result.metrics.only_move_density, 0.40);
  if (const auto score = unforgiving.average()) {
    result.forgiveness = 1.0 - *score;
  }

  WeightedScore unclear;
  unclear.add(result.metrics.branching_complexity, 0.40);
  unclear.add(result.metrics.evaluation_volatility, 0.30);
  unclear.add(result.metrics.tactical_density, 0.30);
  if (const auto score = unclear.average()) {
    result.clarity = 1.0 - *score;
  }

  int known = 0;
  known += result.metrics.evaluation_loss_if_inaccurate_cp.has_value();
  known += result.metrics.only_move_density.has_value();
  known += result.metrics.evaluation_volatility.has_value();
  known += result.metrics.branching_complexity.has_value();
  known += result.metrics.king_exposure.has_value();
  known += result.metrics.calculation_depth_required.has_value();
  known += result.metrics.tactical_density.has_value();
  known += result.metrics.position_stability.has_value();
  result.confidence = clamp01(known / 8.0);
  return result;
}

}  // namespace

PracticalityAssessment build_objective_assessment(
    const CandidateMoveSet& candidates,
    const std::optional<PositionFeatures>& features,
    const std::optional<TacticalAnalysis>& tactics) {
  return aggregate(build_metrics(candidates, features, tactics));
}

}  // namespace kchess::ai::detail
