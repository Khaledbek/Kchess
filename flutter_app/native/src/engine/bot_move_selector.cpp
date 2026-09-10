#include "engine/bot_move_selector.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace kchess {
namespace {

// -----------------------------------------------------------------------------
// Section: Candidate scoring helpers
// -----------------------------------------------------------------------------

int normalized_elo(const int requested_elo) {
  if (requested_elo < kBotEloMinimum || requested_elo > kBotEloMaximum
      || requested_elo % kBotEloStep != 0) {
    throw std::invalid_argument("Bot Elo must be 100..3200 in steps of 100");
  }
  return requested_elo;
}

double clamp_unit_random(const double value) noexcept {
  if (!std::isfinite(value) || value <= 0.0) return 0.0;
  if (value >= 1.0) return std::nextafter(1.0, 0.0);
  return value;
}

double line_strength_cp(const EngineLine& line, const int fallback_rank) noexcept {
  if (line.mate_in.has_value()) {
    const int mate = *line.mate_in;
    const double distance = static_cast<double>(std::min(std::abs(mate), 1000));
    return mate > 0 ? 100000.0 - distance * 10.0
                    : -100000.0 + distance * 10.0;
  }
  if (line.evaluation_cp.has_value()) {
    return static_cast<double>(*line.evaluation_cp);
  }
  return -40.0 * static_cast<double>(std::max(0, fallback_rank - 1));
}

int candidate_loss_cp(
    const EngineLine& best,
    const EngineLine& candidate,
    const int fallback_rank) noexcept {
  const double loss = std::max(
      0.0, line_strength_cp(best, 1) - line_strength_cp(candidate, fallback_rank));
  return static_cast<int>(std::lround(std::min(loss, 100000.0)));
}

std::vector<EngineLine> normalized_candidates(const std::vector<EngineLine>& lines) {
  std::vector<EngineLine> candidates;
  candidates.reserve(lines.size());
  std::unordered_set<std::string> seen_moves;
  for (const auto& line : lines) {
    if (line.best_move().empty()) continue;
    if (!seen_moves.insert(line.best_move()).second) continue;
    candidates.push_back(line);
  }
  std::sort(candidates.begin(), candidates.end(), [](const EngineLine& left, const EngineLine& right) {
    const int left_rank = left.rank > 0 ? left.rank : std::numeric_limits<int>::max();
    const int right_rank = right.rank > 0 ? right.rank : std::numeric_limits<int>::max();
    return left_rank < right_rank;
  });
  for (std::size_t index = 0; index < candidates.size(); ++index) {
    if (candidates[index].rank <= 0) {
      candidates[index].rank = static_cast<int>(index) + 1;
    }
  }
  return candidates;
}

BotMoveChoice one_choice(const EngineLine& line, const double probability = 1.0) {
  BotMoveChoice choice;
  choice.move = line.best_move();
  choice.rank = line.rank;
  choice.probability = probability;
  return choice;
}

const EngineLine* find_move(
    const std::vector<EngineLine>& lines,
    const std::string& move) noexcept {
  const auto found = std::find_if(lines.begin(), lines.end(), [&](const EngineLine& line) {
    return line.best_move() == move;
  });
  return found == lines.end() ? nullptr : &*found;
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Elo-to-search profile
// -----------------------------------------------------------------------------

BotDifficultyProfile bot_difficulty_profile(const int requested_elo) {
  const int elo = normalized_elo(requested_elo);
  const double raw_strength = static_cast<double>(elo - kBotEloMinimum)
      / static_cast<double>(kBotEloMaximum - kBotEloMinimum);
  // The old selector was noticeably too soft around club strength. A mild
  // concave calibration raises the middle without changing either endpoint.
  const double strength = std::pow(std::clamp(raw_strength, 0.0, 1.0), 0.82);
  const double weakness = 1.0 - strength;

  BotDifficultyProfile profile;
  profile.requested_elo = elo;
  profile.strength = strength;
  profile.use_stockfish_limit_strength =
      elo >= kBotNativeStrengthThreshold && elo < kBotEloMaximum;
  if (profile.use_stockfish_limit_strength) {
    profile.stockfish_uci_elo = std::clamp(
        elo, kStockfish18LowestUciElo, kStockfish18HighestUciElo);
  }

  if (elo == kBotEloMaximum) {
    profile.scout_depth = 20;
    profile.verification_depth = 20;
    profile.maximum_target_rank = 1;
    profile.typical_loss_cp = 0;
    profile.maximum_verified_loss_cp = 0;
    profile.scout_node_budget = 0;
    profile.verification_node_budget = 180000;
    profile.scout_time_budget_ms = 0;
    // Nodes remain the primary full-strength budget. The wall-clock limit is
    // only a safety valve for unusually slow hardware/positions.
    profile.verification_time_budget_ms = 1400;
    profile.best_move_probability = 1.0;
    profile.rank_bias = 8.0;
    return profile;
  }

  // The scout is deliberately shallow. Its only job is to rank a small root
  // pool; tactical correctness comes from the targeted verification search.
  profile.scout_depth = std::clamp(
      5 + static_cast<int>(std::lround(3.0 * strength)), 5, 8);
  profile.verification_depth = std::clamp(
      8 + static_cast<int>(std::lround(8.0 * strength)), 8, 16);
  profile.maximum_target_rank = std::clamp(
      2 + static_cast<int>(std::lround(28.0 * std::pow(weakness, 1.45))),
      2,
      kBotMaximumCandidateLines);

  // Around 1500 this is roughly 1.8-2.0 pawns: mistakes still happen, but a
  // routine piece hang is no longer accepted merely because a shallow rank
  // happened to land far down the list.
  profile.typical_loss_cp = std::clamp(
      45 + static_cast<int>(std::lround(430.0 * std::pow(weakness, 1.55))),
      45,
      475);
  profile.maximum_verified_loss_cp = std::clamp(
      150 + static_cast<int>(std::lround(900.0 * std::pow(weakness, 2.15))),
      150,
      1050);
  // Latency is controlled by nodes first and wall-clock second. The node curve
  // rises with Elo, while the hard time cap prevents one tactically awkward
  // position from stalling the UI on slower hardware.
  profile.scout_node_budget = static_cast<std::uint64_t>(std::lround(
      1800.0 + 7200.0 * std::pow(strength, 1.35)));
  profile.verification_node_budget = static_cast<std::uint64_t>(std::lround(
      5500.0 + 62000.0 * std::pow(strength, 1.75)));
  profile.scout_time_budget_ms = std::clamp(
      60 + static_cast<int>(std::lround(85.0 * strength)), 60, 145);
  profile.verification_time_budget_ms = std::clamp(
      150 + static_cast<int>(std::lround(500.0 * std::pow(strength, 1.35))),
      150,
      650);
  profile.best_move_probability = std::clamp(
      0.18 + 0.78 * std::pow(strength, 1.10), 0.18, 0.96);
  profile.rank_bias = 1.10 + 3.20 * strength;
  return profile;
}

// -----------------------------------------------------------------------------
// Section: Probability-first move planning
// -----------------------------------------------------------------------------

BotMovePlan plan_bot_move(
    const int requested_elo,
    const double quality_random,
    const double diversity_random,
    const int position_ply) {
  const auto profile = bot_difficulty_profile(requested_elo);
  BotMovePlan plan;
  plan.use_stockfish_limit_strength = profile.use_stockfish_limit_strength;
  plan.stockfish_uci_elo = profile.stockfish_uci_elo;
  plan.scout_depth = profile.scout_depth;
  plan.verification_depth = profile.verification_depth;
  plan.maximum_verified_loss_cp = profile.maximum_verified_loss_cp;
  plan.scout_node_budget = profile.scout_node_budget;
  plan.verification_node_budget = profile.verification_node_budget;
  plan.scout_time_budget_ms = profile.scout_time_budget_ms;
  plan.verification_time_budget_ms = profile.verification_time_budget_ms;
  plan.opening_phase = position_ply >= 0 && position_ply < 12;

  if (profile.requested_elo == kBotEloMaximum
      || profile.use_stockfish_limit_strength) {
    // Native strength limiting performs its own hidden candidate search and
    // randomized weak-move choice. Requesting KChess MultiPV on top would
    // duplicate that work, so runtime bot play asks for one public PV only.
    plan.target_rank = 1;
    plan.scout_lines = 1;
    return plan;
  }

  const double roll = clamp_unit_random(quality_random);
  plan.diversity_roll = clamp_unit_random(diversity_random);
  double best_probability = profile.best_move_probability;
  int hard_loss_cap = profile.maximum_verified_loss_cp;
  int typical_loss = profile.typical_loss_cp;

  if (plan.opening_phase) {
    // Opening errors should be repertoire choices, not free material. Update 64
    // will add dedicated diversity; for now keep the low-Elo error envelope tight.
    best_probability = std::max(best_probability, 0.30);
    typical_loss = std::min(typical_loss, 45);
    hard_loss_cap = std::min(hard_loss_cap, 100);
  }

  plan.maximum_verified_loss_cp = hard_loss_cap;
  if (roll < best_probability) {
    plan.target_loss_cp = 0;
  } else {
    const double tail = (roll - best_probability) / (1.0 - best_probability);
    // Sample a loss magnitude before searching. Weak bots have a wider tail, but
    // the hard cap still prevents a shallow scout accident from becoming a huge
    // tactical blunder.
    const double weakness_shape = 0.72 + 0.55 * profile.strength;
    const double shaped = std::pow(clamp_unit_random(tail), weakness_shape);
    const int upper = std::max(typical_loss, hard_loss_cap);
    plan.target_loss_cp = std::clamp(
        static_cast<int>(std::lround(
            typical_loss * 0.35 + shaped * static_cast<double>(upper - typical_loss * 0.35))),
        0,
        hard_loss_cap);
  }

  // Low Elo no longer asks for rank 20/30. A small fixed scout pool is enough to
  // find moves across the desired loss envelope, keeping runtime bounded.
  plan.target_rank = plan.target_loss_cp == 0 ? 1 : 2;
  // Diversity is intentionally narrow: it only breaks ties between moves whose
  // scout scores are close to the already sampled quality target. The opening
  // gets a wider repertoire window, while middlegames prefer tighter equality.
  plan.diversity_window_cp = plan.opening_phase
      ? std::clamp(55 - static_cast<int>(std::lround(25.0 * profile.strength)), 35, 55)
      : std::clamp(28 - static_cast<int>(std::lround(14.0 * profile.strength)), 16, 28);
  plan.scout_lines = std::clamp(
      10 - static_cast<int>(std::lround(4.0 * profile.strength)),
      5,
      10);
  return plan;
}

// -----------------------------------------------------------------------------
// Section: Cheap scout selection and targeted verification
// -----------------------------------------------------------------------------

BotMoveChoice choose_scout_bot_move(
    const std::vector<EngineLine>& scout_lines,
    const BotMovePlan& plan) {
  auto candidates = normalized_candidates(scout_lines);
  if (candidates.empty()) return {};
  if (candidates.size() == 1 || plan.target_loss_cp <= 0) {
    return one_choice(candidates.front());
  }

  const EngineLine& best = candidates.front();
  const bool best_is_non_losing_mate =
      !best.mate_in.has_value() || *best.mate_in >= 0;

  struct WeightedCandidate {
    const EngineLine* line{nullptr};
    int loss_cp{0};
    int distance_cp{0};
    double weight{0.0};
  };

  std::vector<WeightedCandidate> safe;
  safe.reserve(candidates.size());
  int minimum_distance = std::numeric_limits<int>::max();
  for (std::size_t index = 0; index < candidates.size(); ++index) {
    const EngineLine& candidate = candidates[index];
    const int loss = candidate_loss_cp(best, candidate, static_cast<int>(index) + 1);
    const bool throws_forced_mate = best_is_non_losing_mate
        && candidate.mate_in.has_value() && *candidate.mate_in < 0;
    if (throws_forced_mate || loss > plan.maximum_verified_loss_cp) continue;

    const int distance = std::abs(loss - plan.target_loss_cp);
    minimum_distance = std::min(minimum_distance, distance);
    safe.push_back(WeightedCandidate{.line = &candidate, .loss_cp = loss, .distance_cp = distance});
  }
  if (safe.empty()) return one_choice(best);

  // Only moves close to the best match for the pre-sampled quality target enter
  // the lottery. For target_loss=0 this becomes a strict near-equal-move pool.
  const int tolerance = std::max(
      plan.diversity_window_cp,
      static_cast<int>(std::lround(static_cast<double>(plan.target_loss_cp) * 0.12)));
  const double temperature = std::max(8.0, static_cast<double>(tolerance) * 0.72);
  double total_weight = 0.0;
  for (auto& candidate : safe) {
    if (candidate.distance_cp > minimum_distance + tolerance) continue;
    const int excess = candidate.distance_cp - minimum_distance;
    // Exponential weighting makes the closest quality matches most likely while
    // still allowing another genuinely equivalent move to be chosen. A tiny
    // rank term prevents a broad shallow scout from over-favouring remote lines.
    candidate.weight = std::exp(-static_cast<double>(excess) / temperature)
        / (1.0 + 0.035 * static_cast<double>(std::max(0, candidate.line->rank - 1)));
    total_weight += candidate.weight;
  }

  if (total_weight <= 0.0) {
    const auto closest = std::min_element(
        safe.begin(), safe.end(), [](const WeightedCandidate& left, const WeightedCandidate& right) {
          if (left.distance_cp != right.distance_cp) return left.distance_cp < right.distance_cp;
          return left.loss_cp < right.loss_cp;
        });
    return one_choice(*closest->line);
  }

  double needle = clamp_unit_random(plan.diversity_roll) * total_weight;
  const EngineLine* selected = safe.front().line;
  for (const auto& candidate : safe) {
    if (candidate.weight <= 0.0) continue;
    selected = candidate.line;
    if (needle < candidate.weight) break;
    needle -= candidate.weight;
  }

  auto choice = one_choice(*selected);
  choice.probability = 1.0;
  return choice;
}

BotMoveChoice finalize_verified_bot_move(
    const std::vector<EngineLine>& verified_lines,
    const BotMoveChoice& planned_choice,
    const BotMovePlan& plan) {
  auto candidates = normalized_candidates(verified_lines);
  if (candidates.empty()) return planned_choice;

  // Searchmoves ranks only the verified subset, so compare scores directly.
  const auto best = std::max_element(
      candidates.begin(), candidates.end(), [](const EngineLine& left, const EngineLine& right) {
        return line_strength_cp(left, left.rank) < line_strength_cp(right, right.rank);
      });
  if (best == candidates.end()) return planned_choice;

  const EngineLine* planned = find_move(candidates, planned_choice.move);
  if (planned != nullptr) {
    const double loss = std::max(
        0.0, line_strength_cp(*best, best->rank) - line_strength_cp(*planned, planned->rank));
    const bool losing_mate = planned->mate_in.has_value()
        && *planned->mate_in < 0
        && (!best->mate_in.has_value() || *best->mate_in >= 0);
    const int verified_limit = plan.target_loss_cp <= 0 && plan.diversity_window_cp > 0
        ? std::min(plan.maximum_verified_loss_cp, plan.diversity_window_cp)
        : plan.maximum_verified_loss_cp;
    if (!losing_mate && loss <= static_cast<double>(verified_limit)) {
      auto accepted = planned_choice;
      accepted.probability = 1.0;
      return accepted;
    }
  }

  // Planned line failed verification. Prefer the safest verified alternative;
  // this is the hard tactical abort condition that prevents a club bot from
  // turning a shallow ranking accident into a full piece blunder.
  auto fallback = one_choice(*best);
  fallback.rank = best->rank;
  return fallback;
}

// -----------------------------------------------------------------------------
// Section: Complete-set compatibility selection
// -----------------------------------------------------------------------------

BotMoveChoice choose_bot_move(
    const std::vector<EngineLine>& lines,
    const int requested_elo,
    const double unit_random) {
  const double diversity_random = std::fmod(
      clamp_unit_random(unit_random) * 0.7548776662466927 + 0.5698402909980532, 1.0);
  const auto plan = plan_bot_move(requested_elo, unit_random, diversity_random, 20);
  return choose_scout_bot_move(lines, plan);
}

}  // namespace kchess
