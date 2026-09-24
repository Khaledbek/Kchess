#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "graph_store.h"
#include "knowledge_quality_store.h"

struct sqlite3;

namespace kchess::knowledge {

// -----------------------------------------------------------------------------
// Section: Temporal conflict contract
// -----------------------------------------------------------------------------

enum class KnowledgeConflictResolution {
  kUnresolved = 0,
  kCurrentSupersedesHistorical,
  kBalanced,
  kInsufficientEvidence,
};

enum class KnowledgeTemporalChange {
  kNone = 0,
  kImproved,
  kDeclined,
};

struct KnowledgeConflictRecord {
  std::string id;
  std::string profile_id;
  std::string subject_key;
  std::int64_t historical_version_id{0};
  KnowledgeNodeId historical_node_id;
  KnowledgeNodeId current_node_id;
  KnowledgeEdgeId relation_edge_id;
  KnowledgeConflictResolution resolution{KnowledgeConflictResolution::kUnresolved};
  KnowledgeTemporalChange temporal_change{KnowledgeTemporalChange::kNone};
  double historical_confidence{0.0};
  double current_confidence{0.0};
  std::int64_t first_detected_ms{0};
  std::int64_t last_evaluated_ms{0};
};

[[nodiscard]] std::string_view to_string(
    KnowledgeConflictResolution value) noexcept;
[[nodiscard]] std::string_view to_string(KnowledgeTemporalChange value) noexcept;

// -----------------------------------------------------------------------------
// Section: Conflict resolution and temporal development
// -----------------------------------------------------------------------------

// ConflictResolver compares the current player-profile assertions with immutable
// Knowledge Graph versions. Historical assertions remain queryable and are
// materialized as compact graph nodes only when they actually conflict with a
// current assertion. It never mutates the learned ChessProfile or source data.
class ConflictResolver {
 public:
  ConflictResolver(std::filesystem::path data_directory, GraphStore& graph,
                   KnowledgeQualityStore& quality_store);
  ~ConflictResolver();

  ConflictResolver(const ConflictResolver&) = delete;
  ConflictResolver& operator=(const ConflictResolver&) = delete;

  void open();
  void close() noexcept;
  [[nodiscard]] bool is_open() const noexcept { return db_ != nullptr; }

  // Resolves strength<->weakness changes for one player profile. A former
  // weakness becoming a strength is classified as improvement; the inverse is
  // decline. Same-kind historical revisions are history, not contradictions.
  [[nodiscard]] std::vector<KnowledgeConflictRecord> refresh_profile_conflicts(
      const std::string& profile_id, std::int64_t evaluated_at_ms);

  [[nodiscard]] std::vector<KnowledgeConflictRecord> conflicts_for_profile(
      const std::string& profile_id, std::size_t limit = 100) const;

 private:
  std::filesystem::path database_path_;
  GraphStore& graph_;
  KnowledgeQualityStore& quality_store_;
  sqlite3* db_{nullptr};
};

}  // namespace kchess::knowledge
