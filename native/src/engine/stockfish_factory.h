#pragma once

#include <memory>
#include <string_view>

#include "engine/chess_engine.h"

namespace kchess {

inline constexpr std::string_view kStockfish18Id = "stockfish18";
inline constexpr std::string_view kStockfish19Id = "stockfish19";

inline constexpr std::string_view normalize_stockfish_engine_id(
    const std::string_view engine_id) noexcept {
  return engine_id == kStockfish19Id ? kStockfish19Id : kStockfish18Id;
}
std::shared_ptr<ChessEngine> create_stockfish_engine(std::string_view engine_id);

}  // namespace kchess
