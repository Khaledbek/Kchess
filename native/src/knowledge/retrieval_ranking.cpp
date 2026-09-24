#include "retrieval_ranking.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace kchess::knowledge {
namespace {

constexpr double kRerankBlend = 0.18;

[[nodiscard]] double clamp01(const double value) noexcept {
  if (!std::isfinite(value)) return 0.0;
  return std::clamp(value, 0.0, 1.0);
}

[[nodiscard]] double cosine_like_to_unit(const double value) noexcept {
  if (!std::isfinite(value)) return 0.0;
  return clamp01((value + 1.0) * 0.5);
}

[[nodiscard]] double lexical_to_unit(const double value) noexcept {
  if (!std::isfinite(value) || value <= 0.0) return 0.0;
  // Lexical search uses an additive field score with no fixed upper bound.
  // Saturation preserves ordering without letting repeated terms overwhelm all
  // other channels.
  return value / (value + 8.0);
}

[[nodiscard]] double graph_distance_to_unit(
    const std::optional<std::size_t> distance, const bool seed) noexcept {
  if (seed) return 1.0;
  if (!distance) return 0.0;
  return 1.0 / (1.0 + static_cast<double>(*distance));
}

[[nodiscard]] double signal_query_relevance(
    const HybridRetrievalSignals& signals) noexcept {
  double best = 0.0;
  if (signals.exact_statistics) best = std::max(best, 1.0);
  if (signals.seed) best = std::max(best, 1.0);
  if (signals.lexical_score) {
    best = std::max(best, lexical_to_unit(*signals.lexical_score));
  }
  if (signals.vector_score) {
    best = std::max(best, cosine_like_to_unit(*signals.vector_score));
  }
  if (signals.position_score) {
    best = std::max(best, cosine_like_to_unit(*signals.position_score));
  }
  if (signals.graph || signals.graph_distance) {
    best = std::max(best,
                    graph_distance_to_unit(signals.graph_distance, signals.seed));
  }
  return best;
}

struct WeightedMean {
  double numerator{0.0};
  double denominator{0.0};

  void add(const double value, const double weight, const bool available = true) {
    if (!available || !std::isfinite(value) || !std::isfinite(weight) ||
        weight <= 0.0) {
      return;
    }
    numerator += clamp01(value) * weight;
    denominator += weight;
  }

  [[nodiscard]] double value() const noexcept {
    if (denominator <= std::numeric_limits<double>::epsilon()) return 0.0;
    return clamp01(numerator / denominator);
  }
};

[[nodiscard]] KnowledgeRankingWeights normalized_policy_weights(
    KnowledgeRankingWeights weights, const KnowledgeQueryExecutionPlan& plan) {
  if (plan.prefer_recent) {
    weights.freshness *= 1.8;
    weights.query_relevance *= 1.05;
  }
  if (plan.prefer_evidence) {
    weights.confidence *= 1.5;
    weights.source_quality *= 1.5;
    weights.coverage *= 1.2;
  }
  return weights;
}

[[nodiscard]] KnowledgeRankBreakdown breakdown_from_signals(
    const HybridRetrievalSignals& signals) {
  KnowledgeRankBreakdown out;
  out.query_relevance = signal_query_relevance(signals);
  out.exact_statistics = signals.exact_statistics ? 1.0 : 0.0;
  out.graph_proximity =
      graph_distance_to_unit(signals.graph_distance, signals.seed);
  out.lexical_relevance =
      signals.lexical_score ? lexical_to_unit(*signals.lexical_score) : 0.0;
  out.semantic_relevance = signals.vector_score
      ? cosine_like_to_unit(*signals.vector_score)
      : 0.0;
  out.position_similarity = signals.position_score
      ? cosine_like_to_unit(*signals.position_score)
      : 0.0;
  return out;
}

[[nodiscard]] double score_breakdown(
    const KnowledgeRankBreakdown& b, const HybridRetrievalSignals& signals,
    const KnowledgeRankingWeights& weights,
    const bool has_confidence, const bool has_coverage, const bool has_freshness,
    const bool has_source_quality, const bool has_importance) {
  WeightedMean mean;
  mean.add(b.query_relevance, weights.query_relevance);
  mean.add(b.exact_statistics, weights.exact_statistics,
           signals.exact_statistics);
  mean.add(b.graph_proximity, weights.graph_proximity,
           signals.graph || signals.graph_distance.has_value() || signals.seed);
  mean.add(b.lexical_relevance, weights.lexical_relevance,
           signals.lexical_score.has_value());
  mean.add(b.semantic_relevance, weights.semantic_relevance,
           signals.vector_score.has_value());
  mean.add(b.position_similarity, weights.position_similarity,
           signals.position_score.has_value());
  mean.add(b.confidence, weights.confidence, has_confidence);
  mean.add(b.coverage, weights.coverage, has_coverage);
  mean.add(b.freshness, weights.freshness, has_freshness);
  mean.add(b.source_quality, weights.source_quality, has_source_quality);
  mean.add(b.importance, weights.importance, has_importance);
  return mean.value();
}

void apply_node_quality(KnowledgeRankBreakdown& breakdown,
                        const KnowledgeQualityStore* store,
                        const KnowledgeNodeId& id,
                        bool& has_confidence, bool& has_coverage,
                        bool& has_freshness, bool& has_source_quality,
                        bool& has_importance) {
  if (store == nullptr || !store->is_open() || id.empty()) return;
  const auto quality = store->quality({KnowledgeEntryKind::kNode, id.value});
  if (!quality) return;
  breakdown.confidence = clamp01(quality->confidence);
  breakdown.coverage = clamp01(quality->coverage);
  breakdown.source_quality = clamp01(quality->source_quality);
  breakdown.importance = clamp01(quality->importance);
  has_confidence = true;
  has_coverage = true;
  has_source_quality = true;
  has_importance = true;
  if (quality->freshness) {
    breakdown.freshness = clamp01(*quality->freshness);
    has_freshness = true;
  }
}

void apply_chunk_quality(KnowledgeRankBreakdown& breakdown,
                         const KnowledgeChunkMetadata& metadata,
                         bool& has_confidence, bool& has_coverage,
                         bool& has_freshness, bool& has_source_quality,
                         bool& has_importance) {
  if (metadata.confidence) {
    breakdown.confidence = clamp01(*metadata.confidence);
    has_confidence = true;
  }
  if (metadata.coverage) {
    breakdown.coverage = clamp01(*metadata.coverage);
    has_coverage = true;
  }
  if (metadata.freshness) {
    breakdown.freshness = clamp01(*metadata.freshness);
    has_freshness = true;
  }
  if (metadata.source_quality) {
    breakdown.source_quality = clamp01(*metadata.source_quality);
    has_source_quality = true;
  }
  if (metadata.importance) {
    breakdown.importance = clamp01(*metadata.importance);
    has_importance = true;
  }
}

[[nodiscard]] HybridRetrievalBudgets bounded_budgets(
    HybridRetrievalBudgets budgets) {
  budgets.max_seed_nodes = std::clamp<std::size_t>(budgets.max_seed_nodes, 1, 8);
  budgets.hop_depth = std::clamp<std::size_t>(budgets.hop_depth, 1, 2);
  budgets.complex_hop_depth =
      std::clamp<std::size_t>(budgets.complex_hop_depth, budgets.hop_depth, 3);
  budgets.max_expanded_nodes =
      std::clamp<std::size_t>(budgets.max_expanded_nodes, 1, 100);
  budgets.max_candidate_chunks =
      std::clamp<std::size_t>(budgets.max_candidate_chunks, 1, 40);
  budgets.max_graph_chunks =
      std::min(budgets.max_graph_chunks, budgets.max_candidate_chunks);
  budgets.max_lexical_chunks =
      std::min(budgets.max_lexical_chunks, budgets.max_candidate_chunks);
  budgets.max_vector_chunks =
      std::min(budgets.max_vector_chunks, budgets.max_candidate_chunks);
  budgets.max_position_candidates =
      std::clamp<std::size_t>(budgets.max_position_candidates, 1, 20);
  budgets.max_rerank_candidates = std::clamp<std::size_t>(
      budgets.max_rerank_candidates, 1, std::min<std::size_t>(20, budgets.max_candidate_chunks));
  return budgets;
}

void sort_nodes(std::vector<RankedKnowledgeNode>& nodes) {
  std::stable_sort(nodes.begin(), nodes.end(),
                   [](const auto& lhs, const auto& rhs) {
                     if (lhs.score != rhs.score) return lhs.score > rhs.score;
                     return lhs.candidate.node.id.value < rhs.candidate.node.id.value;
                   });
}

void sort_chunks(std::vector<RankedKnowledgeChunk>& chunks) {
  std::stable_sort(chunks.begin(), chunks.end(),
                   [](const auto& lhs, const auto& rhs) {
                     if (lhs.score != rhs.score) return lhs.score > rhs.score;
                     return lhs.candidate.chunk.metadata.id.value <
                            rhs.candidate.chunk.metadata.id.value;
                   });
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Planning policy
// -----------------------------------------------------------------------------

KnowledgeQueryExecutionPlan KnowledgeQueryPlanner::plan(
    const KnowledgeQueryRoute& route,
    const HybridRetrievalBudgets& requested_budgets) const {
  KnowledgeQueryExecutionPlan out;
  out.retrieval_budgets = bounded_budgets(requested_budgets);
  const bool weaknesses = std::find(route.profile_scope.topics.begin(),
      route.profile_scope.topics.end(), "weaknesses") != route.profile_scope.topics.end();
  if (weaknesses) { out.statistic_metric = "scorePercent"; out.prefer_low_statistic = true; }
  for (const auto& entity : route.entities) {
    if (entity.kind != KnowledgeQueryEntityKind::kStatisticMetric) continue;
    if (entity.canonical_value == "frequency") { out.statistic_metric = "games"; out.prefer_low_statistic = false; }
    else if (entity.canonical_value == "win_rate" || entity.canonical_value == "score") out.statistic_metric = "scorePercent";
    else if (entity.canonical_value == "accuracy") out.statistic_metric = "averageAccuracy";
  }

  switch (route.intent) {
    case KnowledgeQueryIntent::kExactStatistic:
      out.retrieval_budgets.hop_depth = 1;
      out.retrieval_budgets.complex_hop_depth = 2;
      out.max_ranked_nodes = 16;
      out.max_ranked_chunks = 6;
      out.ranking_weights.exact_statistics *= 2.6;
      out.ranking_weights.semantic_relevance *= 0.45;
      out.ranking_weights.position_similarity = 0.0;
      break;
    case KnowledgeQueryIntent::kRelationship:
      out.complex_query = true;
      out.retrieval_budgets.hop_depth = 2;
      out.retrieval_budgets.complex_hop_depth = 3;
      out.ranking_weights.graph_proximity *= 1.9;
      out.ranking_weights.semantic_relevance *= 1.15;
      break;
    case KnowledgeQueryIntent::kCausalAnalysis:
      out.complex_query = true;
      out.prefer_evidence = true;
      out.retrieval_budgets.hop_depth = 2;
      out.retrieval_budgets.complex_hop_depth = 3;
      out.max_ranked_chunks = 8;
      out.ranking_weights.graph_proximity *= 1.8;
      out.ranking_weights.exact_statistics *= 1.35;
      break;
    case KnowledgeQueryIntent::kTrend:
      out.prefer_recent = true;
      out.max_ranked_chunks = 8;
      out.ranking_weights.freshness *= 2.0;
      out.ranking_weights.exact_statistics *= 1.4;
      out.ranking_weights.graph_proximity *= 1.25;
      break;
    case KnowledgeQueryIntent::kEvidence:
      out.prefer_evidence = true;
      out.max_ranked_chunks = 8;
      out.ranking_weights.confidence *= 1.8;
      out.ranking_weights.source_quality *= 1.8;
      out.ranking_weights.coverage *= 1.35;
      out.ranking_weights.graph_proximity *= 1.25;
      break;
    case KnowledgeQueryIntent::kSimilarity:
      out.ranking_weights.position_similarity *= 2.7;
      out.ranking_weights.semantic_relevance *= 1.25;
      out.ranking_weights.exact_statistics *= 0.5;
      break;
    case KnowledgeQueryIntent::kCurrentPosition:
      out.ranking_weights.position_similarity *= 2.5;
      out.ranking_weights.graph_proximity *= 1.3;
      out.ranking_weights.semantic_relevance *= 1.2;
      break;
    case KnowledgeQueryIntent::kGeneralPersonal:
      out.max_ranked_chunks = 8;
      break;
    case KnowledgeQueryIntent::kGeneralChess:
      out.max_ranked_nodes = 8;
      out.ranking_weights.semantic_relevance *= 1.7;
      out.ranking_weights.lexical_relevance *= 1.5;
      out.ranking_weights.graph_proximity *= 0.5;
      out.ranking_weights.exact_statistics = 0.0;
      break;
    case KnowledgeQueryIntent::kUnknown:
      out.max_ranked_nodes = 8;
      out.max_ranked_chunks = 6;
      out.ranking_weights.lexical_relevance *= 1.5;
      out.ranking_weights.graph_proximity *= 0.7;
      break;
  }

  // The planner can tighten enabled channels through weights/budgets but must
  // never re-enable a channel the Update-118 route excluded.
  if (!route.channels.exact_statistics) out.ranking_weights.exact_statistics = 0.0;
  if (std::find(route.profile_scope.topics.begin(), route.profile_scope.topics.end(), "rating") !=
      route.profile_scope.topics.end()) out.max_ranked_nodes = 24;
  if (!route.channels.graph) out.ranking_weights.graph_proximity = 0.0;
  if (!route.channels.lexical) out.ranking_weights.lexical_relevance = 0.0;
  if (!route.channels.vector) out.ranking_weights.semantic_relevance = 0.0;
  if (!route.channels.position_similarity) {
    out.ranking_weights.position_similarity = 0.0;
  }
  return out;
}

// -----------------------------------------------------------------------------
// Section: Ranking
// -----------------------------------------------------------------------------

KnowledgeRetrievalRanker::KnowledgeRetrievalRanker(
    const KnowledgeQualityStore* quality_store)
    : quality_store_(quality_store) {}

RankedKnowledgeRetrieval KnowledgeRetrievalRanker::rank(
    const HybridRetrievalResult& candidates, std::string_view,
    const KnowledgeQueryExecutionPlan& plan) const {
  RankedKnowledgeRetrieval out;
  const auto weights = normalized_policy_weights(plan.ranking_weights, plan);

  out.nodes.reserve(candidates.nodes.size());
  for (const auto& candidate : candidates.nodes) {
    RankedKnowledgeNode ranked;
    ranked.candidate = candidate;
    ranked.breakdown = breakdown_from_signals(candidate.signals);

    bool has_confidence = false;
    bool has_coverage = false;
    bool has_freshness = false;
    bool has_source_quality = false;
    bool has_importance = false;
    apply_node_quality(ranked.breakdown, candidate.sources.empty() ? quality_store_ : nullptr, candidate.node.id,
                       has_confidence, has_coverage, has_freshness,
                       has_source_quality, has_importance);
    ranked.score = score_breakdown(
        ranked.breakdown, candidate.signals, weights, has_confidence,
        has_coverage, has_freshness, has_source_quality, has_importance);
    out.nodes.push_back(std::move(ranked));
  }
  sort_nodes(out.nodes);
  const auto metric = [&](const RankedKnowledgeNode& ranked) {
    if (plan.prefer_low_statistic) {
      const auto games = ranked.candidate.node.properties.find("games");
      if (games != ranked.candidate.node.properties.end()) {
        const auto* count = std::get_if<std::int64_t>(&games->second);
        if (count && *count < 5) return std::numeric_limits<double>::infinity();
      }
    }
    const auto it = ranked.candidate.node.properties.find(plan.statistic_metric);
    if (it != ranked.candidate.node.properties.end()) {
      if (const auto* value = std::get_if<double>(&it->second)) return *value;
      if (const auto* value = std::get_if<std::int64_t>(&it->second)) return static_cast<double>(*value);
    }
    return plan.prefer_low_statistic ? std::numeric_limits<double>::infinity() : -1.0;
  };
  std::stable_sort(out.nodes.begin(), out.nodes.end(), [&](const auto& a, const auto& b) {
    if (a.score != b.score) return a.score > b.score;
    if (a.candidate.signals.exact_statistics && b.candidate.signals.exact_statistics && metric(a) != metric(b))
      return plan.prefer_low_statistic ? metric(a) < metric(b) : metric(a) > metric(b);
    return a.candidate.node.id.value < b.candidate.node.id.value;
  });
  if (out.nodes.size() > plan.max_ranked_nodes) {
    out.nodes.resize(plan.max_ranked_nodes);
  }

  out.chunks.reserve(candidates.chunks.size());
  for (const auto& candidate : candidates.chunks) {
    RankedKnowledgeChunk ranked;
    ranked.candidate = candidate;
    ranked.breakdown = breakdown_from_signals(candidate.signals);

    bool has_confidence = false;
    bool has_coverage = false;
    bool has_freshness = false;
    bool has_source_quality = false;
    bool has_importance = false;
    apply_chunk_quality(ranked.breakdown, candidate.chunk.metadata,
                        has_confidence, has_coverage, has_freshness,
                        has_source_quality, has_importance);
    ranked.score = score_breakdown(
        ranked.breakdown, candidate.signals, weights, has_confidence,
        has_coverage, has_freshness, has_source_quality, has_importance);
    out.chunks.push_back(std::move(ranked));
  }
  sort_chunks(out.chunks);
  if (out.chunks.size() > plan.max_ranked_chunks) {
    out.chunks.resize(plan.max_ranked_chunks);
  }
  return out;
}

}  // namespace kchess::knowledge
