#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

#include "analysis/move_classifier_sf19.h"

namespace {

using kchess::MoveCategory;
using kchess::MoveClassifierInput;
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
      // Regression for the supplied game/live-arrow mismatch: once the played
      // move is the authoritative engine recommendation, a disagreeing stale
      // score sample must never demote it to Excellent/Good/Okay or worse.
      auto input = ranked(4, 420, -250);
      input.common.played_is_best = true;
      input.common.second_best_evaluation_cp = 410;
      expect_category(input, MoveCategory::best,
                      "authoritative SF19 bestmove can never be demoted by stale score/rank data");
    }
    {
      auto input = ranked(2, 80, 70);
      expect_category(input, MoveCategory::best,
                      "score-equivalent SF19 alternative is also Best");
    }
    {
      auto input = ranked(2, 100, 55);
      expect_category(input, MoveCategory::excellent,
                      "rank 2 with light loss is Excellent");
    }
    {
      auto input = ranked(3, 100, 25);
      expect_category(input, MoveCategory::good,
                      "rank 3 with light loss is Good");
    }
    {
      auto input = ranked(4, 100, -20);
      expect_category(input, MoveCategory::okay,
                      "rank 4 with light loss is Okay");
    }

    // -----------------------------------------------------------------------
    // Section: Critical and Brilliant
    // -----------------------------------------------------------------------
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
      expect_category(input, MoveCategory::brilliant,
                      "sound best minor-piece sacrifice can be Brilliant");
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
      auto input = ranked(5, -300, -500);
      input.common.allowed_forced_mate = true;
      input.common.played_mate_in = -1;
      expect_category(input, MoveCategory::blunder,
                      "newly hanging mate is always Blunder");
    }
    {
      auto input = ranked(5, 120, -100);
      input.played_final_material_delta_cp = -320;
      expect_category(input, MoveCategory::blunder,
                      "sustained piece loss plus score drop is Blunder");
    }
    {
      auto input = ranked(5, 120, 20);
      input.played_final_material_delta_cp = -100;
      expect_category(input, MoveCategory::mistake,
                      "sustained pawn loss plus score drop is Mistake");
    }
    {
      auto input = ranked(5, 200, 100);
      input.played_final_material_delta_cp = 0;
      input.best_final_material_delta_cp = 0;
      expect_category(input, MoveCategory::miss,
                      "+2 to +1 without material self-damage is a Miss");
    }

    // -----------------------------------------------------------------------
    // Section: SF19 regressions from the supplied game
    // -----------------------------------------------------------------------
    {
      // 3.Qh5 at the comparison depth: large practical drop, but no sustained
      // >2-pawn material loss and no newly hung mate. It must not be Blunder.
      auto input = ranked(5, -24, -185);
      input.played_final_material_delta_cp = 0;
      expect_category(input, MoveCategory::mistake,
                      "SF19 Qh5-like non-material error is Mistake, not Blunder");
    }
    {
      // 4.e5: the move falls outside the top four and loses substantial
      // decision value. Saturated SF19 WDL must not hide it as a harmless move.
      auto input = ranked(5, -156, -392);
      input.played_final_material_delta_cp = 0;
      expect_category(input, MoveCategory::mistake,
                      "SF19 e5-like outside-top-four drop is Mistake");
    }
    {
      // 5...Qe7: a winning position falls close to equality. Even if a short
      // PV has not yet resolved all material consequences, this is a result
      // collapse and is therefore a Blunder.
      auto input = ranked(5, 363, 93);
      input.played_final_material_delta_cp = 0;
      expect_category(input, MoveCategory::blunder,
                      "SF19 winning-advantage collapse is Blunder");
    }
    {
      // 14.Bb5 Bxb5 15.Qxb5 is an ordinary exchange sequence. The transient
      // intermediate bishop disappearance must not be read as a dropped piece.
      auto input = ranked(2, -639, -670);
      input.common.material_loss_cp = 330;  // old transient evidence
      input.played_final_material_delta_cp = 0;
      expect_category(input, MoveCategory::excellent,
                      "temporary exchange loss does not create a false Mistake");
    }
    {
      // Large CP numbers in an already won position must not create a harsh
      // label solely because SF19's numeric scale is wider than SF18's.
      auto input = ranked(3, 934, 872);
      input.played_final_material_delta_cp = 0;
      expect_category(input, MoveCategory::good,
                      "SF19 saturated winning score still respects rank 3 Good");
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
