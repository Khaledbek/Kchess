#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "dependency_tracker.h"
#include "graph_store.h"

namespace kchess {
class Database;
class StatisticsService;
}

namespace kchess::knowledge {

// -----------------------------------------------------------------------------
// Section: Statistics-to-graph projection contract
// -----------------------------------------------------------------------------

struct StatisticsGraphProjectionReport {
  std::string profile_id;
  std::size_t nodes_upserted{0};
  std::size_t edges_upserted{0};
  std::vector<KnowledgeInvalidation> invalidated_entries;
  std::vector<KnowledgeEntryRef> changed_entries;
  std::vector<KnowledgeEntryRef> removed_entries;
};

// Projects existing native statistics into the general Knowledge Graph without
// recomputing or copying authoritative game payloads. The StatisticsService and
// persisted learning aggregates remain the source of truth; this class only
// creates compact routing nodes/edges and source/version references.
class StatisticsGraphProjector {
 public:
  StatisticsGraphProjector(Database& database, StatisticsService& statistics,
                           GraphStore& graph, DependencyTracker& dependencies);

  // Reprojects the active profile's shared player owner through StatisticsService.
  // Returns an empty profile_id when no profile is active. Stores must be open.
  [[nodiscard]] StatisticsGraphProjectionReport project_active_profile(
      std::int64_t observed_at_ms);

 private:
  Database& database_;
  StatisticsService& statistics_;
  GraphStore& graph_;
  DependencyTracker& dependencies_;
};

}  // namespace kchess::knowledge
