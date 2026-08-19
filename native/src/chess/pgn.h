#pragma once

#include <map>
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

}  // namespace kchess
