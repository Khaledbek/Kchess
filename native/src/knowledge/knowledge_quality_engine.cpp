#include "knowledge_quality_engine.h"

#include <utility>

namespace kchess::knowledge {

KnowledgeQualityEngine::KnowledgeQualityEngine(
    GraphStore& graph, DependencyTracker& dependencies,
    KnowledgeQualityStore& store)
    : graph_(graph), dependencies_(dependencies), store_(store) {}

std::optional<KnowledgeQualityMetrics> KnowledgeQualityEngine::refresh(
    const KnowledgeEntryRef& entry, const std::int64_t evaluated_at_ms) {
  if (entry.empty() || evaluated_at_ms < 0) return std::nullopt;
  const auto provenance = dependencies_.provenance_for(entry);

  KnowledgeQualityMetrics metrics;
  metrics.entry = entry;
  metrics.evaluated_at_ms = evaluated_at_ms;

  if (entry.kind == KnowledgeEntryKind::kNode) {
    const auto node = graph_.node(KnowledgeNodeId{entry.id});
    if (!node) {
      store_.remove(entry);
      return std::nullopt;
    }
    const auto confidence =
        confidence_.assess_node(*node, provenance, evaluated_at_ms);
    metrics.confidence = confidence.confidence;
    metrics.coverage = coverage_.assess_node(*node, provenance, confidence);
    metrics.freshness = confidence.freshness;
    metrics.source_quality = confidence.source_quality;
    metrics.evidence_diversity = confidence.evidence_diversity;
    metrics.importance = confidence.importance;
    metrics.sample_size = confidence.sample_size;
    metrics.evidence_count = confidence.evidence_count;
    metrics.temporal_scope = confidence.temporal_scope;
    metrics.freshness_basis = confidence.freshness_basis;
  } else if (entry.kind == KnowledgeEntryKind::kEdge) {
    const auto edge = graph_.edge(KnowledgeEdgeId{entry.id});
    if (!edge) {
      store_.remove(entry);
      return std::nullopt;
    }
    const auto confidence =
        confidence_.assess_edge(*edge, provenance, evaluated_at_ms);
    metrics.confidence = confidence.confidence;
    metrics.coverage = coverage_.assess_edge(*edge, provenance, confidence);
    metrics.freshness = confidence.freshness;
    metrics.source_quality = confidence.source_quality;
    metrics.evidence_diversity = confidence.evidence_diversity;
    metrics.importance = confidence.importance;
    metrics.sample_size = confidence.sample_size;
    metrics.evidence_count = confidence.evidence_count;
    metrics.temporal_scope = confidence.temporal_scope;
    metrics.freshness_basis = confidence.freshness_basis;
  } else {
    // Chunk metadata already has dedicated quality columns and will be wired by
    // the retrieval/embedding pipeline. Do not create a second chunk truth.
    return std::nullopt;
  }

  store_.upsert(metrics);
  return metrics;
}

std::vector<KnowledgeQualityMetrics> KnowledgeQualityEngine::refresh_batch(
    const std::int64_t evaluated_at_ms, const std::size_t limit,
    const std::size_t offset) {
  std::vector<KnowledgeQualityMetrics> result;
  for (const auto& entry : store_.entries(limit, offset)) {
    if (auto refreshed = refresh(entry, evaluated_at_ms)) {
      result.push_back(std::move(*refreshed));
    }
  }
  return result;
}

}  // namespace kchess::knowledge
