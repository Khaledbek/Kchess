#include "text_semantic_retrieval.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace kchess::knowledge {
namespace {

constexpr std::size_t kMaximumSemanticCandidates = 100;
constexpr std::size_t kMaximumSemanticResults = 40;

bool valid_model(const ai::EmbeddingModel& model) noexcept {
  return model.available() && !model.id().empty() && !model.version().empty() &&
         model.dimensions() > 0 &&
         model.dimensions() <= std::numeric_limits<std::uint32_t>::max();
}

std::optional<std::vector<float>> safe_embed(
    const ai::EmbeddingModel& model, const std::string_view text) noexcept {
  try {
    return model.embed(text);
  } catch (...) {
    return std::nullopt;
  }
}

bool valid_vector(const std::vector<float>& values,
                  const std::size_t expected_dimensions) noexcept {
  if (values.size() != expected_dimensions || values.empty()) return false;
  for (const float value : values) {
    if (!std::isfinite(value)) return false;
  }
  return true;
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Text embedding runtime bridge
// -----------------------------------------------------------------------------

ChunkTextEmbeddingWriter::ChunkTextEmbeddingWriter(
    VectorIndex& vector_index, const ai::EmbeddingModel& embedding_model)
    : vector_index_(&vector_index), embedding_model_(&embedding_model) {}

bool ChunkTextEmbeddingWriter::available() const noexcept {
  return vector_index_ != nullptr && vector_index_->is_open() &&
         embedding_model_ != nullptr && valid_model(*embedding_model_);
}

std::optional<std::string> ChunkTextEmbeddingWriter::upsert(
    const KnowledgeChunk& chunk, const std::int64_t observed_at_ms) const {
  if (!available() || observed_at_ms < 0 ||
      !is_valid_chunk_metadata(chunk.metadata) || chunk.content.empty()) {
    return std::nullopt;
  }

  const auto values = safe_embed(*embedding_model_, chunk.content);
  if (!values || !valid_vector(*values, embedding_model_->dimensions())) {
    return std::nullopt;
  }

  KnowledgeVectorRecord record;
  record.metadata.owner_kind = EmbeddingOwnerKind::kChunk;
  record.metadata.owner_id = chunk.metadata.id.value;
  record.metadata.player_id = chunk.metadata.player_id;
  record.metadata.vector_space = KnowledgeVectorSpace::kTextSemantic;
  record.metadata.model_id = std::string(embedding_model_->id());
  record.metadata.model_version = std::string(embedding_model_->version());
  record.metadata.dimensions =
      static_cast<std::uint32_t>(embedding_model_->dimensions());
  record.metadata.source_version = chunk.metadata.source_version;
  record.metadata.created_at_ms = observed_at_ms;
  record.metadata.updated_at_ms = observed_at_ms;
  record.metadata.embedding_id = make_knowledge_embedding_id(
      record.metadata.owner_kind, record.metadata.owner_id,
      record.metadata.vector_space, record.metadata.model_id,
      record.metadata.model_version);
  record.values = *values;

  if (!is_valid_vector_record(record)) return std::nullopt;
  vector_index_->upsert(record);
  return record.metadata.embedding_id;
}

std::optional<std::vector<float>> ChunkTextEmbeddingWriter::embed_query(
    const std::string_view text) const {
  if (!available() || text.empty()) return std::nullopt;
  auto values = safe_embed(*embedding_model_, text);
  if (!values || !valid_vector(*values, embedding_model_->dimensions())) {
    return std::nullopt;
  }
  return values;
}

// -----------------------------------------------------------------------------
// Section: Semantic chunk search
// -----------------------------------------------------------------------------

SemanticChunkSearch::SemanticChunkSearch(
    const ChunkRegistry& chunks, const VectorIndex& vectors,
    const ai::EmbeddingModel& embedding_model, const TextReranker* reranker)
    : chunks_(&chunks),
      vectors_(&vectors),
      embedding_model_(&embedding_model),
      reranker_(reranker) {}

std::vector<SemanticChunkSearchHit> SemanticChunkSearch::search(
    const SemanticChunkSearchRequest& request) const {
  if (chunks_ == nullptr || vectors_ == nullptr || embedding_model_ == nullptr ||
      !chunks_->is_open() || !vectors_->is_open() || request.query_text.empty() ||
      request.limit == 0 || !valid_model(*embedding_model_)) {
    return {};
  }

  const auto query_values = safe_embed(*embedding_model_, request.query_text);
  if (!query_values ||
      !valid_vector(*query_values, embedding_model_->dimensions())) {
    return {};
  }

  const auto result_limit =
      std::min(request.limit, kMaximumSemanticResults);
  const auto candidate_limit = std::min(
      kMaximumSemanticCandidates,
      std::max({request.candidate_limit, result_limit, std::size_t{1}}));

  KnowledgeVectorQuery vector_query;
  vector_query.vector_space = KnowledgeVectorSpace::kTextSemantic;
  vector_query.player_id = request.player_id;
  vector_query.model_id = std::string(embedding_model_->id());
  vector_query.model_version = std::string(embedding_model_->version());
  vector_query.values = *query_values;
  vector_query.limit = candidate_limit;
  vector_query.minimum_score = request.minimum_vector_score;

  const auto vector_matches = vectors_->search(vector_query);
  std::vector<SemanticChunkSearchHit> hits;
  hits.reserve(std::min(vector_matches.size(), candidate_limit));
  std::unordered_set<std::string> seen_chunks;
  for (const auto& match : vector_matches) {
    if (match.metadata.owner_kind != EmbeddingOwnerKind::kChunk ||
        seen_chunks.contains(match.metadata.owner_id)) {
      continue;
    }
    const auto chunk = chunks_->chunk({match.metadata.owner_id});
    if (!chunk || chunk->metadata.embedding_id != match.metadata.embedding_id ||
        chunk->metadata.source_version != match.metadata.source_version) {
      continue;
    }
    seen_chunks.insert(match.metadata.owner_id);
    hits.push_back({*chunk, match.score, std::nullopt});
  }

  std::size_t embedded = 0;
  for (const auto& chunk : request.current_candidates) {
    if (embedded >= 8 || hits.size() >= candidate_limit) break;
    if (chunk.metadata.player_id != request.player_id || seen_chunks.contains(chunk.metadata.id.value)) continue;
    ++embedded;
    const auto values = safe_embed(*embedding_model_, chunk.content);
    if (!values || !valid_vector(*values, query_values->size())) continue;
    double dot = 0.0, a = 0.0, b = 0.0;
    for (std::size_t i = 0; i < values->size(); ++i) {
      dot += static_cast<double>((*values)[i]) * (*query_values)[i];
      a += static_cast<double>((*values)[i]) * (*values)[i];
      b += static_cast<double>((*query_values)[i]) * (*query_values)[i];
    }
    if (a <= 0.0 || b <= 0.0) continue;
    const float score = static_cast<float>(std::clamp(dot / std::sqrt(a * b), -1.0, 1.0));
    if (score < request.minimum_vector_score) continue;
    seen_chunks.insert(chunk.metadata.id.value);
    hits.push_back({chunk, score, std::nullopt});
  }
  if (hits.empty()) return {};
  std::stable_sort(hits.begin(), hits.end(), [](const auto& a, const auto& b) {
    if (a.vector_score != b.vector_score) return a.vector_score > b.vector_score;
    return a.chunk.metadata.id.value < b.chunk.metadata.id.value;
  });

  if (reranker_ != nullptr && reranker_->available()) {
    std::vector<TextRerankCandidate> candidates;
    candidates.reserve(hits.size());
    for (const auto& hit : hits) {
      candidates.push_back({hit.chunk.metadata.id, hit.chunk.content,
                            hit.chunk.metadata.type, hit.chunk.metadata.topic,
                            hit.vector_score});
    }

    std::vector<TextRerankScore> scores;
    try {
      scores = reranker_->score(request.query_text, candidates);
    } catch (...) {
      scores.clear();
    }
    std::unordered_map<std::string, double> valid_scores;
    valid_scores.reserve(scores.size());
    for (const auto& item : scores) {
      if (item.chunk_id.empty() || !std::isfinite(item.score)) continue;
      if (!seen_chunks.contains(item.chunk_id.value)) continue;
      valid_scores.emplace(item.chunk_id.value, item.score);
    }
    for (auto& hit : hits) {
      if (const auto it = valid_scores.find(hit.chunk.metadata.id.value);
          it != valid_scores.end()) {
        hit.rerank_score = it->second;
      }
    }

    std::stable_sort(hits.begin(), hits.end(),
                     [](const SemanticChunkSearchHit& lhs,
                        const SemanticChunkSearchHit& rhs) {
                       const bool lhs_scored = lhs.rerank_score.has_value();
                       const bool rhs_scored = rhs.rerank_score.has_value();
                       if (lhs_scored != rhs_scored) return lhs_scored;
                       if (lhs_scored && *lhs.rerank_score != *rhs.rerank_score) {
                         return *lhs.rerank_score > *rhs.rerank_score;
                       }
                       if (lhs.vector_score != rhs.vector_score) {
                         return lhs.vector_score > rhs.vector_score;
                       }
                       return lhs.chunk.metadata.id.value <
                              rhs.chunk.metadata.id.value;
                     });
  }

  if (hits.size() > result_limit) hits.resize(result_limit);
  return hits;
}

}  // namespace kchess::knowledge
