#pragma once

#include <functional>
#include <optional>
#include <vector>

#include "candidate_move_system.h"
#include "context_builder.h"
#include "dto/coach_request.h"
#include "dto/evidence.h"
#include "dto/query_plan.h"
#include "engine_budget.h"
#include "position/position_features.h"
#include "practicality/player_practicality.h"

namespace kchess::ai {

class EmbeddingModel;

// -----------------------------------------------------------------------------
// Section: Existing-system source hooks
// -----------------------------------------------------------------------------

using EvidenceSource = std::function<std::optional<EvidenceItem>(
    const CoachRequest&, const QueryPlan&)>;

struct EngineCandidateBundle {
  EvidenceItem engine_evidence;
  CandidateMoveSnapshot candidate_snapshot;
};

using EngineCandidateSource = std::function<std::optional<EngineCandidateBundle>(
    const CoachRequest&, const QueryPlan&)>;
using CandidateSnapshotSource = std::function<std::optional<CandidateMoveSnapshot>(
    const CoachRequest&, const QueryPlan&)>;
using PracticalityPlayerSource =
    std::function<std::optional<PracticalityPlayerContext>(
        const CoachRequest&, const QueryPlan&)>;

struct EvidenceSources {
  EvidenceSource cache;
  EvidenceSource existing_analysis;
  CandidateSnapshotSource existing_candidates;
  EvidenceSource theory;
  EvidenceSource opening;
  EvidenceSource user_profile;
  PracticalityPlayerSource practicality_player;
  EvidenceSource engine;
  EngineCandidateSource engine_candidates;
};

// -----------------------------------------------------------------------------
// Section: Retrieval result
// -----------------------------------------------------------------------------

struct RetrievedEvidence {
  std::vector<EvidenceItem> items;
  std::optional<PositionFeatures> position_features;
  std::optional<CandidateMoveSet> candidate_moves;
  std::optional<PracticalityPlayerContext> practicality_player;
  EngineBudgetDecision engine_budget;
};

class EvidenceRetriever {
 public:
  explicit EvidenceRetriever(EvidenceSources sources = {});

  [[nodiscard]] RetrievedEvidence retrieve(
      const CoachRequest& request,
      const QueryPlan& plan,
      const CoachContext& context,
      const EmbeddingModel* embeddings = nullptr) const;

 private:
  EvidenceSources sources_;
};

}  // namespace kchess::ai
