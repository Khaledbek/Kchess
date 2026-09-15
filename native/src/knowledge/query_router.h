#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "../../ai/dto/query_plan.h"
#include "entity_extractor.h"
#include "knowledge_graph_contract.h"

namespace kchess::knowledge {

// -----------------------------------------------------------------------------
// Section: Knowledge-query routing contract
// -----------------------------------------------------------------------------

enum class KnowledgeQueryIntent {
  kUnknown = 0,
  kExactStatistic,
  kRelationship,
  kCausalAnalysis,
  kTrend,
  kEvidence,
  kSimilarity,
  kCurrentPosition,
  kGeneralPersonal,
  kGeneralChess,
};

enum class KnowledgeScopeRelevance {
  kNone = 0,
  kOptional,
  kRequired,
};

struct KnowledgeRetrievalChannels {
  bool exact_statistics{false};
  bool graph{false};
  bool lexical{false};
  bool vector{false};
  bool position_similarity{false};
};

struct KnowledgeQueryRoute {
  KnowledgeQueryIntent intent{KnowledgeQueryIntent::kUnknown};
  ai::QueryFamily query_family{ai::QueryFamily::unknown};
  ai::CoachIntent coach_intent{ai::CoachIntent::unknown};
  KnowledgeScopeRelevance current_board{KnowledgeScopeRelevance::kNone};
  KnowledgeScopeRelevance historical_profile{KnowledgeScopeRelevance::kNone};
  KnowledgeRetrievalChannels channels;
  std::vector<KnowledgeQueryEntity> entities;
  bool current_position_available{false};
  bool requires_player_scope{false};
  double confidence{0.0};
  ai::ProfileQueryScope profile_scope;
};

// One eligibility contract for exact, graph, lexical and vector candidates.
// Traversal may pass routing containers; only matching evidence is emitted.
[[nodiscard]] bool knowledge_node_matches_scope(
    const KnowledgeNode& node, const KnowledgeQueryRoute& route);

// The coach QueryPlan remains the single coarse intent/profile/position
// classifier. This router refines only Knowledge-Graph retrieval behavior and
// must not independently reclassify the user into a different coach family.
class KnowledgeQueryRouter {
 public:
  [[nodiscard]] KnowledgeQueryRoute route(
      std::string_view query_text, const ai::QueryPlan& plan,
      bool current_position_available) const;

 private:
  KnowledgeEntityExtractor entity_extractor_;
};

[[nodiscard]] std::string_view to_string(KnowledgeQueryIntent intent) noexcept;
[[nodiscard]] std::string_view to_string(
    KnowledgeScopeRelevance relevance) noexcept;

}  // namespace kchess::knowledge
