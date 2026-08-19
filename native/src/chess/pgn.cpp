#include "chess/pgn.h"

#include <algorithm>
#include <cctype>
#include <deque>
#include <optional>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string_view>

#include "chess/fen.h"
#include "engine/stockfish_runtime.h"
#include "movegen.h"
#include "position.h"
#include "types.h"
#include "uci.h"

namespace kchess {
namespace {

std::string trim(std::string value) {
  const auto first = std::find_if_not(value.begin(), value.end(), [](const unsigned char c) {
    return std::isspace(c) != 0;
  });
  const auto last = std::find_if_not(value.rbegin(), value.rend(), [](const unsigned char c) {
                      return std::isspace(c) != 0;
                    }).base();
  return first >= last ? std::string{} : std::string(first, last);
}

std::string unescape_tag(std::string value) {
  std::string result;
  result.reserve(value.size());
  bool escaped = false;
  for (const char character : value) {
    if (escaped) {
      result.push_back(character);
      escaped = false;
    } else if (character == '\\') {
      escaped = true;
    } else {
      result.push_back(character);
    }
  }
  if (escaped) result.push_back('\\');
  return result;
}

std::vector<std::string> tokenize_movetext(
    const std::string& text,
    std::vector<std::string>& comments) {
  std::vector<std::string> tokens;
  std::string token;
  auto flush = [&] {
    if (!token.empty()) {
      tokens.push_back(token);
      token.clear();
    }
  };
  for (std::size_t index = 0; index < text.size();) {
    const char character = text[index];
    if (std::isspace(static_cast<unsigned char>(character))) {
      flush();
      ++index;
      continue;
    }
    if (character == '{') {
      flush();
      const auto end = text.find('}', index + 1);
      if (end == std::string::npos) {
        throw std::runtime_error("Unterminated PGN brace comment");
      }
      comments.push_back(trim(text.substr(index + 1, end - index - 1)));
      index = end + 1;
      continue;
    }
    if (character == ';') {
      flush();
      const auto end = text.find_first_of("\r\n", index + 1);
      const auto length = end == std::string::npos ? text.size() - index - 1
                                                   : end - index - 1;
      comments.push_back(trim(text.substr(index + 1, length)));
      index = end == std::string::npos ? text.size() : end;
      continue;
    }
    if (character == '(' || character == ')') {
      flush();
      tokens.emplace_back(1, character);
      ++index;
      continue;
    }
    token.push_back(character);
    ++index;
  }
  flush();
  return tokens;
}

std::optional<std::string> strip_move_number(std::string token) {
  std::size_t digits = 0;
  while (digits < token.size() && std::isdigit(static_cast<unsigned char>(token[digits]))) {
    ++digits;
  }
  if (digits > 0 && digits < token.size() && token[digits] == '.') {
    std::size_t position = digits;
    while (position < token.size() && token[position] == '.') ++position;
    token.erase(0, position);
  }
  if (token.empty() || token == "..." || token == "e.p." || token == "ep") {
    return std::nullopt;
  }
  return token;
}

Stockfish::PieceType san_piece_type(const char value) {
  switch (value) {
    case 'N':
      return Stockfish::KNIGHT;
    case 'B':
      return Stockfish::BISHOP;
    case 'R':
      return Stockfish::ROOK;
    case 'Q':
      return Stockfish::QUEEN;
    case 'K':
      return Stockfish::KING;
    default:
      return Stockfish::PAWN;
  }
}

Stockfish::PieceType promotion_piece_type(const char value) {
  switch (value) {
    case 'N':
      return Stockfish::KNIGHT;
    case 'B':
      return Stockfish::BISHOP;
    case 'R':
      return Stockfish::ROOK;
    case 'Q':
      return Stockfish::QUEEN;
    default:
      return Stockfish::NO_PIECE_TYPE;
  }
}

Stockfish::Move resolve_san(const Stockfish::Position& position, std::string san) {
  const auto nag = san.find('$');
  if (nag != std::string::npos) san.erase(nag);
  while (!san.empty() && (san.back() == '!' || san.back() == '?')) san.pop_back();
  const bool claims_mate = san.find('#') != std::string::npos;
  const bool claims_check = claims_mate || san.find('+') != std::string::npos;
  san.erase(std::remove_if(san.begin(), san.end(), [](const char value) {
              return value == '+' || value == '#';
            }),
            san.end());

  if (san == "O-O" || san == "0-0" || san == "O-O-O" || san == "0-0-0") {
    const bool king_side = san == "O-O" || san == "0-0";
    for (const auto move : Stockfish::MoveList<Stockfish::LEGAL>(position)) {
      if (move.type_of() == Stockfish::CASTLING
          && (move.to_sq() > move.from_sq()) == king_side) {
        return move;
      }
    }
    return Stockfish::Move::none();
  }

  Stockfish::PieceType promotion = Stockfish::NO_PIECE_TYPE;
  const auto promotion_marker = san.find('=');
  if (promotion_marker != std::string::npos) {
    if (promotion_marker + 1 >= san.size()) return Stockfish::Move::none();
    promotion = promotion_piece_type(san[promotion_marker + 1]);
    san.erase(promotion_marker);
  }
  if (san.size() < 2) return Stockfish::Move::none();
  const char target_file = san[san.size() - 2];
  const char target_rank = san[san.size() - 1];
  if (target_file < 'a' || target_file > 'h' || target_rank < '1' || target_rank > '8') {
    return Stockfish::Move::none();
  }
  const auto target = static_cast<Stockfish::Square>(
      (target_rank - '1') * 8 + (target_file - 'a'));
  san.erase(san.size() - 2);

  const Stockfish::PieceType piece_type =
      !san.empty() && std::string_view("NBRQK").find(san.front()) != std::string_view::npos
      ? san_piece_type(san.front())
      : Stockfish::PAWN;
  if (piece_type != Stockfish::PAWN) san.erase(san.begin());

  const bool capture = san.find('x') != std::string::npos;
  san.erase(std::remove(san.begin(), san.end(), 'x'), san.end());
  const std::string disambiguation = san;

  std::vector<Stockfish::Move> candidates;
  for (const auto move : Stockfish::MoveList<Stockfish::LEGAL>(position)) {
    if (move.type_of() == Stockfish::CASTLING || move.to_sq() != target
        || Stockfish::type_of(position.moved_piece(move)) != piece_type) {
      continue;
    }
    const bool move_is_capture = position.capture(move);
    if (capture != move_is_capture) continue;
    if (promotion != Stockfish::NO_PIECE_TYPE) {
      if (move.type_of() != Stockfish::PROMOTION || move.promotion_type() != promotion) continue;
    } else if (move.type_of() == Stockfish::PROMOTION) {
      continue;
    }
    const char from_file = static_cast<char>('a' + Stockfish::file_of(move.from_sq()));
    const char from_rank = static_cast<char>('1' + Stockfish::rank_of(move.from_sq()));
    if (!disambiguation.empty()
        && ((disambiguation.size() == 1 && disambiguation[0] != from_file
             && disambiguation[0] != from_rank)
            || (disambiguation.size() == 2
                && (disambiguation[0] != from_file || disambiguation[1] != from_rank)))) {
      continue;
    }
    if (claims_check && !position.gives_check(move)) continue;
    candidates.push_back(move);
  }
  if (candidates.size() != 1) return Stockfish::Move::none();

  if (claims_mate) {
    auto states = std::deque<Stockfish::StateInfo>(1);
    Stockfish::Position after;
    after.set(position.fen(), false, &states.back());
    states.emplace_back();
    after.do_move(candidates.front(), states.back(), nullptr);
    if (Stockfish::MoveList<Stockfish::LEGAL>(after).size() != 0 || !after.checkers()) {
      return Stockfish::Move::none();
    }
  }
  return candidates.front();
}

}  // namespace

PgnParseResult parse_pgn(const std::string& pgn) noexcept {
  try {
    if (trim(pgn).empty()) return {.error = "PGN is empty"};

    ParsedGame game;
    game.raw_pgn = pgn;
    std::istringstream lines(pgn);
    std::string line;
    std::string movetext;
    const std::regex tag_pattern(
        R"pgn(^\s*\[([A-Za-z0-9_]+)\s+"((?:\\.|[^"])*)"\]\s*$)pgn");
    bool movetext_started = false;
    while (std::getline(lines, line)) {
      std::smatch match;
      if (!movetext_started && std::regex_match(line, match, tag_pattern)) {
        game.tags[match[1].str()] = unescape_tag(match[2].str());
      } else {
        if (!trim(line).empty()) movetext_started = true;
        movetext += line;
        movetext.push_back('\n');
      }
    }

    const auto setup = game.tags.find("SetUp");
    const auto fen_tag = game.tags.find("FEN");
    game.initial_fen = setup != game.tags.end() && setup->second == "1"
                           ? (fen_tag == game.tags.end() ? std::string{} : fen_tag->second)
                           : kStartFen;
    const auto fen_validation = validate_fen(game.initial_fen);
    if (!fen_validation.valid) {
      return {.error = "PGN start position is invalid: " + fen_validation.error};
    }
    game.initial_fen = fen_validation.normalized;

    const auto tokens = tokenize_movetext(movetext, game.comments);
    std::vector<std::string> mainline;
    std::vector<std::size_t> variation_stack;
    int depth = 0;
    for (auto token : tokens) {
      if (token == "(") {
        ++depth;
        game.variations.push_back(
            {.starts_after_ply = static_cast<int>(mainline.size()), .nesting_depth = depth});
        variation_stack.push_back(game.variations.size() - 1);
        continue;
      }
      if (token == ")") {
        if (depth == 0 || variation_stack.empty()) {
          return {.error = "PGN contains an unmatched closing variation"};
        }
        --depth;
        variation_stack.pop_back();
        continue;
      }
      if (!token.empty() && token.front() == '$') {
        game.nags.push_back(token);
        continue;
      }
      if (token == "1-0" || token == "0-1" || token == "1/2-1/2" || token == "*") {
        continue;
      }
      auto san = strip_move_number(std::move(token));
      if (!san.has_value()) continue;
      const auto inline_nag = san->find('$');
      if (inline_nag != std::string::npos) {
        game.nags.push_back(san->substr(inline_nag));
        san->erase(inline_nag);
      }
      if (depth == 0) {
        mainline.push_back(*san);
      } else {
        game.variations[variation_stack.back()].san_tokens.push_back(*san);
      }
    }
    if (depth != 0) return {.error = "PGN contains an unterminated variation"};

    initialize_stockfish_runtime();
    std::deque<Stockfish::StateInfo> states(1);
    Stockfish::Position position;
    position.set(game.initial_fen, false, &states.back());

    std::istringstream fen_fields(game.initial_fen);
    std::vector<std::string> start_fields;
    std::string field;
    while (fen_fields >> field) start_fields.push_back(field);
    int move_number = std::stoi(start_fields[5]);

    for (std::size_t ply = 0; ply < mainline.size(); ++ply) {
      const std::string before = position.fen();
      const auto side = position.side_to_move();
      const auto move = resolve_san(position, mainline[ply]);
      if (move == Stockfish::Move::none()) {
        return {.error = "Illegal or ambiguous SAN at ply " + std::to_string(ply)
                         + ": " + mainline[ply]};
      }
      const std::string uci = Stockfish::UCIEngine::move(move, false);
      states.emplace_back();
      position.do_move(move, states.back(), nullptr);
      game.moves.push_back({
          .ply_index = static_cast<int>(ply),
          .move_number = move_number,
          .side_to_move = side == Stockfish::WHITE ? "white" : "black",
          .san = mainline[ply],
          .uci = uci,
          .fen_before = before,
          .fen_after = position.fen(),
      });
      if (side == Stockfish::BLACK) ++move_number;
    }

    return {.valid = true, .game = std::move(game)};
  } catch (const std::exception& error) {
    return {.error = std::string("PGN parsing failed: ") + error.what()};
  } catch (...) {
    return {.error = "PGN parsing failed"};
  }
}

}  // namespace kchess
