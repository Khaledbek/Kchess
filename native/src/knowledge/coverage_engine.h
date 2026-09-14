#pragma once

#include <vector>

#include "confidence_engine.h"

namespace kchess::knowledge {

// -----------------------------------------------------------------------------
// Section: Entry-level coverage assessment
// -----------------------------------------------------------------------------

// CoverageEngine measures support coverage for one graph assertion. It is not a
// global profile-domain coverage score: Update 121 can aggregate these entry
// metrics into Knowledge Gaps without confusing "known fact" with "all games in
// this topic are covered".
class CoverageEngine {
 public:
  [[nodiscard]] double assess_node(
      const KnowledgeNode& node,
      const std::vector<KnowledgeProvenanceRecord>& provenance,
      const KnowledgeConfidenceAssessment& confidence) const;

  [[nodiscard]] double assess_edge(
      const KnowledgeEdge& edge,
      const std::vector<KnowledgeProvenanceRecord>& provenance,
      const KnowledgeConfidenceAssessment& confidence) const;
};

}  // namespace kchess::knowledge
