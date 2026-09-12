#include "player_practicality.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include <nlohmann/json.hpp>

namespace kchess::ai {
namespace {

// -----------------------------------------------------------------------------
// Section: Objective quality gate
// -----------------------------------------------------------------------------

constexpr int kMaxPracticalLossCp = 80;

double clamp01(double value) {
  return std::clamp(value, 0.0, 1.0);
}

std::optional<int> loss_from_best(const CandidateMove& best,
                                  const CandidateMove& candidate) {
  if (best.move_uci == candidate.move_uci) return 0;
  if (best.evaluation_cp && candidate.evaluation_cp) {
    return std::abs(*best.evaluation_cp - *candidate.evaluation_cp);
  }
  if (best.mate_in && candidate.mate_in) {
    const bool same_sign = (*best.mate_in >= 0) == (*candidate.mate_in >= 0);
    if (!same_sign) return 10000;
    return std::min(10000,
                    25 * std::abs(std::abs(*best.mate_in) -
                                  std::abs(*candidate.mate_in)));
  }
  if (best.mate_in || candidate.mate_in) return 10000;
  return std::nullopt;
}

bool objectively_eligible(const CandidateMove& candidate,
                           const std::optional<int>& loss) {
  if (candidate.rank == 1) return true;
  return loss.has_value() && *loss <= kMaxPracticalLossCp;
}

// -----------------------------------------------------------------------------
// Section: Player fit
// -----------------------------------------------------------------------------

std::optional<double> player_skill(const PracticalityPlayerContext& player) {
  double sum = 0.0;
  double weight = 0.0;
  if (player.rating) {
    sum += clamp01((*player.rating - 600.0) / 1800.0) * 0.50;
    weight += 0.50;
  }
  if (player.calculation_strength) {
    sum += clamp01(*player.calculation_strength) * 0.30;
    weight += 0.30;
  }
  if (player.tactical_strength) {
    sum += clamp01(*player.tactical_strength) * 0.20;
    weight += 0.20;
  }
  if (weight == 0.0) return std::nullopt;
  return clamp01(sum / weight);
}

std::optional<double> candidate_difficulty(
    const CandidateMove& candidate,
    const PracticalityAssessment& objective) {
  const double line_depth = clamp01(candidate.pv_uci.size() / 10.0);
  if (!objective.difficulty) return line_depth;
  return clamp01(0.70 * *objective.difficulty + 0.30 * line_depth);
}

std::optional<double> candidate_risk(
    const std::optional<int>& loss,
    const PracticalityAssessment& objective) {
  const std::optional<double> quality_risk =
      loss ? std::optional<double>(clamp01(*loss / 80.0)) : std::nullopt;
  if (objective.risk && quality_risk) {
    return clamp01(0.75 * *objective.risk + 0.25 * *quality_risk);
  }
  return objective.risk ? objective.risk : quality_risk;
}

std::optional<double> fit_score(
    const PracticalCandidateFit& candidate,
    const PracticalityAssessment& objective,
    const PracticalityPlayerContext& player) {
  const auto skill = player_skill(player);
  if (!skill || !candidate.difficulty) return std::nullopt;

  const double skill_fit =
      1.0 - clamp01((*candidate.difficulty - *skill) / 0.65);
  const double clarity = objective.clarity.value_or(0.5);
  const double forgiveness = objective.forgiveness.value_or(0.5);
  double risk_fit = 0.75;
  if (candidate.risk && player.risk_tolerance) {
    risk_fit = 1.0 - std::abs(*candidate.risk - clamp01(*player.risk_tolerance));
  } else if (candidate.risk) {
    risk_fit = 1.0 - 0.35 * *candidate.risk;
  }

  const double quality = candidate.evaluation_loss_cp
                             ? 1.0 - 0.20 * clamp01(
                                         *candidate.evaluation_loss_cp /
                                         static_cast<double>(kMaxPracticalLossCp))
                             : (candidate.engine_rank == 1 ? 1.0 : 0.0);
  return clamp01(0.40 * skill_fit + 0.20 * clarity + 0.20 * forgiveness +
                 0.10 * risk_fit + 0.10 * quality);
}

PracticalCandidateFit build_fit(const CandidateMove& candidate,
                                const CandidateMove& best,
                                const PracticalityAssessment& objective,
                                const PracticalityPlayerContext& player) {
  PracticalCandidateFit result;
  result.move_uci = candidate.move_uci;
  result.engine_rank = candidate.rank;
  result.evaluation_loss_cp = loss_from_best(best, candidate);
  result.objectively_eligible =
      objectively_eligible(candidate, result.evaluation_loss_cp);
  result.difficulty = candidate_difficulty(candidate, objective);
  result.risk = candidate_risk(result.evaluation_loss_cp, objective);
  if (result.objectively_eligible) {
    result.fit = fit_score(result, objective, player);
  }
  return result;
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Player-aware assessment
// -----------------------------------------------------------------------------

PlayerPracticalityAssessment PlayerPracticalityEngine::assess(
    const CandidateMoveSet& candidates,
    const PracticalityAssessment& objective,
    const PracticalityPlayerContext& player) const {
  PlayerPracticalityAssessment result;
  result.player = player;
  if (!candidates.best) return result;

  result.candidates.push_back(
      build_fit(*candidates.best, *candidates.best, objective, player));
  for (const auto& alternative : candidates.alternatives) {
    result.candidates.push_back(
        build_fit(alternative, *candidates.best, objective, player));
  }

  const PracticalCandidateFit* choice = nullptr;
  for (const auto& candidate : result.candidates) {
    if (!candidate.objectively_eligible || !candidate.fit) continue;
    if (choice == nullptr || *candidate.fit > *choice->fit + 0.03 ||
        (std::abs(*candidate.fit - *choice->fit) <= 0.03 &&
         candidate.engine_rank < choice->engine_rank)) {
      choice = &candidate;
    }
  }
  if (choice) result.practical_choice_uci = choice->move_uci;

  const bool has_player_signal = player_skill(player).has_value();
  result.confidence = has_player_signal
                          ? clamp01(0.55 * objective.confidence +
                                    0.45 * clamp01(player.confidence))
                          : 0.0;
  return result;
}

// -----------------------------------------------------------------------------
// Section: Evidence serialization
// -----------------------------------------------------------------------------

EvidenceItem PlayerPracticalityEngine::evidence(
    const PlayerPracticalityAssessment& assessment) const {
  nlohmann::json player{{"confidence", clamp01(assessment.player.confidence)}};
  if (assessment.player.rating) player["rating"] = *assessment.player.rating;
  if (assessment.player.calculation_strength) {
    player["calculationStrength"] =
        clamp01(*assessment.player.calculation_strength);
  }
  if (assessment.player.tactical_strength) {
    player["tacticalStrength"] = clamp01(*assessment.player.tactical_strength);
  }
  if (assessment.player.risk_tolerance) {
    player["riskTolerance"] = clamp01(*assessment.player.risk_tolerance);
  }

  nlohmann::json candidates = nlohmann::json::array();
  for (const auto& candidate : assessment.candidates) {
    nlohmann::json item{{"move", candidate.move_uci},
                        {"engineRank", candidate.engine_rank},
                        {"objectivelyEligible", candidate.objectively_eligible}};
    if (candidate.evaluation_loss_cp) {
      item["evaluationLossCp"] = *candidate.evaluation_loss_cp;
    }
    if (candidate.difficulty) item["difficulty"] = *candidate.difficulty;
    if (candidate.risk) item["risk"] = *candidate.risk;
    if (candidate.fit) item["fit"] = *candidate.fit;
    candidates.push_back(std::move(item));
  }

  nlohmann::json payload{{"version", 1},
                         {"playerAdjusted", true},
                         {"objectiveLossLimitCp", kMaxPracticalLossCp},
                         {"player", std::move(player)},
                         {"candidates", std::move(candidates)}};
  if (assessment.practical_choice_uci) {
    payload["practicalChoice"] = *assessment.practical_choice_uci;
  }
  return {
      .id = "practicality.player.v1",
      .kind = EvidenceKind::practicality,
      .payload = payload.dump(),
      .confidence = assessment.confidence,
  };
}

}  // namespace kchess::ai
