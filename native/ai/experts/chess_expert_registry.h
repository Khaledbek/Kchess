#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "dto/evidence_plan.h"

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Chess expert registry contract
// -----------------------------------------------------------------------------

// The registry describes available categories of chess intelligence. It does
// not execute them. Runtime adapters remain owned by the existing KChess
// services and are bound by later evidence-aggregation updates.
enum class ChessExpertExecutionClass : std::uint8_t {
  persisted = 0,
  deterministic_local = 1,
  learned_local = 2,
  engine = 3,
};

struct ChessExpertDescriptor {
  EvidenceSource source{EvidenceSource::existing_analysis};
  std::string_view id;
  ChessExpertExecutionClass execution_class{
      ChessExpertExecutionClass::deterministic_local};
  bool requires_position{false};
  bool requires_profile{false};
  bool requires_conversation{false};
  std::span<const EvidenceNeed> supported_needs;
};

// Returns the canonical descriptor for a planner-visible source. Every value
// in EvidenceSource must have exactly one descriptor.
[[nodiscard]] const ChessExpertDescriptor& chess_expert_descriptor(
    EvidenceSource source);

// Stable complete registry used by diagnostics, aggregation and model-contract
// checks. Descriptor order follows EvidenceSource declaration order.
[[nodiscard]] std::span<const ChessExpertDescriptor> chess_expert_registry();

[[nodiscard]] bool chess_expert_supports(EvidenceSource source,
                                         EvidenceNeed need);

// Returns all registered sources capable of satisfying a need. This is a
// capability lookup only; it does not decide which expert should run.
[[nodiscard]] std::vector<EvidenceSource> chess_experts_for_need(
    EvidenceNeed need);

[[nodiscard]] std::string_view evidence_source_id(EvidenceSource source);

}  // namespace kchess::ai
