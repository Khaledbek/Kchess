// -----------------------------------------------------------------------------
// Section: Opening weakness detection rules
// -----------------------------------------------------------------------------

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "services/opening_weakness.h"

namespace {

using kchess::statistics::OpeningGameEvidence;
using kchess::statistics::OpeningMoveError;
using kchess::statistics::find_opening_weaknesses;

void expect(const bool condition, const std::string& message) {
  if (!condition) throw std::runtime_error(message);
}

constexpr const char* kFriedLiver =
    "Italian Game: Two Knights Defense, Fried Liver Attack";
// Black to move after 1.e4 e5 2.Nf3 Nc6 3.Bc4 Nf6 4.Ng5 d5 5.exd5.
constexpr const char* kAfterExd5 =
    "r1bqkb1r/ppp2ppp/2n2n2/3Pp1N1/2B5/8/PPPP1PPP/RNBQK2R b KQkq -";

OpeningGameEvidence game(const std::string& name, const std::string& color,
                         const std::string& outcome, bool analysed = false,
                         std::vector<OpeningMoveError> errors = {}) {
  return {.eco = "C57", .name = name, .color = color, .outcome = outcome,
          .analysed = analysed, .errors = std::move(errors)};
}

// 5...Nxd5 is the move that walks into the Fried Liver; ply 9 is Black's fifth.
OpeningMoveError nxd5(const std::string& category = "mistake") {
  return {.ply = 9, .category = category, .san = "Nxd5", .position = kAfterExd5,
          .recommended = "Na5"};
}

void test_losing_a_line_repeatedly_is_a_weakness() {
  std::vector<OpeningGameEvidence> games;
  for (int i = 0; i < 4; ++i) games.push_back(game(kFriedLiver, "black", "loss"));
  games.push_back(game(kFriedLiver, "black", "win"));
  const auto found = find_opening_weaknesses(games, 6);
  expect(found.size() == 1, "A line lost 4 of 5 times is reported");
  expect(found[0].level == "variation", "It is reported as the variation");
  expect(found[0].name == kFriedLiver && found[0].color == "black",
      "It names the variation and the side it was played with");
  expect(found[0].family == "Italian Game", "It knows its family");
  expect(found[0].poor_results && !found[0].frequent_errors && !found[0].recurring,
      "Only the results are to blame");
}

void test_small_or_winning_samples_are_not_weaknesses() {
  // Three losses out of three is not yet a pattern.
  std::vector<OpeningGameEvidence> few(3, game(kFriedLiver, "black", "loss"));
  expect(find_opening_weaknesses(few, 6).empty(), "Three games are too few to judge");

  // A winning line with the odd opening slip is fine.
  std::vector<OpeningGameEvidence> winning;
  for (int i = 0; i < 6; ++i) {
    winning.push_back(game("Scandinavian Defense", "black", i < 4 ? "win" : "loss",
                           true, {{.ply = 3 + 2 * i, .category = "mistake",
                                   .san = "Qd8", .position = "p" + std::to_string(i)}}));
  }
  expect(find_opening_weaknesses(winning, 6).empty(),
      "Frequent slips in a line that is being won are not a weakness");

  // Games whose side is unknown say nothing about the profile.
  std::vector<OpeningGameEvidence> unknown(6, game(kFriedLiver, "unknown", "loss"));
  expect(find_opening_weaknesses(unknown, 6).empty(), "Unknown sides are ignored");
}

void test_the_same_mistake_twice_is_called_out() {
  // Only two games, both won: results alone would never flag this line, but
  // the same move in the same position went wrong in both.
  const std::vector<OpeningGameEvidence> games = {
      game(kFriedLiver, "black", "win", true, {nxd5()}),
      game(kFriedLiver, "black", "draw", true, {nxd5("blunder")}),
  };
  const auto found = find_opening_weaknesses(games, 6);
  expect(found.size() == 1, "A repeated mistake is reported");
  const auto& weakness = found[0];
  expect(weakness.recurring.has_value(), "The mistake itself is named");
  const auto& mistake = *weakness.recurring;
  expect(mistake.san == "Nxd5" && mistake.recommended == "Na5",
      "It names the move played and the engine's move");
  expect(mistake.move_number == 5 && mistake.side == "black",
      "It says when: 5... for Black");
  expect(mistake.count == 2, "It counts the games it happened in");
  expect(mistake.category == "blunder", "It keeps the worst verdict");
  expect(weakness.analysed_games == 2 && weakness.games_with_errors == 2 &&
             weakness.errors == 2 && weakness.blunders == 1,
      "The error counts cover every analysed game");
}

void test_frequent_errors_need_losses_too() {
  std::vector<OpeningGameEvidence> games;
  for (int i = 0; i < 3; ++i) {
    // Different slips each time, so nothing recurs.
    games.push_back(game(kFriedLiver, "black", i == 0 ? "win" : "loss", true,
                         {{.ply = 7 + 2 * i, .category = "mistake", .san = "h6",
                           .position = "position-" + std::to_string(i)}}));
  }
  const auto found = find_opening_weaknesses(games, 6);
  expect(found.size() == 1 && found[0].frequent_errors && !found[0].poor_results,
      "Errors in every analysed game of a losing line are reported");
}

void test_family_only_when_no_variation_explains_it() {
  // Losses spread over four Sicilian lines: no single variation qualifies, but
  // the family as a whole does.
  std::vector<OpeningGameEvidence> spread;
  for (const char* line : {"Sicilian Defense: Najdorf Variation",
                           "Sicilian Defense: Dragon Variation",
                           "Sicilian Defense: Alapin Variation",
                           "Sicilian Defense: Smith-Morra Gambit"}) {
    spread.push_back(game(line, "black", "loss"));
  }
  spread.push_back(game("Sicilian Defense: Najdorf Variation", "black", "loss"));
  const auto families = find_opening_weaknesses(spread, 6);
  expect(families.size() == 1 && families[0].level == "family" &&
             families[0].name == "Sicilian Defense",
      "Spread-out losses are reported once, for the family");

  // When a variation explains it, the family is not repeated.
  std::vector<OpeningGameEvidence> focused(5, game(kFriedLiver, "black", "loss"));
  const auto variations = find_opening_weaknesses(focused, 6);
  expect(variations.size() == 1 && variations[0].level == "variation",
      "A weak variation is not echoed by its family");
}

void test_worst_first_and_limited() {
  std::vector<OpeningGameEvidence> games;
  // Lost 4 of 5.
  for (int i = 0; i < 4; ++i) games.push_back(game("French Defense", "white", "loss"));
  games.push_back(game("French Defense", "white", "win"));
  // Lost 12 of 12: more evidence, so worse.
  for (int i = 0; i < 12; ++i) games.push_back(game("Englund Gambit", "white", "loss"));
  // Lost 6 of 8 as Black.
  for (int i = 0; i < 6; ++i) games.push_back(game("Caro-Kann Defense", "black", "loss"));
  for (int i = 0; i < 2; ++i) games.push_back(game("Caro-Kann Defense", "black", "draw"));

  const auto all = find_opening_weaknesses(games, 6);
  expect(all.size() == 3, "Every weak line is found");
  expect(all[0].name == "Englund Gambit", "The worst line comes first");
  expect(all[0].severity > all[1].severity && all[1].severity > all[2].severity,
      "Lines are ordered by severity");
  expect(find_opening_weaknesses(games, 2).size() == 2, "The list is capped");
}

void test_opening_phase() {
  using kchess::statistics::opening_phase_end_ply;
  expect(opening_phase_end_ply(std::nullopt) == 20, "Unnamed games judge 10 moves");
  expect(opening_phase_end_ply(6) == 26, "Ten moves each past a short named line");
  expect(opening_phase_end_ply(30) == 40, "Long named lines are capped at move 20");
  expect(kchess::statistics::opening_family("Italian Game: Two Knights Defense, Fried Liver Attack") ==
             "Italian Game",
      "The family is the name before the first separator");
}

}  // namespace

int main() {
  try {
    test_losing_a_line_repeatedly_is_a_weakness();
    test_small_or_winning_samples_are_not_weaknesses();
    test_the_same_mistake_twice_is_called_out();
    test_frequent_errors_need_losses_too();
    test_family_only_when_no_variation_explains_it();
    test_worst_first_and_limited();
    test_opening_phase();
  } catch (const std::exception& error) {
    std::cerr << "opening weakness tests failed: " << error.what() << std::endl;
    return 1;
  }
  std::cout << "opening weakness tests passed" << std::endl;
  return 0;
}
