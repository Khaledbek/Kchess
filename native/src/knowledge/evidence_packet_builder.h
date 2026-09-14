#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "chunk_registry.h"
#include "dependency_tracker.h"
#include "graph_store.h"
#include "knowledge_quality_store.h"
#include "retrieval_ranking.h"

namespace kchess::knowledge {

// -----------------------------------------------------------------------------
// Section: Coach evidence packet contract
// -----------------------------------------------------------------------------

enum class KnowledgePacketSection {
  kFact = 0,
  kObservation,
  kEvidence,
  kUncertainty,
};

struct KnowledgePacketSourceTrace {
  KnowledgeEntryRef entry;
  std::vector<KnowledgeSourceRef> sources;
  std::optional<KnowledgeChunkId> chunk_id;
  HybridRetrievalSignals retrieval_signals;
  double retrieval_score{0.0};
};

struct KnowledgePacketGraphTraceStep {
  KnowledgeEdgeId edge_id;
  KnowledgeNodeId node_id;
  KnowledgeEdgeKind edge_kind{KnowledgeEdgeKind::kUnknown};
  std::size_t depth{0};
};

struct KnowledgeEvidencePacketItem {
  std::string id;
  KnowledgePacketSection section{KnowledgePacketSection::kEvidence};
  std::string topic;
  std::string content;
  double retrieval_score{0.0};
  std::optional<double> confidence;
  std::optional<double> coverage;
  std::optional<double> freshness;
  std::vector<KnowledgeEntryRef> graph_entries;
  std::vector<KnowledgeSourceRef> sources;
  KnowledgeProperties properties;
  KnowledgeNodeKind node_kind{KnowledgeNodeKind::kUnknown};
};

struct KnowledgePacketUncertainty {
  std::string code;
  std::string detail;
  std::optional<KnowledgeEntryRef> entry;
};

struct KnowledgeAnswerability {
  bool answerable{false};
  double confidence{0.0};
  bool has_exact_support{false};
  bool has_graph_support{false};
  bool has_source_trace{false};
  bool current_position_satisfied{true};
  std::vector<std::string> reasons;
  std::vector<std::string> missing_topics;
  std::vector<std::string> missing_time_controls;
  std::vector<std::string> missing_phases;
  std::vector<std::string> missing_player_colors;
};

struct KnowledgeEvidencePacketBudget {
  std::size_t max_tokens{1500};
  std::size_t max_facts{8};
  std::size_t max_observations{8};
  std::size_t max_evidence{8};
  std::size_t max_uncertainties{6};
  std::size_t current_position_tokens{180};
  std::size_t recent_context_tokens{240};
};

struct KnowledgeEvidencePacket {
  std::string query;
  std::vector<KnowledgeEvidencePacketItem> facts;
  std::vector<KnowledgeEvidencePacketItem> observations;
  std::vector<KnowledgeEvidencePacketItem> evidence;
  std::vector<KnowledgePacketUncertainty> uncertainties;
  std::optional<std::string> current_position;
  std::optional<std::string> recent_context;
  std::vector<KnowledgePacketSourceTrace> source_trace;
  std::vector<KnowledgePacketGraphTraceStep> graph_trace;
  KnowledgeAnswerability answerability;
  KnowledgeEvidencePacketBudget budget;
  std::size_t estimated_tokens{0};
  bool token_budget_exhausted{false};
};

struct KnowledgeEvidencePacketRequest {
  std::string player_id;
  std::string query_text;
  KnowledgeQueryRoute route;
  KnowledgeQueryExecutionPlan execution_plan;
  RankedKnowledgeRetrieval ranked;
  HybridRetrievalResult retrieval;
  std::optional<std::string> current_position;
  std::optional<std::string> recent_context;
};

[[nodiscard]] std::string_view to_string(KnowledgePacketSection section) noexcept;

// Builds the provider-neutral, bounded evidence packet consumed by the Coach in
// Update 123. It only packages already-retrieved Knowledge Graph material; it
// never starts analysis, recomputes statistics, changes ranking, or invents
// missing evidence.
class EvidencePacketBuilder {
 public:
  EvidencePacketBuilder(GraphStore& graph, ChunkRegistry& chunks,
                        DependencyTracker& dependencies,
                        const KnowledgeQualityStore* quality_store = nullptr);

  [[nodiscard]] KnowledgeEvidencePacket build(
      const KnowledgeEvidencePacketRequest& request) const;

 private:
  GraphStore& graph_;
  ChunkRegistry& chunks_;
  DependencyTracker& dependencies_;
  const KnowledgeQualityStore* quality_store_{nullptr};
};

}  // namespace kchess::knowledge
