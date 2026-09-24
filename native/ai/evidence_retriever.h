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
#include "experts/existing_analysis_expert.h"
#include "practicality/player_practicality.h"

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Existing-system source hooks
// -----------------------------------------------------------------------------

using EvidenceSourceHook = std::function<std::optional<EvidenceItem>(
    const CoachRequest&, const QueryPlan&)>;
using ProfileEvidenceSource = std::function<std::optional<EvidenceItem>(
    const CoachRequest&, const QueryPlan&)>;

struct EngineCandidateBundle {
  EvidenceItem engine_evidence;
  CandidateMoveSnapshot candidate_snapshot;
};

using EngineCandidateSource = std::function<std::optional<EngineCandidateBundle>(
    const CoachRequest&, const QueryPlan&)>;
using CandidateSnapshotSource = std::function<std::optional<CandidateMoveSnapshot>(
    const CoachRequest&, const QueryPlan&)>;
using CompletedMoveAnalysisSource = std::function<std::optional<EvidenceItem>(
    const CoachRequest&, const std::string& original_fen)>;
using PracticalityPlayerSource =
    std::function<std::optional<PracticalityPlayerContext>(
        const CoachRequest&, const QueryPlan&)>;

struct EvidenceSources {
  EvidenceSourceHook cache;
  EvidenceSourceHook existing_analysis;
  CandidateSnapshotSource existing_candidates;
  CompletedMoveAnalysisSource completed_move_analysis;
  EvidenceSourceHook theory;
  EvidenceSourceHook opening;
  ProfileEvidenceSource user_profile;
  PracticalityPlayerSource practicality_player;
  EvidenceSourceHook engine;
  EngineCandidateSource engine_candidates;
};

// -----------------------------------------------------------------------------
// Section: Retrieval result
// -----------------------------------------------------------------------------

struct RetrievedEvidence {
  std::vector<EvidenceItem> items;
  std::optional<CandidateMoveSet> candidate_moves;
  std::optional<PracticalityPlayerContext> practicality_player;
  ExistingAnalysisCoverage existing_analysis_coverage;
  EngineBudgetDecision engine_budget;
};

class EvidenceRetriever {
 public:
  explicit EvidenceRetriever(EvidenceSources sources = {});

  [[nodiscard]] RetrievedEvidence retrieve(
      const CoachRequest& request,
      const QueryPlan& plan,
      const CoachContext& context) const;
  [[nodiscard]] std::optional<EvidenceItem> completed_move_analysis(
      const CoachRequest& request, const std::string& original_fen) const;

 private:
  EvidenceSources sources_;
};

}  // namespace kchess::ai
