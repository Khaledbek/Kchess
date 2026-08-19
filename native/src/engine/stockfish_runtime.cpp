#include "engine/stockfish_runtime.h"

#include <mutex>

#include "bitboard.h"
#include "position.h"

namespace kchess {

void initialize_stockfish_runtime() {
  static std::once_flag initialized;
  std::call_once(initialized, [] {
    Stockfish::Bitboards::init();
    Stockfish::Position::init();
  });
}

}  // namespace kchess
