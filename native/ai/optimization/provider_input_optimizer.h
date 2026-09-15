#pragma once

#include <cstddef>
#include <vector>

#include "../coach_types.h"
#include "../dto/evidence.h"

namespace kchess::ai {

struct QueryPlan;

// -----------------------------------------------------------------------------
// Section: Provider input budget
// -----------------------------------------------------------------------------

struct ProviderInputOptimizationStats {
  std::size_t input_items{0};
  std::size_t empty_items{0};
  std::size_t profile_filtered_items{0};
  std::size_t exact_duplicate_items{0};
  std::size_t selected_items{0};
  std::size_t estimated_input_tokens{0};
  std::size_t estimated_selected_tokens{0};
  std::size_t evidence_budget_tokens{0};
};

[[nodiscard]] std::vector<EvidenceItem> optimize_provider_evidence(
    const std::vector<EvidenceItem>& evidence,
    ResponseDepth depth,
    std::size_t context_tokens,
    const QueryPlan* plan = nullptr,
    ProviderInputOptimizationStats* stats = nullptr);

}  // namespace kchess::ai
