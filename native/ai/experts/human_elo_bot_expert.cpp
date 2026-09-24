#include "experts/human_elo_bot_expert.h"

#include <algorithm>
#include <cmath>
#include <numeric>

#include <nlohmann/json.hpp>

#include "engine/bot_move_selector.h"

namespace kchess::ai {
namespace {

// -----------------------------------------------------------------------------
// Section: Candidate normalization
// -----------------------------------------------------------------------------

struct HumanCandidate {
  const CandidateMove* candidate{nullptr};
  int loss_cp{0};
  double weight{0.0};
};

double candidate_strength_cp(const CandidateMove& candidate) noexcept {
  if (candidate.mate_in.has_value()) {
    const int mate = *candidate.mate_in;
    const double distance = static_cast<double>(std::min(std::abs(mate), 1000));
    return mate > 0 ? 100000.0 - distance * 10.0
                    : -100000.0 + distance * 10.0;
  }
  if (candidate.evaluation_cp.has_value()) {
    return static_cast<double>(*candidate.evaluation_cp);
  }
  return -40.0 * static_cast<double>(std::max(0, candidate.rank - 1));
}

std::vector<const CandidateMove*> objective_candidates(
    const CandidateMoveSet& candidates) {
  std::vector<const CandidateMove*> result;
  if (candidates.best.has_value()) result.push_back(&*candidates.best);
  for (const auto& candidate : candidates.alternatives) {
    if (std::none_of(result.begin(), result.end(), [&](const CandidateMove* item) {
          return item->move_uci == candidate.move_uci;
        })) {
      result.push_back(&candidate);
    }
  }
  return result;
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Human-likelihood projection from the existing bot policy
// -----------------------------------------------------------------------------

HumanEloPrediction HumanEloBotExpert::predict(
    const CandidateMoveSet& candidates,
    const int requested_elo) const {
  HumanEloPrediction result;
  result.requested_elo = requested_elo;

  const auto objective = objective_candidates(candidates);
  if (objective.empty()) return result;

  const auto profile = kchess::bot_difficulty_profile(requested_elo);
  const double best_strength = candidate_strength_cp(*objective.front());

  std::vector<HumanCandidate> weighted;
  weighted.reserve(objective.size());
  for (std::size_t index = 0; index < objective.size(); ++index) {
    const auto* candidate = objective[index];
    const int loss_cp = static_cast<int>(std::lround(std::clamp(
        best_strength - candidate_strength_cp(*candidate), 0.0, 100000.0)));

    double weight = 0.0;
    if (index == 0) {
      weight = std::max(1e-6, profile.best_move_probability);
    } else if (loss_cp <= profile.maximum_verified_loss_cp) {
      const double scale = std::max(35.0,
          static_cast<double>(profile.typical_loss_cp) * 0.70);
      const double distance = std::abs(
          static_cast<double>(loss_cp - profile.typical_loss_cp));
      const double loss_fit = std::exp(-distance / scale);
      const double rank_penalty = std::pow(
          1.0 / static_cast<double>(std::max(1, candidate->rank)),
          std::max(0.35, profile.rank_bias * 0.55));
      weight = std::max(1e-9, loss_fit * rank_penalty);
    }

    weighted.push_back({
        .candidate = candidate,
        .loss_cp = loss_cp,
        .weight = weight,
    });
  }

  if (weighted.size() > 1) {
    const double best_mass = std::clamp(profile.best_move_probability, 0.0, 1.0);
    double alternative_total = 0.0;
    for (std::size_t i = 1; i < weighted.size(); ++i) {
      alternative_total += weighted[i].weight;
    }
    weighted.front().weight = best_mass;
    if (alternative_total > 0.0) {
      const double remaining = 1.0 - best_mass;
      for (std::size_t i = 1; i < weighted.size(); ++i) {
        weighted[i].weight = remaining * weighted[i].weight / alternative_total;
      }
    } else {
      weighted.front().weight = 1.0;
      for (std::size_t i = 1; i < weighted.size(); ++i) {
        weighted[i].weight = 0.0;
      }
    }
  } else {
    weighted.front().weight = 1.0;
  }

  std::sort(weighted.begin(), weighted.end(), [](const HumanCandidate& left,
                                                 const HumanCandidate& right) {
    if (left.weight != right.weight) return left.weight > right.weight;
    return left.candidate->rank < right.candidate->rank;
  });

  result.moves.reserve(weighted.size());
  for (const auto& item : weighted) {
    result.moves.push_back({
        .candidate_id = item.candidate->candidate_id,
        .move_uci = item.candidate->move_uci,
        .engine_rank = item.candidate->rank,
        .loss_cp = item.loss_cp,
        .probability = item.weight,
    });
  }
  return result;
}

EvidenceItem HumanEloBotExpert::evidence(
    const CandidateMoveSet& candidates,
    const int requested_elo) const {
  const auto prediction = predict(candidates, requested_elo);

  nlohmann::json payload{
      {"version", 1},
      {"source", "kchess.bot_difficulty_policy.v1"},
      {"requested_elo", prediction.requested_elo},
      {"meaning", "human_move_likelihood_not_objective_evaluation"},
      {"moves", nlohmann::json::array()},
  };
  for (const auto& move : prediction.moves) {
    payload["moves"].push_back({
        {"candidate_id", move.candidate_id},
        {"engine_rank", move.engine_rank},
        {"loss_cp", move.loss_cp},
        {"probability", move.probability},
    });
  }

  return {
      .id = "human_model.elo_bot.v1",
      .kind = EvidenceKind::human_model,
      .payload = payload.dump(),
      .confidence = prediction.moves.empty() ? 0.0 : 0.75,
  };
}

EvidenceItem HumanEloBotExpert::evidence_grid(
    const CandidateMoveSet& candidates,
    const std::optional<int> learner_elo) const {
  std::vector<int> ratings{800, 1200, 1600, 2000, 2400};
  if (learner_elo.has_value()) {
    const int clamped = std::clamp(*learner_elo, 400, 3000);
    if (std::find(ratings.begin(), ratings.end(), clamped) == ratings.end()) {
      ratings.push_back(clamped);
      std::sort(ratings.begin(), ratings.end());
    }
  }

  nlohmann::json payload{
      {"version", 2},
      {"source", "kchess.bot_difficulty_policy.v1"},
      {"meaning", "human_move_likelihood_not_objective_evaluation"},
      {"rating_bands", nlohmann::json::array()},
  };
  if (learner_elo.has_value()) payload["learner_elo"] = *learner_elo;

  bool any = false;
  for (const int rating : ratings) {
    const auto prediction = predict(candidates, rating);
    if (prediction.moves.empty()) continue;
    nlohmann::json band{{"elo", rating}, {"moves", nlohmann::json::array()}};
    for (const auto& move : prediction.moves) {
      band["moves"].push_back({
          {"candidate_id", move.candidate_id},
          {"engine_rank", move.engine_rank},
          {"loss_cp", move.loss_cp},
          {"probability", move.probability},
      });
    }
    payload["rating_bands"].push_back(std::move(band));
    any = true;
  }

  return {
      .id = "human_model.elo_bot_grid.v1",
      .kind = EvidenceKind::human_model,
      .payload = payload.dump(),
      .confidence = any ? 0.75 : 0.0,
  };
}


}  // namespace kchess::ai
