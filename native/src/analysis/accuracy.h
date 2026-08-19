#pragma once

#include <optional>
#include <vector>

#include "core/models.h"

namespace kchess {

struct AccuracyConfig {
  static constexpr int version = 1;
  double loss_decay{4.0};
};

struct AccuracyMove {
  MoveCategory category{MoveCategory::unknown};
  std::optional<double> loss;
};

double move_accuracy(double loss, const AccuracyConfig& config = {});
std::optional<double> game_accuracy(
    const std::vector<AccuracyMove>& moves,
    const AccuracyConfig& config = {});

}  // namespace kchess
