#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "dependency_tracker.h"
#include "knowledge_graph_contract.h"
#include "knowledge_quality.h"

namespace kchess::knowledge {

// -----------------------------------------------------------------------------
// Section: Confidence/freshness assessment
// -----------------------------------------------------------------------------

struct KnowledgeConfidenceAssessment {
  double confidence{0.0};
  std::optional<double> freshness;
  double source_quality{0.0};
  double evidence_diversity{0.0};
  double importance{0.0};
  std::int64_t sample_size{0};
  std::int64_t evidence_count{0};
  KnowledgeTemporalScope temporal_scope{KnowledgeTemporalScope::kUnknown};
  KnowledgeFreshnessBasis freshness_basis{KnowledgeFreshnessBasis::kUnknown};
};

// ConfidenceEngine measures how well one concrete graph assertion is supported.
// It never changes the learned-profile confidence semantics and never starts
// analysis work. Existing `confidence` properties are treated as upstream input.
class ConfidenceEngine {
 public:
  [[nodiscard]] KnowledgeConfidenceAssessment assess_node(
      const KnowledgeNode& node,
      const std::vector<KnowledgeProvenanceRecord>& provenance,
      std::int64_t evaluated_at_ms) const;

  [[nodiscard]] KnowledgeConfidenceAssessment assess_edge(
      const KnowledgeEdge& edge,
      const std::vector<KnowledgeProvenanceRecord>& provenance,
      std::int64_t evaluated_at_ms) const;
};

}  // namespace kchess::knowledge
