#include "knowledge_graph_contract.h"

#include <initializer_list>
#include <iomanip>
#include <sstream>

namespace kchess::knowledge {
namespace {

// FNV-1a is intentionally used only for compact deterministic IDs, not for
// security. Hash input is length-delimited so concatenated components cannot
// alias merely because separators occur in source IDs.
std::uint64_t stable_hash(std::initializer_list<std::string_view> parts) {
  constexpr std::uint64_t kOffset = 14695981039346656037ULL;
  constexpr std::uint64_t kPrime = 1099511628211ULL;
  std::uint64_t hash = kOffset;
  for (const auto part : parts) {
    std::uint64_t size = static_cast<std::uint64_t>(part.size());
    for (int i = 0; i < 8; ++i) {
      hash ^= static_cast<unsigned char>((size >> (i * 8)) & 0xffU);
      hash *= kPrime;
    }
    for (const unsigned char ch : part) {
      hash ^= ch;
      hash *= kPrime;
    }
  }
  return hash;
}

std::string hex64(std::uint64_t value) {
  std::ostringstream out;
  out << std::hex << std::setfill('0') << std::setw(16) << value;
  return out.str();
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Stable identities
// -----------------------------------------------------------------------------

KnowledgeNodeId make_knowledge_node_id(std::string_view kind_name,
                                       std::string_view canonical_key) {
  if (kind_name.empty() || canonical_key.empty()) return {};
  return {"kg:n:" + std::string(kind_name) + ":" +
          hex64(stable_hash({kind_name, canonical_key}))};
}

KnowledgeEdgeId make_knowledge_edge_id(const KnowledgeNodeId& from,
                                       std::string_view relation_name,
                                       const KnowledgeNodeId& to,
                                       std::string_view discriminator) {
  if (from.empty() || relation_name.empty() || to.empty()) return {};
  return {"kg:e:" + std::string(relation_name) + ":" +
          hex64(stable_hash(
              {from.value, relation_name, to.value, discriminator}))};
}

// -----------------------------------------------------------------------------
// Section: Enum names
// -----------------------------------------------------------------------------

std::string_view to_string(KnowledgeNodeKind kind) noexcept {
  switch (kind) {
    case KnowledgeNodeKind::kPlayer: return "player";
    case KnowledgeNodeKind::kAccount: return "account";
    case KnowledgeNodeKind::kProvider: return "provider";
    case KnowledgeNodeKind::kGame: return "game";
    case KnowledgeNodeKind::kPosition: return "position";
    case KnowledgeNodeKind::kAnalysis: return "analysis";
    case KnowledgeNodeKind::kStatistic: return "statistic";
    case KnowledgeNodeKind::kEvidence: return "evidence";
    case KnowledgeNodeKind::kChunk: return "chunk";
    case KnowledgeNodeKind::kOpeningFamily: return "opening_family";
    case KnowledgeNodeKind::kOpening: return "opening";
    case KnowledgeNodeKind::kVariation: return "variation";
    case KnowledgeNodeKind::kPositionFamily: return "position_family";
    case KnowledgeNodeKind::kPawnStructure: return "pawn_structure";
    case KnowledgeNodeKind::kMiddlegameStructure: return "middlegame_structure";
    case KnowledgeNodeKind::kEndgameType: return "endgame_type";
    case KnowledgeNodeKind::kMaterialConfiguration:
      return "material_configuration";
    case KnowledgeNodeKind::kPieceConfiguration: return "piece_configuration";
    case KnowledgeNodeKind::kTacticalMotif: return "tactical_motif";
    case KnowledgeNodeKind::kStrategicMotif: return "strategic_motif";
    case KnowledgeNodeKind::kKingSafetyPattern: return "king_safety_pattern";
    case KnowledgeNodeKind::kExchangePattern: return "exchange_pattern";
    case KnowledgeNodeKind::kPlanPattern: return "plan_pattern";
    case KnowledgeNodeKind::kTransitionPattern: return "transition_pattern";
    case KnowledgeNodeKind::kStrength: return "strength";
    case KnowledgeNodeKind::kWeakness: return "weakness";
    case KnowledgeNodeKind::kHabit: return "habit";
    case KnowledgeNodeKind::kBehavior: return "behavior";
    case KnowledgeNodeKind::kStylePattern: return "style_pattern";
    case KnowledgeNodeKind::kResultPattern: return "result_pattern";
    case KnowledgeNodeKind::kTimeManagementPattern:
      return "time_management_pattern";
    case KnowledgeNodeKind::kComplexityPattern: return "complexity_pattern";
    case KnowledgeNodeKind::kConversionPattern: return "conversion_pattern";
    case KnowledgeNodeKind::kDefensePattern: return "defense_pattern";
    case KnowledgeNodeKind::kRecoveryPattern: return "recovery_pattern";
    case KnowledgeNodeKind::kRepertoirePattern: return "repertoire_pattern";
    case KnowledgeNodeKind::kTrend: return "trend";
    case KnowledgeNodeKind::kHypothesis: return "hypothesis";
    case KnowledgeNodeKind::kKnowledgeGap: return "knowledge_gap";
    case KnowledgeNodeKind::kRatingPeriod: return "rating_period";
    case KnowledgeNodeKind::kTimeControl: return "time_control";
    case KnowledgeNodeKind::kOpponentStrengthBand:
      return "opponent_strength_band";
    case KnowledgeNodeKind::kDatePeriod: return "date_period";
    case KnowledgeNodeKind::kColor: return "color";
    case KnowledgeNodeKind::kResultType: return "result_type";
    case KnowledgeNodeKind::kTerminationType: return "termination_type";
    case KnowledgeNodeKind::kUnknown: return "unknown";
  }
  return "unknown";
}

std::string_view to_string(KnowledgeEdgeKind kind) noexcept {
  switch (kind) {
    case KnowledgeEdgeKind::kPlays: return "plays";
    case KnowledgeEdgeKind::kReaches: return "reaches";
    case KnowledgeEdgeKind::kTranspositionOf: return "transposition_of";
    case KnowledgeEdgeKind::kHasStructure: return "has_structure";
    case KnowledgeEdgeKind::kHasMotif: return "has_motif";
    case KnowledgeEdgeKind::kTransitionsTo: return "transitions_to";
    case KnowledgeEdgeKind::kOftenTransitionsTo: return "often_transitions_to";
    case KnowledgeEdgeKind::kLeadsTo: return "leads_to";
    case KnowledgeEdgeKind::kOftenLeadsTo: return "often_leads_to";
    case KnowledgeEdgeKind::kPrecedes: return "precedes";
    case KnowledgeEdgeKind::kFollows: return "follows";
    case KnowledgeEdgeKind::kContains: return "contains";
    case KnowledgeEdgeKind::kSimilarTo: return "similar_to";
    case KnowledgeEdgeKind::kStrongIn: return "strong_in";
    case KnowledgeEdgeKind::kWeakIn: return "weak_in";
    case KnowledgeEdgeKind::kImprovingIn: return "improving_in";
    case KnowledgeEdgeKind::kDecliningIn: return "declining_in";
    case KnowledgeEdgeKind::kCorrelatedWith: return "correlated_with";
    case KnowledgeEdgeKind::kContributesTo: return "contributes_to";
    case KnowledgeEdgeKind::kSupportedBy: return "supported_by";
    case KnowledgeEdgeKind::kContradictedBy: return "contradicted_by";
    case KnowledgeEdgeKind::kDerivedFrom: return "derived_from";
    case KnowledgeEdgeKind::kExplainedBy: return "explained_by";
    case KnowledgeEdgeKind::kHasStatistics: return "has_statistics";
    case KnowledgeEdgeKind::kLostBy: return "lost_by";
    case KnowledgeEdgeKind::kWonBy: return "won_by";
    case KnowledgeEdgeKind::kSavedFrom: return "saved_from";
    case KnowledgeEdgeKind::kMissedWin: return "missed_win";
    case KnowledgeEdgeKind::kMissedDraw: return "missed_draw";
    case KnowledgeEdgeKind::kCommonIn: return "common_in";
    case KnowledgeEdgeKind::kRareIn: return "rare_in";
    case KnowledgeEdgeKind::kUnderrepresentedIn: return "underrepresented_in";
    case KnowledgeEdgeKind::kDependsOn: return "depends_on";
    case KnowledgeEdgeKind::kUnknown: return "unknown";
  }
  return "unknown";
}

std::string_view to_string(KnowledgeAssertionKind kind) noexcept {
  switch (kind) {
    case KnowledgeAssertionKind::kFact: return "fact";
    case KnowledgeAssertionKind::kObservation: return "observation";
    case KnowledgeAssertionKind::kHypothesis: return "hypothesis";
  }
  return "fact";
}

// -----------------------------------------------------------------------------
// Section: Persistence-name parsing
// -----------------------------------------------------------------------------

std::optional<KnowledgeNodeKind> knowledge_node_kind_from_string(
    std::string_view value) noexcept {
#define KCHESS_NODE_KIND_CASE(enum_value, text_value) \
  if (value == text_value) return KnowledgeNodeKind::enum_value
  KCHESS_NODE_KIND_CASE(kPlayer, "player");
  KCHESS_NODE_KIND_CASE(kAccount, "account");
  KCHESS_NODE_KIND_CASE(kProvider, "provider");
  KCHESS_NODE_KIND_CASE(kGame, "game");
  KCHESS_NODE_KIND_CASE(kPosition, "position");
  KCHESS_NODE_KIND_CASE(kAnalysis, "analysis");
  KCHESS_NODE_KIND_CASE(kStatistic, "statistic");
  KCHESS_NODE_KIND_CASE(kEvidence, "evidence");
  KCHESS_NODE_KIND_CASE(kChunk, "chunk");
  KCHESS_NODE_KIND_CASE(kOpeningFamily, "opening_family");
  KCHESS_NODE_KIND_CASE(kOpening, "opening");
  KCHESS_NODE_KIND_CASE(kVariation, "variation");
  KCHESS_NODE_KIND_CASE(kPositionFamily, "position_family");
  KCHESS_NODE_KIND_CASE(kPawnStructure, "pawn_structure");
  KCHESS_NODE_KIND_CASE(kMiddlegameStructure, "middlegame_structure");
  KCHESS_NODE_KIND_CASE(kEndgameType, "endgame_type");
  KCHESS_NODE_KIND_CASE(kMaterialConfiguration, "material_configuration");
  KCHESS_NODE_KIND_CASE(kPieceConfiguration, "piece_configuration");
  KCHESS_NODE_KIND_CASE(kTacticalMotif, "tactical_motif");
  KCHESS_NODE_KIND_CASE(kStrategicMotif, "strategic_motif");
  KCHESS_NODE_KIND_CASE(kKingSafetyPattern, "king_safety_pattern");
  KCHESS_NODE_KIND_CASE(kExchangePattern, "exchange_pattern");
  KCHESS_NODE_KIND_CASE(kPlanPattern, "plan_pattern");
  KCHESS_NODE_KIND_CASE(kTransitionPattern, "transition_pattern");
  KCHESS_NODE_KIND_CASE(kStrength, "strength");
  KCHESS_NODE_KIND_CASE(kWeakness, "weakness");
  KCHESS_NODE_KIND_CASE(kHabit, "habit");
  KCHESS_NODE_KIND_CASE(kBehavior, "behavior");
  KCHESS_NODE_KIND_CASE(kStylePattern, "style_pattern");
  KCHESS_NODE_KIND_CASE(kResultPattern, "result_pattern");
  KCHESS_NODE_KIND_CASE(kTimeManagementPattern, "time_management_pattern");
  KCHESS_NODE_KIND_CASE(kComplexityPattern, "complexity_pattern");
  KCHESS_NODE_KIND_CASE(kConversionPattern, "conversion_pattern");
  KCHESS_NODE_KIND_CASE(kDefensePattern, "defense_pattern");
  KCHESS_NODE_KIND_CASE(kRecoveryPattern, "recovery_pattern");
  KCHESS_NODE_KIND_CASE(kRepertoirePattern, "repertoire_pattern");
  KCHESS_NODE_KIND_CASE(kTrend, "trend");
  KCHESS_NODE_KIND_CASE(kHypothesis, "hypothesis");
  KCHESS_NODE_KIND_CASE(kKnowledgeGap, "knowledge_gap");
  KCHESS_NODE_KIND_CASE(kRatingPeriod, "rating_period");
  KCHESS_NODE_KIND_CASE(kTimeControl, "time_control");
  KCHESS_NODE_KIND_CASE(kOpponentStrengthBand, "opponent_strength_band");
  KCHESS_NODE_KIND_CASE(kDatePeriod, "date_period");
  KCHESS_NODE_KIND_CASE(kColor, "color");
  KCHESS_NODE_KIND_CASE(kResultType, "result_type");
  KCHESS_NODE_KIND_CASE(kTerminationType, "termination_type");
#undef KCHESS_NODE_KIND_CASE
  return std::nullopt;
}

std::optional<KnowledgeEdgeKind> knowledge_edge_kind_from_string(
    std::string_view value) noexcept {
#define KCHESS_EDGE_KIND_CASE(enum_value, text_value) \
  if (value == text_value) return KnowledgeEdgeKind::enum_value
  KCHESS_EDGE_KIND_CASE(kPlays, "plays");
  KCHESS_EDGE_KIND_CASE(kReaches, "reaches");
  KCHESS_EDGE_KIND_CASE(kTranspositionOf, "transposition_of");
  KCHESS_EDGE_KIND_CASE(kHasStructure, "has_structure");
  KCHESS_EDGE_KIND_CASE(kHasMotif, "has_motif");
  KCHESS_EDGE_KIND_CASE(kTransitionsTo, "transitions_to");
  KCHESS_EDGE_KIND_CASE(kOftenTransitionsTo, "often_transitions_to");
  KCHESS_EDGE_KIND_CASE(kLeadsTo, "leads_to");
  KCHESS_EDGE_KIND_CASE(kOftenLeadsTo, "often_leads_to");
  KCHESS_EDGE_KIND_CASE(kPrecedes, "precedes");
  KCHESS_EDGE_KIND_CASE(kFollows, "follows");
  KCHESS_EDGE_KIND_CASE(kContains, "contains");
  KCHESS_EDGE_KIND_CASE(kSimilarTo, "similar_to");
  KCHESS_EDGE_KIND_CASE(kStrongIn, "strong_in");
  KCHESS_EDGE_KIND_CASE(kWeakIn, "weak_in");
  KCHESS_EDGE_KIND_CASE(kImprovingIn, "improving_in");
  KCHESS_EDGE_KIND_CASE(kDecliningIn, "declining_in");
  KCHESS_EDGE_KIND_CASE(kCorrelatedWith, "correlated_with");
  KCHESS_EDGE_KIND_CASE(kContributesTo, "contributes_to");
  KCHESS_EDGE_KIND_CASE(kSupportedBy, "supported_by");
  KCHESS_EDGE_KIND_CASE(kContradictedBy, "contradicted_by");
  KCHESS_EDGE_KIND_CASE(kDerivedFrom, "derived_from");
  KCHESS_EDGE_KIND_CASE(kExplainedBy, "explained_by");
  KCHESS_EDGE_KIND_CASE(kHasStatistics, "has_statistics");
  KCHESS_EDGE_KIND_CASE(kLostBy, "lost_by");
  KCHESS_EDGE_KIND_CASE(kWonBy, "won_by");
  KCHESS_EDGE_KIND_CASE(kSavedFrom, "saved_from");
  KCHESS_EDGE_KIND_CASE(kMissedWin, "missed_win");
  KCHESS_EDGE_KIND_CASE(kMissedDraw, "missed_draw");
  KCHESS_EDGE_KIND_CASE(kCommonIn, "common_in");
  KCHESS_EDGE_KIND_CASE(kRareIn, "rare_in");
  KCHESS_EDGE_KIND_CASE(kUnderrepresentedIn, "underrepresented_in");
  KCHESS_EDGE_KIND_CASE(kDependsOn, "depends_on");
#undef KCHESS_EDGE_KIND_CASE
  return std::nullopt;
}

std::optional<KnowledgeAssertionKind> knowledge_assertion_kind_from_string(
    std::string_view value) noexcept {
  if (value == "fact") return KnowledgeAssertionKind::kFact;
  if (value == "observation") return KnowledgeAssertionKind::kObservation;
  if (value == "hypothesis") return KnowledgeAssertionKind::kHypothesis;
  return std::nullopt;
}

// -----------------------------------------------------------------------------
// Section: Contract validation
// -----------------------------------------------------------------------------

bool is_valid_contract_node(const KnowledgeNode& node) noexcept {
  return !node.id.empty() && node.kind != KnowledgeNodeKind::kUnknown &&
         node.schema_version > 0 &&
         node.schema_version <= kKnowledgeGraphSchemaVersion;
}

bool is_valid_contract_edge(const KnowledgeEdge& edge) noexcept {
  return !edge.id.empty() && !edge.from.empty() && !edge.to.empty() &&
         edge.kind != KnowledgeEdgeKind::kUnknown && edge.schema_version > 0 &&
         edge.schema_version <= kKnowledgeGraphSchemaVersion;
}

}  // namespace kchess::knowledge
