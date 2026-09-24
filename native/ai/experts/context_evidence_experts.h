#pragma once

#include <span>
#include <vector>

#include "dto/evidence.h"
#include "dto/evidence_plan.h"

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Common expert evidence envelope
// -----------------------------------------------------------------------------

// Normalized bridge between existing KChess evidence streams and the deterministic
// evidence aggregation pipeline. The wrapped EvidenceItem remains authoritative;
// this envelope records which expert owns it and which evidence needs it satisfies.
struct ExpertEvidence {
  EvidenceSource source{EvidenceSource::position_features};
  EvidenceItem item;
  std::vector<EvidenceNeed> satisfies;
  bool objective_truth{false};
};

class PositionContextExpert {
 public:
  [[nodiscard]] std::vector<ExpertEvidence> collect(
      std::span<const EvidenceItem> evidence) const;
};

class TacticalContextExpert {
 public:
  [[nodiscard]] std::vector<ExpertEvidence> collect(
      std::span<const EvidenceItem> evidence) const;
};

class OpeningContextExpert {
 public:
  [[nodiscard]] std::vector<ExpertEvidence> collect(
      std::span<const EvidenceItem> evidence) const;
};

class ProfileContextExpert {
 public:
  [[nodiscard]] std::vector<ExpertEvidence> collect(
      std::span<const EvidenceItem> evidence) const;
};

// Normalizes every already-produced KChess evidence item into the common
// expert envelope without executing any source. This is the bridge used by the
// final evidence aggregator before provider-context selection.
[[nodiscard]] std::vector<ExpertEvidence> normalize_expert_evidence(
    std::span<const EvidenceItem> evidence);

}  // namespace kchess::ai
