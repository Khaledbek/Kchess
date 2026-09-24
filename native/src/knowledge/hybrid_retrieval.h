#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "chunk_registry.h"
#include "graph_store.h"
#include "position_similarity.h"
#include "query_router.h"

namespace kchess::knowledge {

// -----------------------------------------------------------------------------
// Section: Hybrid retrieval budgets
// -----------------------------------------------------------------------------

struct HybridRetrievalBudgets {
  std::size_t max_seed_nodes{8};
  std::size_t hop_depth{2};
  std::size_t complex_hop_depth{3};
  std::size_t max_expanded_nodes{100};
  std::size_t max_candidate_chunks{40};
  std::size_t max_graph_chunks{16};
  std::size_t max_lexical_chunks{16};
  std::size_t max_vector_chunks{16};
  std::size_t max_position_candidates{20};
  std::size_t max_rerank_candidates{20};  // consumed by Update 120
};

struct HybridRetrievalSignals {
  bool exact_statistics{false};
  bool graph{false};
  bool lexical{false};
  bool vector{false};
  bool position_similarity{false};
  bool seed{false};
  std::optional<std::size_t> graph_distance;
  std::optional<double> lexical_score;
  std::optional<float> vector_score;
  std::optional<float> position_score;
};

struct HybridNodeCandidate {
  KnowledgeNode node;
  HybridRetrievalSignals signals;
  std::vector<KnowledgeSourceRef> sources;
};

struct HybridChunkCandidate {
  KnowledgeChunk chunk;
  HybridRetrievalSignals signals;
};

struct HybridRetrievalRequest {
  std::string player_id;
  std::string query_text;
  KnowledgeQueryRoute route;
  std::optional<std::string> current_fen;
  HybridRetrievalBudgets budgets;
  std::vector<HybridNodeCandidate> authoritative_nodes;
};

struct HybridRetrievalResult {
  std::vector<KnowledgeNodeId> seed_nodes;
  std::vector<HybridNodeCandidate> nodes;
  std::vector<KnowledgeTraversalStep> graph_steps;
  std::vector<HybridChunkCandidate> chunks;
  std::size_t expanded_nodes{0};
  bool seed_budget_exhausted{false};
  bool traversal_budget_exhausted{false};
  bool chunk_budget_exhausted{false};
};

// -----------------------------------------------------------------------------
// Section: Exact + graph + lexical + vector candidate collection
// -----------------------------------------------------------------------------

// HybridRetrievalEngine only collects bounded, explainable candidates. It does
// not compute the final cross-channel rank; Update 120 owns final ranking,
// query planning and reranking policy.
class HybridRetrievalEngine {
 public:
  HybridRetrievalEngine(GraphStore& graph, ChunkRegistry& chunks,
                        const PositionSimilarityIndex* position_similarity = nullptr);

  [[nodiscard]] HybridRetrievalResult retrieve(
      const HybridRetrievalRequest& request) const;

 private:
  GraphStore& graph_;
  ChunkRegistry& chunks_;
  const PositionSimilarityIndex* position_similarity_{nullptr};
};

}  // namespace kchess::knowledge
