#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "../../ai/dto/query_plan.h"

namespace kchess::knowledge {

// -----------------------------------------------------------------------------
// Section: Query entity contract
// -----------------------------------------------------------------------------

enum class KnowledgeQueryEntityKind {
  kUnknown = 0,
  kEcoCode,
  kOpeningName,
  kTimeControl,
  kGamePhase,
  kPlayerColor,
  kResult,
  kTermination,
  kTemporalScope,
  kStatisticMetric,
  kPositionReference,
  kEvidenceRequest,
};

struct KnowledgeQueryEntity {
  KnowledgeQueryEntityKind kind{KnowledgeQueryEntityKind::kUnknown};
  std::string canonical_value;
  std::string surface;
  double confidence{0.0};
};

// Extracts deterministic routing entities from user text. It intentionally
// produces normalized query hints, not graph truth: final graph IDs are resolved
// by retrieval against the persisted Knowledge Graph.
class KnowledgeEntityExtractor {
 public:
  [[nodiscard]] std::vector<KnowledgeQueryEntity> extract(
      std::string_view query_text, const ai::QueryPlan& plan) const;
};

[[nodiscard]] std::string_view to_string(
    KnowledgeQueryEntityKind kind) noexcept;

}  // namespace kchess::knowledge
