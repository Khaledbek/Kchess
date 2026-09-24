#include "small_models.h"

#include <algorithm>
#include <cmath>

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Embedding math
// -----------------------------------------------------------------------------

double cosine_similarity(const std::vector<float>& a,
                         const std::vector<float>& b) noexcept {
  if (a.empty() || a.size() != b.size()) return 0.0;
  double dot = 0.0;
  double norm_a = 0.0;
  double norm_b = 0.0;
  for (std::size_t i = 0; i < a.size(); ++i) {
    dot += static_cast<double>(a[i]) * b[i];
    norm_a += static_cast<double>(a[i]) * a[i];
    norm_b += static_cast<double>(b[i]) * b[i];
  }
  if (norm_a <= 0.0 || norm_b <= 0.0) return 0.0;
  return std::clamp(dot / std::sqrt(norm_a * norm_b), -1.0, 1.0);
}

}  // namespace kchess::ai
