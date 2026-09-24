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

// -----------------------------------------------------------------------------
// Section: Result/time/transition projection contract
// -----------------------------------------------------------------------------

struct ResultTransitionProjectionReport {
  std::string profile_id;
  std::size_t games_considered{0};
  std::size_t games_projected{0};
  std::size_t time_trouble_games{0};
  std::size_t missed_wins{0};
  std::size_t saved_losses{0};
  std::size_t nodes_upserted{0};
  std::size_t edges_upserted{0};
  std::vector<KnowledgeInvalidation> invalidated_entries;
  std::vector<KnowledgeEntryRef> changed_entries;
  std::vector<KnowledgeEntryRef> removed_entries;
};

// Projects existing game-result, clock and persisted move-analysis facts into
// causal/routing relationships. It never starts engine work or reclassifies a
// move; missing authoritative evidence remains missing.
class ResultTransitionGraphProjector {
 public:
  ResultTransitionGraphProjector(Database& database, GraphStore& graph,
                                 DependencyTracker& dependencies);

  [[nodiscard]] ResultTransitionProjectionReport project_active_profile(
      std::int64_t observed_at_ms);

 private:
  Database& database_;
  GraphStore& graph_;
  DependencyTracker& dependencies_;
};

}  // namespace kchess::knowledge
