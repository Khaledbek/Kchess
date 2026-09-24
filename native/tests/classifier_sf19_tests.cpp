#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

#include "analysis/move_classifier_sf19.h"

namespace {

using kchess::MoveCategory;
using kchess::MoveAnalysisFacts;
using kchess::MoveClassifierSf19Input;

int assertions = 0;

void expect(const bool condition, const std::string& message) {
  ++assertions;
  if (!condition) throw std::runtime_error(message);
}

void expect_category(
    const MoveClassifierSf19Input& input,
    const MoveCategory expected,
    const std::string& message) {
  const auto actual = kchess::classify_move_sf19(input);
  expect(actual == expected, message);
}

MoveClassifierSf19Input ranked(
    const int rank,
    const int best_cp,
    const int played_cp) {
  MoveClassifierSf19Input input;
  input.common.legal_move_count = 20;
  input.common.played_rank = rank;
  input.common.best_evaluation_cp = best_cp;
  input.common.played_evaluation_cp = played_cp;
  return input;
}

}  // namespace

int main() {
  try {
    // -----------------------------------------------------------------------
    // Section: Priority and engine rank
    // -----------------------------------------------------------------------
    {
      auto input = ranked(1, 25, 25);
      input.common.legal_move_count = 1;
      input.common.played_is_best = true;
      expect_category(input, MoveCategory::forced,
                      "one legal move is Forced");
    }
    {
      auto input = ranked(1, 25, 25);
      input.common.theory = true;
      input.common.played_is_best = true;
      expect_category(input, MoveCategory::theory,
                      "book move remains Theory when it is not forced");
    }
    {
      auto input = ranked(1, 80, 80);
      input.common.played_is_best = true;
      expect_category(input, MoveCategory::best,
                      "final SF19 bestmove is Best");
    }
    {
      auto input = ranked(1, 80, 70);
      input.common.played_is_best = true;
      expect_category(input, MoveCategory::best,
                      "small independent SF19 score drift does not demote the bestmove");
    }
    {
      // Rank-1 is authoritative inside the exact published root snapshot. A
      // contradictory after-position search is handled by the service-level
      // stability gate and must not turn the engine's own arrow into Blunder.
      auto input = ranked(1, 20, -400);
      input.common.played_is_best = true;
      input.common.sacrifice_against_best_defense = true;
      input.common.sacrifice_piece_value_cp = 330;
      input.common.sacrifice_net_material_loss_cp = 300;
      expect_category(input, MoveCategory::best,
                      "SF19 published rank-1 remains Best inside its root snapshot");
    }
    {
      auto input = ranked(2, 80, 70);
      expect_category(input, MoveCategory::best,
                      "score-equivalent SF19 alternative is also Best");
    }
    {
      auto input = ranked(3, 100, 85);
      expect_category(input, MoveCategory::best,
                      "SF19 rank 3 inside the equivalent cluster is also Best");
    }
    {
      auto input = ranked(5, 100, 35);
      expect_category(input, MoveCategory::excellent,
                      "SF19 rank 5 may be Excellent when its loss is still light");
    }
    {
      auto input = ranked(2, 100, 55);
      expect_category(input, MoveCategory::excellent,
                      "rank 2 with light loss is Excellent");
    }
    {
      auto input = ranked(3, 100, 25);
      expect_category(input, MoveCategory::excellent,
                      "rank 3 can be Excellent when it stays in the Excellent cluster");
    }
    {
      auto input = ranked(4, 100, -20);
      expect_category(input, MoveCategory::good,
                      "rank 4 can be Good when it stays in the Good cluster");
    }

    // -----------------------------------------------------------------------
    // Section: Critical and Brilliant
    // -----------------------------------------------------------------------
    {
      auto input = ranked(1, 100, 100);
      input.common.played_is_best = true;
      input.common.second_best_evaluation_cp = 60;
      expect_category(input, MoveCategory::best,
                      "SF19 0.4-pawn rank-1 gap is not Great");
    }
    {
      auto input = ranked(1, 180, 180);
      input.common.played_is_best = true;
      input.common.second_best_evaluation_cp = 20;
      expect_category(input, MoveCategory::critical,
                      "rank 1 is Critical when rank 2 is clearly worse");
    }
    {
      auto input = ranked(1, 160, 160);
      input.common.played_is_best = true;
      input.common.second_best_evaluation_cp = 120;
      input.common.sacrifice_against_best_defense = true;
      input.common.sacrifice_piece_value_cp = 320;
      input.common.sacrifice_net_material_loss_cp = 220;
      input.best_move_verified_after = true;
      expect_category(input, MoveCategory::brilliant,
                      "sound best minor-piece sacrifice can be Brilliant");
    }
    {
      auto input = ranked(1, 160, 160);
      input.common.played_move_uci = "f5g6";
      input.common.played_is_best = true;
      input.common.material_exposure_present = true;
      input.common.material_exposure_unprotected = true;
      input.common.material_exposure_other_piece = true;
      input.common.material_exposure_preexisting_piece = true;
      input.common.material_exposure_piece_value_cp = 320;
      input.common.material_exposure_net_loss_cp = 320;
      input.best_move_verified_after = true;
      expect_category(input, MoveCategory::brilliant,
                      "verified fxg6-style ignored exposure can be Brilliant");
    }
    {
      auto input = ranked(1, 160, 160);
      input.common.played_is_best = true;
      input.common.sacrifice_against_best_defense = true;
      input.common.sacrifice_piece_value_cp = 320;
      input.common.sacrifice_net_material_loss_cp = 220;
      expect_category(input, MoveCategory::best,
                      "unconfirmed SF19 sacrifice is never labelled Brilliant");
    }
    {
      auto input = ranked(1, 160, 160);
      input.common.played_is_best = true;
      input.common.sacrifice_against_best_defense = true;
      input.common.sacrifice_piece_value_cp = 100;
      input.common.sacrifice_net_material_loss_cp = 100;
      expect_category(input, MoveCategory::best,
                      "pawn sacrifice alone is never Brilliant");
    }

    // -----------------------------------------------------------------------
    // Section: Mate, material and missed opportunity
    // -----------------------------------------------------------------------
    {
      auto input = ranked(6, -300, -500);
      input.common.allowed_forced_mate = true;
      input.common.played_mate_in = -1;
      expect_category(input, MoveCategory::blunder,
                      "newly hanging mate is always Blunder");
    }
    {
      auto input = ranked(2, 0, 0);
      input.common.best_mate_in = 3;
      input.common.played_mate_in = 5;
      expect_category(input, MoveCategory::excellent,
                      "SF19 M3 to M5 uses mate distance and remains Excellent");
    }
    {
      auto input = ranked(4, 0, 0);
      input.common.best_mate_in = 3;
      input.common.played_mate_in = 10;
      expect_category(input, MoveCategory::okay,
                      "SF19 M3 to M10 is Okay despite both lines being forced mates");
    }
    {
      auto input = ranked(6, 90, -60);
      input.common.best_expected_score = 0.66;
      input.common.played_expected_score = 0.59;
      input.common.played_final_material_delta_cp = 0;
      expect_category(input, MoveCategory::inaccuracy,
                      "SF19 moderate value loss maps to the distinct Inaccuracy class");
    }
    {
      auto input = ranked(6, 120, -100);
      input.common.played_final_material_delta_cp = -320;
      expect_category(input, MoveCategory::mistake,
                      "sustained piece loss does not override non-catastrophic decision loss");
    }
    {
      auto input = ranked(6, 120, 20);
      input.common.played_final_material_delta_cp = -100;
      expect_category(input, MoveCategory::mistake,
                      "sustained pawn loss plus score drop is Mistake");
    }
    {
      auto input = ranked(6, 200, 100);
      input.common.played_final_material_delta_cp = 0;
      input.common.best_final_material_delta_cp = 0;
      expect_category(input, MoveCategory::miss,
                      "+2 to +1 without material self-damage is a Miss");
    }

    // -----------------------------------------------------------------------
    // Section: SF19 regressions from the supplied game
    // -----------------------------------------------------------------------
    {
      // 3.Qh5 at the comparison depth: large practical drop, but no sustained
      // >2-pawn material loss and no newly hung mate. It must not be Blunder.
      auto input = ranked(6, -24, -185);
      input.common.played_final_material_delta_cp = 0;
      expect_category(input, MoveCategory::mistake,
                      "SF19 Qh5-like non-material error is Mistake, not Blunder");
    }
    {
      // 4.e5: the move falls outside the top five and loses substantial
      // decision value. Saturated SF19 WDL must not hide it as a harmless move.
      auto input = ranked(6, -156, -392);
      input.common.played_final_material_delta_cp = 0;
      expect_category(input, MoveCategory::mistake,
                      "SF19 e5-like outside-top-five drop is Mistake");
    }
    {
      // 5...Qe7: a winning position falls close to equality. Even if a short
      // PV has not yet resolved all material consequences, this is a result
      // collapse and is therefore a Blunder.
      auto input = ranked(6, 363, 93);
      input.common.played_final_material_delta_cp = 0;
      expect_category(input, MoveCategory::blunder,
                      "SF19 winning-advantage collapse is Blunder");
    }
    {
      // 14.Bb5 Bxb5 15.Qxb5 is an ordinary exchange sequence. The transient
      // intermediate bishop disappearance must not be read as a dropped piece.
      auto input = ranked(2, -639, -670);
      input.common.material_loss_cp = 330;  // old transient evidence
      input.common.played_final_material_delta_cp = 0;
      expect_category(input, MoveCategory::excellent,
                      "temporary exchange loss does not create a false Mistake");
    }
    {
      // Large CP numbers in an already won position must not create a harsh
      // label solely because SF19's numeric scale is wider than SF18's.
      auto input = ranked(3, 934, 872);
      input.common.played_final_material_delta_cp = 0;
      expect_category(input, MoveCategory::excellent,
                      "SF19 saturated winning score uses the Excellent quality cluster");
    }

    std::cout << "Stockfish 19 classifier tests passed (" << assertions
              << " assertions).\n";
    return EXIT_SUCCESS;
  } catch (const std::exception& error) {
    std::cerr << "Stockfish 19 classifier test failed after " << assertions
              << " assertions: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
