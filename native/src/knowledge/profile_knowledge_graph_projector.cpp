#include "profile_knowledge_graph_projector.h"

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ai/profile/chess_profile.h"
#include "persistence/database.h"

namespace kchess::knowledge {
namespace {

// -----------------------------------------------------------------------------
// Section: Stable identities and source locators
// -----------------------------------------------------------------------------

std::string stable_fingerprint(std::string_view value) {
  std::uint64_t hash = 1469598103934665603ULL;
  for (const unsigned char byte : value) {
    hash ^= static_cast<std::uint64_t>(byte);
    hash *= 1099511628211ULL;
  }
  std::ostringstream out;
  out << std::hex << std::setfill('0') << std::setw(16) << hash;
  return out.str();
}

KnowledgeNode make_node(KnowledgeNodeKind kind, std::string_view canonical_key,
                        KnowledgeAssertionKind assertion_kind,
                        KnowledgeProperties properties = {}) {
  KnowledgeNode node;
  node.kind = kind;
  node.assertion_kind = assertion_kind;
  node.id = make_knowledge_node_id(to_string(kind), canonical_key);
  node.properties = std::move(properties);
  return node;
}

KnowledgeNode make_fact_node(KnowledgeNodeKind kind,
                             std::string_view canonical_key,
                             KnowledgeProperties properties = {}) {
  return make_node(kind, canonical_key, KnowledgeAssertionKind::kFact,
                   std::move(properties));
}

KnowledgeEdge make_edge(const KnowledgeNode& from, KnowledgeEdgeKind kind,
                        const KnowledgeNode& to,
                        KnowledgeProperties properties = {},
                        std::string_view discriminator = {}) {
  KnowledgeEdge edge;
  edge.from = from.id;
  edge.to = to.id;
  edge.kind = kind;
  edge.id = make_knowledge_edge_id(from.id, to_string(kind), to.id,
                                   discriminator);
  edge.properties = std::move(properties);
  return edge;
}

std::string source_key(const KnowledgeSourceRef& source) {
  return source.source_type + "\n" + source.source_id;
}

KnowledgeSourceRef model_source(const std::string& profile_id,
                                std::string_view payload) {
  return {.source_type = "learned_profile",
          .source_id = "profile:" + profile_id,
          .source_version = "fnv1a64:" + stable_fingerprint(payload)};
}

KnowledgeSourceRef evidence_source(
    const AiProfileEvidenceRegistryRow& row) {
  return {.source_type = "profile_evidence_registry",
          .source_id = row.evidence_id,
          .source_version = "revision:" + std::to_string(row.source_version)};
}

KnowledgeSourceRef move_analysis_source(
    const PlayerProfileGameSourceRow& row) {
  return {.source_type = "shared_move_analysis",
          .source_id = "game:" + row.game_id,
          .source_version = "revision:" + std::to_string(row.source_version)};
}

KnowledgeSourceRef manifest_source(
    const std::string& profile_id,
    const std::map<std::string, KnowledgeSourceRef>& sources) {
  std::ostringstream canonical;
  for (const auto& [key, source] : sources) {
    (void)source;
    canonical << key << ';';
  }
  return {.source_type = "profile_knowledge_manifest",
          .source_id = "profile:" + profile_id,
          .source_version = "fnv1a64:" + stable_fingerprint(canonical.str())};
}

KnowledgeNode player_node(const std::string& profile_id) {
  return make_fact_node(KnowledgeNodeKind::kPlayer, "profile:" + profile_id,
                        {{"profile_id", profile_id}});
}

KnowledgeNode game_node(const std::string& profile_id,
                        const std::string& game_id) {
  return make_fact_node(KnowledgeNodeKind::kGame, "game:" + game_id,
                        {{"game_id", game_id}, {"profile_id", profile_id}});
}

// -----------------------------------------------------------------------------
// Section: Projection accumulator
// -----------------------------------------------------------------------------

struct PendingNode {
  KnowledgeNode node;
  std::map<std::string, KnowledgeSourceRef> sources;
};

struct PendingEdge {
  KnowledgeEdge edge;
  std::map<std::string, KnowledgeSourceRef> sources;
};

class ProjectionAccumulator {
 public:
  void node(KnowledgeNode value, const KnowledgeSourceRef& source) {
    const auto id = value.id.value;
    auto [it, inserted] =
        nodes_.emplace(id, PendingNode{.node = value, .sources = {}});
    if (!inserted) it->second.node = std::move(value);
    it->second.sources[source_key(source)] = source;
    sources_[source_key(source)] = source;
  }

  void edge(KnowledgeEdge value, const KnowledgeSourceRef& source) {
    const auto id = value.id.value;
    auto [it, inserted] =
        edges_.emplace(id, PendingEdge{.edge = value, .sources = {}});
    if (!inserted) it->second.edge = std::move(value);
    it->second.sources[source_key(source)] = source;
    sources_[source_key(source)] = source;
  }

  void add_node_source(const KnowledgeNodeId& id,
                       const KnowledgeSourceRef& source) {
    const auto found = nodes_.find(id.value);
    if (found == nodes_.end()) return;
    found->second.sources[source_key(source)] = source;
    sources_[source_key(source)] = source;
  }

  void add_edge_source(const KnowledgeEdgeId& id,
                       const KnowledgeSourceRef& source) {
    const auto found = edges_.find(id.value);
    if (found == edges_.end()) return;
    found->second.sources[source_key(source)] = source;
    sources_[source_key(source)] = source;
  }

  const auto& nodes() const { return nodes_; }
  const auto& edges() const { return edges_; }
  const auto& sources() const { return sources_; }

 private:
  std::map<std::string, PendingNode> nodes_;
  std::map<std::string, PendingEdge> edges_;
  std::map<std::string, KnowledgeSourceRef> sources_;
};

std::vector<KnowledgeSourceRef> source_values(
    const std::map<std::string, KnowledgeSourceRef>& sources,
    const KnowledgeSourceRef& manifest) {
  auto merged = sources;
  merged[source_key(manifest)] = manifest;
  std::vector<KnowledgeSourceRef> result;
  result.reserve(merged.size());
  for (const auto& [_, source] : merged) result.push_back(source);
  return result;
}

// -----------------------------------------------------------------------------
// Section: Learned profile concepts
// -----------------------------------------------------------------------------

KnowledgeProperties pattern_properties(const std::string& profile_id,
                                             const ai::PlayerPattern& pattern) {
  KnowledgeProperties properties{{"profile_id", profile_id},
          {"pattern_id", pattern.id},
          {"confidence", pattern.confidence},
          {"severity", pattern.severity},
          {"occurrences", static_cast<std::int64_t>(pattern.occurrences)},
          {"sample_games", static_cast<std::int64_t>(pattern.sample_games)},
          {"time_control", pattern.time_control},
          {"phase", pattern.phase},
          {"recent", pattern.recent},
          {"trend", pattern.trend},
          {"projection", std::string("profile_knowledge_graph")}};
  if (pattern.analyzed_moves > 0) properties["analyzed_moves"] =
      static_cast<std::int64_t>(pattern.analyzed_moves);
  if (pattern.observed_error_rate) properties["observed_error_rate"] =
      *pattern.observed_error_rate;
  if (pattern.posterior_error_lower) properties["posterior_error_lower_approx"] =
      *pattern.posterior_error_lower;
  if (pattern.posterior_error_upper) properties["posterior_error_upper_approx"] =
      *pattern.posterior_error_upper;
  return properties;
}

KnowledgeNode pattern_node(const std::string& profile_id,
                           const ai::PlayerPattern& pattern) {
  const bool strength = pattern.type == "strength";
  const auto kind = strength ? KnowledgeNodeKind::kStrength
                             : KnowledgeNodeKind::kWeakness;
  return make_node(
      kind,
      "profile:" + profile_id + ":" + std::string(to_string(kind)) + ":" +
          pattern.id,
      KnowledgeAssertionKind::kObservation,
      pattern_properties(profile_id, pattern));
}

KnowledgeNode listed_pattern_node(const std::string& profile_id,
                                  std::string_view id,
                                  KnowledgeNodeKind kind) {
  return make_node(kind,
                   "profile:" + profile_id + ":" +
                       std::string(to_string(kind)) + ":" + std::string(id),
                   KnowledgeAssertionKind::kObservation,
                   {{"profile_id", profile_id},
                    {"pattern_id", std::string(id)},
                    {"projection", std::string("profile_knowledge_graph")}});
}

KnowledgeNode evidence_node(const std::string& profile_id,
                            const AiProfileEvidenceRegistryRow& row) {
  KnowledgeProperties properties{{"evidence_id", row.evidence_id},
                                 {"source_kind", row.source_kind},
                                 {"source_table", row.source_table},
                                 {"source_key", row.source_key},
                                 {"evidence_tier", row.evidence_tier},
                                 {"evidence_level",
                                  static_cast<std::int64_t>(row.evidence_level)},
                                 {"source_version", row.source_version},
                                 {"updated_at", row.updated_at},
                                 {"projection",
                                  std::string("profile_knowledge_graph")}};
  if (row.game_id) properties.emplace("game_id", *row.game_id);
  if (row.ply) properties.emplace("ply", static_cast<std::int64_t>(*row.ply));
  return make_fact_node(
      KnowledgeNodeKind::kEvidence,
      "profile:" + profile_id + ":evidence:" + row.evidence_id,
      std::move(properties));
}

bool materialize_evidence(const AiProfileEvidenceRegistryRow& row) {
  return row.source_kind == "authoritative_analysis" ||
         row.source_kind == "statistics_aggregate" ||
         row.source_kind == "learned_profile";
}

std::optional<std::pair<KnowledgeNodeKind, std::string>> explicit_motif(
    std::string_view value) {
  constexpr std::string_view tactical_colon = "tactical_motif:";
  constexpr std::string_view tactical_dot = "tactical_motif.";
  constexpr std::string_view strategic_colon = "strategic_motif:";
  constexpr std::string_view strategic_dot = "strategic_motif.";
  if (value.starts_with(tactical_colon)) {
    return std::pair{KnowledgeNodeKind::kTacticalMotif,
                     std::string(value.substr(tactical_colon.size()))};
  }
  if (value.starts_with(tactical_dot)) {
    return std::pair{KnowledgeNodeKind::kTacticalMotif,
                     std::string(value.substr(tactical_dot.size()))};
  }
  if (value.starts_with(strategic_colon)) {
    return std::pair{KnowledgeNodeKind::kStrategicMotif,
                     std::string(value.substr(strategic_colon.size()))};
  }
  if (value.starts_with(strategic_dot)) {
    return std::pair{KnowledgeNodeKind::kStrategicMotif,
                     std::string(value.substr(strategic_dot.size()))};
  }
  return std::nullopt;
}

bool major_error(std::string_view classification) {
  return classification == "inaccuracy" || classification == "miss" || classification == "mistake" ||
         classification == "blunder";
}

bool recovery_move(const PlayerProfileMoveSourceRow& move) {
  const bool stable_classification =
      move.classification == "best" || move.classification == "excellent" ||
      move.classification == "critical" || move.classification == "brilliant";
  if (!stable_classification) return false;
  return !move.expected_score_loss || *move.expected_score_loss <= 0.05;
}

struct RecoverySummary {
  int opportunities{0};
  int recoveries{0};
  int cascade_sequences{0};
  int max_cascade_length{0};
  std::set<std::string> opportunity_games;
  std::set<std::string> recovery_games;
  std::set<std::string> cascade_games;
  std::map<std::string, KnowledgeSourceRef> opportunity_sources;
  std::map<std::string, KnowledgeSourceRef> recovery_sources;
  std::map<std::string, KnowledgeSourceRef> cascade_sources;
};

void observe_recovery(const PlayerProfileGameSourceRow& game,
                      const std::vector<PlayerProfileMoveSourceRow>& moves,
                      RecoverySummary& summary) {
  if (moves.empty()) return;
  const auto source = move_analysis_source(game);

  for (std::size_t i = 0; i + 1 < moves.size(); ++i) {
    if (!major_error(moves[i].classification)) continue;
    ++summary.opportunities;
    summary.opportunity_games.insert(game.game_id);
    summary.opportunity_sources[source_key(source)] = source;
    if (!major_error(moves[i + 1].classification) && recovery_move(moves[i + 1])) {
      ++summary.recoveries;
      summary.recovery_games.insert(game.game_id);
      summary.recovery_sources[source_key(source)] = source;
    }
  }

  int run = 0;
  bool counted_sequence = false;
  for (const auto& move : moves) {
    if (major_error(move.classification)) {
      ++run;
      summary.max_cascade_length = std::max(summary.max_cascade_length, run);
      if (run >= 2 && !counted_sequence) {
        ++summary.cascade_sequences;
        counted_sequence = true;
        summary.cascade_games.insert(game.game_id);
        summary.cascade_sources[source_key(source)] = source;
      }
    } else {
      run = 0;
      counted_sequence = false;
    }
  }
}

// -----------------------------------------------------------------------------
// Section: Evidence link helpers
// -----------------------------------------------------------------------------

using EvidenceByGame =
    std::map<std::string, const AiProfileEvidenceRegistryRow*>;

EvidenceByGame authoritative_evidence_by_game(
    const std::vector<AiProfileEvidenceRegistryRow>& evidence) {
  EvidenceByGame result;
  for (const auto& row : evidence) {
    if (row.source_kind != "authoritative_analysis" || !row.game_id) continue;
    const auto found = result.find(*row.game_id);
    if (found == result.end() ||
        row.evidence_level > found->second->evidence_level ||
        (row.evidence_level == found->second->evidence_level &&
         row.source_version > found->second->source_version)) {
      result[*row.game_id] = &row;
    }
  }
  return result;
}

void link_supporting_examples(
    ProjectionAccumulator& pending, const std::string& profile_id,
    const KnowledgeNode& concept_node, const ai::PlayerPattern& pattern,
    const KnowledgeSourceRef& model,
    const EvidenceByGame& evidence_by_game) {
  for (const auto& example : pattern.example_positions) {
    const auto found = evidence_by_game.find(example.game_id);
    if (found == evidence_by_game.end()) continue;
    const auto& row = *found->second;
    const auto evidence = evidence_node(profile_id, row);
    const auto source = evidence_source(row);
    pending.node(evidence, source);
    auto edge = make_edge(
        concept_node, KnowledgeEdgeKind::kSupportedBy, evidence,
        {{"example_ply", static_cast<std::int64_t>(example.ply)},
         {"projection", std::string("profile_knowledge_graph")}},
        "profile_knowledge_graph:example:" + example.game_id + ":" +
            std::to_string(example.ply));
    pending.edge(edge, model);
    pending.add_edge_source(edge.id, source);
  }
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Learned profile and evidence graph projection
// -----------------------------------------------------------------------------

ProfileKnowledgeGraphProjector::ProfileKnowledgeGraphProjector(
    Database& database, GraphStore& graph, DependencyTracker& dependencies)
    : database_(database), graph_(graph), dependencies_(dependencies) {}

ProfileKnowledgeProjectionReport
ProfileKnowledgeGraphProjector::project_active_profile(
    const std::int64_t observed_at_ms) {
  ProfileKnowledgeProjectionReport report;
  const auto active = database_.active_profile();
  if (!active) return report;
  const std::string profile_id = database_.player_profile_owner_id(active->id);
  report.profile_id = profile_id;

  const auto payload = database_.ai_chess_profile_payload(profile_id);
  if (!payload) return report;
  const auto profile = ai::chess_profile_from_json(*payload);
  if (!profile) return report;

  const auto registry = database_.ai_profile_evidence_registry(profile_id);
  const auto evidence_by_game = authoritative_evidence_by_game(registry);
  const auto model = model_source(profile_id, *payload);
  ProjectionAccumulator pending;

  const auto player = player_node(profile_id);
  graph_.upsert_node(player);

  // Evidence nodes are payload-free source locators. We intentionally omit the
  // metadata-only per-game entries unless a concept specifically needs one.
  for (const auto& row : registry) {
    if (!materialize_evidence(row)) continue;
    const auto node = evidence_node(profile_id, row);
    const auto source = evidence_source(row);
    pending.node(node, source);
    ++report.evidence_nodes;

    if (row.game_id) {
      const auto game = game_node(profile_id, *row.game_id);
      graph_.upsert_node(game);
      pending.edge(make_edge(node, KnowledgeEdgeKind::kDerivedFrom, game,
                             {{"projection",
                               std::string("profile_knowledge_graph")}},
                             "profile_knowledge_graph:evidence_game"),
                   source);
    }
  }

  std::map<std::string, KnowledgeNode> concepts_by_pattern;
  std::set<std::string> projected_strengths;
  std::set<std::string> projected_weaknesses;

  for (const auto& pattern : profile->patterns) {
    if (pattern.type != "strength" && pattern.type != "weakness") continue;
    const auto concept_node = pattern_node(profile_id, pattern);
    pending.node(concept_node, model);
    concepts_by_pattern[pattern.id] = concept_node;
    const auto relation = pattern.type == "strength"
                              ? KnowledgeEdgeKind::kStrongIn
                              : KnowledgeEdgeKind::kWeakIn;
    pending.edge(make_edge(player, relation, concept_node,
                           {{"projection",
                             std::string("profile_knowledge_graph")}},
                           "profile_knowledge_graph:pattern"),
                 model);
    link_supporting_examples(pending, profile_id, concept_node, pattern, model,
                             evidence_by_game);
    if (pattern.type == "strength") {
      projected_strengths.insert(pattern.id);
      ++report.strengths;
    } else {
      projected_weaknesses.insert(pattern.id);
      ++report.weaknesses;
    }

    if (pattern.trend != "insufficient_data") {
      const auto trend = make_node(
          KnowledgeNodeKind::kTrend,
          "profile:" + profile_id + ":pattern:" + pattern.id + ":trend:" +
              pattern.trend,
          KnowledgeAssertionKind::kObservation,
          {{"pattern_id", pattern.id},
           {"state", pattern.trend},
           {"recent", pattern.recent},
           {"projection", std::string("profile_knowledge_graph")}});
      pending.node(trend, model);
      pending.edge(make_edge(trend, KnowledgeEdgeKind::kDerivedFrom, concept_node,
                             {{"projection",
                               std::string("profile_knowledge_graph")}},
                             "profile_knowledge_graph:trend_source"),
                   model);
      pending.edge(make_edge(player, KnowledgeEdgeKind::kContains, trend,
                             {{"projection",
                               std::string("profile_knowledge_graph")}},
                             "profile_knowledge_graph:trend"),
                   model);
      if (pattern.trend == "improving") {
        pending.edge(make_edge(player, KnowledgeEdgeKind::kImprovingIn,
                               concept_node,
                               {{"projection",
                                 std::string("profile_knowledge_graph")}},
                               "profile_knowledge_graph:improving"),
                     model);
      } else if (pattern.trend == "worsening") {
        pending.edge(make_edge(player, KnowledgeEdgeKind::kDecliningIn,
                               concept_node,
                               {{"projection",
                                 std::string("profile_knowledge_graph")}},
                               "profile_knowledge_graph:declining"),
                     model);
      }
      ++report.trends;
    }

    if (pattern.id == "time_pressure_errors") {
      const auto behavior = make_node(
          KnowledgeNodeKind::kBehavior,
          "profile:" + profile_id + ":behavior:time_pressure_errors",
          KnowledgeAssertionKind::kObservation,
          {{"behavior", std::string("time_pressure_errors")},
           {"confidence", pattern.confidence},
           {"severity", pattern.severity},
           {"time_control", pattern.time_control},
           {"phase", pattern.phase},
           {"projection", std::string("profile_knowledge_graph")}});
      pending.node(behavior, model);
      pending.edge(make_edge(behavior, KnowledgeEdgeKind::kDerivedFrom,
                             concept_node,
                             {{"projection",
                               std::string("profile_knowledge_graph")}},
                             "profile_knowledge_graph:behavior_source"),
                   model);
      pending.edge(make_edge(player, KnowledgeEdgeKind::kContains, behavior,
                             {{"projection",
                               std::string("profile_knowledge_graph")}},
                             "profile_knowledge_graph:behavior"),
                   model);
      ++report.behaviors;
    }
  }

  for (const auto& id : profile->strengths) {
    if (projected_strengths.contains(id)) continue;
    const auto concept_node =
        listed_pattern_node(profile_id, id, KnowledgeNodeKind::kStrength);
    pending.node(concept_node, model);
    pending.edge(make_edge(player, KnowledgeEdgeKind::kStrongIn, concept_node,
                           {{"projection",
                             std::string("profile_knowledge_graph")}},
                           "profile_knowledge_graph:listed_strength"),
                 model);
    concepts_by_pattern[id] = concept_node;
    ++report.strengths;
  }
  for (const auto& id : profile->weaknesses) {
    if (projected_weaknesses.contains(id)) continue;
    const auto concept_node =
        listed_pattern_node(profile_id, id, KnowledgeNodeKind::kWeakness);
    pending.node(concept_node, model);
    pending.edge(make_edge(player, KnowledgeEdgeKind::kWeakIn, concept_node,
                           {{"projection",
                             std::string("profile_knowledge_graph")}},
                           "profile_knowledge_graph:listed_weakness"),
                 model);
    concepts_by_pattern[id] = concept_node;
    ++report.weaknesses;
  }

  for (const auto& mistake : profile->common_mistakes) {
    const auto habit = make_node(
        KnowledgeNodeKind::kHabit,
        "profile:" + profile_id + ":habit:" + mistake.id,
        KnowledgeAssertionKind::kObservation,
        {{"habit", mistake.id},
         {"occurrences", static_cast<std::int64_t>(mistake.occurrences)},
         {"rate", mistake.rate},
         {"confidence", mistake.confidence},
         {"projection", std::string("profile_knowledge_graph")}});
    pending.node(habit, model);
    pending.edge(make_edge(player, KnowledgeEdgeKind::kContains, habit,
                           {{"projection",
                             std::string("profile_knowledge_graph")}},
                           "profile_knowledge_graph:habit"),
                 model);
    ++report.habits;
  }

  if (profile->risk_tolerance.value) {
    const auto behavior = make_node(
        KnowledgeNodeKind::kBehavior,
        "profile:" + profile_id + ":behavior:risk_tolerance",
        KnowledgeAssertionKind::kObservation,
        {{"behavior", std::string("risk_tolerance")},
         {"value", *profile->risk_tolerance.value},
         {"confidence", profile->risk_tolerance.confidence},
         {"projection", std::string("profile_knowledge_graph")}});
    pending.node(behavior, model);
    pending.edge(make_edge(player, KnowledgeEdgeKind::kContains, behavior,
                           {{"projection",
                             std::string("profile_knowledge_graph")}},
                           "profile_knowledge_graph:risk_behavior"),
                 model);
    ++report.behaviors;
  }

  // Motif nodes are emitted only for explicitly namespaced upstream profile
  // identifiers. Generic misses/blunders are not silently relabeled as chess
  // motifs because the current source does not identify a fork/pin/plan/etc.
  for (const auto& preference : profile->preferences) {
    const auto motif = explicit_motif(preference);
    if (!motif || motif->second.empty()) continue;
    const auto node = make_node(
        motif->first,
        "profile:" + profile_id + ":" + std::string(to_string(motif->first)) +
            ":" + motif->second,
        KnowledgeAssertionKind::kObservation,
        {{"motif_id", motif->second},
         {"projection", std::string("profile_knowledge_graph")}});
    pending.node(node, model);
    pending.edge(make_edge(player, KnowledgeEdgeKind::kHasMotif, node,
                           {{"projection",
                             std::string("profile_knowledge_graph")}},
                           "profile_knowledge_graph:explicit_motif"),
                 model);
    if (motif->first == KnowledgeNodeKind::kTacticalMotif) {
      ++report.tactical_motifs;
    } else {
      ++report.strategic_motifs;
    }
  }

  for (const auto& hypothesis_value : profile->hypotheses) {
    const auto hypothesis = make_node(
        KnowledgeNodeKind::kHypothesis,
        "profile:" + profile_id + ":hypothesis:" + hypothesis_value.id,
        KnowledgeAssertionKind::kHypothesis,
        {{"hypothesis_id", hypothesis_value.id},
         {"pattern_id", hypothesis_value.pattern_id},
         {"status", hypothesis_value.status},
         {"confidence", hypothesis_value.confidence},
         {"evidence_for",
          static_cast<std::int64_t>(hypothesis_value.evidence_for)},
         {"evidence_against",
          static_cast<std::int64_t>(hypothesis_value.evidence_against)},
         {"updated_at", hypothesis_value.updated_at},
         {"projection", std::string("profile_knowledge_graph")}});
    pending.node(hypothesis, model);
    pending.edge(make_edge(player, KnowledgeEdgeKind::kContains, hypothesis,
                           {{"projection",
                             std::string("profile_knowledge_graph")}},
                           "profile_knowledge_graph:hypothesis"),
                 model);
    if (const auto found = concepts_by_pattern.find(hypothesis_value.pattern_id);
        found != concepts_by_pattern.end()) {
      pending.edge(make_edge(hypothesis, KnowledgeEdgeKind::kDerivedFrom,
                             found->second,
                             {{"projection",
                               std::string("profile_knowledge_graph")}},
                             "profile_knowledge_graph:hypothesis_pattern"),
                   model);
    }
    ++report.hypotheses;
  }

  for (const auto& time_control : profile->time_control_profiles) {
    if (time_control.trend == "insufficient_data") continue;
    const auto context = make_fact_node(
        KnowledgeNodeKind::kTimeControl,
        "time_control:" + time_control.id,
        {{"type", time_control.id}});
    graph_.upsert_node(context);
    const auto trend = make_node(
        KnowledgeNodeKind::kTrend,
        "profile:" + profile_id + ":time_control:" + time_control.id +
            ":trend:" + time_control.trend,
        KnowledgeAssertionKind::kObservation,
        {{"scope", std::string("time_control")},
         {"time_control", time_control.id},
         {"state", time_control.trend},
         {"confidence", time_control.confidence},
         {"projection", std::string("profile_knowledge_graph")}});
    pending.node(trend, model);
    pending.edge(make_edge(trend, KnowledgeEdgeKind::kDependsOn, context,
                           {{"projection",
                             std::string("profile_knowledge_graph")}},
                           "profile_knowledge_graph:time_control_trend_scope"),
                 model);
    pending.edge(make_edge(player, KnowledgeEdgeKind::kContains, trend,
                           {{"projection",
                             std::string("profile_knowledge_graph")}},
                           "profile_knowledge_graph:time_control_trend"),
                 model);
    ++report.trends;
  }

  // Recovery and error cascades are aggregate observations over the same saved
  // move_analysis rows already used by the profile pipeline. No engine request
  // or alternative move classifier is involved.
  RecoverySummary recovery;
  const auto game_sources = database_.player_profile_game_sources(profile_id);
  for (const auto& game : game_sources) {
    if (!game.analysis_complete) continue;
    observe_recovery(game,
                     database_.player_profile_move_sources(game.game_id,
                                                           game.player_color),
                     recovery);
  }

  if (recovery.opportunities > 0) {
    const auto recovery_node = make_node(
        KnowledgeNodeKind::kRecoveryPattern,
        "profile:" + profile_id + ":recovery:post_error_stabilization",
        KnowledgeAssertionKind::kObservation,
        {{"pattern", std::string("post_error_stabilization")},
         {"opportunities", static_cast<std::int64_t>(recovery.opportunities)},
         {"recoveries", static_cast<std::int64_t>(recovery.recoveries)},
         {"rate", static_cast<double>(recovery.recoveries) /
                      static_cast<double>(recovery.opportunities)},
         {"sample_games",
          static_cast<std::int64_t>(recovery.opportunity_games.size())},
         {"projection", std::string("profile_knowledge_graph")}});
    pending.node(recovery_node, model);
    for (const auto& [_, source] : recovery.opportunity_sources) {
      pending.add_node_source(recovery_node.id, source);
    }
    pending.edge(make_edge(player, KnowledgeEdgeKind::kContains, recovery_node,
                           {{"projection",
                             std::string("profile_knowledge_graph")}},
                           "profile_knowledge_graph:recovery"),
                 model);
    for (const auto& game_id : recovery.recovery_games) {
      const auto found = evidence_by_game.find(game_id);
      if (found == evidence_by_game.end()) continue;
      const auto& row = *found->second;
      const auto evidence = evidence_node(profile_id, row);
      const auto source = evidence_source(row);
      pending.node(evidence, source);
      auto edge = make_edge(
          recovery_node, KnowledgeEdgeKind::kSupportedBy, evidence,
          {{"projection", std::string("profile_knowledge_graph")}},
          "profile_knowledge_graph:recovery_evidence:" + game_id);
      pending.edge(edge, model);
      pending.add_edge_source(edge.id, source);
    }
    ++report.recovery_patterns;
  }

  if (recovery.cascade_sequences > 0) {
    const auto cascade = make_node(
        KnowledgeNodeKind::kTransitionPattern,
        "profile:" + profile_id + ":transition:error_cascade",
        KnowledgeAssertionKind::kObservation,
        {{"transition", std::string("error_cascade")},
         {"occurrences",
          static_cast<std::int64_t>(recovery.cascade_sequences)},
         {"max_run_length",
          static_cast<std::int64_t>(recovery.max_cascade_length)},
         {"sample_games",
          static_cast<std::int64_t>(recovery.cascade_games.size())},
         {"projection", std::string("profile_knowledge_graph")}});
    pending.node(cascade, model);
    for (const auto& [_, source] : recovery.cascade_sources) {
      pending.add_node_source(cascade.id, source);
    }
    pending.edge(make_edge(player, KnowledgeEdgeKind::kContains, cascade,
                           {{"projection",
                             std::string("profile_knowledge_graph")}},
                           "profile_knowledge_graph:error_cascade"),
                 model);
    for (const auto& game_id : recovery.cascade_games) {
      const auto found = evidence_by_game.find(game_id);
      if (found == evidence_by_game.end()) continue;
      const auto& row = *found->second;
      const auto evidence = evidence_node(profile_id, row);
      const auto source = evidence_source(row);
      pending.node(evidence, source);
      auto edge = make_edge(
          cascade, KnowledgeEdgeKind::kSupportedBy, evidence,
          {{"projection", std::string("profile_knowledge_graph")}},
          "profile_knowledge_graph:cascade_evidence:" + game_id);
      pending.edge(edge, model);
      pending.add_edge_source(edge.id, source);
    }
    ++report.error_cascades;
  }

  // The learned model is always a dependency of the semantic profile layer.
  // Analysis-derived recovery/cascade nodes additionally carry their game
  // analysis sources. The profile-wide manifest detects removed concepts and
  // removed evidence locators without rebuilding unrelated graph layers.
  auto source_catalog = pending.sources();
  source_catalog[source_key(model)] = model;
  const auto manifest = manifest_source(profile_id, source_catalog);

  std::vector<KnowledgeInvalidation> cleanup_invalidations;
  for (const auto& [_, source] : source_catalog) {
    const auto invalidated = dependencies_.invalidate_source_change(
        source.source_type, source.source_id, source.source_version,
        observed_at_ms);
    cleanup_invalidations.insert(cleanup_invalidations.end(),
                                 invalidated.begin(), invalidated.end());
    report.invalidated_entries.insert(report.invalidated_entries.end(),
                                      invalidated.begin(), invalidated.end());
  }
  const auto manifest_invalidated = dependencies_.invalidate_source_change(
      manifest.source_type, manifest.source_id, manifest.source_version,
      observed_at_ms);
  cleanup_invalidations.insert(cleanup_invalidations.end(),
                               manifest_invalidated.begin(),
                               manifest_invalidated.end());
  report.invalidated_entries.insert(report.invalidated_entries.end(),
                                    manifest_invalidated.begin(),
                                    manifest_invalidated.end());

  std::set<std::string> live_nodes;
  std::set<std::string> live_edges;
  for (const auto& [id, value] : pending.nodes()) {
    const auto provenance = source_values(value.sources, manifest);
    const KnowledgeEntryRef entry{KnowledgeEntryKind::kNode, id};
    const bool graph_changed = graph_.upsert_node(value.node);
    const bool dependency_changed = !dependencies_.dependencies_match(entry, provenance);
    if (dependency_changed) {
      dependencies_.replace_provenance(entry, provenance, observed_at_ms);
      dependencies_.replace_dependencies(entry, provenance, observed_at_ms);
    }
    live_nodes.insert(id);
    if (graph_changed) ++report.nodes_upserted;
    if (graph_changed || dependency_changed) report.changed_entries.push_back(entry);
  }
  for (const auto& [id, value] : pending.edges()) {
    const auto provenance = source_values(value.sources, manifest);
    const KnowledgeEntryRef entry{KnowledgeEntryKind::kEdge, id};
    const bool graph_changed = graph_.upsert_edge(value.edge);
    const bool dependency_changed = !dependencies_.dependencies_match(entry, provenance);
    if (dependency_changed) {
      dependencies_.replace_provenance(entry, provenance, observed_at_ms);
      dependencies_.replace_dependencies(entry, provenance, observed_at_ms);
    }
    live_edges.insert(id);
    if (graph_changed) ++report.edges_upserted;
    if (graph_changed || dependency_changed) report.changed_entries.push_back(entry);
  }

  // Only entries carrying this projection's manifest can appear in this set;
  // shared Player/Game/TimeControl identities were intentionally not registered
  // as projection-owned dependencies and therefore are never removed here.
  for (const auto& invalidation : cleanup_invalidations) {
    if (invalidation.entry.kind == KnowledgeEntryKind::kEdge &&
        !live_edges.contains(invalidation.entry.id)) {
      if (graph_.remove_edge(KnowledgeEdgeId{invalidation.entry.id})) {
        report.removed_entries.push_back(invalidation.entry);
      }
    } else if (invalidation.entry.kind == KnowledgeEntryKind::kNode &&
               !live_nodes.contains(invalidation.entry.id)) {
      if (graph_.remove_node(KnowledgeNodeId{invalidation.entry.id})) {
        report.removed_entries.push_back(invalidation.entry);
      }
    }
  }

  return report;
}

}  // namespace kchess::knowledge
