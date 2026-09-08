#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "analysis/move_classifier.h"
#include "chess/move.h"

namespace {

int assertions = 0;

void expect(const bool condition, const char* message) {
  ++assertions;
  if (!condition) throw std::runtime_error(message);
}

}  // namespace

int main() {
  using kchess::MoveCategory;
  using kchess::MoveClassifierInput;
  try {
    const auto only_reply = kchess::position_context(
        "8/8/8/8/8/5k2/8/r6K w - - 0 1");
    expect(only_reply.in_check && only_reply.legal_move_count == 1,
           "fixture has exactly one legal king reply to check");
    expect(kchess::classify_move({
               .played_is_best = true,
               .sacrifice_against_best_defense = true,
               .only_move_tactical = true,
               .was_in_check_before_move = true,
               .legal_move_count = only_reply.legal_move_count,
               .best_expected_score = .8,
               .played_expected_score = .8,
               .second_best_expected_score = .4,
           }) == MoveCategory::best,
           "the sole legal check response is never Brilliant");

    const auto multiple_replies = kchess::position_context(
        "7k/8/8/8/8/8/8/r6K w - - 0 1");
    expect(multiple_replies.in_check && multiple_replies.legal_move_count > 1,
           "fixture has multiple legal replies to check");
    expect(kchess::classify_move({
               .played_is_best = true,
               .only_move_tactical = true,
               .was_in_check_before_move = true,
               .legal_move_count = multiple_replies.legal_move_count,
               .best_expected_score = .8,
               .played_expected_score = .8,
               .second_best_expected_score = .4,
           }) == MoveCategory::critical,
           "a decisive check response may be Critical but is never Brilliant");
    expect(kchess::classify_move({
               .played_is_best = true,
               .sacrifice_against_best_defense = true,
               .was_in_check_before_move = true,
               .legal_move_count = multiple_replies.legal_move_count,
               .best_expected_score = .8,
               .played_expected_score = .8,
               .second_best_expected_score = .4,
           }) != MoveCategory::brilliant,
           "even a sacrificial-looking reply to check is never Brilliant");

    expect(kchess::root_move_sacrifice_against_best_defense(
               "3rk3/8/8/8/8/8/8/3QK3 w - - 0 1", {"d1d8", "e8d8"}),
           "the root queen sacrifice is verified against the best defensive reply");
    const std::string delayed_sacrifice_fen =
        "3rk3/p7/8/8/8/8/8/K2Q4 w - - 0 1";
    const std::vector<std::string> delayed_sacrifice_pv{
        "a1b1", "a7a6", "d1d8", "e8d8"};
    expect(kchess::material_sacrifice_in_pv(
               delayed_sacrifice_fen, delayed_sacrifice_pv, 4),
           "legacy deep-PV detector sees a later sacrifice in the line");
    expect(!kchess::root_move_sacrifice_against_best_defense(
               delayed_sacrifice_fen, delayed_sacrifice_pv),
           "a sacrifice several plies later does not make the current move Brilliant");
    expect(!kchess::root_move_sacrifice_against_best_defense(
               "3rk3/8/8/8/8/8/P7/3QK3 w - - 0 1", {"a2a3", "d8d1"}),
           "an unrelated quiet move is not a sacrifice when the reply captures a piece that was already hanging");
    expect(kchess::classify_move({
               .played_is_best = true,
               .sacrifice_against_best_defense = true,
               .legal_move_count = 10,
               .best_expected_score = .85,
               .played_expected_score = .85,
               .second_best_expected_score = .55,
           }) == MoveCategory::brilliant,
           "a rank-1 sacrifice that survives best defence and is uniquely strong may be Brilliant");
    expect(kchess::classify_move({
               .played_is_best = false,
               .sacrifice_against_best_defense = true,
               .legal_move_count = 10,
               .best_expected_score = .85,
               .played_expected_score = .84,
               .second_best_expected_score = .55,
               .best_evaluation_cp = 180,
               .played_evaluation_cp = 170,
               .second_best_evaluation_cp = 20,
           }) != MoveCategory::brilliant,
           "a near-best sacrifice is never Brilliant; Brilliant is root-rank-1 strict");
    expect(kchess::classify_move({
               .played_is_best = true,
               .sacrifice_against_best_defense = true,
               .legal_move_count = 12,
               .best_expected_score = 1.0,
               .played_expected_score = 1.0,
               .second_best_expected_score = .72,
               .best_mate_in = 5,
               .played_mate_in = 5,
           }) == MoveCategory::brilliant,
           "a rank-1 sacrifice that forces mate against best defence may be Brilliant");
    expect(kchess::classify_move({
               .played_is_best = true,
               .legal_move_count = 12,
               .best_expected_score = 1.0,
               .played_expected_score = 1.0,
               .second_best_expected_score = .72,
               .best_mate_in = 5,
               .played_mate_in = 5,
           }) == MoveCategory::critical,
           "the same unique mating idea without a sacrifice is Great/Critical, not Brilliant");

    expect(kchess::classify_move({
               .played_is_best = true,
               .legal_move_count = 20,
               .best_expected_score = .6,
               .played_expected_score = .6,
               .second_best_expected_score = .58,
           }) == MoveCategory::best,
           "a normal engine-best move remains Best");
    expect(kchess::classify_move({
               .played_is_best = false,
               .legal_move_count = 20,
               .best_expected_score = .99,
               .played_expected_score = .99,
               .second_best_expected_score = .99,
               .best_evaluation_cp = 420,
               .played_evaluation_cp = 405,
               .second_best_evaluation_cp = 405,
           }) == MoveCategory::excellent,
           "a WDL-tied alternative is Excellent, never Best unless it is Stockfish rank 1");
    expect(kchess::classify_move({
               .played_is_best = false,
               .legal_move_count = 20,
               .best_expected_score = .99,
               .played_expected_score = .99,
               .second_best_expected_score = .99,
               .best_evaluation_cp = 420,
               .played_evaluation_cp = 330,
               .second_best_evaluation_cp = 405,
           }) == MoveCategory::okay,
           "a 90cp WDL-tied alternative is Okay rather than inflated to Excellent");
    expect(kchess::classify_move({
               .played_is_best = false,
               .legal_move_count = 20,
               .best_expected_score = .99,
               .played_expected_score = .99,
               .second_best_expected_score = .99,
               .best_evaluation_cp = 420,
               .played_evaluation_cp = 210,
               .second_best_evaluation_cp = 405,
           }) == MoveCategory::mistake,
           "a large CP drop cannot hide behind saturated WDL");
    expect(kchess::classify_move({
               .played_is_best = true,
               .legal_move_count = 20,
               .best_expected_score = .82,
               .played_expected_score = .82,
               .second_best_expected_score = .68,
               .best_evaluation_cp = 180,
               .played_evaluation_cp = 180,
               .second_best_evaluation_cp = 10,
           }) == MoveCategory::critical,
           "a large CP gap can identify a genuinely narrow Great-move decision");
    expect(kchess::classify_move({
               .played_is_best = true,
               .legal_move_count = 20,
               .best_expected_score = .99,
               .played_expected_score = .99,
               .second_best_expected_score = .98,
               .best_evaluation_cp = 1000,
               .played_evaluation_cp = 1000,
               .second_best_evaluation_cp = 800,
           }) == MoveCategory::best,
           "a large CP gap in an already trivial win is Best, not Great");
    expect(kchess::classify_move({
               .played_is_best = false,
               .sacrifice_against_best_defense = true,
               .legal_move_count = 20,
               .best_expected_score = .99,
               .played_expected_score = .99,
               .second_best_expected_score = .99,
               .best_evaluation_cp = 500,
               .played_evaluation_cp = 320,
               .second_best_evaluation_cp = 300,
           }) != MoveCategory::brilliant,
           "saturated WDL cannot promote a materially worse sacrifice to Brilliant");
    expect(kchess::classify_move({
               .played_is_best = true,
               .legal_move_count = 20,
               .best_expected_score = .74,
               .played_expected_score = .74,
               .second_best_expected_score = .65,
           }) == MoveCategory::best,
           "a modest gap alone does not make an otherwise stable best move Critical");
    expect(kchess::classify_move({
               .played_is_best = true,
               .legal_move_count = 20,
               .best_expected_score = .78,
               .played_expected_score = .78,
               .second_best_expected_score = .46,
           }) == MoveCategory::critical,
           "a best move whose alternatives lose a result band is Critical");
    expect(kchess::classify_move({
               .legal_move_count = 20,
               .best_expected_score = .6,
               .played_expected_score = .54,
           }) == MoveCategory::okay,
           "a clear inaccuracy remains plausibly Okay");
    expect(kchess::classify_move({
               .legal_move_count = 20,
               .best_expected_score = .7,
               .played_expected_score = .58,
           }) == MoveCategory::mistake,
           "a meaningful chance loss is a Mistake");
    expect(kchess::classify_move({
               .legal_move_count = 20,
               .best_expected_score = .8,
               .played_expected_score = .05,
           }) == MoveCategory::blunder,
           "a collapse from a winning position is a Blunder");
    expect(kchess::classify_move({
               .only_move_tactical = true,
               .legal_move_count = 20,
               .best_expected_score = .82,
               .played_expected_score = .45,
               .second_best_expected_score = .50,
           }) == MoveCategory::miss,
           "a concrete unique tactical opportunity that is thrown away is a Miss");
    expect(kchess::classify_move({
               .legal_move_count = 20,
               .best_expected_score = .82,
               .played_expected_score = .45,
               .second_best_expected_score = .50,
           }) == MoveCategory::blunder,
           "the same large collapse without tactical evidence is a Blunder, not a generic Miss");
    expect(kchess::classify_move({
               .legal_move_count = 20,
               .best_expected_score = .95,
               .played_expected_score = .66,
           }) == MoveCategory::mistake,
           "remaining clearly winning after a large drop is capped at Mistake instead of inflating Blunder counts");
    expect(kchess::classify_move({
               .legal_move_count = 20,
               .best_expected_score = .20,
               .played_expected_score = .00,
           }) == MoveCategory::mistake,
           "an already lost position cannot create a new Blunder from threshold noise");
    expect(kchess::classify_move({
               .missed_forced_mate = true,
               .legal_move_count = 20,
               .best_expected_score = .9,
               .played_expected_score = .7,
           }) == MoveCategory::miss,
           "missing a forced mate is a Miss rather than generic threshold noise");
    expect(kchess::classify_move({
               .played_is_best = true,
               .was_in_check_before_move = true,
               .legal_move_count = 1,
               .best_expected_score = .04,
               .played_expected_score = .04,
           }) == MoveCategory::best,
           "forced mate defence is Best, never Brilliant");
    expect(kchess::classify_move({
               .allowed_forced_mate = true,
               .legal_move_count = 15,
               .best_expected_score = .03,
               .played_expected_score = .01,
           }) == MoveCategory::mistake,
           "allowing a new forced mate is at least a Mistake even when already lost");

    const kchess::PositionEvaluation favorable_to_side{
        .wdl = kchess::WdlScore{.wins = 700, .draws = 200, .losses = 100}};
    const kchess::PositionEvaluation unfavorable_to_side{
        .wdl = kchess::WdlScore{.wins = 100, .draws = 200, .losses = 700}};
    expect(std::abs(*kchess::expected_score_side_to_move(favorable_to_side) - .8) < 1e-9
               && std::abs(*kchess::expected_score_mover_after_move(unfavorable_to_side) - .8) < 1e-9,
           "White improvement uses White before and inverted Black-after perspective");
    expect(std::abs(*kchess::expected_score_mover_after_move(favorable_to_side) - .2) < 1e-9,
           "White deterioration is inverted exactly once after the side switch");
    expect(std::abs(*kchess::expected_score_side_to_move(favorable_to_side) - .8) < 1e-9
               && std::abs(*kchess::expected_score_mover_after_move(unfavorable_to_side) - .8) < 1e-9,
           "Black improvement uses Black before and inverted White-after perspective");
    expect(std::abs(*kchess::expected_score_mover_after_move(favorable_to_side) - .2) < 1e-9,
           "Black deterioration is inverted exactly once after the side switch");

    const std::string variation_start =
        "rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 2";
    std::string variation_fen = variation_start;
    const std::vector<std::string> variation_moves{
        "g1f3", "b8c6", "f1b5", "a7a6", "b5a4"};
    for (const auto& move : variation_moves) {
      const auto applied = kchess::apply_legal_uci_move(variation_fen, move);
      expect(!applied.san.empty() && applied.fen_after != variation_fen,
             "each half-move in the five-ply variation is legal and advances FEN");
      variation_fen = applied.fen_after;
    }
    expect(variation_start.find("5N2") == std::string::npos,
           "temporary variation does not mutate its original FEN value");

    std::cout << "All " << assertions << " focused classifier assertions passed.\n";
    return EXIT_SUCCESS;
  } catch (const std::exception& error) {
    std::cerr << "Classifier quality test failed: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
