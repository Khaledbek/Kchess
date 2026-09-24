#include "experts/existing_analysis_expert.h"

#include <algorithm>

namespace kchess::ai {
namespace {

template <typename T>
void add_unique(std::vector<T>& values, T value) {
  if (std::find(values.begin(), values.end(), value) == values.end()) {
    values.push_back(value);
  }
}

const CandidateMove* primary_candidate(const CandidateMoveSet& candidates) {
  if (candidates.focus.has_value()) return &*candidates.focus;
  if (candidates.best.has_value()) return &*candidates.best;
  return nullptr;
}

std::vector<const CandidateMove*> all_candidates(const CandidateMoveSet& set) {
  std::vector<const CandidateMove*> result;
  if (set.best) result.push_back(&*set.best);
  if (set.focus && (!set.best || set.focus->move_uci != set.best->move_uci)) {
    result.push_back(&*set.focus);
  }
  for (const auto& candidate : set.alternatives) {
    result.push_back(&candidate);
  }
  if (set.user_move) {
    const bool duplicate = std::any_of(
        result.begin(), result.end(), [&](const CandidateMove* candidate) {
          return candidate->move_uci == set.user_move->move_uci;
        });
    if (!duplicate) result.push_back(&*set.user_move);
  }
  return result;
}

bool has_evaluation(const CandidateMove& candidate) {
  return candidate.evaluation_cp.has_value() || candidate.mate_in.has_value() ||
         candidate.expected_score.has_value();
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Coverage inspection
// -----------------------------------------------------------------------------

bool ExistingAnalysisCoverage::satisfies(EvidenceNeed need) const {
  return std::find(satisfied_needs.begin(), satisfied_needs.end(), need) !=
         satisfied_needs.end();
}

ExistingAnalysisCoverage ExistingAnalysisExpert::inspect(
    const QueryPlan& plan,
    const std::vector<EvidenceItem>& items,
    const std::optional<CandidateMoveSet>& candidates) const {
  ExistingAnalysisCoverage coverage;
  coverage.has_persisted_analysis = std::any_of(
      items.begin(), items.end(), [](const EvidenceItem& item) {
        return item.kind == EvidenceKind::existing_analysis ||
               item.kind == EvidenceKind::cache;
      });

  if (!candidates.has_value()) return coverage;
  coverage.has_candidate_set = true;

  const auto all = all_candidates(*candidates);
  const auto* primary = primary_candidate(*candidates);
  if (primary == nullptr || all.empty()) return coverage;

  add_unique(coverage.satisfied_needs, EvidenceNeed::candidate_moves);

  const bool every_candidate_evaluated =
      std::all_of(all.begin(), all.end(), [](const CandidateMove* candidate) {
        return has_evaluation(*candidate);
      });
  if (every_candidate_evaluated) {
    add_unique(coverage.satisfied_needs, EvidenceNeed::move_evaluations);
  }

  if (candidates->critical_reply_uci.has_value()) {
    add_unique(coverage.satisfied_needs, EvidenceNeed::best_reply);
  }

  if (!primary->pv_uci.empty()) {
    add_unique(coverage.satisfied_needs, EvidenceNeed::principal_variation);
  }

  if (primary->mate_in.has_value()) {
    add_unique(coverage.satisfied_needs, EvidenceNeed::forced_mate);
  }

  if (all.size() >= 2 && every_candidate_evaluated) {
    add_unique(coverage.satisfied_needs, EvidenceNeed::move_comparison);
  }

  // A normal top-N cached MultiPV can never prove that omitted legal root
  // moves are absent, and therefore never satisfies legal_moves. Likewise,
  // centipawn/expected-score deltas are not a material balance proof, so
  // material_consequences deliberately remains unsatisfied here.
  (void)plan;
  return coverage;
}

}  // namespace kchess::ai
