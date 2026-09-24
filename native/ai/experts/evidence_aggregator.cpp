#include "experts/evidence_aggregator.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

namespace kchess::ai {
namespace {

// -----------------------------------------------------------------------------
// Section: Deterministic matching / ranking helpers
// -----------------------------------------------------------------------------

template <typename T>
bool contains(std::span<const T> values, T value) {
  return std::find(values.begin(), values.end(), value) != values.end();
}

template <typename T>
void add_unique(std::vector<T>& values, T value) {
  if (std::find(values.begin(), values.end(), value) == values.end()) {
    values.push_back(value);
  }
}

bool source_requested(const EvidencePlan& plan, EvidenceSource source) {
  return std::find(plan.sources.begin(), plan.sources.end(), source) !=
         plan.sources.end();
}

std::vector<EvidenceNeed> matched_needs(const EvidencePlan& plan,
                                        const ExpertEvidence& evidence) {
  std::vector<EvidenceNeed> result;
  for (const auto need : evidence.satisfies) {
    if (std::find(plan.needs.begin(), plan.needs.end(), need) !=
        plan.needs.end()) {
      add_unique(result, need);
    }
  }
  return result;
}

double score_evidence(const EvidencePlan& plan, const ExpertEvidence& evidence,
                      std::span<const EvidenceNeed> matches) {
  const double confidence = std::clamp(evidence.item.confidence, 0.0, 1.0);
  double score = confidence;
  score += static_cast<double>(matches.size()) * 2.0;
  if (source_requested(plan, evidence.source)) score += 2.5;
  if (evidence.objective_truth) score += 1.5;
  return score;
}

bool eligible(const EvidencePlan& plan, const ExpertEvidence& evidence,
              std::span<const EvidenceNeed> matches) {
  if (plan.needs.empty()) return source_requested(plan, evidence.source);
  if (!matches.empty()) return true;
  return plan.allow_optional_sources && source_requested(plan, evidence.source);
}

std::string identity_key(const ExpertEvidence& evidence) {
  if (!evidence.item.id.empty()) return evidence.item.id;
  return std::to_string(static_cast<int>(evidence.item.kind)) + ":" +
         evidence.item.payload;
}

bool same_payload(const ExpertEvidence& left, const ExpertEvidence& right) {
  return left.item.kind == right.item.kind &&
         left.item.payload == right.item.payload;
}

bool prefer_left(const AggregatedEvidenceItem& left,
                 const AggregatedEvidenceItem& right) {
  if (left.evidence.objective_truth != right.evidence.objective_truth) {
    return left.evidence.objective_truth;
  }
  if (left.relevance_score != right.relevance_score) {
    return left.relevance_score > right.relevance_score;
  }
  if (left.evidence.item.confidence != right.evidence.item.confidence) {
    return left.evidence.item.confidence > right.evidence.item.confidence;
  }
  return static_cast<int>(left.evidence.source) <
         static_cast<int>(right.evidence.source);
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Evidence aggregation
// -----------------------------------------------------------------------------

EvidenceAggregationResult EvidenceAggregator::aggregate(
    const EvidencePlan& plan, std::span<const ExpertEvidence> evidence,
    const std::size_t max_items) const {
  EvidenceAggregationResult result;

  for (const auto& item : evidence) {
    auto matches = matched_needs(plan, item);
    if (!eligible(plan, item, matches)) continue;

    AggregatedEvidenceItem candidate{
        .evidence = item,
        .matched_needs = std::move(matches),
        .relevance_score = 0.0,
    };
    candidate.relevance_score =
        score_evidence(plan, candidate.evidence, candidate.matched_needs);

    const auto key = identity_key(candidate.evidence);
    const auto existing = std::find_if(
        result.items.begin(), result.items.end(),
        [&](const AggregatedEvidenceItem& current) {
          return identity_key(current.evidence) == key;
        });

    if (existing == result.items.end()) {
      result.items.push_back(std::move(candidate));
      continue;
    }

    if (same_payload(existing->evidence, candidate.evidence)) {
      ++result.duplicates_removed;
    } else {
      ++result.conflicts_resolved;
      add_unique(result.conflicting_ids, key);
    }

    if (!prefer_left(*existing, candidate)) {
      *existing = std::move(candidate);
    }
  }

  std::stable_sort(result.items.begin(), result.items.end(),
                   [](const AggregatedEvidenceItem& left,
                      const AggregatedEvidenceItem& right) {
                     if (left.relevance_score != right.relevance_score) {
                       return left.relevance_score > right.relevance_score;
                     }
                     if (left.evidence.objective_truth !=
                         right.evidence.objective_truth) {
                       return left.evidence.objective_truth;
                     }
                     if (left.evidence.item.confidence !=
                         right.evidence.item.confidence) {
                       return left.evidence.item.confidence >
                              right.evidence.item.confidence;
                     }
                     return left.evidence.item.id < right.evidence.item.id;
                   });

  if (max_items > 0 && result.items.size() > max_items) {
    // Preserve coverage before spending the remaining provider-context budget
    // on globally high-scoring evidence. A pure top-N truncation can otherwise
    // drop the only item satisfying a low-frequency need even though a second
    // near-duplicate item for a common need survives.
    std::vector<bool> selected(result.items.size(), false);
    std::size_t selected_count = 0;

    for (const auto need : plan.needs) {
      if (selected_count >= max_items) break;
      for (std::size_t i = 0; i < result.items.size(); ++i) {
        if (selected[i]) continue;
        if (contains<EvidenceNeed>(result.items[i].matched_needs, need)) {
          selected[i] = true;
          ++selected_count;
          ++result.coverage_reserved;
          break;
        }
      }
    }

    for (std::size_t i = 0; i < result.items.size() &&
                            selected_count < max_items;
         ++i) {
      if (selected[i]) continue;
      selected[i] = true;
      ++selected_count;
    }

    std::vector<AggregatedEvidenceItem> compacted;
    compacted.reserve(max_items);
    for (std::size_t i = 0; i < result.items.size(); ++i) {
      if (selected[i]) compacted.push_back(std::move(result.items[i]));
    }
    result.items_dropped_by_budget = result.items.size() - compacted.size();
    result.items = std::move(compacted);
  }

  for (const auto& item : result.items) {
    for (const auto need : item.matched_needs) {
      add_unique(result.satisfied_needs, need);
    }
  }
  for (const auto need : plan.needs) {
    if (std::find(result.satisfied_needs.begin(), result.satisfied_needs.end(),
                  need) == result.satisfied_needs.end()) {
      result.missing_needs.push_back(need);
    }
  }

  return result;
}

}  // namespace kchess::ai
