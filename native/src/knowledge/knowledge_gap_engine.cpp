#include "knowledge_gap_engine.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <string_view>
#include <unordered_set>

namespace kchess::knowledge {
namespace {

constexpr double kLowCoverageThreshold = 0.45;
constexpr double kLowConfidenceThreshold = 0.45;
constexpr double kStaleFreshnessThreshold = 0.35;
constexpr double kMinimumGapPriority = 0.30;

std::optional<std::string> text_property(const KnowledgeNode& node,
                                         std::string_view key) {
  const auto it = node.properties.find(std::string(key));
  if (it == node.properties.end()) return std::nullopt;
  if (const auto* value = std::get_if<std::string>(&it->second)) return *value;
  return std::nullopt;
}

bool player_scoped_to(const KnowledgeNode& node, std::string_view profile_id) {
  const auto owner = text_property(node, "profile_id");
  return owner && *owner == profile_id;
}

bool actionable_kind(const KnowledgeNodeKind kind) {
  switch (kind) {
    case KnowledgeNodeKind::kStatistic:
    case KnowledgeNodeKind::kOpeningFamily:
    case KnowledgeNodeKind::kOpening:
    case KnowledgeNodeKind::kVariation:
    case KnowledgeNodeKind::kPositionFamily:
    case KnowledgeNodeKind::kPawnStructure:
    case KnowledgeNodeKind::kMiddlegameStructure:
    case KnowledgeNodeKind::kEndgameType:
    case KnowledgeNodeKind::kTacticalMotif:
    case KnowledgeNodeKind::kStrategicMotif:
    case KnowledgeNodeKind::kKingSafetyPattern:
    case KnowledgeNodeKind::kExchangePattern:
    case KnowledgeNodeKind::kPlanPattern:
    case KnowledgeNodeKind::kTransitionPattern:
    case KnowledgeNodeKind::kStrength:
    case KnowledgeNodeKind::kWeakness:
    case KnowledgeNodeKind::kHabit:
    case KnowledgeNodeKind::kBehavior:
    case KnowledgeNodeKind::kStylePattern:
    case KnowledgeNodeKind::kResultPattern:
    case KnowledgeNodeKind::kTimeManagementPattern:
    case KnowledgeNodeKind::kComplexityPattern:
    case KnowledgeNodeKind::kConversionPattern:
    case KnowledgeNodeKind::kDefensePattern:
    case KnowledgeNodeKind::kRecoveryPattern:
    case KnowledgeNodeKind::kRepertoirePattern:
    case KnowledgeNodeKind::kTrend:
    case KnowledgeNodeKind::kHypothesis:
      return true;
    default:
      return false;
  }
}

std::string topic_for(const KnowledgeNode& node) {
  static constexpr std::string_view keys[] = {
      "topic", "opening_name", "opening_family", "metric", "phase",
      "pattern", "pattern_type", "time_control", "label", "subject_key"};
  for (const auto key : keys) {
    if (const auto value = text_property(node, key); value && !value->empty()) {
      return *value;
    }
  }
  return std::string(to_string(node.kind));
}

std::vector<std::string> source_games_for(DependencyTracker& dependencies,
                                          const KnowledgeEntryRef& entry) {
  std::set<std::string> ids;
  for (const auto& record : dependencies.provenance_for(entry)) {
    constexpr std::string_view prefix = "game:";
    if (record.source.source_id.starts_with(prefix) &&
        record.source.source_id.size() > prefix.size()) {
      ids.insert(record.source.source_id.substr(prefix.size()));
    }
  }
  return {ids.begin(), ids.end()};
}

bool conflict_is_actionable(const KnowledgeConflictRecord& conflict) {
  return conflict.resolution == KnowledgeConflictResolution::kUnresolved ||
      conflict.resolution == KnowledgeConflictResolution::kBalanced ||
      conflict.resolution == KnowledgeConflictResolution::kInsufficientEvidence;
}

bool contains_reason(const std::vector<KnowledgeGapReason>& reasons,
                     const KnowledgeGapReason reason) {
  return std::find(reasons.begin(), reasons.end(), reason) != reasons.end();
}

std::string joined_reasons(const std::vector<KnowledgeGapReason>& reasons) {
  std::string result;
  for (const auto reason : reasons) {
    if (!result.empty()) result += ',';
    result += to_string(reason);
  }
  return result;
}

void add_unique(std::vector<std::string>& values, const std::string& value) {
  if (value.empty()) return;
  if (std::find(values.begin(), values.end(), value) == values.end()) {
    values.push_back(value);
  }
}

}  // namespace

std::string_view to_string(const KnowledgeGapReason value) noexcept {
  switch (value) {
    case KnowledgeGapReason::kLowCoverage: return "low_coverage";
    case KnowledgeGapReason::kLowConfidence: return "low_confidence";
    case KnowledgeGapReason::kStaleEvidence: return "stale_evidence";
    case KnowledgeGapReason::kConflictingEvidence: return "conflicting_evidence";
  }
  return "unknown";
}

KnowledgeGapEngine::KnowledgeGapEngine(GraphStore& graph,
                                       DependencyTracker& dependencies,
                                       KnowledgeQualityStore& quality,
                                       ConflictResolver& conflicts)
    : graph_(graph),
      dependencies_(dependencies),
      quality_(quality),
      conflicts_(conflicts) {}

std::vector<KnowledgeGapRecord> KnowledgeGapEngine::refresh_profile_gaps(
    const std::string& profile_id, const std::int64_t evaluated_at_ms,
    const std::size_t max_entries, const std::size_t max_gaps) {
  if (profile_id.empty() || max_entries == 0 || max_gaps == 0) return {};

  std::unordered_set<std::string> stale_entries;
  for (const auto& invalidation : dependencies_.pending_invalidations(max_entries)) {
    if (invalidation.entry.kind == KnowledgeEntryKind::kNode) {
      stale_entries.insert(invalidation.entry.id);
    }
  }

  std::unordered_set<std::string> conflicted_nodes;
  for (const auto& conflict : conflicts_.conflicts_for_profile(profile_id, max_entries)) {
    if (!conflict_is_actionable(conflict)) continue;
    conflicted_nodes.insert(conflict.current_node_id.value);
    conflicted_nodes.insert(conflict.historical_node_id.value);
  }

  std::vector<KnowledgeGapRecord> gaps;
  const auto entries = quality_.entries(max_entries, 0);
  gaps.reserve(std::min<std::size_t>(entries.size(), max_gaps));

  for (const auto& entry : entries) {
    if (entry.kind != KnowledgeEntryKind::kNode) continue;
    const auto node = graph_.node(KnowledgeNodeId{entry.id});
    if (!node || node->kind == KnowledgeNodeKind::kKnowledgeGap ||
        !actionable_kind(node->kind) || !player_scoped_to(*node, profile_id)) {
      continue;
    }

    const auto metrics = quality_.quality(entry);
    if (!metrics) continue;

    std::vector<KnowledgeGapReason> reasons;
    if (metrics->coverage < kLowCoverageThreshold) {
      reasons.push_back(KnowledgeGapReason::kLowCoverage);
    }
    if (metrics->confidence < kLowConfidenceThreshold) {
      reasons.push_back(KnowledgeGapReason::kLowConfidence);
    }
    const double freshness = metrics->freshness.value_or(1.0);
    if ((metrics->freshness && freshness < kStaleFreshnessThreshold) ||
        stale_entries.contains(entry.id)) {
      reasons.push_back(KnowledgeGapReason::kStaleEvidence);
    }
    if (conflicted_nodes.contains(entry.id)) {
      reasons.push_back(KnowledgeGapReason::kConflictingEvidence);
    }
    if (reasons.empty()) continue;

    const double coverage_gap = 1.0 - std::clamp(metrics->coverage, 0.0, 1.0);
    const double confidence_gap = 1.0 - std::clamp(metrics->confidence, 0.0, 1.0);
    const double stale_gap = contains_reason(reasons, KnowledgeGapReason::kStaleEvidence)
        ? std::max(1.0 - std::clamp(freshness, 0.0, 1.0), 0.5)
        : 0.0;
    const double conflict_gap =
        contains_reason(reasons, KnowledgeGapReason::kConflictingEvidence) ? 1.0 : 0.0;
    const double priority = std::clamp(
        coverage_gap * 0.34 + confidence_gap * 0.22 + stale_gap * 0.14 +
            conflict_gap * 0.18 + std::clamp(metrics->importance, 0.0, 1.0) * 0.12,
        0.0, 1.0);
    if (priority < kMinimumGapPriority) continue;

    KnowledgeGapRecord gap;
    gap.target = entry;
    gap.target_kind = node->kind;
    gap.topic = topic_for(*node);
    gap.priority = priority;
    gap.coverage = metrics->coverage;
    gap.confidence = metrics->confidence;
    gap.freshness = freshness;
    gap.reasons = std::move(reasons);
    gap.source_game_ids = source_games_for(dependencies_, entry);
    gap.gap_node_id = make_knowledge_node_id(
        to_string(KnowledgeNodeKind::kKnowledgeGap),
        profile_id + "|node|" + entry.id);
    gaps.push_back(std::move(gap));
  }

  std::stable_sort(gaps.begin(), gaps.end(), [](const auto& left, const auto& right) {
    if (std::abs(left.priority - right.priority) > 1e-9) {
      return left.priority > right.priority;
    }
    return left.target.id < right.target.id;
  });
  if (gaps.size() > max_gaps) gaps.resize(max_gaps);

  std::unordered_set<std::string> desired_ids;
  desired_ids.reserve(gaps.size());

  KnowledgeNode player;
  player.id = make_knowledge_node_id(to_string(KnowledgeNodeKind::kPlayer), profile_id);
  if (const auto existing_player = graph_.node(player.id)) {
    player = *existing_player;
  } else {
    player.kind = KnowledgeNodeKind::kPlayer;
    player.assertion_kind = KnowledgeAssertionKind::kFact;
    player.properties = {{"profile_id", profile_id}};
    graph_.upsert_node(player);
  }

  for (const auto& gap : gaps) {
    desired_ids.insert(gap.gap_node_id.value);
    KnowledgeNode node;
    node.id = gap.gap_node_id;
    node.kind = KnowledgeNodeKind::kKnowledgeGap;
    node.assertion_kind = KnowledgeAssertionKind::kObservation;
    node.properties = {
        {"profile_id", profile_id},
        {"target_entry_kind", std::string(to_string(gap.target.kind))},
        {"target_entry_id", gap.target.id},
        {"target_node_kind", std::string(to_string(gap.target_kind))},
        {"topic", gap.topic},
        {"priority", gap.priority},
        {"coverage", gap.coverage},
        {"confidence", gap.confidence},
        {"freshness", gap.freshness},
        {"reasons", joined_reasons(gap.reasons)},
        {"source_game_count", static_cast<std::int64_t>(gap.source_game_ids.size())},
        {"evaluated_at_ms", evaluated_at_ms},
    };
    graph_.upsert_node(node);

    KnowledgeEdge contains;
    contains.from = player.id;
    contains.to = node.id;
    contains.kind = KnowledgeEdgeKind::kContains;
    contains.id = make_knowledge_edge_id(
        contains.from, to_string(contains.kind), contains.to, "knowledge_gap");
    contains.properties = {{"knowledge_gap", true}, {"priority", gap.priority}};
    graph_.upsert_edge(contains);

    KnowledgeEdge target;
    target.from = node.id;
    target.to = KnowledgeNodeId{gap.target.id};
    target.kind = KnowledgeEdgeKind::kDependsOn;
    target.id = make_knowledge_edge_id(
        target.from, to_string(target.kind), target.to, "gap_target");
    target.properties = {{"knowledge_gap", true}};
    graph_.upsert_edge(target);
  }

  const auto existing = graph_.find_nodes(KnowledgeNodeLookup{
      .kind = KnowledgeNodeKind::kKnowledgeGap,
      .text_properties = {{"profile_id", profile_id}},
      .case_insensitive_text = false,
      .limit = std::max<std::size_t>(max_gaps * 4, 128),
  });
  for (const auto& node : existing) {
    if (!desired_ids.contains(node.id.value)) graph_.remove_node(node.id);
  }

  return gaps;
}

KnowledgeGapActiveLearningPlan KnowledgeGapEngine::build_active_learning_plan(
    const std::vector<KnowledgeGapRecord>& gaps,
    const std::size_t max_priority_games) const {
  KnowledgeGapActiveLearningPlan plan;
  std::set<std::string> seen_games;

  for (const auto& gap : gaps) {
    if (gap.priority < kMinimumGapPriority) continue;
    for (const auto& game_id : gap.source_game_ids) {
      if (plan.priority_game_ids.size() >= max_priority_games) break;
      if (seen_games.insert(game_id).second) plan.priority_game_ids.push_back(game_id);
    }

    if (gap.target_kind == KnowledgeNodeKind::kOpening ||
        gap.target_kind == KnowledgeNodeKind::kOpeningFamily ||
        gap.target_kind == KnowledgeNodeKind::kVariation ||
        gap.target_kind == KnowledgeNodeKind::kRepertoirePattern) {
      add_unique(plan.priority_openings, gap.topic);
    }
    if (gap.target_kind == KnowledgeNodeKind::kEndgameType ||
        gap.target_kind == KnowledgeNodeKind::kConversionPattern ||
        gap.target_kind == KnowledgeNodeKind::kDefensePattern) {
      plan.prioritize_endgame_length = true;
    }
    if (gap.target_kind == KnowledgeNodeKind::kResultPattern ||
        gap.target_kind == KnowledgeNodeKind::kRecoveryPattern) {
      plan.prioritize_losses = true;
    }
  }
  return plan;
}

}  // namespace kchess::knowledge
