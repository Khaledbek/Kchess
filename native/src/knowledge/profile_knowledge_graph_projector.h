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
// Section: Learned-profile/evidence projection contract
// -----------------------------------------------------------------------------

struct ProfileKnowledgeProjectionReport {
  std::string profile_id;
  std::size_t strengths{0};
  std::size_t weaknesses{0};
  std::size_t behaviors{0};
  std::size_t habits{0};
  std::size_t trends{0};
  std::size_t hypotheses{0};
  std::size_t tactical_motifs{0};
  std::size_t strategic_motifs{0};
  std::size_t evidence_nodes{0};
  std::size_t recovery_patterns{0};
  std::size_t error_cascades{0};
  std::size_t nodes_upserted{0};
  std::size_t edges_upserted{0};
  std::vector<KnowledgeInvalidation> invalidated_entries;
  std::vector<KnowledgeEntryRef> changed_entries;
  std::vector<KnowledgeEntryRef> removed_entries;
};

// Projects the already learned ChessProfile plus payload-free evidence locators
// and persisted move-analysis observations into the general Knowledge Graph.
// It never learns a second profile, starts engine work, or copies PGN/analysis
// payloads into graph properties.
class ProfileKnowledgeGraphProjector {
 public:
  ProfileKnowledgeGraphProjector(Database& database, GraphStore& graph,
                                 DependencyTracker& dependencies);

  [[nodiscard]] ProfileKnowledgeProjectionReport project_active_profile(
      std::int64_t observed_at_ms);

 private:
  Database& database_;
  GraphStore& graph_;
  DependencyTracker& dependencies_;
};

}  // namespace kchess::knowledge
