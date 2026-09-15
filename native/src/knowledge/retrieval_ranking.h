#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hybrid_retrieval.h"
#include "knowledge_quality_store.h"
#include "text_semantic_retrieval.h"

namespace kchess::knowledge {

// -----------------------------------------------------------------------------
// Section: Knowledge retrieval planning
// -----------------------------------------------------------------------------

struct KnowledgeRankingWeights {
  double query_relevance{0.24};
  double exact_statistics{0.10};
  double graph_proximity{0.12};
  double lexical_relevance{0.10};
  double semantic_relevance{0.10};
  double position_similarity{0.06};
  double confidence{0.08};
  double coverage{0.05};
  double freshness{0.05};
  double source_quality{0.04};
  double importance{0.06};
};

struct KnowledgeQueryExecutionPlan {
  HybridRetrievalBudgets retrieval_budgets;
  KnowledgeRankingWeights ranking_weights;
  std::size_t max_ranked_nodes{24};
  std::size_t max_ranked_chunks{8};
  std::size_t rerank_candidate_limit{20};
  bool complex_query{false};
  bool prefer_recent{false};
  bool prefer_evidence{false};
  bool allow_text_reranker{true};
  std::string statistic_metric{"games"};
  bool prefer_low_statistic{false};
};

// This planner refines budgets/weights for an already-authoritative
// KnowledgeQueryRoute. It never changes Coach intent/query family or enables a
// retrieval channel that Update 118 disabled.
class KnowledgeQueryPlanner {
 public:
  [[nodiscard]] KnowledgeQueryExecutionPlan plan(
      const KnowledgeQueryRoute& route,
      const HybridRetrievalBudgets& requested_budgets = {}) const;
};

// -----------------------------------------------------------------------------
// Section: Explainable hybrid ranking
// -----------------------------------------------------------------------------

struct KnowledgeRankBreakdown {
  double query_relevance{0.0};
  double exact_statistics{0.0};
  double graph_proximity{0.0};
  double lexical_relevance{0.0};
  double semantic_relevance{0.0};
  double position_similarity{0.0};
  double confidence{0.0};
  double coverage{0.0};
  double freshness{0.0};
  double source_quality{0.0};
  double importance{0.0};
  std::optional<double> rerank_relevance;
};

struct RankedKnowledgeNode {
  HybridNodeCandidate candidate;
  KnowledgeRankBreakdown breakdown;
  double score{0.0};
};

struct RankedKnowledgeChunk {
  HybridChunkCandidate candidate;
  KnowledgeRankBreakdown breakdown;
  double score{0.0};
};

struct RankedKnowledgeRetrieval {
  std::vector<RankedKnowledgeNode> nodes;
  std::vector<RankedKnowledgeChunk> chunks;
};

// Cross-channel ranking is a retrieval policy, never a truth source. Missing
// quality metadata is omitted from the weighted mean rather than treated as
// negative evidence. Reranking may only reorder candidates already collected
// by HybridRetrievalEngine.
class KnowledgeRetrievalRanker {
 public:
  explicit KnowledgeRetrievalRanker(
      const KnowledgeQualityStore* quality_store = nullptr,
      const TextReranker* reranker = nullptr);

  [[nodiscard]] RankedKnowledgeRetrieval rank(
      const HybridRetrievalResult& candidates, std::string_view query_text,
      const KnowledgeQueryExecutionPlan& plan) const;

 private:
  const KnowledgeQualityStore* quality_store_{nullptr};
  const TextReranker* reranker_{nullptr};
};

}  // namespace kchess::knowledge
