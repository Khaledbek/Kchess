#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct sqlite3;

namespace kchess::knowledge {

// -----------------------------------------------------------------------------
// Section: Vector-space contract
// -----------------------------------------------------------------------------

enum class KnowledgeVectorSpace {
  kTextSemantic = 0,
  kChessPosition,
};

enum class EmbeddingOwnerKind {
  kChunk = 0,
  kNode,
};

[[nodiscard]] std::string_view to_string(KnowledgeVectorSpace value) noexcept;
[[nodiscard]] std::optional<KnowledgeVectorSpace>
knowledge_vector_space_from_string(std::string_view value) noexcept;
[[nodiscard]] std::string_view to_string(EmbeddingOwnerKind value) noexcept;
[[nodiscard]] std::optional<EmbeddingOwnerKind>
embedding_owner_kind_from_string(std::string_view value) noexcept;

// Stable for the same semantic owner + vector space + exact model version.
[[nodiscard]] std::string make_knowledge_embedding_id(
    EmbeddingOwnerKind owner_kind, std::string_view owner_id,
    KnowledgeVectorSpace vector_space, std::string_view model_id,
    std::string_view model_version);

struct KnowledgeEmbeddingMetadata {
  std::string embedding_id;
  EmbeddingOwnerKind owner_kind{EmbeddingOwnerKind::kChunk};
  std::string owner_id;
  // Empty means globally reusable knowledge. Player-scoped embeddings carry the
  // native profile/player routing identity so retrieval cannot cross scopes.
  std::string player_id;
  KnowledgeVectorSpace vector_space{KnowledgeVectorSpace::kTextSemantic};
  std::string model_id;
  std::string model_version;
  std::uint32_t dimensions{0};
  std::string source_version;
  std::int64_t created_at_ms{0};
  std::int64_t updated_at_ms{0};
};

struct KnowledgeVectorRecord {
  KnowledgeEmbeddingMetadata metadata;
  std::vector<float> values;
};

struct KnowledgeVectorQuery {
  KnowledgeVectorSpace vector_space{KnowledgeVectorSpace::kTextSemantic};
  std::string player_id;
  std::string model_id;
  std::string model_version;
  std::vector<float> values;
  std::size_t limit{20};
  float minimum_score{-1.0F};
};

struct KnowledgeVectorMatch {
  KnowledgeEmbeddingMetadata metadata;
  float score{0.0F};
};

[[nodiscard]] bool is_valid_embedding_metadata(
    const KnowledgeEmbeddingMetadata& metadata) noexcept;
[[nodiscard]] bool is_valid_vector_record(
    const KnowledgeVectorRecord& record) noexcept;

// -----------------------------------------------------------------------------
// Section: Runtime abstraction
// -----------------------------------------------------------------------------

class VectorIndex {
 public:
  virtual ~VectorIndex() = default;

  virtual void open() = 0;
  virtual void close() noexcept = 0;
  [[nodiscard]] virtual bool is_open() const noexcept = 0;

  virtual void upsert(const KnowledgeVectorRecord& record) = 0;
  [[nodiscard]] virtual std::optional<KnowledgeEmbeddingMetadata> metadata(
      std::string_view embedding_id) const = 0;
  virtual bool remove(std::string_view embedding_id) = 0;
  [[nodiscard]] virtual std::vector<KnowledgeVectorMatch> search(
      const KnowledgeVectorQuery& query) const = 0;
};

// Persistent exact-search baseline. It intentionally favors a stable storage
// and retrieval contract over an ANN implementation. A later HNSW/other index
// can implement VectorIndex without changing callers or embedding metadata.
class SqliteVectorIndex final : public VectorIndex {
 public:
  explicit SqliteVectorIndex(std::filesystem::path data_directory);
  ~SqliteVectorIndex() override;

  SqliteVectorIndex(const SqliteVectorIndex&) = delete;
  SqliteVectorIndex& operator=(const SqliteVectorIndex&) = delete;

  void open() override;
  void close() noexcept override;
  [[nodiscard]] bool is_open() const noexcept override { return db_ != nullptr; }

  void upsert(const KnowledgeVectorRecord& record) override;
  [[nodiscard]] std::optional<KnowledgeEmbeddingMetadata> metadata(
      std::string_view embedding_id) const override;
  bool remove(std::string_view embedding_id) override;
  [[nodiscard]] std::vector<KnowledgeVectorMatch> search(
      const KnowledgeVectorQuery& query) const override;

 private:
  std::filesystem::path database_path_;
  sqlite3* db_{nullptr};
};

}  // namespace kchess::knowledge
