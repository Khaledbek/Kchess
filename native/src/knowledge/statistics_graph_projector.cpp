#include "statistics_graph_projector.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <iomanip>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "persistence/database.h"
#include "services/statistics_service.h"

namespace kchess::knowledge {
namespace {

using Json = nlohmann::json;

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

KnowledgeSourceRef statistics_source(const std::string& profile_id,
                                     std::string_view facet,
                                     std::string_view canonical_payload) {
  return KnowledgeSourceRef{
      .source_type = "statistics_service",
      .source_id = "profile:" + profile_id + ":" + std::string(facet),
      .source_version = "fnv1a64:" + stable_fingerprint(canonical_payload),
  };
}

KnowledgeNode make_node(KnowledgeNodeKind kind, std::string_view canonical_key,
                        KnowledgeProperties properties = {}) {
  KnowledgeNode node;
  node.kind = kind;
  node.assertion_kind = KnowledgeAssertionKind::kFact;
  node.id = make_knowledge_node_id(to_string(kind), canonical_key);
  node.properties = std::move(properties);
  return node;
}

KnowledgeEdge make_edge(const KnowledgeNode& from, KnowledgeEdgeKind kind,
                        const KnowledgeNode& to,
                        KnowledgeProperties properties = {},
                        std::string_view discriminator = {}) {
  KnowledgeEdge edge;
  edge.from = from.id;
  edge.to = to.id;
  edge.kind = kind;
  edge.id = make_knowledge_edge_id(from.id, to_string(kind), to.id, discriminator);
  edge.properties = std::move(properties);
  return edge;
}

class ProjectionWriter {
 public:
  ProjectionWriter(GraphStore& graph, DependencyTracker& dependencies,
                   StatisticsGraphProjectionReport& report,
                   std::int64_t observed_at_ms)
      : graph_(graph), dependencies_(dependencies), report_(report),
        observed_at_ms_(observed_at_ms) {}

  void begin_source(const KnowledgeSourceRef& source) {
    const auto invalidated = dependencies_.invalidate_source_change(
        source.source_type, source.source_id, source.source_version,
        observed_at_ms_);
    report_.invalidated_entries.insert(report_.invalidated_entries.end(),
                                       invalidated.begin(), invalidated.end());
    source_ = source;
  }

  void node(const KnowledgeNode& value) {
    const KnowledgeEntryRef entry{KnowledgeEntryKind::kNode, value.id.value};
    const bool graph_changed = graph_.upsert_node(value);
    const bool dependency_changed = attach(entry);
    if (graph_changed) ++report_.nodes_upserted;
    if (graph_changed || dependency_changed) report_.changed_entries.push_back(entry);
  }

  void edge(const KnowledgeEdge& value) {
    const KnowledgeEntryRef entry{KnowledgeEntryKind::kEdge, value.id.value};
    const bool graph_changed = graph_.upsert_edge(value);
    const bool dependency_changed = attach(entry);
    if (graph_changed) ++report_.edges_upserted;
    if (graph_changed || dependency_changed) report_.changed_entries.push_back(entry);
  }

 private:
  bool attach(const KnowledgeEntryRef& entry) {
    if (!source_) throw std::logic_error("statistics projection source not selected");
    const std::vector<KnowledgeSourceRef> sources{*source_};
    if (dependencies_.dependencies_match(entry, sources)) return false;
    dependencies_.replace_provenance(entry, sources, observed_at_ms_);
    dependencies_.replace_dependencies(entry, sources, observed_at_ms_);
    return true;
  }

  GraphStore& graph_;
  DependencyTracker& dependencies_;
  StatisticsGraphProjectionReport& report_;
  std::int64_t observed_at_ms_;
  std::optional<KnowledgeSourceRef> source_;
};

KnowledgeNode player_node(const std::string& profile_id) {
  return make_node(KnowledgeNodeKind::kPlayer, "profile:" + profile_id,
                   {{"profile_id", profile_id}});
}

KnowledgeNode statistic_node(const std::string& profile_id,
                             std::string_view statistic_key,
                             KnowledgeProperties properties) {
  properties.emplace("profile_id", profile_id);
  properties.emplace("statistic_key", std::string(statistic_key));
  return make_node(KnowledgeNodeKind::kStatistic,
                   "profile:" + profile_id + ":" + std::string(statistic_key),
                   std::move(properties));
}

void connect_statistic(ProjectionWriter& writer, const KnowledgeNode& player,
                       const KnowledgeNode& statistic) {
  writer.node(statistic);
  writer.edge(make_edge(player, KnowledgeEdgeKind::kHasStatistics, statistic));
}

}  // namespace

StatisticsGraphProjector::StatisticsGraphProjector(
    Database& database, StatisticsService& statistics, GraphStore& graph,
    DependencyTracker& dependencies)
    : database_(database), statistics_(statistics), graph_(graph),
      dependencies_(dependencies) {}

StatisticsGraphProjectionReport StatisticsGraphProjector::project_active_profile(
    const std::int64_t observed_at_ms) {
  StatisticsGraphProjectionReport report;
  const auto active = database_.active_profile();
  if (!active) return report;
  const auto owner = database_.player_profile_owner_id(active->id);
  report.profile_id = owner;
  const auto facts = Json::parse(statistics_.player_knowledge_json(owner));
  ProjectionWriter writer(graph_, dependencies_, report, observed_at_ms);
  const auto player = player_node(owner);
  if (!graph_.node(player.id)) graph_.upsert_node(player);
  for (const auto& fact : facts) {
    KnowledgeProperties properties;
    for (auto it = fact.begin(); it != fact.end(); ++it) {
      if (it.value().is_string()) properties[it.key()] = it.value().get<std::string>();
      else if (it.value().is_boolean()) properties[it.key()] = it.value().get<bool>();
      else if (it.value().is_number_integer()) properties[it.key()] = it.value().get<std::int64_t>();
      else if (it.value().is_number_float()) properties[it.key()] = it.value().get<double>();
    }
    const auto key = fact.at("statistic_key").get<std::string>();
    writer.begin_source(statistics_source(owner, key, fact.dump()));
    const auto statistic = statistic_node(owner, key, std::move(properties));
    connect_statistic(writer, player, statistic);
    const auto connect_context = [&](KnowledgeNode context, KnowledgeEdgeKind relation) {
      // Shared semantic entities may already have richer topology/provenance.
      if (!graph_.node(context.id)) graph_.upsert_node(context);
      writer.edge(relation == KnowledgeEdgeKind::kHasStatistics
          ? make_edge(context, relation, statistic, {}, "statistics_context")
          : make_edge(statistic, relation, context, {}, "statistics_context"));
    };
    if (fact.contains("opening_name")) {
      const auto name = fact.value("opening_name", "");
      const auto eco = fact.value("opening_eco", "");
      const auto family = fact.value("opening_family", "");
      connect_context(make_node(KnowledgeNodeKind::kOpening,
          "opening:" + eco + ":" + name, {{"name", name}, {"eco", eco}}),
          KnowledgeEdgeKind::kHasStatistics);
      if (!family.empty()) connect_context(make_node(KnowledgeNodeKind::kOpeningFamily,
          "family:" + family, {{"name", family}}), KnowledgeEdgeKind::kDependsOn);
    }
    if (fact.contains("time_control")) {
      const auto tc = fact.value("time_control", "");
      connect_context(make_node(KnowledgeNodeKind::kTimeControl,
          "time_control:" + tc, {{"type", tc}}), KnowledgeEdgeKind::kDependsOn);
    }
    if (fact.contains("player_color")) {
      const auto color = fact.value("player_color", "");
      connect_context(make_node(KnowledgeNodeKind::kColor,
          "color:" + color, {{"color", color}}), KnowledgeEdgeKind::kDependsOn);
    }
    if (fact.contains("termination_type")) {
      const auto type = fact.value("termination_type", "unknown");
      connect_context(make_node(KnowledgeNodeKind::kTerminationType,
          "termination:" + type, {{"type", type}}), KnowledgeEdgeKind::kDependsOn);
    }
    if (fact.contains("sourceAccountId")) {
      const auto account_id = fact.value("sourceAccountId", "");
      const auto provider_name = fact.value("sourceProvider", "");
      const auto account = make_node(KnowledgeNodeKind::kAccount, "account:" + account_id,
          {{"profile_id", owner}, {"account_id", account_id},
           {"name", fact.value("sourceAccount", "")}});
      const auto provider = make_node(KnowledgeNodeKind::kProvider,
          "provider:" + provider_name, {{"name", provider_name}});
      if (!graph_.node(account.id)) graph_.upsert_node(account);
      if (!graph_.node(provider.id)) graph_.upsert_node(provider);
      writer.edge(make_edge(player, KnowledgeEdgeKind::kContains, account));
      writer.edge(make_edge(account, KnowledgeEdgeKind::kDependsOn, provider));
      writer.edge(make_edge(account, KnowledgeEdgeKind::kHasStatistics, statistic));
    }
  }
  return report;
}
}  // namespace kchess::knowledge
