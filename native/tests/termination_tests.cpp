#include <iostream>
#include <string>

#include "services/termination.h"

namespace {

using namespace kchess;

int assertions = 0;
int failures = 0;

void expect_bucket(
    const std::string& what,
    const std::string& pgn,
    const std::string& result,
    const std::string& expected) {
  ++assertions;
  const std::string got = termination_bucket(pgn, result);
  if (got != expected) {
    ++failures;
    std::cerr << "FAIL " << what << ": got '" << got << "', expected '"
              << expected << "'\n";
  }
}

std::string with_tag(const std::string& value) {
  return "[Termination \"" + value + "\"]\n1. e4 e5";
}

// Chess.com writes "<username> won on time", so keyword matching used to see
// the opponent's name: "chessmaster50" read as a fifty-move draw, which put a
// timeout *loss* in the draw bucket and surfaced as "1 draw" showing a loss.
void player_names_do_not_leak_into_the_reason() {
  expect_bucket("name containing 50, on time",
                with_tag("chessmaster50 won on time"), "1-0", "timeout");
  expect_bucket("name containing resign, on time",
                with_tag("resigner99 won on time"), "1-0", "timeout");
  expect_bucket("name containing time, resigned",
                with_tag("timelord won by resignation"), "0-1", "resignation");
  expect_bucket("name containing drawn, checkmated",
                with_tag("drawnout won by checkmate"), "1-0", "checkmate");
}

void real_draws_still_classify_as_draws() {
  expect_bucket("agreement", with_tag("Game drawn by agreement"), "1/2-1/2",
                "draw");
  expect_bucket("repetition", with_tag("Game drawn by repetition"), "1/2-1/2",
                "draw");
  expect_bucket("insufficient material",
                with_tag("Game drawn by insufficient material"), "1/2-1/2",
                "draw");
  expect_bucket("fifty-move rule", with_tag("Game drawn by 50-move rule"),
                "1/2-1/2", "draw");
  expect_bucket("stalemate", with_tag("Game drawn by stalemate"), "1/2-1/2",
                "draw");
}

void ordinary_endings_and_fallbacks() {
  expect_bucket("checkmate", with_tag("player won by checkmate"), "1-0",
                "checkmate");
  expect_bucket("resignation", with_tag("player won by resignation"), "1-0",
                "resignation");
  expect_bucket("timeout", with_tag("player won on time"), "1-0", "timeout");
  expect_bucket("abandonment", with_tag("player won by abandonment"), "1-0",
                "other");
  // Lichess tags carry no player name and must survive untouched.
  expect_bucket("lichess time forfeit", with_tag("Time forfeit"), "1-0",
                "timeout");
  expect_bucket("lichess normal draw", with_tag("Normal"), "1/2-1/2", "draw");
  // Imports without a Termination tag fall back to the result string.
  expect_bucket("no tag, ascii draw", "1. e4 e5", "1/2-1/2", "draw");
  expect_bucket("no tag, unicode draw", "1. e4 e5", "\xc2\xbd-\xc2\xbd",
                "draw");
  expect_bucket("no tag, checkmate marker", "1. e4 e5 2. Qh5 Nc6 3. Qxf7#",
                "1-0", "checkmate");
}

}  // namespace

int main() {
  player_names_do_not_leak_into_the_reason();
  real_draws_still_classify_as_draws();
  ordinary_endings_and_fallbacks();
  if (failures != 0) {
    std::cerr << failures << " of " << assertions << " termination assertions failed\n";
    return 1;
  }
  std::cout << "termination tests passed (" << assertions << " assertions)\n";
  return 0;
}
