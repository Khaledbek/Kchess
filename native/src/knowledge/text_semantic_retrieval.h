#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "../../ai/models/small_models.h"
#include "chunk_registry.h"
#include "vector_index.h"

namespace kchess::knowledge {

// -----------------------------------------------------------------------------
// Section: Text embedding runtime bridge
// -----------------------------------------------------------------------------

// Reuses the existing provider-neutral Coach EmbeddingModel contract. The
// Knowledge Graph owns persistence/routing of embeddings, not model inference.
class ChunkTextEmbeddingWriter {
 public:
  ChunkTextEmbeddingWriter(VectorIndex& vector_index,
                           const ai::EmbeddingModel& embedding_model);

  [[nodiscard]] bool available() const noexcept;

  // Produces/replaces the active text-semantic embedding for one current
  // KnowledgeChunk. The chunk's source_version is copied into vector metadata,
  // so a later chunk refresh automatically makes the old vector stale.
  [[nodiscard]] std::optional<std::string> upsert(
      const KnowledgeChunk& chunk, std::int64_t observed_at_ms) const;

  [[nodiscard]] std::optional<std::vector<float>> embed_query(
      std::string_view text) const;

 private:
  VectorIndex* vector_index_{nullptr};
  const ai::EmbeddingModel* embedding_model_{nullptr};
};

// -----------------------------------------------------------------------------
// Section: Optional semantic reranker contract
// -----------------------------------------------------------------------------

struct TextRerankCandidate {
  KnowledgeChunkId chunk_id;
  std::string content;
  std::string type;
  std::string topic;
  float vector_score{0.0F};
};

struct TextRerankScore {
  KnowledgeChunkId chunk_id;
  double score{0.0};
};

class TextReranker {
 public:
  virtual ~TextReranker() = default;

  [[nodiscard]] virtual std::string_view id() const noexcept = 0;
  [[nodiscard]] virtual std::string_view version() const noexcept = 0;
  [[nodiscard]] virtual bool available() const noexcept = 0;

  // A reranker may only score candidates supplied by the retrieval layer. It
  // must never introduce extra graph/chunk IDs or become a source of chess
  // truth. Missing/invalid scores fall back to vector order.
  [[nodiscard]] virtual std::vector<TextRerankScore> score(
      std::string_view query,
      const std::vector<TextRerankCandidate>& candidates) const = 0;
};

// -----------------------------------------------------------------------------
// Section: Semantic chunk search
// -----------------------------------------------------------------------------

struct SemanticChunkSearchRequest {
  std::string player_id;
  std::string query_text;
  std::size_t limit{8};
  std::size_t candidate_limit{20};
  float minimum_vector_score{-1.0F};
  // Already scoped graph/lexical candidates lacking a current persisted vector.
  // At most eight are embedded in memory; foreground queries never index a corpus.
  std::vector<KnowledgeChunk> current_candidates;
};

struct SemanticChunkSearchHit {
  KnowledgeChunk chunk;
  float vector_score{0.0F};
  std::optional<double> rerank_score;
};

class SemanticChunkSearch {
 public:
  SemanticChunkSearch(const ChunkRegistry& chunks, const VectorIndex& vectors,
                      const ai::EmbeddingModel& embedding_model,
                      const TextReranker* reranker = nullptr);

  [[nodiscard]] std::vector<SemanticChunkSearchHit> search(
      const SemanticChunkSearchRequest& request) const;

 private:
  const ChunkRegistry* chunks_{nullptr};
  const VectorIndex* vectors_{nullptr};
  const ai::EmbeddingModel* embedding_model_{nullptr};
  const TextReranker* reranker_{nullptr};
};

}  // namespace kchess::knowledge
