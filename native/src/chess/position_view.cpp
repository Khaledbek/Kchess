#include "chess/position_view.h"

#include <array>
#include <cctype>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "chess/fen.h"

namespace kchess {
namespace {

std::vector<std::string> fields(const std::string& value) {
  std::istringstream input(value);
  std::vector<std::string> result;
  for (std::string field; input >> field;) result.push_back(std::move(field));
  return result;
}

}  // namespace

std::string position_view_json(const std::string& fen) {
  const auto validation = validate_fen(fen);
  if (!validation.valid) throw std::invalid_argument(validation.error);
  const auto parts = fields(validation.normalized);

  std::array<std::string, 64> pieces{};
  std::size_t square = 0;
  for (const char token : parts[0]) {
    if (token == '/') continue;
    if (std::isdigit(static_cast<unsigned char>(token))) {
      square += static_cast<std::size_t>(token - '0');
    } else if (square < pieces.size()) {
      pieces[square++] = std::string(1, token);
    }
  }
  if (square != pieces.size()) throw std::invalid_argument("Invalid FEN board layout");

  const bool white_to_move = parts[1] == "w";
  nlohmann::json json{
      {"fen", validation.normalized},
      {"pieces", pieces},
      {"sideToMove", white_to_move ? "white" : "black"},
      {"draggableColor", white_to_move ? "white" : "black"},
      {"fullmoveNumber", std::stoi(parts[5])},
  };
  return json.dump();
}

bool same_chess_position(const std::string& left, const std::string& right) {
  const auto left_validation = validate_fen(left);
  const auto right_validation = validate_fen(right);
  if (!left_validation.valid || !right_validation.valid) return false;
  const auto left_fields = fields(left_validation.normalized);
  const auto right_fields = fields(right_validation.normalized);
  if (left_fields.size() < 4 || right_fields.size() < 4) return false;
  for (std::size_t index = 0; index < 4; ++index) {
    if (left_fields[index] != right_fields[index]) return false;
  }
  return true;
}

bool white_to_move(const std::string& fen) {
  const auto validation = validate_fen(fen);
  if (!validation.valid) throw std::invalid_argument(validation.error);
  const auto parts = fields(validation.normalized);
  return parts.size() >= 2 && parts[1] == "w";
}

}  // namespace kchess
