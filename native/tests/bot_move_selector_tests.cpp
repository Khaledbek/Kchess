#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "engine/bot_move_selector.h"

namespace {

// -----------------------------------------------------------------------------
// Section: Centipawn-loss bot policy regressions
// -----------------------------------------------------------------------------

int assertions = 0;

void expect(const bool condition, const std::string& message) {
  ++assertions;
  if (!condition) throw std::runtime_error(message);
}

kchess::EngineLine line(const int rank, const int cp) {
  kchess::EngineLine result;
  result.rank = rank;
  result.evaluation_cp = cp;
  result.moves = {"m" + std::to_string(rank)};
  return result;
}

std::vector<kchess::EngineLine> sample_lines(const int count, const int loss_step = 25) {
  std::vector<kchess::EngineLine> result;
  result.reserve(static_cast<std::size_t>(count));
  for (int rank = 1; rank <= count; ++rank) {
    result.push_back(line(rank, -(rank - 1) * loss_step));
  }
  return result;
}

}  // namespace

int main() {
  try {
    const auto beginner = kchess::bot_difficulty_profile(100);
    const auto pre_native = kchess::bot_difficulty_profile(1200);
    const auto native_floor = kchess::bot_difficulty_profile(1300);
    const auto club = kchess::bot_difficulty_profile(1500);
    const auto strong = kchess::bot_difficulty_profile(3000);
    const auto maximum = kchess::bot_difficulty_profile(3200);

    expect(!pre_native.use_stockfish_limit_strength,
           "1200 Elo remains on the KChess low-Elo path for the next update");
    expect(native_floor.use_stockfish_limit_strength
               && native_floor.stockfish_uci_elo == kchess::kStockfish18LowestUciElo,
           "1300 Elo maps safely to Stockfish 18's calibrated 1320 Elo floor");
    expect(club.use_stockfish_limit_strength && club.stockfish_uci_elo == 1500
               && strong.use_stockfish_limit_strength && strong.stockfish_uci_elo == 3000,
           "1400..3100 Elo is delegated directly to Stockfish 18 UCI_Elo");
    expect(!maximum.use_stockfish_limit_strength && maximum.stockfish_uci_elo == 0,
           "3200 disables strength limiting and stays unrestricted");
    expect(beginner.typical_loss_cp >= 350
               && beginner.maximum_verified_loss_cp >= 800,
           "100 Elo receives a wide but still bounded centipawn loss envelope");
    expect(pre_native.typical_loss_cp < beginner.typical_loss_cp
               && pre_native.maximum_verified_loss_cp < beginner.maximum_verified_loss_cp,
           "low-Elo centipawn error envelopes tighten monotonically toward 1200");
    expect(maximum.maximum_target_rank == 1
               && maximum.verification_depth == 20
               && maximum.best_move_probability == 1.0,
           "3200 Elo remains the exact full-strength rank-1 endpoint");
    expect(beginner.scout_depth < club.verification_depth
               && club.verification_depth < maximum.verification_depth,
           "deep verification effort rises with Elo while the scout stays shallow");
    expect(beginner.scout_node_budget < club.scout_node_budget
               && club.scout_node_budget < strong.scout_node_budget,
           "scout node budgets rise monotonically with Elo");
    expect(beginner.verification_node_budget < club.verification_node_budget
               && club.verification_node_budget < strong.verification_node_budget
               && strong.verification_node_budget < maximum.verification_node_budget,
           "verification node budgets rise with playing strength");
    expect(club.scout_time_budget_ms < club.verification_time_budget_ms,
           "the cheap scout has a stricter wall-clock cap than verification");

    const auto native_club = kchess::plan_bot_move(1500, 0.995, 20);
    expect(native_club.use_stockfish_limit_strength
               && native_club.stockfish_uci_elo == 1500
               && native_club.scout_lines == 1,
           "native-strength play requests one public PV and no KChess loss scout");

    const auto low_best = kchess::plan_bot_move(1200, 0.0, 20);
    const auto low_tail = kchess::plan_bot_move(1200, 0.999, 20);
    const auto low_opening = kchess::plan_bot_move(1200, 0.999, 2);
    expect(low_best.target_loss_cp == 0,
           "low-Elo best-move probability is sampled before search");
    expect(low_tail.target_loss_cp > 0
               && low_tail.target_loss_cp <= low_tail.maximum_verified_loss_cp,
           "low-Elo tail samples a bounded centipawn loss instead of an arbitrary rank");
    expect(low_tail.scout_lines >= 5 && low_tail.scout_lines <= 10,
           "low-Elo scout width stays small and independent of the sampled error size");
    expect(low_opening.target_loss_cp <= 100
               && low_opening.maximum_verified_loss_cp <= 100,
           "opening mistakes are clamped to a small centipawn envelope");

    kchess::BotMovePlan loss_plan;
    loss_plan.target_loss_cp = 155;
    loss_plan.maximum_verified_loss_cp = 220;
    const auto scout = kchess::choose_scout_bot_move(sample_lines(8, 25), loss_plan);
    expect(scout.move == "m7",
           "scout chooses the candidate closest to the pre-sampled centipawn loss");

    loss_plan.target_loss_cp = 500;
    loss_plan.maximum_verified_loss_cp = 180;
    const auto bounded_scout = kchess::choose_scout_bot_move(sample_lines(10, 70), loss_plan);
    expect(bounded_scout.move != "m10",
           "hard loss envelope rejects a shallow candidate that is far too costly");

    auto mate_best = line(1, 20);
    auto mate_blunder = line(2, 10);
    mate_blunder.mate_in = -2;
    loss_plan.target_loss_cp = 150;
    loss_plan.maximum_verified_loss_cp = 1000;
    const auto mate_safe = kchess::choose_scout_bot_move(
        std::vector<kchess::EngineLine>{mate_best, mate_blunder}, loss_plan);
    expect(mate_safe.move == "m1",
           "low-Elo scout never chooses a forced losing mate when a non-losing move exists");

    kchess::BotMoveChoice planned_verified{.move = "m2", .rank = 2, .probability = 1.0};
    loss_plan.maximum_verified_loss_cp = 190;
    const auto verified_good = std::vector<kchess::EngineLine>{line(1, 20), line(2, -120)};
    const auto accepted = kchess::finalize_verified_bot_move(
        verified_good, planned_verified, loss_plan);
    expect(accepted.move == "m2",
           "targeted verification accepts a human-sized inaccuracy inside the Elo envelope");

    const auto verified_bad = std::vector<kchess::EngineLine>{line(1, 20), line(2, -450)};
    const auto rejected = kchess::finalize_verified_bot_move(
        verified_bad, planned_verified, loss_plan);
    expect(rejected.move == "m1",
           "targeted verification aborts a candidate that becomes a tactical blunder");

    const auto max_low_roll = kchess::plan_bot_move(3200, 0.0, 0);
    const auto max_high_roll = kchess::plan_bot_move(3200, 0.999999, 30);
    expect(max_low_roll.target_rank == 1 && max_high_roll.target_rank == 1,
           "3200 always plans rank 1 regardless of opening phase or randomness");

    bool invalid_rejected = false;
    try {
      (void)kchess::bot_difficulty_profile(1550);
    } catch (const std::invalid_argument&) {
      invalid_rejected = true;
    }
    expect(invalid_rejected, "non-100 Elo increments are rejected natively");

    std::cout << "Bot move selector tests passed (" << assertions
              << " assertions).\n";
    return EXIT_SUCCESS;
  } catch (const std::exception& error) {
    std::cerr << "Bot move selector test failed after " << assertions
              << " assertions: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
