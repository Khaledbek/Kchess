#include "analysis/accuracy.h"

#include <algorithm>
#include <cmath>

namespace kchess {

double move_accuracy(const double loss, const AccuracyConfig& config) {
  if (!std::isfinite(loss) || !std::isfinite(config.loss_decay) || config.loss_decay < 0.0) {
    return 0.0;
  }
  return std::clamp(100.0 * std::exp(-config.loss_decay * std::clamp(loss, 0.0, 1.0)),
                    0.0, 100.0);
}

std::optional<double> game_accuracy(
    const std::vector<AccuracyMove>& moves, const AccuracyConfig& config) {
  double total = 0.0;
  int measured = 0;
  int theory = 0;
  for (const auto& move : moves) {
    if (move.category == MoveCategory::theory) {
      ++theory;
      continue;
    }
    if (!move.loss.has_value() || !std::isfinite(*move.loss)) continue;
    total += move_accuracy(*move.loss, config);
    ++measured;
  }
  if (measured > 0) return std::clamp(total / measured, 0.0, 100.0);
  if (theory > 0) return 100.0;  // Explicit all-theory fallback.
  return std::nullopt;
}

}  // namespace kchess
