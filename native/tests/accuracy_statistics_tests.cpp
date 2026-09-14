// -----------------------------------------------------------------------------
// Section: Accuracy statistics: phases, trend and the shared accuracy formula
// -----------------------------------------------------------------------------

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "analysis/accuracy.h"
#include "services/accuracy_statistics.h"

namespace {

using namespace kchess;
using namespace kchess::statistics;

void expect(const bool condition, const std::string& message) {
  if (!condition) throw std::runtime_error(message);
}

bool near(const double a, const double b, const double tolerance = 1e-9) {
  return std::abs(a - b) <= tolerance;
}

AnalysedGame game(double accuracy, int blunders = 0) {
  AnalysedGame result;
  result.color = "white";
  result.time_control = "blitz";
  result.accuracy = accuracy;
  result.blunders = blunders;
  return result;
}

void test_phase_boundaries_match_the_phase_card() {
  expect(phase_of_ply(0) == GamePhase::opening, "1. is the opening");
  expect(phase_of_ply(23) == GamePhase::opening, "12... is still the opening");
  expect(phase_of_ply(24) == GamePhase::middlegame, "13. starts the middlegame");
  expect(phase_of_ply(59) == GamePhase::middlegame, "30... is still the middlegame");
  expect(phase_of_ply(60) == GamePhase::endgame, "31. starts the endgame");
}

// Phase accuracy is only meaningful if it is the game formula over fewer moves.
void test_aggregate_is_the_game_formula() {
  AccuracyMove sharp;
  sharp.legal_move_count = 20;
  sharp.best = {.evaluation_cp = 40, .depth = 12};
  sharp.second_best = {.evaluation_cp = -150, .depth = 12};
  sharp.played_root = {.evaluation_cp = -150, .depth = 12};
  sharp.played_after = {.evaluation_cp = -150, .depth = 13};
  AccuracyMove quiet;
  quiet.legal_move_count = 30;
  quiet.played_is_best = true;
  quiet.best = {.evaluation_cp = 20, .depth = 12};
  quiet.second_best = {.evaluation_cp = 10, .depth = 12};
  quiet.played_root = {.evaluation_cp = 20, .depth = 12};
  quiet.played_after = {.evaluation_cp = 20, .depth = 13};
  AccuracyMove book;
  book.theory = true;

  const std::vector<AccuracyMove> moves = {sharp, quiet, book};
  std::vector<AccuracySample> samples;
  for (const auto& move : moves) {
    samples.push_back({.theory = move.theory,
                       .accuracy = move.theory ? std::nullopt : move_accuracy(move),
                       .weight = move.theory ? 0.0 : move_accuracy_weight(move)});
  }
  const auto from_moves = game_accuracy(moves);
  const auto from_samples = aggregate_accuracy(samples);
  expect(from_moves.has_value() && from_samples.has_value(), "Both score the game");
  expect(near(*from_moves, *from_samples), "Stored samples reproduce game accuracy");

  // The blend of weighted arithmetic and harmonic means, by hand.
  const auto blended = aggregate_accuracy({{.accuracy = 100.0, .weight = 1.0},
                                           {.accuracy = 50.0, .weight = 1.0}});
  expect(blended.has_value() && near(*blended, (75.0 + 200.0 / 3.0) / 2.0),
      "Arithmetic and harmonic means are averaged");
  expect(aggregate_accuracy({{.theory = true}}) == 100.0, "Only theory scores 100");
  expect(!aggregate_accuracy({}).has_value(), "Nothing to score is null");
}

void test_phase_accuracy() {
  AnalysedGame first = game(80);
  first.phase_samples[0] = {{.theory = true}, {.accuracy = 90.0, .weight = 1.0}};
  first.phase_samples[2] = {{.accuracy = 40.0, .weight = 1.0}};
  first.phase_errors = {0, 0, 2};
  AnalysedGame second = game(70);
  second.phase_samples[0] = {{.accuracy = 70.0, .weight = 1.0}};
  second.phase_samples[1] = {{.theory = true}};  // book only: no verdict
  second.phase_errors = {1, 0, 0};

  const auto phases = phase_accuracy({first, second});
  expect(phases[0].games == 2 && near(*phases[0].accuracy, 80.0),
      "Opening accuracy averages the games that played one");
  expect(near(phases[0].errors_per_game, 0.5), "Opening errors per game");
  expect(phases[1].games == 1 && !phases[1].accuracy.has_value(),
      "A phase of book moves only counts but has no accuracy");
  expect(phases[2].games == 1 && near(*phases[2].accuracy, 40.0) &&
             near(phases[2].errors_per_game, 2.0),
      "Endgame figures come from the one game that reached it");
}

void test_trend() {
  std::vector<AnalysedGame> few(9, game(70));
  const auto insufficient = accuracy_trend(few);
  expect(insufficient.verdict == "insufficient" && insufficient.games_needed == 1,
      "Nine games are one short of a verdict");

  std::vector<AnalysedGame> rising;
  for (int i = 0; i < 10; ++i) rising.push_back(game(70, 3));
  for (int i = 0; i < 10; ++i) rising.push_back(game(75, 1));
  const auto improving = accuracy_trend(rising);
  expect(improving.verdict == "improving" && improving.window == 10,
      "Five points up over the last ten games is improving");
  expect(near(*improving.recent_accuracy, 75) && near(*improving.previous_accuracy, 70),
      "The two stretches are reported");
  expect(near(*improving.recent_blunders, 1) && near(*improving.previous_blunders, 3),
      "Blunders per game are compared too");

  std::vector<AnalysedGame> falling;
  for (int i = 0; i < 10; ++i) falling.push_back(game(75));
  for (int i = 0; i < 10; ++i) falling.push_back(game(72));
  expect(accuracy_trend(falling).verdict == "declining", "Three points down is declining");

  std::vector<AnalysedGame> flat;
  for (int i = 0; i < 10; ++i) flat.push_back(game(70));
  for (int i = 0; i < 10; ++i) flat.push_back(game(71.5));
  expect(accuracy_trend(flat).verdict == "steady", "A point and a half is noise");

  // Old history does not dilute the comparison: only the last 40 games count.
  std::vector<AnalysedGame> long_history(100, game(50));
  for (int i = 0; i < 20; ++i) long_history.push_back(game(70));
  for (int i = 0; i < 20; ++i) long_history.push_back(game(80));
  const auto recent_only = accuracy_trend(long_history);
  expect(recent_only.window == 20 && near(*recent_only.previous_accuracy, 70),
      "The window is capped at twenty games");
}

void test_rolling_accuracy() {
  const auto rolling = rolling_accuracy({game(60), game(80), game(100), game(40)}, 2);
  expect(rolling.size() == 4, "One point per game");
  expect(near(rolling[0], 60) && near(rolling[1], 70) && near(rolling[2], 90) &&
             near(rolling[3], 70),
      "Each point averages itself and the game before");
}

}  // namespace

int main() {
  try {
    test_phase_boundaries_match_the_phase_card();
    test_aggregate_is_the_game_formula();
    test_phase_accuracy();
    test_trend();
    test_rolling_accuracy();
  } catch (const std::exception& error) {
    std::cerr << "accuracy statistics tests failed: " << error.what() << std::endl;
    return 1;
  }
  std::cout << "accuracy statistics tests passed" << std::endl;
  return 0;
}
