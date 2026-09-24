#include "confidence_engine.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <set>
#include <string>
#include <string_view>

namespace kchess::knowledge {
namespace {

constexpr std::int64_t kDayMs = 24LL * 60LL * 60LL * 1000LL;

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

[[nodiscard]] std::optional<std::int64_t> property_int(
    const KnowledgeProperties& properties, const std::string_view key) {
  const auto found = properties.find(std::string(key));
  if (found == properties.end()) return std::nullopt;
  if (const auto* value = std::get_if<std::int64_t>(&found->second)) return *value;
  return std::nullopt;
}

[[nodiscard]] std::optional<bool> property_bool(
    const KnowledgeProperties& properties, const std::string_view key) {
  const auto found = properties.find(std::string(key));
  if (found == properties.end()) return std::nullopt;
  if (const auto* value = std::get_if<bool>(&found->second)) return *value;
  return std::nullopt;
}

[[nodiscard]] std::optional<std::string> property_string(
    const KnowledgeProperties& properties, const std::string_view key) {
  const auto found = properties.find(std::string(key));
  if (found == properties.end()) return std::nullopt;
  if (const auto* value = std::get_if<std::string>(&found->second)) return *value;
  return std::nullopt;
}

[[nodiscard]] bool manifest_source(const std::string_view type) {
  return type.find("manifest") != std::string_view::npos;
}

[[nodiscard]] double quality_for_source(const std::string_view type) {
  if (type == "shared_move_analysis") return 1.0;
  if (type == "statistics_service") return 0.98;
  if (type == "games_db_opening" || type == "games_db_positions") return 0.96;
  if (type == "result_transition_game") return 0.96;
  if (type == "opening_theory") return 0.95;
  if (type == "profile_evidence_registry") return 0.93;
  if (type == "learned_profile") return 0.90;
  return 0.80;
}

[[nodiscard]] std::string source_family(const std::string_view type) {
  if (type == "shared_move_analysis") return "analysis";
  if (type == "statistics_service") return "statistics";
  if (type == "games_db_opening" || type == "games_db_positions" ||
      type == "result_transition_game") {
    return "games";
  }
  if (type == "opening_theory") return "theory";
  if (type == "profile_evidence_registry" || type == "learned_profile") {
    return "profile";
  }
  return "other";
}

[[nodiscard]] std::int64_t infer_sample_size(
    const KnowledgeProperties& properties) {
  for (const auto key : {"sample_size", "sample_games", "games", "opportunities",
                         "occurrences", "analyzed_moves"}) {
    if (const auto value = property_int(properties, key); value && *value > 0) {
      return *value;
    }
  }
  const auto supporting = property_int(properties, "evidence_for").value_or(0);
  const auto opposing = property_int(properties, "evidence_against").value_or(0);
  return std::max<std::int64_t>(0, supporting + opposing);
}

[[nodiscard]] KnowledgeTemporalScope infer_scope(
    const KnowledgeProperties& properties) {
  if (property_bool(properties, "recent").value_or(false)) {
    return KnowledgeTemporalScope::kRecent;
  }
  const auto scope = property_string(properties, "scope");
  if (!scope) return KnowledgeTemporalScope::kUnknown;
  if (*scope == "recent" || *scope == "recent_form") {
    return KnowledgeTemporalScope::kRecent;
  }
  if (*scope == "mixed") return KnowledgeTemporalScope::kMixed;
  if (*scope == "lifetime" || *scope == "overall" || *scope == "color" ||
      *scope == "time_control" || *scope == "opening_family" ||
      *scope == "opening_coverage" || *scope == "termination" ||
      *scope == "accuracy") {
    return KnowledgeTemporalScope::kLifetime;
  }
  return KnowledgeTemporalScope::kUnknown;
}

[[nodiscard]] std::optional<double> timestamp_freshness(
    const KnowledgeProperties& properties, const std::int64_t evaluated_at_ms,
    KnowledgeFreshnessBasis& basis) {
  const auto updated_at = property_int(properties, "updated_at");
  if (!updated_at || *updated_at <= 0 || evaluated_at_ms <= 0) return std::nullopt;

  // Existing profile timestamps may be seconds or milliseconds. Normalize only
  // by magnitude; no domain payload is copied into the graph.
  std::int64_t timestamp_ms = *updated_at;
  if (timestamp_ms < 100000000000LL) timestamp_ms *= 1000LL;
  const auto age_ms = std::max<std::int64_t>(0, evaluated_at_ms - timestamp_ms);
  const double age_days = static_cast<double>(age_ms) / static_cast<double>(kDayMs);
  basis = KnowledgeFreshnessBasis::kEvidenceTimestamp;
  return clamp01(std::exp(-std::log(2.0) * age_days / 180.0));
}

[[nodiscard]] std::optional<double> infer_freshness(
    const KnowledgeProperties& properties,
    const std::vector<KnowledgeProvenanceRecord>& provenance,
    const std::int64_t evaluated_at_ms, const KnowledgeTemporalScope scope,
    KnowledgeFreshnessBasis& basis) {
  if (auto explicit_timestamp =
          timestamp_freshness(properties, evaluated_at_ms, basis)) {
    return explicit_timestamp;
  }
  if (scope == KnowledgeTemporalScope::kRecent) {
    basis = KnowledgeFreshnessBasis::kExplicitRecentScope;
    return 1.0;
  }

  std::int64_t newest_observation = 0;
  for (const auto& item : provenance) {
    if (manifest_source(item.source.source_type)) continue;
    newest_observation = std::max(newest_observation, item.last_seen_ms);
  }
  if (newest_observation <= 0 || evaluated_at_ms <= 0) return std::nullopt;

  const auto age_ms = std::max<std::int64_t>(0, evaluated_at_ms - newest_observation);
  const double age_days = static_cast<double>(age_ms) / static_cast<double>(kDayMs);
  basis = KnowledgeFreshnessBasis::kSourceObservation;
  return clamp01(std::exp(-std::log(2.0) * age_days / 365.0));
}

[[nodiscard]] double assertion_base(const KnowledgeAssertionKind kind) {
  switch (kind) {
    case KnowledgeAssertionKind::kFact:
      return 0.88;
    case KnowledgeAssertionKind::kObservation:
      return 0.62;
    case KnowledgeAssertionKind::kHypothesis:
      return 0.38;
  }
  return 0.50;
}

[[nodiscard]] double node_importance(const KnowledgeNode& node) {
  if (const auto explicit_value = property_double(node.properties, "importance")) {
    return clamp01(*explicit_value);
  }
  if (const auto severity = property_double(node.properties, "severity")) {
    return clamp01(0.45 + 0.55 * clamp01(*severity));
  }
  switch (node.kind) {
    case KnowledgeNodeKind::kPlayer:
      return 0.95;
    case KnowledgeNodeKind::kWeakness:
    case KnowledgeNodeKind::kStrength:
    case KnowledgeNodeKind::kHypothesis:
    case KnowledgeNodeKind::kKnowledgeGap:
    case KnowledgeNodeKind::kTrend:
      return 0.85;
    case KnowledgeNodeKind::kStatistic:
    case KnowledgeNodeKind::kOpening:
    case KnowledgeNodeKind::kOpeningFamily:
    case KnowledgeNodeKind::kEndgameType:
    case KnowledgeNodeKind::kMiddlegameStructure:
    case KnowledgeNodeKind::kTacticalMotif:
    case KnowledgeNodeKind::kStrategicMotif:
      return 0.70;
    case KnowledgeNodeKind::kEvidence:
    case KnowledgeNodeKind::kAnalysis:
      return 0.55;
    case KnowledgeNodeKind::kGame:
    case KnowledgeNodeKind::kPosition:
      return 0.40;
    default:
      return 0.60;
  }
}

[[nodiscard]] double edge_importance(const KnowledgeEdge& edge) {
  if (const auto explicit_value = property_double(edge.properties, "importance")) {
    return clamp01(*explicit_value);
  }
  switch (edge.kind) {
    case KnowledgeEdgeKind::kWeakIn:
    case KnowledgeEdgeKind::kStrongIn:
    case KnowledgeEdgeKind::kContributesTo:
    case KnowledgeEdgeKind::kLostBy:
    case KnowledgeEdgeKind::kWonBy:
    case KnowledgeEdgeKind::kSavedFrom:
    case KnowledgeEdgeKind::kMissedWin:
    case KnowledgeEdgeKind::kMissedDraw:
    case KnowledgeEdgeKind::kContradictedBy:
    case KnowledgeEdgeKind::kImprovingIn:
    case KnowledgeEdgeKind::kDecliningIn:
      return 0.85;
    case KnowledgeEdgeKind::kSupportedBy:
    case KnowledgeEdgeKind::kDerivedFrom:
    case KnowledgeEdgeKind::kExplainedBy:
      return 0.75;
    default:
      return 0.60;
  }
}

KnowledgeConfidenceAssessment assess(
    const KnowledgeProperties& properties, const KnowledgeAssertionKind assertion,
    const std::vector<KnowledgeProvenanceRecord>& provenance,
    const std::int64_t evaluated_at_ms, const double importance) {
  KnowledgeConfidenceAssessment result;
  result.sample_size = infer_sample_size(properties);
  result.temporal_scope = infer_scope(properties);
  result.importance = importance;

  std::set<std::string> families;
  double source_quality_sum = 0.0;
  for (const auto& item : provenance) {
    if (manifest_source(item.source.source_type)) continue;
    ++result.evidence_count;
    source_quality_sum += quality_for_source(item.source.source_type);
    families.insert(source_family(item.source.source_type));
  }
  result.source_quality = result.evidence_count == 0
                              ? 0.50
                              : source_quality_sum /
                                    static_cast<double>(result.evidence_count);
  const double source_independence =
      1.0 - std::exp(-static_cast<double>(result.evidence_count) / 4.0);
  const double family_diversity =
      clamp01(static_cast<double>(families.size()) / 4.0);
  result.evidence_diversity =
      clamp01(0.65 * source_independence + 0.35 * family_diversity);

  result.freshness = infer_freshness(properties, provenance, evaluated_at_ms,
                                     result.temporal_scope,
                                     result.freshness_basis);

  const double support_strength = result.sample_size > 0
                                      ? 1.0 - std::exp(-static_cast<double>(result.sample_size) / 8.0)
                                      : clamp01(static_cast<double>(result.evidence_count) / 3.0);
  const double freshness = result.freshness.value_or(0.65);
  const auto upstream = property_double(properties, "confidence");
  const double base = assertion_base(assertion);

  double confidence = 0.0;
  if (upstream) {
    confidence = 0.32 * base + 0.40 * clamp01(*upstream) +
                 0.13 * result.source_quality + 0.10 * support_strength +
                 0.05 * result.evidence_diversity;
  } else {
    confidence = 0.52 * base + 0.23 * result.source_quality +
                 0.15 * support_strength + 0.10 * result.evidence_diversity;
  }
  confidence *= 0.88 + 0.12 * freshness;

  if (assertion == KnowledgeAssertionKind::kObservation) {
    confidence = std::min(confidence, 0.95);
  } else if (assertion == KnowledgeAssertionKind::kHypothesis) {
    confidence = std::min(confidence, 0.80);
  } else {
    confidence = std::min(confidence, 0.995);
  }
  if (result.evidence_count == 0) {
    const double unsupported_cap = assertion == KnowledgeAssertionKind::kFact
                                       ? 0.45
                                       : assertion == KnowledgeAssertionKind::kObservation
                                             ? 0.30
                                             : 0.20;
    confidence = std::min(confidence, unsupported_cap);
  }
  result.confidence = clamp01(confidence);
  return result;
}

}  // namespace

KnowledgeConfidenceAssessment ConfidenceEngine::assess_node(
    const KnowledgeNode& node,
    const std::vector<KnowledgeProvenanceRecord>& provenance,
    const std::int64_t evaluated_at_ms) const {
  return assess(node.properties, node.assertion_kind, provenance,
                evaluated_at_ms, node_importance(node));
}

KnowledgeConfidenceAssessment ConfidenceEngine::assess_edge(
    const KnowledgeEdge& edge,
    const std::vector<KnowledgeProvenanceRecord>& provenance,
    const std::int64_t evaluated_at_ms) const {
  // Edge existence is a routing fact. Observation/hypothesis uncertainty is
  // represented by its supporting source confidence and endpoint assertions.
  return assess(edge.properties, KnowledgeAssertionKind::kFact, provenance,
                evaluated_at_ms, edge_importance(edge));
}

}  // namespace kchess::knowledge
