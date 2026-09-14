#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "dependency_tracker.h"
#include "knowledge_graph_contract.h"

struct sqlite3;

namespace kchess::knowledge {

// -----------------------------------------------------------------------------
// Section: Chunk contract
// -----------------------------------------------------------------------------

enum class KnowledgeChunkGranularity {
  kSummary = 0,
  kTopic,
  kEntity,
  kEvidence,
};

struct KnowledgeChunkId {
  std::string value;

  [[nodiscard]] bool empty() const noexcept { return value.empty(); }
  friend bool operator==(const KnowledgeChunkId&, const KnowledgeChunkId&) = default;
};

// Chunk IDs are stable for the same logical semantic unit. revision_key may be
// used when a source intentionally exposes multiple independently addressable
// chunks for the same type/topic pair.
[[nodiscard]] KnowledgeChunkId make_knowledge_chunk_id(
    std::string_view player_id, std::string_view type, std::string_view topic,
    KnowledgeChunkGranularity granularity, std::string_view revision_key = {});

[[nodiscard]] std::string_view to_string(KnowledgeChunkGranularity value) noexcept;
[[nodiscard]] std::optional<KnowledgeChunkGranularity>
knowledge_chunk_granularity_from_string(std::string_view value) noexcept;

struct KnowledgeChunkMetadata {
  KnowledgeChunkId id;
  std::string player_id;
  std::string type;
  std::string topic;
  KnowledgeChunkGranularity granularity{KnowledgeChunkGranularity::kTopic};
  std::string source_type;
  std::optional<std::string> opening;
  std::optional<std::string> eco;
  std::optional<std::string> color;
  std::optional<std::string> result;
  std::optional<std::string> time_control;
  std::optional<std::string> date_range;
  std::optional<std::string> rating_range;
  std::optional<std::string> evidence_type;
  std::optional<double> confidence;
  std::optional<double> coverage;
  std::optional<double> freshness;
  std::optional<double> importance;
  std::optional<std::int64_t> sample_size;
  std::optional<double> source_quality;
  std::string source_version;
  std::optional<std::string> embedding_id;
  std::uint32_t schema_version{kKnowledgeGraphSchemaVersion};
};

struct KnowledgeChunk {
  KnowledgeChunkMetadata metadata;
  std::string content;
  std::vector<KnowledgeSourceRef> sources;
};

enum class ChunkGraphLinkKind {
  kDescribes = 0,
  kEvidenceFor,
  kSummarizes,
};

struct ChunkGraphLink {
  KnowledgeChunkId chunk_id;
  KnowledgeEntryRef entry;
  ChunkGraphLinkKind kind{ChunkGraphLinkKind::kDescribes};
};

struct KnowledgeLexicalChunkHit {
  KnowledgeChunk chunk;
  double lexical_score{0.0};
  std::size_t matched_terms{0};
};

[[nodiscard]] std::string_view to_string(ChunkGraphLinkKind value) noexcept;
[[nodiscard]] std::optional<ChunkGraphLinkKind> chunk_graph_link_kind_from_string(
    std::string_view value) noexcept;

[[nodiscard]] bool is_valid_chunk_metadata(
    const KnowledgeChunkMetadata& metadata) noexcept;

// -----------------------------------------------------------------------------
// Section: SQLite chunk registry
// -----------------------------------------------------------------------------

// ChunkRegistry owns semantic text units and retrieval metadata. It does not
// copy PGNs, complete engine output or statistics tables. Sources are stable
// locators back to authoritative KChess stores; graph links are routing refs.
class ChunkRegistry {
 public:
  explicit ChunkRegistry(std::filesystem::path data_directory);
  ~ChunkRegistry();

  ChunkRegistry(const ChunkRegistry&) = delete;
  ChunkRegistry& operator=(const ChunkRegistry&) = delete;

  void open();
  void close() noexcept;
  [[nodiscard]] bool is_open() const noexcept { return db_ != nullptr; }

  void upsert_chunk(const KnowledgeChunk& chunk);
  [[nodiscard]] std::optional<KnowledgeChunk> chunk(
      const KnowledgeChunkId& id) const;
  bool remove_chunk(const KnowledgeChunkId& id);

  [[nodiscard]] std::vector<KnowledgeChunkMetadata> find(
      std::string_view player_id, std::optional<std::string_view> type = std::nullopt,
      std::optional<KnowledgeChunkGranularity> granularity = std::nullopt,
      std::size_t limit = 100) const;

  [[nodiscard]] std::vector<KnowledgeLexicalChunkHit> lexical_search(
      std::string_view player_id, const std::vector<std::string>& terms,
      std::size_t limit = 20) const;

  [[nodiscard]] std::vector<KnowledgeChunk> linked_chunks(
      std::string_view player_id, const std::vector<KnowledgeEntryRef>& entries,
      std::size_t limit = 40) const;

  void replace_graph_links(const KnowledgeChunkId& chunk_id,
                           const std::vector<ChunkGraphLink>& links);
  [[nodiscard]] std::vector<ChunkGraphLink> graph_links(
      const KnowledgeChunkId& chunk_id) const;

 private:
  std::filesystem::path database_path_;
  sqlite3* db_{nullptr};
};

}  // namespace kchess::knowledge
