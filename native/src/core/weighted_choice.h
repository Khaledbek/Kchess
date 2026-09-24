#pragma once

#include <cstdint>
#include <random>
#include <vector>

namespace kchess {

// Draws one candidate proportionally to its `weight` member. A set whose
// weights are all zero degrades to a uniform draw; an empty set returns null.
// The returned pointer refers to an element of `items` and remains valid as
// long as that vector is not modified.
template <typename T>
const T* pick_weighted(const std::vector<T>& items, std::mt19937& random) {
  if (items.empty()) return nullptr;

  std::uint64_t total = 0;
  for (const auto& item : items) total += item.weight;

  if (total == 0) {
    std::uniform_int_distribution<std::size_t> uniform(0, items.size() - 1);
    return &items[uniform(random)];
  }

  std::uniform_int_distribution<std::uint64_t> distribution(0, total - 1);
  std::uint64_t roll = distribution(random);
  for (const auto& item : items) {
    if (roll < item.weight) return &item;
    roll -= item.weight;
  }
  return &items.back();
}

}  // namespace kchess
