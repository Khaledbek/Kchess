#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace kchess::knowledge {

// -----------------------------------------------------------------------------
// Section: Contract and schema versioning
// -----------------------------------------------------------------------------

inline constexpr std::uint32_t kKnowledgeGraphContractVersion = 1;
inline constexpr std::uint32_t kKnowledgeGraphSchemaVersion = 1;

struct KnowledgeGraphVersion {
  std::uint32_t contract{kKnowledgeGraphContractVersion};
  std::uint32_t schema{kKnowledgeGraphSchemaVersion};

  [[nodiscard]] bool compatible_with(const KnowledgeGraphVersion& other) const
      noexcept {
    return contract == other.contract && schema == other.schema;
  }
};

// -----------------------------------------------------------------------------
// Section: Stable identities
// -----------------------------------------------------------------------------

struct KnowledgeNodeId {
  std::string value;

  [[nodiscard]] bool empty() const noexcept { return value.empty(); }
  friend bool operator==(const KnowledgeNodeId&, const KnowledgeNodeId&) =
      default;
};

struct KnowledgeEdgeId {
  std::string value;

  [[nodiscard]] bool empty() const noexcept { return value.empty(); }
  friend bool operator==(const KnowledgeEdgeId&, const KnowledgeEdgeId&) =
      default;
};

// IDs are deterministic for the same canonical identity. canonical_key must be
// a source-stable machine identity (database ID, canonical position key, stable
// profile pattern ID, etc.), never localized/user-visible text.
[[nodiscard]] KnowledgeNodeId make_knowledge_node_id(
    std::string_view kind_name, std::string_view canonical_key);

// discriminator is used only when more than one semantic edge with the same
// endpoints and relation can legitimately exist.
[[nodiscard]] KnowledgeEdgeId make_knowledge_edge_id(
    const KnowledgeNodeId& from, std::string_view relation_name,
    const KnowledgeNodeId& to, std::string_view discriminator = {});

// -----------------------------------------------------------------------------
// Section: Property graph base types
// -----------------------------------------------------------------------------

using KnowledgePropertyValue =
    std::variant<bool, std::int64_t, double, std::string>;
using KnowledgeProperties = std::map<std::string, KnowledgePropertyValue>;

enum class KnowledgeNodeKind {
  kUnknown = 0,
  kPlayer,
  kAccount,
  kProvider,
  kGame,
  kPosition,
  kAnalysis,
  kStatistic,
  kEvidence,
  kChunk,
  kOpeningFamily,
  kOpening,
  kVariation,
  kPositionFamily,
  kPawnStructure,
  kMiddlegameStructure,
  kEndgameType,
  kMaterialConfiguration,
  kPieceConfiguration,
  kTacticalMotif,
  kStrategicMotif,
  kKingSafetyPattern,
  kExchangePattern,
  kPlanPattern,
  kTransitionPattern,
  kStrength,
  kWeakness,
  kHabit,
  kBehavior,
  kStylePattern,
  kResultPattern,
  kTimeManagementPattern,
  kComplexityPattern,
  kConversionPattern,
  kDefensePattern,
  kRecoveryPattern,
  kRepertoirePattern,
  kTrend,
  kHypothesis,
  kKnowledgeGap,
  kRatingPeriod,
  kTimeControl,
  kOpponentStrengthBand,
  kDatePeriod,
  kColor,
  kResultType,
  kTerminationType,
};

enum class KnowledgeEdgeKind {
  kUnknown = 0,
  kPlays,
  kReaches,
  kTranspositionOf,
  kHasStructure,
  kHasMotif,
  kTransitionsTo,
  kOftenTransitionsTo,
  kLeadsTo,
  kOftenLeadsTo,
  kPrecedes,
  kFollows,
  kContains,
  kSimilarTo,
  kStrongIn,
  kWeakIn,
  kImprovingIn,
  kDecliningIn,
  kCorrelatedWith,
  kContributesTo,
  kSupportedBy,
  kContradictedBy,
  kDerivedFrom,
  kExplainedBy,
  kHasStatistics,
  kLostBy,
  kWonBy,
  kSavedFrom,
  kMissedWin,
  kMissedDraw,
  kCommonIn,
  kRareIn,
  kUnderrepresentedIn,
  kDependsOn,
};

enum class KnowledgeAssertionKind {
  kFact = 0,
  kObservation,
  kHypothesis,
};

[[nodiscard]] std::string_view to_string(KnowledgeNodeKind kind) noexcept;
[[nodiscard]] std::string_view to_string(KnowledgeEdgeKind kind) noexcept;
[[nodiscard]] std::string_view to_string(KnowledgeAssertionKind kind) noexcept;
[[nodiscard]] std::optional<KnowledgeNodeKind> knowledge_node_kind_from_string(
    std::string_view value) noexcept;
[[nodiscard]] std::optional<KnowledgeEdgeKind> knowledge_edge_kind_from_string(
    std::string_view value) noexcept;
[[nodiscard]] std::optional<KnowledgeAssertionKind>
knowledge_assertion_kind_from_string(std::string_view value) noexcept;

struct KnowledgeNode {
  KnowledgeNodeId id;
  KnowledgeNodeKind kind{KnowledgeNodeKind::kUnknown};
  KnowledgeAssertionKind assertion_kind{KnowledgeAssertionKind::kFact};
  KnowledgeProperties properties;
  std::uint32_t schema_version{kKnowledgeGraphSchemaVersion};
};

struct KnowledgeEdge {
  KnowledgeEdgeId id;
  KnowledgeNodeId from;
  KnowledgeNodeId to;
  KnowledgeEdgeKind kind{KnowledgeEdgeKind::kUnknown};
  KnowledgeProperties properties;
  std::uint32_t schema_version{kKnowledgeGraphSchemaVersion};
};

// Validates only contract-level invariants. Storage/source-specific validation
// belongs to later layers and must not be duplicated here.
[[nodiscard]] bool is_valid_contract_node(const KnowledgeNode& node) noexcept;
[[nodiscard]] bool is_valid_contract_edge(const KnowledgeEdge& edge) noexcept;

}  // namespace kchess::knowledge
