#include "knowledge_quality.h"

namespace kchess::knowledge {

std::string_view to_string(const KnowledgeTemporalScope value) noexcept {
  switch (value) {
    case KnowledgeTemporalScope::kUnknown:
      return "unknown";
    case KnowledgeTemporalScope::kLifetime:
      return "lifetime";
    case KnowledgeTemporalScope::kRecent:
      return "recent";
    case KnowledgeTemporalScope::kMixed:
      return "mixed";
  }
  return "unknown";
}

std::optional<KnowledgeTemporalScope> knowledge_temporal_scope_from_string(
    const std::string_view value) noexcept {
  if (value == "unknown") return KnowledgeTemporalScope::kUnknown;
  if (value == "lifetime") return KnowledgeTemporalScope::kLifetime;
  if (value == "recent") return KnowledgeTemporalScope::kRecent;
  if (value == "mixed") return KnowledgeTemporalScope::kMixed;
  return std::nullopt;
}

std::string_view to_string(const KnowledgeFreshnessBasis value) noexcept {
  switch (value) {
    case KnowledgeFreshnessBasis::kUnknown:
      return "unknown";
    case KnowledgeFreshnessBasis::kEvidenceTimestamp:
      return "evidence_timestamp";
    case KnowledgeFreshnessBasis::kExplicitRecentScope:
      return "explicit_recent_scope";
    case KnowledgeFreshnessBasis::kSourceObservation:
      return "source_observation";
  }
  return "unknown";
}

std::optional<KnowledgeFreshnessBasis> knowledge_freshness_basis_from_string(
    const std::string_view value) noexcept {
  if (value == "unknown") return KnowledgeFreshnessBasis::kUnknown;
  if (value == "evidence_timestamp") {
    return KnowledgeFreshnessBasis::kEvidenceTimestamp;
  }
  if (value == "explicit_recent_scope") {
    return KnowledgeFreshnessBasis::kExplicitRecentScope;
  }
  if (value == "source_observation") {
    return KnowledgeFreshnessBasis::kSourceObservation;
  }
  return std::nullopt;
}

bool is_valid_quality_metrics(const KnowledgeQualityMetrics& value) noexcept {
  const auto bounded = [](const double number) {
    return number >= 0.0 && number <= 1.0;
  };
  return !value.entry.empty() && bounded(value.confidence) &&
         bounded(value.coverage) &&
         (!value.freshness || bounded(*value.freshness)) &&
         bounded(value.source_quality) && bounded(value.evidence_diversity) &&
         bounded(value.importance) && value.sample_size >= 0 &&
         value.evidence_count >= 0 && value.evaluated_at_ms >= 0;
}

}  // namespace kchess::knowledge
