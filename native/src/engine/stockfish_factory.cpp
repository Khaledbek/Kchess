#include "engine/stockfish_factory.h"

namespace kchess {

std::shared_ptr<ChessEngine> create_stockfish_engine(const std::string_view engine_id) {
  const auto normalized_id = normalize_stockfish_engine_id(engine_id);
  if (normalized_id == kStockfish19Id) return std::make_shared<Stockfish19Engine>();
  return std::make_shared<StockfishEngine>();
}

}  // namespace kchess
