// -----------------------------------------------------------------------------
// Section: Opening drill state machine over the practice ABI
// -----------------------------------------------------------------------------

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "kchess/core_api.h"

namespace {

using nlohmann::json;

void expect(const bool condition, const std::string& message) {
  if (!condition) throw std::runtime_error(message);
}

std::string take_string(char* value) {
  expect(value != nullptr, "Expected native string result");
  std::string result(value);
  kc_string_free(value);
  return result;
}

struct Core {
  Core() {
    const auto directory =
        std::filesystem::temp_directory_path() / "kchess_opening_drill_tests";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    handle = kc_core_create(directory.string().c_str());
    expect(handle != nullptr, "Core handle must be created");
    expect(kc_core_initialize(handle) == KC_STATUS_OK, "Core must initialize");
  }
  ~Core() { kc_core_destroy(handle); }

  json practice(const json& request) {
    auto* text = kc_practice_command_json(handle, request.dump().c_str());
    if (text == nullptr) {
      const auto* error = kc_core_last_error(handle);
      throw std::runtime_error(std::string("practice command failed: ") +
                               (error != nullptr ? error : "unknown error"));
    }
    return json::parse(take_string(text));
  }

  // The free-board resolver returns null for an illegal move.
  bool legal(const std::string& fen, const std::string& source, const std::string& target) {
    auto* text = kc_resolve_free_board_move_json(
        handle, fen.c_str(), source.c_str(), target.c_str());
    if (text == nullptr) return false;
    kc_string_free(text);
    return true;
  }

  kc_core_handle handle{nullptr};
};

std::string square(int file, int rank) {
  return std::string{static_cast<char>('a' + file), static_cast<char>('1' + rank)};
}

// Every legal move in the position, as source/target pairs. Slow, but it keeps
// the test independent of which scenario the catalogue happens to list first.
std::vector<std::pair<std::string, std::string>> legal_moves(Core& core, const std::string& fen) {
  std::vector<std::pair<std::string, std::string>> moves;
  for (int from = 0; from < 64; ++from) {
    for (int to = 0; to < 64; ++to) {
      if (from == to) continue;
      const auto source = square(from % 8, from / 8);
      const auto target = square(to % 8, to / 8);
      if (core.legal(fen, source, target)) moves.emplace_back(source, target);
    }
  }
  return moves;
}

json start(Core& core, int opening, const std::string& color, int depth) {
  return core.practice({{"op", "start"}, {"kind", "opening"}, {"id", opening},
                        {"color", color}, {"depth", depth}});
}

bool solver_on_move(const json& snapshot) {
  return snapshot.at("position").at("sideToMove") == snapshot.at("solverColor");
}

void test_drill(Core& core) {
  const auto roots = core.practice({{"op", "nodes"}, {"parent", 0}});
  expect(roots.is_array() && !roots.empty(), "The catalogue lists opening scenarios");
  const auto& root = roots.front();
  expect(root.at("targetDepth").get<int>() > 0, "Scenario cards carry a target depth");
  expect(root.at("progress").contains("bestDepth"), "Progress reports the best depth");
  const int opening = root.at("openingId").get<int>();

  auto first = start(core, opening, "white", 3);
  if (first.at("bookExhausted").get<bool>() && first.at("depth").get<int>() == 0) {
    std::cout << "opening drill tests skipped: no opening book on disk" << std::endl;
    return;
  }

  // After start the drill has already played any book reply that was due, so
  // the user is on move with something in the book to find.
  expect(first.at("status") == "active", "A fresh drill is active");
  expect(solver_on_move(first), "A fresh drill waits for the user");
  expect(first.at("bookMoves").get<int>() > 0, "The user has a book move to find");
  expect(first.at("depth") == 0 && first.at("targetDepth") == 3, "Depth starts at zero");
  expect(!first.contains("hint"), "No answer is revealed before a miss");
  expect(first.at("openingMoves").is_array() && !first.at("openingMoves").empty(),
      "The scenario's setup line is reported");
  if (first.contains("opponentMove")) {
    const auto& reply = first.at("opponentMove");
    expect(reply.at("uci").get<std::string>().size() >= 4, "The reply is a UCI move");
    expect(!reply.at("san").get<std::string>().empty(), "The reply has notation");
    expect(reply.at("moveNumber").get<int>() >= 1, "The reply has a move number");
    expect(reply.at("alternatives").get<int>() >= 1, "The reply came from the book");
  }
  core.practice({{"op", "cancel"}, {"session", first.at("session")}});

  // A move outside the book must be refused without touching the board and must
  // reveal the answer. Rim moves are never among a book's top replies, but try
  // each legal move on a fresh session until one is refused, so the test does
  // not depend on the scenario.
  json refused;
  json session;
  const auto moves = legal_moves(core, first.at("position").at("fen"));
  expect(!moves.empty(), "The user has legal moves");
  for (const auto& [source, target] : moves) {
    session = start(core, opening, "white", 3);
    // The fresh session may have been dealt a different reply; only a move
    // that is legal in this very position can be judged.
    if (session.at("position").at("fen") != first.at("position").at("fen")) {
      core.practice({{"op", "cancel"}, {"session", session.at("session")}});
      continue;
    }
    const auto result = core.practice({{"op", "move"}, {"session", session.at("session")},
                                       {"source", source}, {"target", target}});
    if (result.at("accepted") == false) {
      refused = result;
      break;
    }
    core.practice({{"op", "cancel"}, {"session", session.at("session")}});
  }
  expect(!refused.is_null(), "Some legal move lies outside the book and is refused");
  expect(refused.contains("hint") && refused.contains("hintSan"), "A miss reveals the answer");
  expect(refused.at("position").at("fen") == first.at("position").at("fen"),
      "A refused move leaves the board untouched");
  expect(refused.at("depth") == 0, "A refused move does not count as depth");
  expect(refused.at("attempts") == 1, "A refused move counts as an attempt");
  expect(refused.at("clean") == false, "A miss spoils a clean run");

  // Playing the revealed answer is accepted, then the drill keeps going on its
  // own: miss, take the hint, answer, until the target depth is reached.
  auto current = refused;
  for (int turn = 0; turn < 10 && current.at("status") == "active"; ++turn) {
    if (!current.contains("hint")) {
      // Provoke the hint with the first legal move that the book refuses.
      for (const auto& [source, target] : legal_moves(core, current.at("position").at("fen"))) {
        const auto probe = core.practice({{"op", "move"}, {"session", session.at("session")},
                                          {"source", source}, {"target", target}});
        current = probe;
        if (probe.at("accepted") == false || probe.at("status") != "active") break;
      }
      if (current.at("status") != "active" || !current.contains("hint")) continue;
    }
    const auto hint = current.at("hint").get<std::string>();
    const int depth_before = current.at("depth").get<int>();
    current = core.practice({{"op", "move"}, {"session", session.at("session")},
                             {"source", hint.substr(0, 2)}, {"target", hint.substr(2, 2)},
                             {"promotion", hint.size() > 4 ? hint.substr(4) : std::string{}}});
    expect(current.at("accepted") == true, "The revealed answer is accepted");
    expect(current.at("depth").get<int>() == depth_before + 1, "An answer adds one depth");
    expect(!current.contains("hint"), "An accepted answer hides the hint again");
    expect(current.at("answer").at("rank") == 1, "The hint is the book's best move");
    expect(current.at("status") != "active" || solver_on_move(current),
        "An active drill always hands the move back to the user");
  }
  expect(current.at("status") == "completed", "The drill completes");
  expect(current.at("depth").get<int>() == 3 || current.at("bookExhausted") == true,
      "The drill stops at the target depth unless the book ran out");
  expect(current.at("progress").at("bestDepth") == current.at("depth"),
      "The reached depth is stored as progress");
  expect(current.at("progress").at("isMastered") == false,
      "A run with misses does not count towards mastery");
}

}  // namespace

int main() {
  try {
    Core core;
    test_drill(core);
  } catch (const std::exception& error) {
    std::cerr << "opening drill tests failed: " << error.what() << std::endl;
    return 1;
  }
  std::cout << "opening drill tests passed" << std::endl;
  return 0;
}
