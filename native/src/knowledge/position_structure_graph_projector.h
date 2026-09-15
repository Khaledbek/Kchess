#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "dependency_tracker.h"
#include "graph_store.h"

namespace kchess {
class Database;
}

namespace kchess::knowledge {

class PositionSimilarityIndex;

// -----------------------------------------------------------------------------
// Section: Position/structure projection contract
// -----------------------------------------------------------------------------

struct PositionStructureProjectionReport {
  std::string profile_id;
  std::size_t games_considered{0};
  std::size_t positions_considered{0};
  std::size_t positions_projected{0};
  std::size_t nodes_upserted{0};
  std::size_t edges_upserted{0};
  std::vector<KnowledgeInvalidation> invalidated_entries;
  std::vector<KnowledgeEntryRef> changed_entries;
  std::vector<KnowledgeEntryRef> removed_entries;
};

// Projects deterministic board-derived structure identities from persisted game
// positions. Games remain authoritative and FEN parsing/feature extraction stays
// in native position intelligence; the graph stores only compact structure
// signatures and relationships.
class PositionStructureGraphProjector {
 public:
  PositionStructureGraphProjector(Database& database, GraphStore& graph,
                                  DependencyTracker& dependencies,
                                  PositionSimilarityIndex* position_similarity = nullptr);

  [[nodiscard]] PositionStructureProjectionReport project_active_profile(
      std::int64_t observed_at_ms);

 private:
  Database& database_;
  GraphStore& graph_;
  DependencyTracker& dependencies_;
  PositionSimilarityIndex* position_similarity_{nullptr};
};

}  // namespace kchess::knowledge
