#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "confidence_engine.h"
#include "coverage_engine.h"
#include "graph_store.h"
#include "knowledge_quality_store.h"

namespace kchess::knowledge {

// -----------------------------------------------------------------------------
// Section: Quality refresh orchestration
// -----------------------------------------------------------------------------

class KnowledgeQualityEngine {
 public:
  KnowledgeQualityEngine(GraphStore& graph, DependencyTracker& dependencies,
                         KnowledgeQualityStore& store);

  [[nodiscard]] std::optional<KnowledgeQualityMetrics> refresh(
      const KnowledgeEntryRef& entry, std::int64_t evaluated_at_ms);

  [[nodiscard]] std::vector<KnowledgeQualityMetrics> refresh_batch(
      std::int64_t evaluated_at_ms, std::size_t limit = 500,
      std::size_t offset = 0);

 private:
  GraphStore& graph_;
  DependencyTracker& dependencies_;
  KnowledgeQualityStore& store_;
  ConfidenceEngine confidence_;
  CoverageEngine coverage_;
};

}  // namespace kchess::knowledge
