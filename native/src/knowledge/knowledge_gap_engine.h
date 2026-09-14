#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "conflict_resolver.h"
#include "dependency_tracker.h"
#include "graph_store.h"
#include "knowledge_quality_store.h"

namespace kchess::knowledge {

// -----------------------------------------------------------------------------
// Section: Knowledge-gap and active-learning contract
// -----------------------------------------------------------------------------

enum class KnowledgeGapReason {
  kLowCoverage,
  kLowConfidence,
  kStaleEvidence,
  kConflictingEvidence,
};

struct KnowledgeGapRecord {
  KnowledgeNodeId gap_node_id;
  KnowledgeEntryRef target;
  KnowledgeNodeKind target_kind{KnowledgeNodeKind::kUnknown};
  std::string topic;
  double priority{0.0};
  double coverage{0.0};
  double confidence{0.0};
  double freshness{1.0};
  std::vector<KnowledgeGapReason> reasons;
  std::vector<std::string> source_game_ids;
};

// This is deliberately a hint set, not a scheduler. PlayerProfileService folds
// it into the existing nine-stage funnel/queue and remains the only owner of
// engine work, retries and resume state.
struct KnowledgeGapActiveLearningPlan {
  std::vector<std::string> priority_game_ids;
  std::vector<std::string> priority_openings;
  std::vector<std::string> priority_time_controls;
  bool prioritize_losses{false};
  bool prioritize_endgame_length{false};
};

// -----------------------------------------------------------------------------
// Section: Gap detection and graph materialization
// -----------------------------------------------------------------------------

class KnowledgeGapEngine {
 public:
  KnowledgeGapEngine(GraphStore& graph, DependencyTracker& dependencies,
                     KnowledgeQualityStore& quality,
                     ConflictResolver& conflicts);

  // Recomputes the bounded set of actionable gaps for one player profile.
  // Existing gap nodes for that profile are replaced incrementally; source
  // payloads remain in their authoritative stores.
  [[nodiscard]] std::vector<KnowledgeGapRecord> refresh_profile_gaps(
      const std::string& profile_id, std::int64_t evaluated_at_ms,
      std::size_t max_entries = 2000, std::size_t max_gaps = 64);

  [[nodiscard]] KnowledgeGapActiveLearningPlan build_active_learning_plan(
      const std::vector<KnowledgeGapRecord>& gaps,
      std::size_t max_priority_games = 64) const;

 private:
  GraphStore& graph_;
  DependencyTracker& dependencies_;
  KnowledgeQualityStore& quality_;
  ConflictResolver& conflicts_;
};

[[nodiscard]] std::string_view to_string(KnowledgeGapReason value) noexcept;

}  // namespace kchess::knowledge
