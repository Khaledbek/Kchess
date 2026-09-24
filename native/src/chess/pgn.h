#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace kchess {

struct ParsedMove {
  int ply_index{0};
  int move_number{1};
  std::string side_to_move;
  std::string san;
  std::string uci;
  std::string fen_before;
  std::string fen_after;
};

struct PgnVariation {
  int starts_after_ply{0};
  int nesting_depth{1};
  std::vector<std::string> san_tokens;
};

struct ParsedGame {
  std::map<std::string, std::string> tags;
  std::vector<ParsedMove> moves;
  std::vector<PgnVariation> variations;
  std::vector<std::string> comments;
  std::vector<std::string> nags;
  std::string initial_fen;
  std::string raw_pgn;
};

struct PgnParseResult {
  bool valid{false};
  ParsedGame game;
  std::string error;
};

PgnParseResult parse_pgn(const std::string& pgn) noexcept;

// Extracts per-mainline-move remaining clock values from PGN comments such as
// {[%clk 0:01:23.4]}. Values are milliseconds and align with ParsedMove ply
// indices. Missing clock comments stay std::nullopt. Variations are ignored.
std::vector<std::optional<std::int64_t>> extract_mainline_clock_millis(
    const std::string& pgn, std::size_t ply_count) noexcept;

}  // namespace kchess
