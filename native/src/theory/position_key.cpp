#include "theory/position_key.h"

#include <array>
#include <sstream>
#include <stdexcept>
#include <vector>

#include "engine/stockfish_runtime.h"
#include "position.h"

namespace kchess {
namespace {

std::string canonical_position_fen(const std::string& fen) {
  std::istringstream input(fen);
  std::array<std::string, 4> fields;
  for (auto& field : fields) {
    if (!(input >> field)) throw std::invalid_argument("FEN has fewer than four fields");
  }
  return fields[0] + " " + fields[1] + " " + fields[2] + " " + fields[3] + " 0 1";
}

int square_index(const char file, const char rank) {
  if (file < 'a' || file > 'h' || rank < '1' || rank > '8') {
    throw std::invalid_argument("Invalid UCI square");
  }
  return (rank - '1') * 8 + (file - 'a');
}

}  // namespace

std::uint64_t stockfish_position_key(const std::string& fen) {
  initialize_stockfish_runtime();
  Stockfish::StateInfo state;
  Stockfish::Position position;
  position.set(canonical_position_fen(fen), false, &state);
  return position.key();
}

std::uint16_t encode_book_move(const std::string& uci_move) {
  if (uci_move.size() != 4 && uci_move.size() != 5) {
    throw std::invalid_argument("KCB move must be four or five UCI characters");
  }
  const int from = square_index(uci_move[0], uci_move[1]);
  const int to = square_index(uci_move[2], uci_move[3]);
  int promotion = 0;
  if (uci_move.size() == 5) {
    switch (uci_move[4]) {
      case 'n': promotion = 1; break;
      case 'b': promotion = 2; break;
      case 'r': promotion = 3; break;
      case 'q': promotion = 4; break;
      default: throw std::invalid_argument("Invalid UCI promotion");
    }
  }
  return static_cast<std::uint16_t>(from | (to << 6) | (promotion << 12));
}

std::string decode_book_move(const std::uint16_t encoded_move) {
  if ((encoded_move & 0x8000U) != 0) throw std::invalid_argument("Reserved move bit is set");
  const int promotion = (encoded_move >> 12) & 7;
  if (promotion > 4) throw std::invalid_argument("Invalid encoded promotion");
  const int from = encoded_move & 63;
  const int to = (encoded_move >> 6) & 63;
  std::string result;
  result += static_cast<char>('a' + from % 8);
  result += static_cast<char>('1' + from / 8);
  result += static_cast<char>('a' + to % 8);
  result += static_cast<char>('1' + to / 8);
  if (promotion != 0) result += std::string(" nbrq")[promotion];
  return result;
}

}  // namespace kchess
