#include "experts/chess_expert_registry.h"

#include <algorithm>
#include <array>
#include <stdexcept>

namespace kchess::ai {
namespace {

// -----------------------------------------------------------------------------
// Section: Expert capabilities
// -----------------------------------------------------------------------------

constexpr std::array kExistingAnalysisNeeds{
    EvidenceNeed::candidate_moves,
    EvidenceNeed::move_evaluations,
    EvidenceNeed::best_reply,
    EvidenceNeed::principal_variation,
    EvidenceNeed::material_consequences,
    EvidenceNeed::forced_mate,
    EvidenceNeed::move_comparison,
};

constexpr std::array kEngineNeeds{
    EvidenceNeed::legal_moves,
    EvidenceNeed::candidate_moves,
    EvidenceNeed::move_evaluations,
    EvidenceNeed::best_reply,
    EvidenceNeed::principal_variation,
    EvidenceNeed::material_consequences,
    EvidenceNeed::forced_mate,
    EvidenceNeed::move_comparison,
};

constexpr std::array kHumanModelNeeds{
    EvidenceNeed::practical_candidates,
    EvidenceNeed::human_move_prediction,
    EvidenceNeed::best_reply,
};

constexpr std::array kPositionFeatureNeeds{
    EvidenceNeed::piece_activity,
    EvidenceNeed::controlled_squares,
    EvidenceNeed::position_structure,
    EvidenceNeed::strategic_plans,
};

constexpr std::array kTacticalNeeds{
    EvidenceNeed::threats,
    EvidenceNeed::tactical_motifs,
    EvidenceNeed::candidate_moves,
};

constexpr std::array kOpeningNeeds{
    EvidenceNeed::opening_context,
    EvidenceNeed::historical_examples,
    EvidenceNeed::practical_candidates,
};

constexpr std::array kProfileNeeds{
    EvidenceNeed::player_tendencies,
    EvidenceNeed::practical_candidates,
    EvidenceNeed::human_move_prediction,
};

constexpr std::array kHistoryNeeds{
    EvidenceNeed::historical_examples,
    EvidenceNeed::player_tendencies,
    EvidenceNeed::opening_context,
};

constexpr std::array kConversationNeeds{
    EvidenceNeed::conversation_reference,
};

constexpr std::array kConceptNeeds{
    EvidenceNeed::concept_explanation,
    EvidenceNeed::strategic_plans,
    EvidenceNeed::tactical_motifs,
    EvidenceNeed::position_structure,
};

constexpr std::array<ChessExpertDescriptor, 10> kExperts{{
    {
        .source = EvidenceSource::existing_analysis,
        .id = "existing_analysis",
        .execution_class = ChessExpertExecutionClass::persisted,
        .requires_position = true,
        .supported_needs = kExistingAnalysisNeeds,
    },
    {
        .source = EvidenceSource::engine,
        .id = "engine",
        .execution_class = ChessExpertExecutionClass::engine,
        .requires_position = true,
        .supported_needs = kEngineNeeds,
    },
    {
        .source = EvidenceSource::human_model,
        .id = "human_model",
        .execution_class = ChessExpertExecutionClass::learned_local,
        .requires_position = true,
        .supported_needs = kHumanModelNeeds,
    },
    {
        .source = EvidenceSource::position_features,
        .id = "position_features",
        .execution_class = ChessExpertExecutionClass::deterministic_local,
        .requires_position = true,
        .supported_needs = kPositionFeatureNeeds,
    },
    {
        .source = EvidenceSource::tactical_detector,
        .id = "tactical_detector",
        .execution_class = ChessExpertExecutionClass::deterministic_local,
        .requires_position = true,
        .supported_needs = kTacticalNeeds,
    },
    {
        .source = EvidenceSource::opening_knowledge,
        .id = "opening_knowledge",
        .execution_class = ChessExpertExecutionClass::persisted,
        .requires_position = true,
        .supported_needs = kOpeningNeeds,
    },
    {
        .source = EvidenceSource::player_profile,
        .id = "player_profile",
        .execution_class = ChessExpertExecutionClass::persisted,
        .requires_profile = true,
        .supported_needs = kProfileNeeds,
    },
    {
        .source = EvidenceSource::game_history,
        .id = "game_history",
        .execution_class = ChessExpertExecutionClass::persisted,
        .requires_profile = true,
        .supported_needs = kHistoryNeeds,
    },
    {
        .source = EvidenceSource::conversation,
        .id = "conversation",
        .execution_class = ChessExpertExecutionClass::deterministic_local,
        .requires_conversation = true,
        .supported_needs = kConversationNeeds,
    },
    {
        .source = EvidenceSource::chess_concepts,
        .id = "chess_concepts",
        .execution_class = ChessExpertExecutionClass::persisted,
        .supported_needs = kConceptNeeds,
    },
}};

constexpr std::size_t source_index(EvidenceSource source) {
  return static_cast<std::size_t>(source);
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Registry access
// -----------------------------------------------------------------------------

const ChessExpertDescriptor& chess_expert_descriptor(EvidenceSource source) {
  const auto index = source_index(source);
  if (index >= kExperts.size() || kExperts[index].source != source) {
    throw std::out_of_range("Unknown chess expert source");
  }
  return kExperts[index];
}

std::span<const ChessExpertDescriptor> chess_expert_registry() {
  return kExperts;
}

bool chess_expert_supports(EvidenceSource source, EvidenceNeed need) {
  const auto& descriptor = chess_expert_descriptor(source);
  return std::find(descriptor.supported_needs.begin(),
                   descriptor.supported_needs.end(),
                   need) != descriptor.supported_needs.end();
}

std::vector<EvidenceSource> chess_experts_for_need(EvidenceNeed need) {
  std::vector<EvidenceSource> result;
  for (const auto& descriptor : kExperts) {
    if (std::find(descriptor.supported_needs.begin(),
                  descriptor.supported_needs.end(),
                  need) != descriptor.supported_needs.end()) {
      result.push_back(descriptor.source);
    }
  }
  return result;
}

std::string_view evidence_source_id(EvidenceSource source) {
  return chess_expert_descriptor(source).id;
}

}  // namespace kchess::ai
