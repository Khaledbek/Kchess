#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <vector>

#include "dto/evidence_plan.h"
#include "experts/context_evidence_experts.h"

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Aggregated evidence contract
// -----------------------------------------------------------------------------

struct AggregatedEvidenceItem {
  ExpertEvidence evidence;
  std::vector<EvidenceNeed> matched_needs;
  double relevance_score{0.0};
};

struct EvidenceAggregationResult {
  std::vector<AggregatedEvidenceItem> items;
  std::vector<EvidenceNeed> satisfied_needs;
  std::vector<EvidenceNeed> missing_needs;

  // Diagnostics only. Conflicts are resolved before provider-visible evidence
  // is produced; duplicate/conflict counters must never alter chess truth.
  std::size_t duplicates_removed{0};
  std::size_t conflicts_resolved{0};
  std::size_t coverage_reserved{0};
  std::size_t items_dropped_by_budget{0};
  std::vector<std::string> conflicting_ids;
};

// Combines already collected expert output into one deterministic evidence
// packet. The aggregator does not execute experts, engines, models, databases or
// providers and never invents missing chess facts.
class EvidenceAggregator {
 public:
  [[nodiscard]] EvidenceAggregationResult aggregate(
      const EvidencePlan& plan, std::span<const ExpertEvidence> evidence,
      std::size_t max_items = 0) const;
};

}  // namespace kchess::ai
