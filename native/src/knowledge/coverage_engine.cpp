#include "coverage_engine.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>

namespace kchess::knowledge {
namespace {

[[nodiscard]] double clamp01(const double value) {
  return std::clamp(value, 0.0, 1.0);
}

[[nodiscard]] std::optional<double> property_double(
    const KnowledgeProperties& properties, const std::string_view key) {
  const auto found = properties.find(std::string(key));
  if (found == properties.end()) return std::nullopt;
  if (const auto* value = std::get_if<double>(&found->second)) return *value;
  if (const auto* value = std::get_if<std::int64_t>(&found->second)) {
    return static_cast<double>(*value);
  }
  return std::nullopt;
}

[[nodiscard]] std::int64_t property_int(const KnowledgeProperties& properties,
                                        const std::string_view key) {
  const auto found = properties.find(std::string(key));
  if (found == properties.end()) return 0;
  if (const auto* value = std::get_if<std::int64_t>(&found->second)) {
    return std::max<std::int64_t>(0, *value);
  }
  return 0;
}

[[nodiscard]] std::optional<double> explicit_domain_coverage(
    const KnowledgeProperties& properties) {
  if (const auto value = property_double(properties, "coverage")) {
    return clamp01(*value);
  }
  const auto with_opening = property_int(properties, "games_with_opening");
  const auto without_opening = property_int(properties, "games_without_opening");
  const auto total = with_opening + without_opening;
  if (total > 0) {
    return clamp01(static_cast<double>(with_opening) /
                   static_cast<double>(total));
  }
  return std::nullopt;
}

[[nodiscard]] double sample_sufficiency(const std::int64_t sample_size,
                                        const double scale) {
  if (sample_size <= 0) return 0.0;
  return clamp01(1.0 - std::exp(-static_cast<double>(sample_size) / scale));
}

[[nodiscard]] double assess(
    const KnowledgeProperties& properties, const KnowledgeAssertionKind assertion,
    const std::vector<KnowledgeProvenanceRecord>&,
    const KnowledgeConfidenceAssessment& confidence) {
  if (const auto explicit_coverage = explicit_domain_coverage(properties)) {
    return *explicit_coverage;
  }

  const double source_presence =
      confidence.evidence_count <= 0
          ? 0.0
          : clamp01(1.0 - std::exp(-static_cast<double>(confidence.evidence_count)));
  if (assertion == KnowledgeAssertionKind::kFact) {
    // One directly sourced fact can be well covered as an assertion without
    // claiming that the surrounding player/topic domain is fully covered.
    if (confidence.evidence_count == 0) return 0.15;
    return clamp01(0.72 + 0.18 * source_presence +
                   0.10 * confidence.evidence_diversity);
  }

  const double scale = assertion == KnowledgeAssertionKind::kHypothesis ? 20.0 : 12.0;
  const double sample = sample_sufficiency(confidence.sample_size, scale);
  return clamp01(0.62 * sample + 0.23 * source_presence +
                 0.15 * confidence.evidence_diversity);
}

}  // namespace

double CoverageEngine::assess_node(
    const KnowledgeNode& node,
    const std::vector<KnowledgeProvenanceRecord>& provenance,
    const KnowledgeConfidenceAssessment& confidence) const {
  return assess(node.properties, node.assertion_kind, provenance, confidence);
}

double CoverageEngine::assess_edge(
    const KnowledgeEdge& edge,
    const std::vector<KnowledgeProvenanceRecord>& provenance,
    const KnowledgeConfidenceAssessment& confidence) const {
  return assess(edge.properties, KnowledgeAssertionKind::kFact, provenance,
                confidence);
}

}  // namespace kchess::knowledge
