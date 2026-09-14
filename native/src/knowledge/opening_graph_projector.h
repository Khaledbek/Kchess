#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "dependency_tracker.h"
#include "graph_store.h"

namespace kchess {
class Database;
class OpeningTheoryProvider;
}

namespace kchess::knowledge {

// -----------------------------------------------------------------------------
// Section: Opening graph projection contract
// -----------------------------------------------------------------------------

struct OpeningGraphProjectionReport {
  std::string profile_id;
  std::size_t games_considered{0};
  std::size_t named_games{0};
  std::size_t nodes_upserted{0};
  std::size_t edges_upserted{0};
  std::size_t transposition_links{0};
  std::vector<KnowledgeInvalidation> invalidated_entries;
  std::vector<KnowledgeEntryRef> changed_entries;
  std::vector<KnowledgeEntryRef> removed_entries;
};

// Projects the already persisted opening classification and opening-prefix
// moves into the general Knowledge Graph. The Games DB remains authoritative;
// this projector stores only compact identities, path relationships and source
// locators. Canonical Position nodes use Stockfish position keys, so different
// move orders converge on the same position identity instead of forming a tree.
class OpeningGraphProjector {
 public:
  OpeningGraphProjector(Database& database, GraphStore& graph,
                        DependencyTracker& dependencies,
                        const OpeningTheoryProvider* theory = nullptr);

  // Reprojects the active profile. Games without a persisted named opening are
  // ignored; opening classification remains owned by the existing KCO path.
  // GraphStore and DependencyTracker must already be open.
  [[nodiscard]] OpeningGraphProjectionReport project_active_profile(
      std::int64_t observed_at_ms);

 private:
  Database& database_;
  GraphStore& graph_;
  DependencyTracker& dependencies_;
  const OpeningTheoryProvider* theory_{nullptr};
};

}  // namespace kchess::knowledge
