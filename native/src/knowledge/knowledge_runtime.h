#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <memory>
#include <optional>
#include <string>

#include "../../ai/dto/coach_request.h"
#include "../../ai/dto/evidence.h"
#include "../../ai/dto/query_plan.h"
#include "knowledge_gap_engine.h"

namespace kchess {
class Database;
class StatisticsService;
}

namespace kchess::knowledge {

class GraphStore;
class DependencyTracker;
class ChunkRegistry;
class KnowledgeQualityStore;
class ConflictResolver;
class SqliteVectorIndex;
class PositionSimilarityIndex;
class QueryTraceStore;

// -----------------------------------------------------------------------------
// Section: Integrated Knowledge Graph runtime
// -----------------------------------------------------------------------------

struct KnowledgeRefreshReport {
  std::string profile_id;
  std::size_t graph_nodes_upserted{0};
  std::size_t graph_edges_upserted{0};
  std::size_t chunks_upserted{0};
  std::size_t chunks_removed{0};
  std::size_t quality_entries_refreshed{0};
  std::size_t conflicts{0};
  std::size_t gaps{0};
  std::size_t changed_entries{0};
  std::size_t chunk_nodes_processed{0};
  bool full_maintenance{false};
  bool game_graph_projection_skipped{false};
  std::size_t background_yield_count{0};
  std::uint64_t background_yield_ms{0};
  std::int64_t refreshed_at_ms{0};
};

// Owns the shared persisted Knowledge Graph runtime used by profile maintenance,
// Coach retrieval and developer diagnostics. Authoritative chess/profile facts
// stay in their existing Database/Statistics/Analysis stores; this layer owns
// only projections, routing metadata, semantic chunks/vectors and traces.
class KnowledgeRuntime {
 public:
  KnowledgeRuntime(Database& database, StatisticsService& statistics,
                   std::filesystem::path data_directory);
  ~KnowledgeRuntime();

  KnowledgeRuntime(const KnowledgeRuntime&) = delete;
  KnowledgeRuntime& operator=(const KnowledgeRuntime&) = delete;

  void open();
  void close() noexcept;
  [[nodiscard]] bool is_open() const noexcept;

  [[nodiscard]] KnowledgeRefreshReport refresh_active_profile(
      std::int64_t observed_at_ms,
      const std::string& graph_source_signature = {});

  [[nodiscard]] KnowledgeGapActiveLearningPlan active_learning_plan(
      const std::string& profile_id, std::int64_t evaluated_at_ms);

  // Returns a provider-compatible profile.context.v3 EvidenceItem assembled
  // from the general graph and the current StatisticsService read model. No
  // engine work, corpus indexing or graph writes are allowed here. When graph
  // maintenance owns the mutex, live facts use the same packet pipeline alone.
  [[nodiscard]] std::optional<ai::EvidenceItem> coach_evidence(
      const ai::CoachRequest& request, const ai::QueryPlan& plan);

  // Developer/diagnostic endpoint. request_json may contain profileId, query,
  // nodeId and limit. Output is bounded JSON; it never exposes provider secrets.
  // This path is non-blocking: while the shared runtime is refreshing it returns
  // status=busy instead of waiting on the long-running runtime mutex.
  [[nodiscard]] std::string inspector_json(const std::string& request_json);

 private:
  struct Impl;

  struct RuntimeActivity {
    bool active{false};
    std::string operation{"idle"};
    std::string phase{"idle"};
    std::string profile_id;
    std::size_t completed{0};
    std::size_t total{0};
    std::int64_t started_at_ms{0};
    std::int64_t phase_started_at_ms{0};
    std::int64_t updated_at_ms{0};
    std::string last_phase;
    std::uint64_t last_phase_duration_ms{0};
  };

  void set_activity(
      std::string operation, std::string phase, std::string profile_id = {},
      std::size_t completed = 0, std::size_t total = 0);
  void clear_activity() noexcept;
  [[nodiscard]] std::string activity_json() const;
  [[nodiscard]] std::string performance_json() const;

  Database& database_;
  StatisticsService& statistics_;
  std::filesystem::path data_directory_;
  mutable std::mutex mutex_;
  mutable std::mutex activity_mutex_;
  RuntimeActivity activity_;
  std::atomic_uint64_t refresh_count_{0};
  std::atomic_uint64_t refresh_total_ms_{0};
  std::atomic_uint64_t refresh_last_ms_{0};
  std::atomic_uint64_t refresh_lock_wait_total_ms_{0};
  std::atomic_uint64_t refresh_lock_wait_last_ms_{0};
  std::atomic_uint64_t refresh_full_maintenance_{0};
  std::atomic_uint64_t refresh_incremental_maintenance_{0};
  std::atomic_uint64_t refresh_changed_entries_{0};
  std::atomic_uint64_t refresh_chunk_nodes_processed_{0};
  std::atomic_uint64_t refresh_quality_entries_processed_{0};
  std::atomic_uint64_t refresh_game_graph_projection_skips_{0};
  std::atomic_uint64_t refresh_background_yield_count_{0};
  std::atomic_uint64_t refresh_background_yield_ms_{0};
  std::atomic_uint64_t coach_evidence_requests_{0};
  std::atomic_uint64_t coach_graph_available_{0};
  std::atomic_uint64_t coach_graph_busy_fallbacks_{0};
  std::atomic_uint64_t coach_total_ms_{0};
  std::atomic_uint64_t coach_last_ms_{0};
  std::unique_ptr<Impl> impl_;
};

}  // namespace kchess::knowledge
