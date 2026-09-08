#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "analysis/move_classifier.h"

namespace {

int assertions = 0;

void expect(const bool condition, const std::string& message) {
  ++assertions;
  if (!condition) throw std::runtime_error(message);
}

void expect_category(
    const kchess::MoveClassifierInput& input,
    const kchess::MoveCategory expected,
    const std::string& message) {
  const auto actual = kchess::classify_move(input);
  expect(actual == expected,
         message + " (got " + kchess::move_category_name(actual) + ")");
}

kchess::MoveClassifierInput ranked_input(
    const int rank,
    const double best,
    const double played,
    const int best_cp,
    const int played_cp) {
  kchess::MoveClassifierInput input;
  input.legal_move_count = 24;
  input.played_rank = rank;
  input.best_expected_score = best;
  input.played_expected_score = played;
  input.best_evaluation_cp = best_cp;
  input.played_evaluation_cp = played_cp;
  return input;
}

}  // namespace

int main() {
  using kchess::MoveCategory;
  using kchess::MoveClassifierInput;

  try {
    // -----------------------------------------------------------------------
    // Section: Priority and engine-rank invariants
    // -----------------------------------------------------------------------
    {
      MoveClassifierInput input;
      input.theory = true;
      input.played_is_best = true;
      input.legal_move_count = 20;
      expect_category(input, MoveCategory::theory, "Theory overrides engine labels");
    }
    {
      const auto context = kchess::position_context(
          "8/8/8/8/8/5k2/8/r6K w - - 0 1");
      expect(context.legal_move_count == 1, "forced fixture has exactly one legal move");
      MoveClassifierInput input;
      input.played_is_best = true;
      input.legal_move_count = context.legal_move_count;
      input.best_expected_score = 0.02;
      input.played_expected_score = 0.02;
      expect_category(input, MoveCategory::forced,
                      "A position with one legal move is Forced, not Best");
    }
    {
      MoveClassifierInput input;
      input.played_is_best = true;
      input.legal_move_count = 20;
      input.best_expected_score = 0.72;
      input.played_expected_score = 0.60;  // independent after-search noise
      input.best_evaluation_cp = 150;
      input.played_evaluation_cp = 70;
      expect_category(input, MoveCategory::best,
                      "Stockfish final bestmove cannot be downgraded by after-search noise");
    }
    {
      auto input = ranked_input(2, 0.700, 0.698, 100, 99);
      expect_category(input, MoveCategory::best,
                      "A score-equivalent alternative is also Best");
    }
    {
      auto input = ranked_input(2, 0.70, 0.68, 100, 65);
      expect_category(input, MoveCategory::excellent,
                      "Engine rank 2 with a light deviation is Excellent");
    }
    {
      auto input = ranked_input(3, 0.70, 0.655, 100, 35);
      expect_category(input, MoveCategory::good,
                      "Engine rank 3 with a light deviation is Good");
    }
    {
      auto input = ranked_input(4, 0.70, 0.63, 100, 0);
      expect_category(input, MoveCategory::okay,
                      "Engine rank 4 with a light deviation is Okay");
    }

    // -----------------------------------------------------------------------
    // Section: Critical / Great and Brilliant
    // -----------------------------------------------------------------------
    {
      MoveClassifierInput input;
      input.played_is_best = true;
      input.legal_move_count = 20;
      input.played_rank = 1;
      input.best_expected_score = 0.72;
      input.played_expected_score = 0.72;
      input.second_best_expected_score = 0.51;
      input.best_evaluation_cp = 160;
      input.played_evaluation_cp = 160;
      input.second_best_evaluation_cp = 20;
      expect_category(input, MoveCategory::critical,
                      "Rank 1 is Critical when the second choice changes practical outcome");
    }
    {
      MoveClassifierInput input;
      input.played_is_best = true;
      input.legal_move_count = 20;
      input.played_rank = 1;
      input.best_expected_score = 0.99;
      input.played_expected_score = 0.99;
      input.second_best_expected_score = 0.985;
      input.best_evaluation_cp = 1000;
      input.played_evaluation_cp = 1000;
      input.second_best_evaluation_cp = 780;
      expect_category(input, MoveCategory::best,
                      "A saturated +10 vs +7.8 CP gap is not falsely Critical");
    }
    {
      MoveClassifierInput input;
      input.played_is_best = true;
      input.legal_move_count = 20;
      input.played_rank = 1;
      input.best_expected_score = 0.18;
      input.played_expected_score = 0.18;
      input.second_best_expected_score = 0.03;
      input.best_evaluation_cp = -250;
      input.played_evaluation_cp = -250;
      input.second_best_evaluation_cp = -600;
      expect_category(input, MoveCategory::critical,
                      "Critical can also be a unique defensive move in a bad position");
    }
    {
      MoveClassifierInput input;
      input.played_is_best = true;
      input.sacrifice_against_best_defense = true;
      input.legal_move_count = 20;
      input.played_rank = 1;
      input.sacrifice_piece_value_cp = 320;
      input.sacrifice_net_material_loss_cp = 220;
      input.best_expected_score = 0.72;
      input.played_expected_score = 0.72;
      input.second_best_expected_score = 0.68;
      input.best_evaluation_cp = 150;
      input.played_evaluation_cp = 150;
      input.second_best_evaluation_cp = 120;
      expect_category(input, MoveCategory::brilliant,
                      "A sound best minor-piece sacrifice may be Brilliant");
    }
    {
      MoveClassifierInput input;
      input.played_is_best = true;
      input.sacrifice_against_best_defense = true;
      input.legal_move_count = 20;
      input.played_rank = 1;
      input.sacrifice_piece_value_cp = 100;
      input.sacrifice_net_material_loss_cp = 100;
      input.best_expected_score = 0.72;
      input.played_expected_score = 0.72;
      input.second_best_expected_score = 0.68;
      input.best_evaluation_cp = 150;
      input.played_evaluation_cp = 150;
      expect_category(input, MoveCategory::best,
                      "A pawn sacrifice alone is never Brilliant");
    }
    {
      MoveClassifierInput input;
      input.played_is_best = true;
      input.sacrifice_against_best_defense = true;
      input.legal_move_count = 20;
      input.played_rank = 1;
      input.sacrifice_piece_value_cp = 500;
      input.sacrifice_net_material_loss_cp = 300;
      input.best_expected_score = 0.42;
      input.played_expected_score = 0.42;
      input.second_best_expected_score = 0.40;
      input.best_evaluation_cp = -25;
      input.played_evaluation_cp = -25;
      expect_category(input, MoveCategory::best,
                      "Giving material without a resulting advantage is not Brilliant");
    }
    {
      MoveClassifierInput input;
      input.played_is_best = true;
      input.sacrifice_against_best_defense = true;
      input.legal_move_count = 20;
      input.played_rank = 1;
      input.sacrifice_piece_value_cp = 500;
      input.sacrifice_net_material_loss_cp = 300;
      input.best_expected_score = 0.995;
      input.played_expected_score = 0.995;
      input.second_best_expected_score = 0.990;
      input.best_evaluation_cp = 900;
      input.played_evaluation_cp = 900;
      expect_category(input, MoveCategory::best,
                      "A sacrifice in an already completely decided win is not Brilliant");
    }

    // -----------------------------------------------------------------------
    // Section: Mate, material and missed opportunities
    // -----------------------------------------------------------------------
    {
      MoveClassifierInput input;
      input.allowed_forced_mate = true;
      input.legal_move_count = 25;
      input.played_rank = 5;
      input.best_expected_score = 0.08;  // roughly already around -3
      input.played_expected_score = 0.0;
      input.best_evaluation_cp = -300;
      input.played_mate_in = -1;
      expect_category(input, MoveCategory::blunder,
                      "Newly allowing M1 is always a Blunder even from an already bad eval");
    }
    {
      MoveClassifierInput input;
      input.missed_forced_mate = true;
      input.legal_move_count = 20;
      input.played_rank = 5;
      input.best_expected_score = 1.0;
      input.played_expected_score = 0.78;
      input.best_mate_in = 3;
      expect_category(input, MoveCategory::miss,
                      "Failing to play an available forced mate is a Miss");
    }
    {
      auto input = ranked_input(5, 0.72, 0.63, 200, 100);
      input.best_material_gain_cp = 0;
      input.played_material_gain_cp = 0;
      input.material_loss_cp = 0;
      expect_category(input, MoveCategory::miss,
                      "+2 to +1 without dropping material is a Missed advantage");
    }
    {
      auto input = ranked_input(5, 0.68, 0.59, 120, 20);
      input.material_loss_cp = 100;
      expect_category(input, MoveCategory::mistake,
                      "A meaningful deterioration plus a pawn of material loss is a Mistake");
    }
    {
      auto input = ranked_input(5, 0.62, 0.48, 120, -100);
      input.material_loss_cp = 320;
      expect_category(input, MoveCategory::blunder,
                      "Losing a minor piece with a severe eval drop is a Blunder");
    }
    {
      auto input = ranked_input(5, 0.55, 0.43, 40, -70);
      input.material_loss_cp = 0;
      expect_category(input, MoveCategory::mistake,
                      "A move outside top four plus a result-band/eval drop is a positional Mistake");
    }
    {
      auto input = ranked_input(3, 0.70, 0.61, 100, 10);
      input.material_loss_cp = 0;
      input.best_material_gain_cp = 200;
      input.played_material_gain_cp = 0;
      expect_category(input, MoveCategory::miss,
                      "Missing a concrete material opportunity can be a Miss without self-damage");
    }

    // -----------------------------------------------------------------------
    // Section: Board-evidence regressions
    // -----------------------------------------------------------------------
    {
      const auto sacrifice = kchess::root_move_sacrifice_evidence(
          "3rk3/8/8/8/8/8/8/3QK3 w - - 0 1", {"d1d8", "e8d8"});
      expect(sacrifice.verified, "direct queen sacrifice is verified");
      expect(sacrifice.offered_piece_value_cp == 900,
             "sacrifice records the offered queen value");
      expect(sacrifice.net_material_loss_cp >= 300,
             "sacrifice records meaningful net material given up");
    }
    {
      const auto not_sacrifice = kchess::root_move_sacrifice_evidence(
          "3rk3/8/8/8/8/8/P7/3QK3 w - - 0 1", {"a2a3", "d8d1"});
      expect(!not_sacrifice.verified,
             "a quiet move cannot inherit a sacrifice from an already hanging piece");
    }
    {
      const auto material = kchess::pv_material_evidence(
          "3rk3/8/8/8/8/8/8/3QK3 w - - 0 1", {"d1d8", "e8d8"}, 2);
      expect(material.maximum_loss_cp >= 300,
             "PV material evidence sees the eventual material loss");
    }

    // -----------------------------------------------------------------------
    // Section: Expected-result conversion
    // -----------------------------------------------------------------------
    {
      const auto neutral = kchess::expected_score_side_to_move({.evaluation_cp = 0});
      const auto plus_two = kchess::expected_score_side_to_move({.evaluation_cp = 200});
      expect(neutral.has_value() && std::abs(*neutral - 0.5) < 1e-9,
             "0cp maps to a neutral expected result");
      expect(plus_two.has_value() && *plus_two > *neutral && *plus_two < 1.0,
             "positive CP maps monotonically to higher practical winning chances");
    }

    std::cout << "Classifier V11 quality tests passed (" << assertions
              << " assertions).\n";
    return EXIT_SUCCESS;
  } catch (const std::exception& error) {
    std::cerr << "Classifier V11 quality test failed after " << assertions
              << " assertions: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
