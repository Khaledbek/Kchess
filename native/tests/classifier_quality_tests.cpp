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
               .material_sacrifice = true,
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
               .material_sacrifice = true,
               .was_in_check_before_move = true,
               .legal_move_count = multiple_replies.legal_move_count,
               .best_expected_score = .8,
               .played_expected_score = .8,
               .second_best_expected_score = .4,
           }) != MoveCategory::brilliant,
           "even a sacrificial-looking reply to check is never Brilliant");

    expect(kchess::material_sacrifice_in_pv(
               "3rk3/8/8/8/8/8/8/3QK3 w - - 0 1", {"d1d8", "e8d8"}),
           "queen-for-rook fixture is detected as a real material sacrifice");
    expect(kchess::classify_move({
               .played_is_best = true,
               .material_sacrifice = true,
               .legal_move_count = 10,
               .best_expected_score = .85,
               .played_expected_score = .85,
               .second_best_expected_score = .55,
           }) == MoveCategory::brilliant,
           "a best move with alternatives, a large gap and sacrifice may be Brilliant");

    expect(kchess::classify_move({
               .played_is_best = true,
               .legal_move_count = 20,
               .best_expected_score = .6,
               .played_expected_score = .6,
               .second_best_expected_score = .58,
           }) == MoveCategory::best,
           "a normal engine-best move remains Best");
    expect(kchess::classify_move({
               .played_is_best = true,
               .legal_move_count = 20,
               .best_expected_score = .74,
               .played_expected_score = .74,
               .second_best_expected_score = .53,
           }) == MoveCategory::best,
           "a sizeable gap alone does not make an otherwise stable best move Critical");
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
           }) == MoveCategory::excellent,
           "an already lost position does not inflate a small loss into Blunder");

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
