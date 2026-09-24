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
    const kchess::MoveAnalysisFacts& input,
    const kchess::MoveCategory expected,
    const std::string& message) {
  const auto actual = kchess::classify_move(input);
  expect(actual == expected,
         message + " (got " + kchess::move_category_name(actual) + ")");
}

kchess::MoveAnalysisFacts ranked_input(
    const int rank,
    const double best,
    const double played,
    const int best_cp,
    const int played_cp) {
  kchess::MoveAnalysisFacts input;
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
  using kchess::MoveAnalysisFacts;

  try {
    // -----------------------------------------------------------------------
    // Section: Priority and engine-rank invariants
    // -----------------------------------------------------------------------
    {
      MoveAnalysisFacts input;
      input.theory = true;
      input.played_is_best = true;
      input.legal_move_count = 20;
      expect_category(input, MoveCategory::theory, "Theory overrides engine labels");
    }
    {
      const auto context = kchess::position_context(
          "8/8/8/8/8/5k2/8/r6K w - - 0 1");
      expect(context.legal_move_count == 1, "forced fixture has exactly one legal move");
      MoveAnalysisFacts input;
      input.played_is_best = true;
      input.legal_move_count = context.legal_move_count;
      input.best_expected_score = 0.02;
      input.played_expected_score = 0.02;
      expect_category(input, MoveCategory::forced,
                      "A position with one legal move is Forced, not Best");
    }
    {
      MoveAnalysisFacts input;
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
      auto input = ranked_input(3, 0.700, 0.695, 100, 92);
      expect_category(input, MoveCategory::best,
                      "Rank 3 inside the equivalent cluster is also Best");
    }
    {
      auto input = ranked_input(5, 0.700, 0.680, 100, 60);
      expect_category(input, MoveCategory::excellent,
                      "Rank 5 may be Excellent when it remains in the light-loss cluster");
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
      MoveAnalysisFacts input;
      input.played_is_best = true;
      input.legal_move_count = 20;
      input.played_rank = 1;
      input.best_evaluation_cp = 100;
      input.played_evaluation_cp = 100;
      input.second_best_evaluation_cp = 60;
      expect_category(input, MoveCategory::best,
                      "A 0.4-pawn rank-1 gap is not Great");
    }
    {
      MoveAnalysisFacts input;
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
      MoveAnalysisFacts input;
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
      MoveAnalysisFacts input;
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
      MoveAnalysisFacts input;
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
      MoveAnalysisFacts input;
      input.played_move_uci = "f5g6";
      input.played_is_best = true;
      input.legal_move_count = 20;
      input.played_rank = 1;
      input.material_exposure_present = true;
      input.material_exposure_unprotected = true;
      input.material_exposure_other_piece = true;
      input.material_exposure_preexisting_piece = true;
      input.material_exposure_piece_value_cp = 320;
      input.material_exposure_net_loss_cp = 320;
      input.best_expected_score = 0.72;
      input.played_expected_score = 0.72;
      input.second_best_expected_score = 0.68;
      input.best_evaluation_cp = 150;
      input.played_evaluation_cp = 150;
      input.second_best_evaluation_cp = 120;
      expect_category(input, MoveCategory::brilliant,
                      "fxg6-style ignored minor-piece exposure can be Brilliant");
    }
    {
      MoveAnalysisFacts input;
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
      MoveAnalysisFacts input;
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
      MoveAnalysisFacts input;
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
      MoveAnalysisFacts input;
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
      MoveAnalysisFacts input;
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
      MoveAnalysisFacts input;
      input.legal_move_count = 20;
      input.played_rank = 2;
      input.best_mate_in = 3;
      input.played_mate_in = 5;
      expect_category(input, MoveCategory::excellent,
                      "M3 to M5 is a small mate-distance loss, not a numeric eval error");
    }
    {
      MoveAnalysisFacts input;
      input.legal_move_count = 20;
      input.played_rank = 4;
      input.best_mate_in = 3;
      input.played_mate_in = 10;
      expect_category(input, MoveCategory::okay,
                      "M3 to M10 keeps mate but loses enough precision to be Okay");
    }
    {
      auto input = ranked_input(6, 0.72, 0.63, 200, 100);
      input.best_material_gain_cp = 0;
      input.played_material_gain_cp = 0;
      input.material_loss_cp = 0;
      expect_category(input, MoveCategory::miss,
                      "+2 to +1 without dropping material is a Missed advantage");
    }
    {
      auto input = ranked_input(6, 0.68, 0.59, 120, 20);
      input.material_loss_cp = 100;
      expect_category(input, MoveCategory::mistake,
                      "A meaningful deterioration plus a pawn of material loss is a Mistake");
    }
    {
      auto input = ranked_input(6, 0.62, 0.48, 120, -100);
      input.material_loss_cp = 320;
      expect_category(input, MoveCategory::mistake,
                      "Material loss alone cannot promote a moderate outcome loss to Blunder");
    }
    {
      auto input = ranked_input(6, 0.66, 0.59, 90, -60);
      input.material_loss_cp = 0;
      expect_category(input, MoveCategory::inaccuracy,
                      "A moderate objective loss is a distinct Inaccuracy, not a Miss or Mistake");
    }
    {
      auto input = ranked_input(6, 0.55, 0.43, 40, -70);
      input.material_loss_cp = 0;
      expect_category(input, MoveCategory::mistake,
                      "A move outside top five plus a result-band/eval drop is a positional Mistake");
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
    // Section: Shared outcome-first severity contract
    // -----------------------------------------------------------------------
    {
      const auto severity = kchess::classify_move_severity(
          0.90, 0.86, 900, -200);
      expect(severity == kchess::MoveSeverity::none,
             "Expected score suppresses a noisy CP-only harsh label");
    }
    {
      const auto severity = kchess::classify_move_severity(
          0.70, 0.59, 80, 60);
      expect(severity == kchess::MoveSeverity::mistake,
             "Meaningful expected-score loss is Mistake without material heuristics");
    }
    {
      const auto severity = kchess::classify_move_severity(
          0.82, 0.41, 120, 100);
      expect(severity == kchess::MoveSeverity::blunder,
             "Catastrophic expected-score collapse is Blunder");
    }
    {
      const auto severity = kchess::classify_move_severity(
          std::nullopt, std::nullopt, 180, -100);
      expect(severity == kchess::MoveSeverity::blunder,
             "CP remains a fallback when normalized outcome evidence is absent");
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
      expect(material.final_net_loss_cp == std::max(0, -material.final_delta_cp),
             "PV material evidence exposes settled net loss separately");
    }
    {
      const auto recovery = kchess::pv_material_evidence(
          "6k1/4bN2/8/8/8/8/8/3Q2K1 w - - 0 1",
          {"d1d8", "e7d8", "f7d8"}, 3);
      expect(recovery.maximum_loss_cp >= 800,
             "PV recovery fixture sees the temporary queen concession");
      expect(recovery.recovered_from_maximum_loss_cp >= 300,
             "PV recovery fixture sees material recovered after the concession");
      expect(recovery.final_net_loss_cp < recovery.maximum_loss_cp,
             "recovery reduces the settled material loss versus the peak loss");
      expect(recovery.maximum_loss_ply == 2,
             "PV recovery records where the deepest concession occurs");
    }
    {
      const auto exposure = kchess::root_move_material_exposure_evidence(
          "r4rk1/p4p1p/3p1npQ/1PpPpP2/4P3/2n4P/Pq2BP2/2R3RK w - - 0 22",
          "f5g6");
      expect(exposure.present,
             "fxg6 fixture exposes materially profitable opponent capture");
      expect(exposure.other_piece_than_mover,
             "fxg6 exposure belongs to another white piece, not the moved pawn");
      expect(exposure.piece_was_already_on_square,
             "fxg6 consciously leaves an existing white piece available");
      expect(exposure.exposed_piece_value_cp >= 300,
             "fxg6 fixture records at least minor-piece material exposure");
      expect(exposure.estimated_net_loss_cp >= 300,
             "fxg6 fixture records a meaningful immediate material concession");
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

    std::cout << "Classifier V18 quality tests passed (" << assertions
              << " assertions).\n";
    return EXIT_SUCCESS;
  } catch (const std::exception& error) {
    std::cerr << "Classifier V18 quality test failed after " << assertions
              << " assertions: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
