#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

#include "dependency_tracker.h"

namespace kchess::knowledge {

// -----------------------------------------------------------------------------
// Section: Knowledge quality contract
// -----------------------------------------------------------------------------

enum class KnowledgeTemporalScope {
  kUnknown = 0,
  kLifetime,
  kRecent,
  kMixed,
};

enum class KnowledgeFreshnessBasis {
  kUnknown = 0,
  kEvidenceTimestamp,
  kExplicitRecentScope,
  kSourceObservation,
};

struct KnowledgeQualityMetrics {
  KnowledgeEntryRef entry;
  double confidence{0.0};
  double coverage{0.0};
  std::optional<double> freshness;
  double source_quality{0.0};
  double evidence_diversity{0.0};
  double importance{0.0};
  std::int64_t sample_size{0};
  std::int64_t evidence_count{0};
  KnowledgeTemporalScope temporal_scope{KnowledgeTemporalScope::kUnknown};
  KnowledgeFreshnessBasis freshness_basis{KnowledgeFreshnessBasis::kUnknown};
  std::int64_t evaluated_at_ms{0};
};

[[nodiscard]] std::string_view to_string(KnowledgeTemporalScope value) noexcept;
[[nodiscard]] std::optional<KnowledgeTemporalScope>
knowledge_temporal_scope_from_string(std::string_view value) noexcept;

[[nodiscard]] std::string_view to_string(KnowledgeFreshnessBasis value) noexcept;
[[nodiscard]] std::optional<KnowledgeFreshnessBasis>
knowledge_freshness_basis_from_string(std::string_view value) noexcept;

[[nodiscard]] bool is_valid_quality_metrics(
    const KnowledgeQualityMetrics& value) noexcept;

}  // namespace kchess::knowledge
