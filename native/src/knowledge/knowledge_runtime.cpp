#include "knowledge_runtime.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "../../ai/models/small_models.h"
#include "chunk_registry.h"
#include "conflict_resolver.h"
#include "dependency_tracker.h"
#include "evidence_packet_builder.h"
#include "graph_store.h"
#include "hybrid_retrieval.h"
#include "knowledge_quality_engine.h"
#include "knowledge_quality_store.h"
#include "opening_graph_projector.h"
#include "position_similarity.h"
#include "position_structure_graph_projector.h"
#include "profile_knowledge_graph_projector.h"
#include "query_router.h"
#include "query_trace_store.h"
#include "result_transition_graph_projector.h"
#include "retrieval_ranking.h"
#include "statistics_graph_projector.h"
#include "text_semantic_retrieval.h"
#include "vector_index.h"
#include "../persistence/database.h"
#include "../services/statistics_service.h"

namespace kchess::knowledge {
namespace {

using json = nlohmann::json;

std::int64_t now_ms() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

constexpr std::int64_t kFullKnowledgeMaintenanceIntervalMs = 6LL * 60 * 60 * 1000;

std::string hex64(std::uint64_t value) {
  std::ostringstream out;
  out << std::hex << std::setfill('0') << std::setw(16) << value;
  return out.str();
}

std::string stable_version(std::string_view value) {
  std::uint64_t hash = 1469598103934665603ULL;
  for (const unsigned char c : value) {
    hash ^= static_cast<std::uint64_t>(c);
    hash *= 1099511628211ULL;
  }
  return "fnv1a64:" + hex64(hash);
}

std::string property_text(const KnowledgePropertyValue& value) {
  if (const auto* item = std::get_if<bool>(&value)) return *item ? "true" : "false";
  if (const auto* item = std::get_if<std::int64_t>(&value)) return std::to_string(*item);
  if (const auto* item = std::get_if<double>(&value)) {
    std::ostringstream out;
    out << std::setprecision(6) << *item;
    return out.str();
  }
  if (const auto* item = std::get_if<std::string>(&value)) return *item;
  return {};
}

json property_json(const KnowledgePropertyValue& value) {
  if (const auto* item = std::get_if<bool>(&value)) return *item;
  if (const auto* item = std::get_if<std::int64_t>(&value)) return *item;
  if (const auto* item = std::get_if<double>(&value)) return *item;
  if (const auto* item = std::get_if<std::string>(&value)) return *item;
  return nullptr;
}

json properties_json(const KnowledgeProperties& properties) {
  json result = json::object();
  for (const auto& [key, value] : properties) result[key] = property_json(value);
  return result;
}

std::optional<std::string> text_property(const KnowledgeNode& node,
                                         std::string_view key) {
  const auto found = node.properties.find(std::string(key));
  if (found == node.properties.end()) return std::nullopt;
  if (const auto* value = std::get_if<std::string>(&found->second)) return *value;
  return std::nullopt;
}

std::string node_topic(const KnowledgeNode& node) {
  for (const auto key : {"name", "statistic_key", "pattern", "pattern_id",
                         "behavior", "state", "topic", "opening", "type",
                         "termination", "scope"}) {
    if (const auto value = text_property(node, key); value && !value->empty()) return *value;
  }
  return std::string(to_string(node.kind));
}

std::string compact_node_content(const KnowledgeNode& node) {
  std::ostringstream out;
  out << "kind=" << to_string(node.kind)
      << "; assertion=" << to_string(node.assertion_kind);
  std::size_t emitted = 0;
  for (const auto& [key, value] : node.properties) {
    if (emitted >= 18) break;
    const auto text = property_text(value);
    if (text.empty()) continue;
    out << "; " << key << '=' << text;
    ++emitted;
  }
  auto value = out.str();
  if (value.size() > 1800) value.resize(1800);
  return value;
}

std::vector<KnowledgeSourceRef> provenance_sources(
    DependencyTracker& dependencies, const KnowledgeNodeId& id) {
  std::vector<KnowledgeSourceRef> result;
  for (const auto& record : dependencies.provenance_for(
           {KnowledgeEntryKind::kNode, id.value})) {
    if (record.source.valid()) result.push_back(record.source);
  }
  return result;
}

std::string query_family_name(ai::QueryFamily family) {
  switch (family) {
    case ai::QueryFamily::personal_chess:
      return "personal_chess";
    case ai::QueryFamily::general_chess:
      return "general_chess";
    case ai::QueryFamily::position:
      return "position";
    case ai::QueryFamily::unknown:
      return "unknown";
  }
  return "unknown";
}

json requested_scope_json(const ai::ProfileQueryScope& scope) {
  return {
      {"topics", scope.topics},
      {"timeControls", scope.time_controls},
      {"phases", scope.phases},
      {"playerColors", scope.player_colors},
      {"needsEndgameMaterialType", scope.needs_endgame_material_type},
      {"wantsProof", scope.wants_proof},
      {"compareScopes", scope.compare_scopes},
  };
}

std::string entry_kind_name(KnowledgeEntryKind kind) {
  return std::string(to_string(kind));
}

json source_json(const KnowledgeSourceRef& source) {
  return {{"sourceType", source.source_type},
          {"sourceId", source.source_id},
          {"sourceVersion", source.source_version}};
}

std::string first_source_kind(const KnowledgeEvidencePacketItem& item) {
  return item.sources.empty() ? "knowledge_graph" : item.sources.front().source_type;
}

std::string epistemic_status(KnowledgePacketSection section) {
  switch (section) {
    case KnowledgePacketSection::kFact:
      return "observed";
    case KnowledgePacketSection::kObservation:
      return "derived";
    case KnowledgePacketSection::kEvidence:
      return "observed";
    case KnowledgePacketSection::kUncertainty:
      return "hypothesis";
  }
  return "derived";
}

json item_data(const KnowledgeEvidencePacketItem& item) {
  json data = properties_json(item.properties);
  if (data.empty()) data["summary"] = item.content;
  if (item.confidence) data["supportConfidence"] = *item.confidence;
  if (item.coverage) data["supportCoverage"] = *item.coverage;
  if (item.freshness) data["sourceFreshness"] = *item.freshness;
  data.erase("profile_id");
  data.erase("statistic_key");
  return data;
}

json item_scope(const KnowledgeEvidencePacketItem& item) {
  json scope = json::object();
  KnowledgeNode node;
  node.properties = item.properties;
  const auto copy = [&](std::string_view from, const char* to) {
    if (const auto value = text_property(node, from); value && !value->empty()) {
      scope[to] = *value;
    }
  };
  copy("time_control", "timeControl");
  copy("phase", "phase");
  copy("color", "playerColor");
  copy("player_color", "playerColor");
  copy("eco", "eco");
  copy("opening_eco", "eco");
  copy("opening", "opening");
  copy("opening_name", "opening");
  copy("topic", "topic");
  copy("temporal_scope", "temporalScope");
  return scope;
}

void append_packet_items(json& chunks,
                         const std::vector<KnowledgeEvidencePacketItem>& items,
                         KnowledgePacketSection section) {
  for (const auto& item : items) {
    chunks.push_back({
        {"nodeId", item.id},
        {"kind", item.topic},
        {"sourceKind", first_source_kind(item)},
        {"retrievalScore", item.retrieval_score},
        {"epistemicStatus", epistemic_status(section)},
        {"evidenceRole", section == KnowledgePacketSection::kEvidence
                              ? "supporting_evidence"
                              : "profile_observation"},
        {"scope", item_scope(item)},
        {"data", item_data(item)},
    });
  }
}

json required_chunk_groups(const KnowledgeEvidencePacket& packet,
                           const KnowledgeQueryRoute& route) {
  json groups = json::array();
  const auto group = [&](const auto& accepts) {
    json ids = json::array();
    for (const auto* section : {&packet.facts, &packet.observations, &packet.evidence})
      for (const auto& item : *section) if (accepts(item)) ids.push_back(item.id);
    if (std::find(groups.begin(), groups.end(), ids) == groups.end()) groups.push_back(std::move(ids));
  };
  const auto matching = [&](const KnowledgeQueryRoute& scoped) {
    group([&](const auto& item) {
      KnowledgeNode node;
      node.kind = item.node_kind;
      node.properties = item.properties;
      return knowledge_node_matches_scope(node, scoped);
    });
  };
  for (const auto& topic : route.profile_scope.topics) {
    auto scoped = route;
    scoped.profile_scope.topics = {topic};
    matching(scoped);
  }
  if (route.profile_scope.compare_scopes) {
    const auto values = [](const auto& axis) { return axis.empty() ? std::vector<std::string>{""} : axis; };
    for (const auto& tc : values(route.profile_scope.time_controls))
      for (const auto& color : values(route.profile_scope.player_colors))
        for (const auto& phase : values(route.profile_scope.phases)) {
          auto scoped = route;
          if (!tc.empty()) scoped.profile_scope.time_controls = {tc};
          if (!color.empty()) scoped.profile_scope.player_colors = {color};
          if (!phase.empty()) scoped.profile_scope.phases = {phase};
          matching(scoped);
        }
  }
  if (route.intent == KnowledgeQueryIntent::kRelationship || route.intent == KnowledgeQueryIntent::kCausalAnalysis)
    group([](const auto& item) { return item.topic == "relationship"; });
  if (route.intent == KnowledgeQueryIntent::kTrend)
    group([](const auto& item) { return item.node_kind == KnowledgeNodeKind::kTrend || item.properties.contains("trend"); });
  if (route.profile_scope.needs_endgame_material_type)
    group([](const auto& item) { return item.node_kind == KnowledgeNodeKind::kEndgameType || item.properties.contains("endgame_type"); });
  return groups;
}

std::string owner_profile_id(Database& database,
                             const std::optional<std::string>& requested) {
  if (requested && !requested->empty()) {
    return database.player_profile_owner_id(*requested);
  }
  const auto active = database.active_profile();
  if (!active) return {};
  return database.player_profile_owner_id(active->id);
}

std::vector<KnowledgeNode> player_graph_nodes(GraphStore& graph,
                                               const std::string& profile_id,
                                               std::size_t limit) {
  std::vector<KnowledgeNode> result;
  std::unordered_set<std::string> seen;
  auto append = [&](const KnowledgeNode& node) {
    if (result.size() >= limit || !seen.insert(node.id.value).second) return;
    result.push_back(node);
  };

  KnowledgeNodeLookup lookup;
  lookup.text_properties.emplace("profile_id", profile_id);
  lookup.limit = std::min<std::size_t>(limit, 1000);
  for (const auto& node : graph.find_nodes(lookup)) append(node);

  const auto player_id = make_knowledge_node_id(to_string(KnowledgeNodeKind::kPlayer), "profile:" + profile_id);
  if (const auto player = graph.node(player_id)) {
    append(*player);
    for (const auto& step : graph.traverse(player_id, 3, limit, GraphDirection::kBoth)) {
      append(step.node);
      if (result.size() >= limit) break;
    }
  }
  return result;
}

bool needs_profile_evidence(const ai::QueryPlan& plan) {
  if (!plan.needs_profile || plan.query_family == ai::QueryFamily::general_chess) {
    return false;
  }
  return std::find(plan.evidence.begin(), plan.evidence.end(),
                   ai::EvidenceKind::user_profile) != plan.evidence.end();
}

std::vector<HybridNodeCandidate> live_statistic_candidates(
    Database& database, StatisticsService& statistics, const std::string& owner,
    const KnowledgeQueryRoute& route) {
  std::int64_t since = 0;
  for (const auto& entity : route.entities) {
    if (entity.kind == KnowledgeQueryEntityKind::kTemporalScope &&
        entity.canonical_value == "recent") since = now_ms() / 1000 - 90LL * 86400;
  }
  const auto facts = json::parse(statistics.player_knowledge_json(
      owner, route.profile_scope.time_controls, route.profile_scope.player_colors, since));
  std::vector<HybridNodeCandidate> out;
  for (const auto& fact : facts) {
    KnowledgeNode node;
    node.kind = KnowledgeNodeKind::kStatistic;
    node.assertion_kind = KnowledgeAssertionKind::kFact;
    const auto key = fact.at("statistic_key").get<std::string>();
    node.id = make_knowledge_node_id(to_string(node.kind), "profile:" + owner + ":" + key);
    for (auto it = fact.begin(); it != fact.end(); ++it) {
      if (it.value().is_string()) node.properties[it.key()] = it.value().get<std::string>();
      else if (it.value().is_boolean()) node.properties[it.key()] = it.value().get<bool>();
      else if (it.value().is_number_integer()) node.properties[it.key()] = it.value().get<std::int64_t>();
      else if (it.value().is_number_float()) node.properties[it.key()] = it.value().get<double>();
    }
    if (!knowledge_node_matches_scope(node, route)) continue;
    HybridRetrievalSignals signals;
    signals.exact_statistics = true;
    out.push_back({std::move(node), signals,
        {{"statistics_service", "profile:" + owner + ":" + key, stable_version(fact.dump())}}});
  }
  for (const auto& progress : database.ai_coach_skill_progress(owner)) {
    KnowledgeNode node;
    node.kind = KnowledgeNodeKind::kStatistic;
    node.assertion_kind = KnowledgeAssertionKind::kFact;
    const auto key = "coach_learning:" + progress.motif_id;
    node.id = make_knowledge_node_id(to_string(node.kind), "profile:" + owner + ":" + key);
    const int attempts = progress.independent_successes + progress.other_attempts;
    node.properties = {{"profile_id", owner}, {"topic", std::string("training")},
        {"statistic_key", key}, {"motif_id", progress.motif_id},
        {"matchedCandidateAttempts", static_cast<std::int64_t>(progress.independent_successes)},
        {"verifiedWeakAttempts", static_cast<std::int64_t>(progress.other_attempts)},
        {"gradedAttempts", static_cast<std::int64_t>(attempts)},
        {"lastPracticedAt", progress.last_practiced_at},
        {"candidateMatchPosteriorMean", (progress.independent_successes + 1.0) / (attempts + 2.0)},
        {"measurement", std::string("graded_quiz_moves_not_independent_skill")}};
    if (!knowledge_node_matches_scope(node, route)) continue;
    HybridRetrievalSignals signals;
    signals.exact_statistics = true;
    out.push_back({std::move(node), signals,
        {{"coach_learning", "profile:" + owner + ":" + key,
          stable_version(key + ":" + std::to_string(progress.independent_successes) +
                         ":" + std::to_string(progress.other_attempts))}}});
  }
  return out;
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Shared runtime implementation
// -----------------------------------------------------------------------------

struct KnowledgeRuntime::Impl {
  explicit Impl(const std::filesystem::path& data_directory)
      : graph(data_directory),
        dependencies(data_directory),
        chunks(data_directory),
        quality(data_directory),
        vectors(data_directory),
        positions(graph, vectors),
        conflicts(data_directory, graph, quality),
        gaps(graph, dependencies, quality, conflicts),
        traces(data_directory) {}

  GraphStore graph;
  DependencyTracker dependencies;
  ChunkRegistry chunks;
  KnowledgeQualityStore quality;
  SqliteVectorIndex vectors;
  PositionSimilarityIndex positions;
  ConflictResolver conflicts;
  KnowledgeGapEngine gaps;
  QueryTraceStore traces;
  KnowledgeQueryRouter router;
  KnowledgeQueryPlanner planner;
  bool opened{false};
  std::string last_profile_id;
  std::int64_t last_refresh_ms{0};
  std::string last_full_maintenance_profile_id;
  std::int64_t last_full_maintenance_ms{0};
};

KnowledgeRuntime::KnowledgeRuntime(Database& database, StatisticsService& statistics,
                                   std::filesystem::path data_directory)
    : database_(database),
      statistics_(statistics),
      data_directory_(std::move(data_directory)),
      impl_(std::make_unique<Impl>(data_directory_)) {}

KnowledgeRuntime::~KnowledgeRuntime() { close(); }

void KnowledgeRuntime::open() {
  std::lock_guard lock(mutex_);
  if (impl_->opened) return;
  impl_->graph.open();
  impl_->dependencies.open();
  impl_->chunks.open();
  impl_->quality.open();
  impl_->vectors.open();
  impl_->conflicts.open();
  impl_->traces.open();
  impl_->opened = true;
}

void KnowledgeRuntime::close() noexcept {
  std::lock_guard lock(mutex_);
  if (!impl_) return;
  impl_->traces.close();
  impl_->conflicts.close();
  impl_->vectors.close();
  impl_->quality.close();
  impl_->chunks.close();
  impl_->dependencies.close();
  impl_->graph.close();
  impl_->opened = false;
}

bool KnowledgeRuntime::is_open() const noexcept {
  std::lock_guard lock(mutex_);
  return impl_ && impl_->opened;
}

void KnowledgeRuntime::set_activity(
    std::string operation, std::string phase, std::string profile_id,
    const std::size_t completed, const std::size_t total) {
  const auto timestamp = now_ms();
  std::lock_guard lock(activity_mutex_);
  const bool new_operation = !activity_.active || activity_.operation != operation ||
      activity_.profile_id != profile_id;
  if (new_operation) {
    activity_.started_at_ms = timestamp;
    activity_.phase_started_at_ms = timestamp;
    activity_.last_phase.clear();
    activity_.last_phase_duration_ms = 0;
  } else if (activity_.phase != phase) {
    activity_.last_phase = activity_.phase;
    activity_.last_phase_duration_ms = activity_.phase_started_at_ms > 0
        ? static_cast<std::uint64_t>(
              std::max<std::int64_t>(0, timestamp - activity_.phase_started_at_ms))
        : 0;
    activity_.phase_started_at_ms = timestamp;
  }
  activity_.active = true;
  activity_.operation = std::move(operation);
  activity_.phase = std::move(phase);
  activity_.profile_id = std::move(profile_id);
  activity_.completed = completed;
  activity_.total = total;
  activity_.updated_at_ms = timestamp;
}

void KnowledgeRuntime::clear_activity() noexcept {
  const auto timestamp = now_ms();
  std::lock_guard lock(activity_mutex_);
  if (activity_.active) {
    activity_.last_phase = activity_.phase;
    activity_.last_phase_duration_ms = activity_.phase_started_at_ms > 0
        ? static_cast<std::uint64_t>(
              std::max<std::int64_t>(0, timestamp - activity_.phase_started_at_ms))
        : 0;
  }
  activity_.active = false;
  activity_.operation = "idle";
  activity_.phase = "idle";
  activity_.profile_id.clear();
  activity_.completed = 0;
  activity_.total = 0;
  activity_.phase_started_at_ms = 0;
  activity_.updated_at_ms = timestamp;
}

std::string KnowledgeRuntime::activity_json() const {
  const auto timestamp = now_ms();
  std::lock_guard lock(activity_mutex_);
  const auto phase_detail = [](const std::string& operation,
                               const std::string& phase) -> std::string {
    if (operation == "knowledge_refresh") {
      if (phase == "statistics") return "project_statistics_graph";
      if (phase == "openings") return "project_opening_graph_from_persisted_games";
      if (phase == "position_structures") return "project_position_structure_graph";
      if (phase == "result_transitions") return "project_result_transition_graph";
      if (phase == "profile_knowledge") return "project_learned_profile_graph";
      if (phase == "quality") return "refresh_knowledge_quality_metadata";
      if (phase == "chunks") return "materialize_graph_node_chunks";
      if (phase == "chunk_cleanup") return "remove_stale_graph_node_chunks";
      if (phase == "conflicts") return "refresh_profile_conflicts";
      if (phase == "knowledge_gaps") return "refresh_profile_knowledge_gaps";
    }
    if (operation == "active_learning") {
      if (phase == "conflicts") return "scan_conflicts_for_learning_priorities";
      if (phase == "knowledge_gaps") return "scan_knowledge_gaps_for_learning_priorities";
      if (phase == "planning") return "build_bounded_learning_priority_plan";
    }
    return phase;
  };
  const auto elapsed = activity_.active && activity_.started_at_ms > 0
      ? std::max<std::int64_t>(0, timestamp - activity_.started_at_ms)
      : 0;
  const auto phase_elapsed = activity_.active && activity_.phase_started_at_ms > 0
      ? std::max<std::int64_t>(0, timestamp - activity_.phase_started_at_ms)
      : 0;
  return json({
      {"active", activity_.active},
      {"operation", activity_.operation},
      {"phase", activity_.phase},
      {"detail", phase_detail(activity_.operation, activity_.phase)},
      {"workClass", activity_.active ? "background_derived_knowledge" : "idle"},
      {"resourceUse", activity_.active
          ? json::array({"cpu", "sqlite_read", "sqlite_write"})
          : json::array()},
      {"holdsKnowledgeRuntimeLock", activity_.active},
      {"blocksInspectorGraphRead", activity_.active},
      {"foregroundAnalysisWritePriority", true},
      {"profileId", activity_.profile_id},
      {"completed", activity_.completed},
      {"total", activity_.total},
      {"progressKnown", activity_.total > 0},
      {"startedAtMs", activity_.started_at_ms},
      {"phaseStartedAtMs", activity_.phase_started_at_ms},
      {"elapsedMs", elapsed},
      {"phaseElapsedMs", phase_elapsed},
      {"lastPhase", activity_.last_phase},
      {"lastPhaseDurationMs", activity_.last_phase_duration_ms},
      {"updatedAtMs", activity_.updated_at_ms},
  }).dump();
}

std::string KnowledgeRuntime::performance_json() const {
  const auto refreshes = refresh_count_.load(std::memory_order_relaxed);
  const auto refresh_total = refresh_total_ms_.load(std::memory_order_relaxed);
  const auto coach_requests = coach_evidence_requests_.load(std::memory_order_relaxed);
  const auto coach_total = coach_total_ms_.load(std::memory_order_relaxed);
  return json({
      {"schema", "knowledge.performance.v1"},
      {"refresh", {
          {"count", refreshes},
          {"lastDurationMs", refresh_last_ms_.load(std::memory_order_relaxed)},
          {"totalDurationMs", refresh_total},
          {"averageDurationMs",
           refreshes > 0 ? static_cast<double>(refresh_total) / refreshes : 0.0},
          {"lastLockWaitMs", refresh_lock_wait_last_ms_.load(std::memory_order_relaxed)},
          {"totalLockWaitMs", refresh_lock_wait_total_ms_.load(std::memory_order_relaxed)},
          {"fullMaintenanceCount", refresh_full_maintenance_.load(std::memory_order_relaxed)},
          {"incrementalMaintenanceCount", refresh_incremental_maintenance_.load(std::memory_order_relaxed)},
          {"changedEntries", refresh_changed_entries_.load(std::memory_order_relaxed)},
          {"chunkNodesProcessed", refresh_chunk_nodes_processed_.load(std::memory_order_relaxed)},
          {"qualityEntriesProcessed", refresh_quality_entries_processed_.load(std::memory_order_relaxed)},
      }},
      {"coachEvidence", {
          {"requests", coach_requests},
          {"graphAvailable", coach_graph_available_.load(std::memory_order_relaxed)},
          {"graphBusyFallbacks", coach_graph_busy_fallbacks_.load(std::memory_order_relaxed)},
          {"lastDurationMs", coach_last_ms_.load(std::memory_order_relaxed)},
          {"totalDurationMs", coach_total},
          {"averageDurationMs",
           coach_requests > 0 ? static_cast<double>(coach_total) / coach_requests : 0.0},
      }},
  }).dump();
}

// -----------------------------------------------------------------------------
// Section: Graph refresh / semantic chunk materialization
// -----------------------------------------------------------------------------

KnowledgeRefreshReport KnowledgeRuntime::refresh_active_profile(
    const std::int64_t observed_at_ms) {
  const auto wait_started = std::chrono::steady_clock::now();
  std::unique_lock lock(mutex_);
  const auto lock_acquired = std::chrono::steady_clock::now();
  KnowledgeRefreshReport report;
  if (!impl_->opened) return report;
  const auto active = database_.active_profile();
  if (!active) return report;
  const auto lock_wait_ms = static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(lock_acquired - wait_started).count());
  const auto refresh_started = lock_acquired;
  refresh_count_.fetch_add(1, std::memory_order_relaxed);
  refresh_lock_wait_total_ms_.fetch_add(lock_wait_ms, std::memory_order_relaxed);
  refresh_lock_wait_last_ms_.store(lock_wait_ms, std::memory_order_relaxed);
  const auto finish_refresh_metrics = [&]() {
    const auto elapsed = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - refresh_started)
            .count());
    refresh_total_ms_.fetch_add(elapsed, std::memory_order_relaxed);
    refresh_last_ms_.store(elapsed, std::memory_order_relaxed);
  };
  const std::string owner = database_.player_profile_owner_id(active->id);
  report.profile_id = owner;
  report.refreshed_at_ms = observed_at_ms;
  set_activity("knowledge_refresh", "statistics", owner);

  try {
  const auto stats = StatisticsGraphProjector(
      database_, statistics_, impl_->graph, impl_->dependencies)
                         .project_active_profile(observed_at_ms);
  set_activity("knowledge_refresh", "openings", owner);
  const auto openings = OpeningGraphProjector(
      database_, impl_->graph, impl_->dependencies)
                            .project_active_profile(observed_at_ms);
  set_activity("knowledge_refresh", "position_structures", owner);
  const auto structures = PositionStructureGraphProjector(
      database_, impl_->graph, impl_->dependencies, &impl_->positions)
                              .project_active_profile(observed_at_ms);
  set_activity("knowledge_refresh", "result_transitions", owner);
  const auto transitions = ResultTransitionGraphProjector(
      database_, impl_->graph, impl_->dependencies)
                               .project_active_profile(observed_at_ms);
  set_activity("knowledge_refresh", "profile_knowledge", owner);
  const auto profile = ProfileKnowledgeGraphProjector(
      database_, impl_->graph, impl_->dependencies)
                           .project_active_profile(observed_at_ms);

  report.graph_nodes_upserted = stats.nodes_upserted + openings.nodes_upserted +
                                structures.nodes_upserted + transitions.nodes_upserted +
                                profile.nodes_upserted;
  report.graph_edges_upserted = stats.edges_upserted + openings.edges_upserted +
                                structures.edges_upserted + transitions.edges_upserted +
                                profile.edges_upserted;

  // Projection reports carry exactly the graph/dependency entries that changed.
  // Keep the expensive quality/chunk maintenance bounded to that set during
  // normal profile updates. A low-frequency full pass remains as a safety net
  // for freshness decay, legacy rows and out-of-band maintenance.
  std::unordered_map<std::string, KnowledgeEntryRef> dirty_entries;
  std::unordered_set<std::string> removed_entries;
  const auto entry_key = [](const KnowledgeEntryRef& entry) {
    return std::string(to_string(entry.kind)) + "\n" + entry.id;
  };
  const auto collect_projection = [&](const auto& projection) {
    for (const auto& entry : projection.changed_entries) {
      dirty_entries[entry_key(entry)] = entry;
    }
    for (const auto& invalidation : projection.invalidated_entries) {
      dirty_entries[entry_key(invalidation.entry)] = invalidation.entry;
    }
    for (const auto& entry : projection.removed_entries) {
      const auto key = entry_key(entry);
      removed_entries.insert(key);
      dirty_entries.erase(key);
    }
  };
  collect_projection(stats);
  collect_projection(openings);
  collect_projection(structures);
  collect_projection(transitions);
  collect_projection(profile);

  report.changed_entries = dirty_entries.size() + removed_entries.size();
  const bool profile_changed = impl_->last_full_maintenance_profile_id != owner;
  const bool clock_reset = impl_->last_full_maintenance_ms > observed_at_ms;
  const bool full_interval_elapsed = impl_->last_full_maintenance_ms == 0 ||
      observed_at_ms - impl_->last_full_maintenance_ms >=
          kFullKnowledgeMaintenanceIntervalMs;
  report.full_maintenance = profile_changed || clock_reset || full_interval_elapsed;
  if (report.full_maintenance) {
    refresh_full_maintenance_.fetch_add(1, std::memory_order_relaxed);
  } else {
    refresh_incremental_maintenance_.fetch_add(1, std::memory_order_relaxed);
  }
  refresh_changed_entries_.fetch_add(report.changed_entries, std::memory_order_relaxed);

  KnowledgeQualityEngine quality_engine(
      impl_->graph, impl_->dependencies, impl_->quality);
  if (report.full_maintenance) {
    set_activity("knowledge_refresh", "quality", owner, 0, 2500);
    for (std::size_t offset = 0; offset < 2500; offset += 500) {
      const auto refreshed = quality_engine.refresh_batch(observed_at_ms, 500, offset);
      report.quality_entries_refreshed += refreshed.size();
      set_activity(
          "knowledge_refresh", "quality", owner,
          report.quality_entries_refreshed, 2500);
      if (refreshed.size() < 500) break;
    }
  } else {
    std::size_t quality_index = 0;
    set_activity("knowledge_refresh", "quality", owner, 0, dirty_entries.size());
    for (const auto& [_, entry] : dirty_entries) {
      if (entry.kind == KnowledgeEntryKind::kChunk) continue;
      if (quality_engine.refresh(entry, observed_at_ms)) {
        ++report.quality_entries_refreshed;
      }
      ++quality_index;
      if (quality_index == dirty_entries.size() || quality_index % 64 == 0) {
        set_activity(
            "knowledge_refresh", "quality", owner, quality_index,
            dirty_entries.size());
      }
    }
  }
  refresh_quality_entries_processed_.fetch_add(
      report.quality_entries_refreshed, std::memory_order_relaxed);

  std::vector<KnowledgeNode> nodes;
  if (report.full_maintenance) {
    nodes = player_graph_nodes(impl_->graph, owner, 2200);
  } else {
    nodes.reserve(dirty_entries.size());
    std::unordered_set<std::string> seen_nodes;
    for (const auto& [_, entry] : dirty_entries) {
      if (entry.kind != KnowledgeEntryKind::kNode ||
          !seen_nodes.insert(entry.id).second) {
        continue;
      }
      if (const auto node = impl_->graph.node(KnowledgeNodeId{entry.id})) {
        nodes.push_back(*node);
      } else {
        const auto stale_chunk = make_knowledge_chunk_id(
            owner, "graph_node", entry.id, KnowledgeChunkGranularity::kEntity);
        if (impl_->chunks.remove_chunk(stale_chunk)) ++report.chunks_removed;
      }
    }
  }

  // Removed graph nodes have deterministic owner-scoped chunk ids, so delete
  // those chunks immediately instead of waiting for the periodic full sweep.
  for (const auto& key : removed_entries) {
    const auto found = key.find('\n');
    if (found == std::string::npos || key.substr(0, found) != "node") continue;
    const auto node_id = key.substr(found + 1);
    const auto stale_chunk = make_knowledge_chunk_id(
        owner, "graph_node", node_id, KnowledgeChunkGranularity::kEntity);
    if (impl_->chunks.remove_chunk(stale_chunk)) ++report.chunks_removed;
  }

  report.chunk_nodes_processed = nodes.size();
  refresh_chunk_nodes_processed_.fetch_add(nodes.size(), std::memory_order_relaxed);
  set_activity("knowledge_refresh", "chunks", owner, 0, nodes.size());
  std::unordered_set<std::string> live_chunks;
  if (report.full_maintenance) live_chunks.reserve(nodes.size());
  std::size_t chunk_index = 0;
  for (const auto& node : nodes) {
    const std::string topic = node_topic(node);
    const auto id = make_knowledge_chunk_id(
        owner, "graph_node", node.id.value, KnowledgeChunkGranularity::kEntity);
    if (report.full_maintenance) live_chunks.insert(id.value);
    const auto content = compact_node_content(node);
    const auto sources = provenance_sources(impl_->dependencies, node.id);
    const auto source_version = stable_version(content);

    KnowledgeChunk chunk;
    chunk.metadata.id = id;
    chunk.metadata.player_id = owner;
    chunk.metadata.type = "graph_node";
    chunk.metadata.topic = topic;
    chunk.metadata.granularity = KnowledgeChunkGranularity::kEntity;
    chunk.metadata.source_type = sources.empty() ? "knowledge_graph"
                                                 : sources.front().source_type;
    chunk.metadata.source_version = source_version;
    if (const auto value = text_property(node, "opening")) chunk.metadata.opening = value;
    if (const auto value = text_property(node, "opening_name")) chunk.metadata.opening = value;
    if (const auto value = text_property(node, "eco")) chunk.metadata.eco = value;
    if (const auto value = text_property(node, "color")) chunk.metadata.color = value;
    if (const auto value = text_property(node, "result")) chunk.metadata.result = value;
    if (const auto value = text_property(node, "time_control")) chunk.metadata.time_control = value;
    if (const auto value = text_property(node, "evidence_type")) chunk.metadata.evidence_type = value;
    if (const auto metrics = impl_->quality.quality(
            {KnowledgeEntryKind::kNode, node.id.value})) {
      chunk.metadata.confidence = metrics->confidence;
      chunk.metadata.coverage = metrics->coverage;
      chunk.metadata.freshness = metrics->freshness;
      chunk.metadata.importance = metrics->importance;
      chunk.metadata.sample_size = metrics->sample_size;
      chunk.metadata.source_quality = metrics->source_quality;
    }
    chunk.content = content;
    chunk.sources = sources;

    // Every node selected for maintenance is dirty (incremental mode) or part
    // of the low-frequency safety sweep. Persist the complete chunk metadata
    // as well as content so refreshed quality/freshness cannot lag behind the
    // graph merely because the compact text itself stayed identical.
    impl_->chunks.upsert_chunk(chunk);
    ++report.chunks_upserted;
    impl_->chunks.replace_graph_links(
        id, {{id, {KnowledgeEntryKind::kNode, node.id.value},
              ChunkGraphLinkKind::kDescribes}});
    const KnowledgeEntryRef chunk_entry{KnowledgeEntryKind::kChunk, id.value};
    if (!impl_->dependencies.dependencies_match(chunk_entry, sources)) {
      impl_->dependencies.replace_provenance(chunk_entry, sources, observed_at_ms);
      impl_->dependencies.replace_dependencies(chunk_entry, sources, observed_at_ms);
    }
    ++chunk_index;
    if (chunk_index == nodes.size() || chunk_index % 64 == 0) {
      set_activity("knowledge_refresh", "chunks", owner, chunk_index, nodes.size());
    }
  }

  if (report.full_maintenance) {
    set_activity("knowledge_refresh", "chunk_cleanup", owner);
    for (const auto& existing : impl_->chunks.find(
             owner, std::string_view{"graph_node"}, std::nullopt, 5000)) {
      if (!live_chunks.contains(existing.id.value) &&
          impl_->chunks.remove_chunk(existing.id)) {
        ++report.chunks_removed;
      }
    }
  }

  set_activity("knowledge_refresh", "conflicts", owner);
  const auto conflicts = impl_->conflicts.refresh_profile_conflicts(owner, observed_at_ms);
  report.conflicts = conflicts.size();
  set_activity("knowledge_refresh", "knowledge_gaps", owner);
  const auto gaps = impl_->gaps.refresh_profile_gaps(owner, observed_at_ms);
  report.gaps = gaps.size();
  if (report.full_maintenance) {
    impl_->last_full_maintenance_profile_id = owner;
    impl_->last_full_maintenance_ms = observed_at_ms;
  }
  impl_->last_profile_id = owner;
  impl_->last_refresh_ms = observed_at_ms;
  clear_activity();
  finish_refresh_metrics();
  return report;
  } catch (...) {
    clear_activity();
    finish_refresh_metrics();
    throw;
  }
}

KnowledgeGapActiveLearningPlan KnowledgeRuntime::active_learning_plan(
    const std::string& profile_id, const std::int64_t evaluated_at_ms) {
  std::lock_guard lock(mutex_);
  if (!impl_->opened || profile_id.empty()) return {};
  const auto owner = database_.player_profile_owner_id(profile_id);
  set_activity("active_learning", "conflicts", owner);
  try {
    (void)impl_->conflicts.refresh_profile_conflicts(owner, evaluated_at_ms);
    set_activity("active_learning", "knowledge_gaps", owner);
    const auto gaps = impl_->gaps.refresh_profile_gaps(owner, evaluated_at_ms);
    set_activity("active_learning", "planning", owner, gaps.size(), gaps.size());
    auto plan = impl_->gaps.build_active_learning_plan(gaps);
    clear_activity();
    return plan;
  } catch (...) {
    clear_activity();
    throw;
  }
}

// -----------------------------------------------------------------------------
// Section: Coach evidence integration
// -----------------------------------------------------------------------------

std::optional<ai::EvidenceItem> KnowledgeRuntime::coach_evidence(
    const ai::CoachRequest& request, const ai::QueryPlan& plan,
    const ai::EmbeddingModel* embeddings) {
  if (!needs_profile_evidence(plan)) return std::nullopt;
  const auto owner = owner_profile_id(database_, request.profile_id);
  if (owner.empty()) return std::nullopt;
  const auto started = std::chrono::steady_clock::now();
  coach_evidence_requests_.fetch_add(1, std::memory_order_relaxed);
  const auto route = impl_->router.route(
      request.user_text, plan, request.position_fen.has_value());
  const auto live = live_statistic_candidates(database_, statistics_, owner, route);
  std::unique_lock lock(mutex_, std::try_to_lock);
  const bool graph_available = lock.owns_lock() && impl_->opened;
  if (graph_available) {
    coach_graph_available_.fetch_add(1, std::memory_order_relaxed);
  } else {
    coach_graph_busy_fallbacks_.fetch_add(1, std::memory_order_relaxed);
  }

  // Keep the query path read-mostly. ProfileService performs authoritative
  // refreshes; this guard only avoids querying an unrelated previously active
  // profile when the app has just switched profiles.
  if (graph_available && impl_->last_profile_id != owner) {
    impl_->last_profile_id = owner;
  }

  std::unique_ptr<SemanticChunkSearch> semantic;
  if (graph_available && embeddings != nullptr && embeddings->available()) {
    semantic = std::make_unique<SemanticChunkSearch>(
        impl_->chunks, impl_->vectors, *embeddings);
  }

  const auto execution = impl_->planner.plan(route);
  HybridRetrievalEngine retrieval_engine(
      impl_->graph, impl_->chunks, semantic.get(), &impl_->positions);
  HybridRetrievalResult retrieval;
  if (graph_available) retrieval = retrieval_engine.retrieve({
      .player_id = owner,
      .query_text = request.user_text,
      .route = route,
      .current_fen = request.position_fen,
      .budgets = execution.retrieval_budgets,
      .authoritative_nodes = live,
  });
  else retrieval.nodes = live;
  const auto ranked = KnowledgeRetrievalRanker(graph_available ? &impl_->quality : nullptr).rank(
      retrieval, request.user_text, execution);
  const auto packet = EvidencePacketBuilder(
      impl_->graph, impl_->chunks, impl_->dependencies, graph_available ? &impl_->quality : nullptr)
                          .build({
                              .player_id = owner,
                              .query_text = request.user_text,
                              .route = route,
                              .execution_plan = execution,
                              .ranked = ranked,
                              .retrieval = retrieval,
                              .current_position = request.position_fen,
                              .recent_context = std::nullopt,
                          });

  json context = {
      {"schema", "profile.context.v3"},
      {"profileId", owner},
      {"queryFamily", query_family_name(plan.query_family)},
      {"graphRevision", graph_available ? impl_->last_refresh_ms : 0},
      {"requestedScope", requested_scope_json(plan.profile_scope)},
      {"scopeComplete", packet.answerability.answerable},
      {"scopeHasRelevantData", !packet.facts.empty() || !packet.observations.empty() ||
                                   !packet.evidence.empty()},
      {"missingTopics", packet.answerability.missing_topics},
      {"missingTimeControls", packet.answerability.missing_time_controls},
      {"missingPhases", packet.answerability.missing_phases},
      {"missingPlayerColors", packet.answerability.missing_player_colors},
      {"providerTokenBudget", packet.budget.max_tokens},
      {"limitations", json::array()},
      {"chunks", json::array()},
      {"contextStatus", packet.answerability.answerable ? "available"
                                                         : "insufficient_evidence"},
  };
  if (!graph_available) context["limitations"].push_back("knowledge_graph_busy_live_statistics_only");
  context["requiredChunkGroups"] = required_chunk_groups(packet, route);
  if (std::any_of(context["requiredChunkGroups"].begin(), context["requiredChunkGroups"].end(),
      [](const auto& group) { return group.empty(); })) {
    context["scopeComplete"] = false;
    context["contextStatus"] = "insufficient_evidence";
    context["limitations"].push_back("requested_scope_missing");
  }

  if (const auto payload = database_.ai_chess_profile_payload(owner)) {
    try {
      const auto learned = json::parse(*payload);
      context["profileConfidence"] = learned.value("confidence", 0.0);
      context["sourceGames"] = learned.value("sourceGames", 0);
      if (learned.contains("background") && learned["background"].is_object()) {
        const auto& background = learned["background"];
        context["historyComplete"] = background.value("historyComplete", false);
        context["historySyncedMonths"] = background.value("historySyncedMonths", 0);
        context["historyAvailableMonths"] = background.value("historyAvailableMonths", 0);
        if (background.value("status", "idle") != "complete")
          context["limitations"].push_back("profile_preparation_in_progress");
      }
    } catch (...) {
      context["profileConfidence"] = 0.0;
      context["sourceGames"] = 0;
    }
  }
  if (!context.contains("profileConfidence")) context["profileConfidence"] = 0.0;
  if (!context.contains("sourceGames")) context["sourceGames"] = 0;
  if (!context.contains("historyComplete")) context["historyComplete"] = false;
  if (!context.contains("historySyncedMonths")) context["historySyncedMonths"] = 0;
  if (!context.contains("historyAvailableMonths")) context["historyAvailableMonths"] = 0;

  append_packet_items(context["chunks"], packet.facts,
                      KnowledgePacketSection::kFact);
  append_packet_items(context["chunks"], packet.observations,
                      KnowledgePacketSection::kObservation);
  append_packet_items(context["chunks"], packet.evidence,
                      KnowledgePacketSection::kEvidence);
  for (const auto& uncertainty : packet.uncertainties) {
    context["limitations"].push_back(uncertainty.code);
    if (!context.contains("uncertainties")) context["uncertainties"] = json::array();
    context["uncertainties"].push_back({{"code", uncertainty.code}, {"detail", uncertainty.detail}});
  }
  for (const auto& reason : packet.answerability.reasons) {
    context["limitations"].push_back(reason);
  }

  json trace = {
      {"schema", "knowledge.query_trace.v1"},
      {"intent", to_string(route.intent)},
      {"queryFamily", query_family_name(route.query_family)},
      {"routeConfidence", route.confidence},
      {"currentBoard", to_string(route.current_board)},
      {"historicalProfile", to_string(route.historical_profile)},
      {"seedNodeIds", json::array()},
      {"selectedNodeIds", json::array()},
      {"selectedChunkIds", json::array()},
      {"answerabilityReasons", packet.answerability.reasons},
      {"sourceTrace", json::array()},
      {"graphTrace", json::array()},
  };
  for (const auto& id : retrieval.seed_nodes) trace["seedNodeIds"].push_back(id.value);
  for (const auto& value : ranked.nodes) trace["selectedNodeIds"].push_back(value.candidate.node.id.value);
  for (const auto& value : ranked.chunks) trace["selectedChunkIds"].push_back(value.candidate.chunk.metadata.id.value);
  for (const auto& value : packet.source_trace) {
    json item = {{"entryKind", entry_kind_name(value.entry.kind)},
                 {"entryId", value.entry.id},
                 {"score", value.retrieval_score},
                 {"sources", json::array()}};
    if (value.chunk_id) item["chunkId"] = value.chunk_id->value;
    for (const auto& source : value.sources) item["sources"].push_back(source_json(source));
    trace["sourceTrace"].push_back(std::move(item));
  }
  for (const auto& step : packet.graph_trace) {
    trace["graphTrace"].push_back({
        {"edgeId", step.edge_id.value},
        {"nodeId", step.node_id.value},
        {"edgeKind", to_string(step.edge_kind)},
        {"depth", step.depth},
    });
  }

  const auto created = now_ms();
  const auto trace_id = "query:" + std::to_string(created) + ":" +
                        stable_version(owner + "|" + request.user_text).substr(8);
  try {
  if (graph_available) impl_->traces.upsert({
      .id = trace_id,
      .profile_id = owner,
      .query_text = request.user_text.substr(0, 1000),
      .intent = std::string(to_string(route.intent)),
      .route_confidence = route.confidence,
      .answerable = packet.answerability.answerable,
      .answerability_confidence = packet.answerability.confidence,
      .seed_count = retrieval.seed_nodes.size(),
      .candidate_node_count = retrieval.nodes.size(),
      .candidate_chunk_count = retrieval.chunks.size(),
      .selected_node_count = ranked.nodes.size(),
      .selected_chunk_count = ranked.chunks.size(),
      .expanded_nodes = retrieval.expanded_nodes,
      .packet_tokens = packet.estimated_tokens,
      .created_at_ms = created,
      .trace_json = trace.dump(),
  });
  } catch (const std::exception&) {
    // Diagnostic persistence must not suppress an already-grounded answer.
  }

  const auto elapsed = static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - started)
          .count());
  coach_total_ms_.fetch_add(elapsed, std::memory_order_relaxed);
  coach_last_ms_.store(elapsed, std::memory_order_relaxed);
  return ai::EvidenceItem{
      .id = "profile.context.v3",
      .kind = ai::EvidenceKind::user_profile,
      .payload = context.dump(),
      .confidence = packet.answerability.confidence,
  };
}

// -----------------------------------------------------------------------------
// Section: Developer Graph Inspector / live diagnostics
// -----------------------------------------------------------------------------

std::string KnowledgeRuntime::inspector_json(const std::string& request_json) {
  json runtime_activity = json::object();
  json runtime_performance = json::object();
  try {
    runtime_activity = json::parse(activity_json());
  } catch (...) {
    runtime_activity = {{"active", false}, {"operation", "unknown"}};
  }
  try {
    runtime_performance = json::parse(performance_json());
  } catch (...) {
    runtime_performance = {{"status", "unavailable"}};
  }

  std::unique_lock lock(mutex_, std::try_to_lock);
  if (!lock.owns_lock()) {
    return json({{"schema", "knowledge.inspector.v1"},
                 {"status", "busy"},
                 {"retryable", true},
                 {"runtimeActivity", std::move(runtime_activity)},
                 {"runtimePerformance", std::move(runtime_performance)}}).dump();
  }
  json request = json::object();
  try {
    if (!request_json.empty()) request = json::parse(request_json);
  } catch (...) {
    return json({{"schema", "knowledge.inspector.v1"},
                 {"status", "invalid_request"}}).dump();
  }
  if (!impl_->opened) {
    return json({{"schema", "knowledge.inspector.v1"},
                 {"status", "unavailable"}}).dump();
  }

  std::optional<std::string> requested_profile;
  if (request.contains("profileId") && request["profileId"].is_string()) {
    requested_profile = request["profileId"].get<std::string>();
  }
  const auto owner = owner_profile_id(database_, requested_profile);
  if (owner.empty()) {
    return json({{"schema", "knowledge.inspector.v1"},
                 {"status", "no_profile"}}).dump();
  }
  const auto limit = std::clamp<std::size_t>(
      static_cast<std::size_t>(std::max(1, request.value("limit", 12))), 1, 25);
  const auto query = request.value("query", std::string{});
  const auto node_id = request.value("nodeId", std::string{});

  std::vector<KnowledgeNode> candidates;
  if (!node_id.empty()) {
    if (const auto node = impl_->graph.node({node_id})) candidates.push_back(*node);
  } else {
    candidates = player_graph_nodes(impl_->graph, owner, 120);
  }
  auto lower = [](std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
      return c < 128 ? static_cast<char>(std::tolower(c)) : static_cast<char>(c);
    });
    return value;
  };
  const auto needle = lower(query);

  json matches = json::array();
  for (const auto& node : candidates) {
    if (matches.size() >= limit) break;
    const auto summary = compact_node_content(node);
    if (!needle.empty()) {
      const auto haystack = lower(node.id.value + " " + std::string(to_string(node.kind)) +
                                  " " + summary);
      if (haystack.find(needle) == std::string::npos) continue;
    }
    json value = {
        {"nodeId", node.id.value},
        {"kind", to_string(node.kind)},
        {"assertion", to_string(node.assertion_kind)},
        {"properties", properties_json(node.properties)},
        {"quality", nullptr},
        {"provenance", json::array()},
        {"relations", json::array()},
        {"chunks", json::array()},
    };
    if (const auto quality = impl_->quality.quality(
            {KnowledgeEntryKind::kNode, node.id.value})) {
      value["quality"] = {
          {"confidence", quality->confidence},
          {"coverage", quality->coverage},
          {"freshness", quality->freshness ? json(*quality->freshness) : json(nullptr)},
          {"sourceQuality", quality->source_quality},
          {"importance", quality->importance},
          {"sampleSize", quality->sample_size},
          {"evidenceCount", quality->evidence_count},
          {"temporalScope", to_string(quality->temporal_scope)},
          {"evaluatedAtMs", quality->evaluated_at_ms},
      };
    }
    for (const auto& record : impl_->dependencies.provenance_for(
             {KnowledgeEntryKind::kNode, node.id.value})) {
      value["provenance"].push_back({
          {"source", source_json(record.source)},
          {"firstSeenMs", record.first_seen_ms},
          {"lastSeenMs", record.last_seen_ms},
      });
    }
    for (const auto& relation : impl_->graph.adjacent(
             node.id, GraphDirection::kBoth, std::nullopt, 6)) {
      value["relations"].push_back({
          {"edgeId", relation.edge.id.value},
          {"edgeKind", to_string(relation.edge.kind)},
          {"from", relation.edge.from.value},
          {"to", relation.edge.to.value},
          {"relatedNodeId", relation.node.id.value},
          {"relatedNodeKind", to_string(relation.node.kind)},
      });
    }
    const auto linked = impl_->chunks.linked_chunks(
        owner, {{KnowledgeEntryKind::kNode, node.id.value}}, 4);
    for (const auto& chunk : linked) {
      value["chunks"].push_back({
          {"chunkId", chunk.metadata.id.value},
          {"type", chunk.metadata.type},
          {"topic", chunk.metadata.topic},
          {"sourceVersion", chunk.metadata.source_version},
          {"content", chunk.content},
      });
    }
    matches.push_back(std::move(value));
  }

  json traces = json::array();
  for (const auto& trace : impl_->traces.recent(owner, 5)) {
    json detail = json::object();
    try {
      detail = json::parse(trace.trace_json);
    } catch (...) {
      detail = {{"status", "invalid_trace_payload"}};
    }
    traces.push_back({
        {"id", trace.id},
        {"query", trace.query_text},
        {"intent", trace.intent},
        {"routeConfidence", trace.route_confidence},
        {"answerable", trace.answerable},
        {"answerabilityConfidence", trace.answerability_confidence},
        {"seedCount", trace.seed_count},
        {"candidateNodes", trace.candidate_node_count},
        {"candidateChunks", trace.candidate_chunk_count},
        {"selectedNodes", trace.selected_node_count},
        {"selectedChunks", trace.selected_chunk_count},
        {"expandedNodes", trace.expanded_nodes},
        {"packetTokens", trace.packet_tokens},
        {"createdAtMs", trace.created_at_ms},
        {"detail", std::move(detail)},
    });
  }

  return json({
      {"schema", "knowledge.inspector.v1"},
      {"status", "ok"},
      {"profileId", owner},
      {"contractVersion", kKnowledgeGraphContractVersion},
      {"graphSchemaVersion", kKnowledgeGraphSchemaVersion},
      {"lastRefreshMs", impl_->last_profile_id == owner ? impl_->last_refresh_ms : 0},
      {"runtimeActivity", std::move(runtime_activity)},
      {"runtimePerformance", std::move(runtime_performance)},
      {"query", query},
      {"matches", std::move(matches)},
      {"recentQueryTraces", std::move(traces)},
  }).dump();
}

}  // namespace kchess::knowledge
