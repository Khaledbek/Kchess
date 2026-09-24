#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "graph_store.h"
#include "vector_index.h"

namespace kchess::ai {
struct PositionFeatures;
}

namespace kchess::knowledge {

// -----------------------------------------------------------------------------
// Section: Deterministic chess-position feature space
// -----------------------------------------------------------------------------

inline constexpr std::string_view kPositionFeatureModelId =
    "kchess.position_features";
inline constexpr std::string_view kPositionFeatureModelVersion = "1";

// Converts the existing deterministic PositionFeatureExtractor DTO into a
// normalized feature vector. This is intentionally not a learned text
// embedding: it is a versioned chess-position feature space whose dimensions
// describe board structure, material, activity, king safety and phase.
class PositionFeatureVectorEncoder {
 public:
  [[nodiscard]] std::vector<float> encode(
      const ai::PositionFeatures& features) const;
  [[nodiscard]] std::vector<float> encode_fen(std::string_view fen) const;

  // Ties a vector to the canonical board identity and encoder version. The
  // canonical key comes from the existing theory/position_key implementation.
  [[nodiscard]] std::string source_version(std::string_view fen) const;
};

// -----------------------------------------------------------------------------
// Section: Position-vector persistence and retrieval
// -----------------------------------------------------------------------------

struct PositionSimilarityMatch {
  KnowledgeNodeId position_node_id;
  std::string embedding_id;
  float score{0.0F};
};

class PositionSimilarityIndex {
 public:
  PositionSimilarityIndex(GraphStore& graph, VectorIndex& vectors);

  // The Position node must already exist in GraphStore. Position vectors are
  // global reusable representations of canonical board states; player scope is
  // applied later by graph/hybrid retrieval, not baked into the board vector.
  [[nodiscard]] std::string upsert_position(
      const KnowledgeNodeId& position_node_id, std::string_view fen,
      std::int64_t observed_at_ms);

  bool remove_position(const KnowledgeNodeId& position_node_id);

  [[nodiscard]] std::vector<PositionSimilarityMatch> similar_to_fen(
      std::string_view fen, std::size_t limit = 20,
      float minimum_score = 0.65F,
      std::optional<KnowledgeNodeId> exclude_node = std::nullopt) const;

 private:
  [[nodiscard]] bool is_current_position_vector(
      const KnowledgeEmbeddingMetadata& metadata,
      const KnowledgeNode& owner) const;

  GraphStore& graph_;
  VectorIndex& vectors_;
  PositionFeatureVectorEncoder encoder_;
};

}  // namespace kchess::knowledge
