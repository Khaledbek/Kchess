#include "provider_input_optimizer.h"

#include <algorithm>
#include <string_view>

namespace kchess::ai {
namespace {

// -----------------------------------------------------------------------------
// Section: Evidence priority and trimming
// -----------------------------------------------------------------------------

int priority(EvidenceKind kind) {
  switch (kind) {
    case EvidenceKind::candidate_moves:
      return -1;
    case EvidenceKind::engine:
    case EvidenceKind::existing_analysis:
      return 0;
    case EvidenceKind::position_features:
    case EvidenceKind::tactical_motifs:
    case EvidenceKind::position_weaknesses:
    case EvidenceKind::strategic_plans:
      return 1;
    case EvidenceKind::opening:
    case EvidenceKind::theory:
    case EvidenceKind::user_profile:
    case EvidenceKind::practicality:
      return 2;
    default:
      return 3;
  }
}

std::size_t evidence_budget(ResponseDepth depth, std::size_t context_tokens) {
  const std::size_t base = depth == ResponseDepth::concise
                               ? 500
                               : depth == ResponseDepth::detailed ? 1400 : 900;
  return context_tokens > 2200 ? std::max<std::size_t>(300, base / 2) : base;
}

std::size_t estimated_tokens(std::string_view text) {
  return (text.size() + 3) / 4;
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Provider evidence selection
// -----------------------------------------------------------------------------

std::vector<EvidenceItem> optimize_provider_evidence(
    const std::vector<EvidenceItem>& evidence,
    ResponseDepth depth,
    std::size_t context_tokens) {
  std::vector<const EvidenceItem*> ordered;
  ordered.reserve(evidence.size());
  for (const auto& item : evidence) {
    if (!item.payload.empty()) ordered.push_back(&item);
  }
  std::stable_sort(ordered.begin(), ordered.end(), [](const auto* a, const auto* b) {
    return priority(a->kind) < priority(b->kind);
  });

  const std::size_t budget = evidence_budget(depth, context_tokens);
  std::size_t used = 0;
  std::vector<EvidenceItem> output;
  for (const auto* item : ordered) {
    const std::size_t cost = estimated_tokens(item->payload) + 12;
    if (!output.empty() && used + cost > budget) continue;
    output.push_back(*item);
    used += cost;
    if (used >= budget) break;
  }
  return output;
}

}  // namespace kchess::ai
