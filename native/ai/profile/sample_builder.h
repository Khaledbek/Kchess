#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "game_relevance_scorer.h"

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Representative initial sample
// -----------------------------------------------------------------------------

struct InitialSample {
  std::vector<std::string> game_ids;
  std::size_t requested_size{0};
};

class InitialSampleBuilder {
 public:
  [[nodiscard]] InitialSample build(
      const std::vector<ProfileGameEvidence>& games,
      std::int64_t now_seconds,
      std::size_t minimum = 40,
      std::size_t preferred = 60,
      std::size_t maximum = 80) const;
};

}  // namespace kchess::ai
