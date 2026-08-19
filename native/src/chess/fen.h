#pragma once

#include <string>

namespace kchess {

struct FenValidationResult {
  bool valid{false};
  std::string normalized;
  std::string error;
};

FenValidationResult validate_fen(const std::string& fen) noexcept;

inline constexpr const char* kStartFen =
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

}  // namespace kchess
