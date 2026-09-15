#include "opening_graph_projector.h"

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "persistence/database.h"
#include "theory/opening_theory_provider.h"
#include "theory/position_key.h"

namespace kchess::knowledge {
namespace {

// -----------------------------------------------------------------------------
// Section: Stable opening identities
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

std::string trim(std::string value) {
  const auto first = value.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) return {};
  const auto last = value.find_last_not_of(" \t\r\n");
  return value.substr(first, last - first + 1);
}

std::string opening_family(std::string_view name) {
  const auto separator = name.find_first_of(":,");
  return trim(std::string(name.substr(0, separator)));
}

std::string position_key_string(const std::string& fen) {
  const auto key = stockfish_position_key(fen);
  std::ostringstream out;
  out << "sf18:" << std::hex << std::setfill('0') << std::setw(16) << key;
  return out.str();
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

KnowledgeNode position_node(const std::string& fen) {
  const std::string key = position_key_string(fen);
  return make_node(KnowledgeNodeKind::kPosition, "position:" + key,
                   {{"position_key", key}});
}

KnowledgeSourceRef game_source(const GameRecord& game, std::size_t opening_limit) {
  std::ostringstream canonical;
  canonical << game.id << '|' << game.opening_eco.value_or("") << '|'
            << game.opening_name.value_or("") << '|'
            << game.opening_ply.value_or(0) << '|';
  const auto limit = std::min(opening_limit, game.moves.size());
  for (std::size_t index = 0; index < limit; ++index) {
    canonical << index << ':' << game.moves[index].uci << ':'
              << game.moves[index].fen_after << ';';
  }
  return KnowledgeSourceRef{
      .source_type = "games_db_opening",
      .source_id = "game:" + game.id,
      .source_version = "fnv1a64:" + stable_fingerprint(canonical.str()),
  };
}

KnowledgeSourceRef manifest_source(const std::string& profile_id,
                                   const std::vector<KnowledgeSourceRef>& games) {
  std::vector<std::string> parts;
  parts.reserve(games.size());
  for (const auto& source : games) {
    parts.push_back(source.source_id + "@" + source.source_version);
  }
  std::sort(parts.begin(), parts.end());
  std::ostringstream canonical;
  for (const auto& part : parts) canonical << part << ';';
  return KnowledgeSourceRef{
      .source_type = "opening_graph_manifest",
      .source_id = "profile:" + profile_id,
      .source_version = "fnv1a64:" + stable_fingerprint(canonical.str()),
  };
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

std::string source_key(const KnowledgeSourceRef& source) {
  return source.source_type + "\n" + source.source_id;
}

class ProjectionAccumulator {
 public:
  void node(KnowledgeNode value, const KnowledgeSourceRef& source) {
    const std::string id = value.id.value;
    auto found = nodes_.find(id);
    if (found == nodes_.end()) {
      found = nodes_.emplace(
          id, PendingNode{.node = std::move(value), .sources = {}}).first;
    } else {
      found->second.node = std::move(value);
    }
    found->second.sources[source_key(source)] = source;
  }

  void edge(KnowledgeEdge value, const KnowledgeSourceRef& source) {
    const std::string id = value.id.value;
    auto found = edges_.find(id);
    if (found == edges_.end()) {
      found = edges_.emplace(
          id, PendingEdge{.edge = std::move(value), .sources = {}}).first;
    } else {
      found->second.edge = std::move(value);
    }
    found->second.sources[source_key(source)] = source;
  }

  [[nodiscard]] const std::map<std::string, PendingNode>& nodes() const {
    return nodes_;
  }
  [[nodiscard]] const std::map<std::string, PendingEdge>& edges() const {
    return edges_;
  }

 private:
  std::map<std::string, PendingNode> nodes_;
  std::map<std::string, PendingEdge> edges_;
};

std::vector<KnowledgeSourceRef> source_values(
    const std::map<std::string, KnowledgeSourceRef>& values,
    const KnowledgeSourceRef& manifest) {
  std::map<std::string, KnowledgeSourceRef> merged = values;
  merged[source_key(manifest)] = manifest;
  std::vector<KnowledgeSourceRef> result;
  result.reserve(merged.size());
  for (const auto& [_, source] : merged) result.push_back(source);
  return result;
}

int theory_end_ply(const OpeningTheoryProvider* theory, const GameRecord& game,
                   std::size_t opening_limit) {
  if (theory == nullptr) return -1;
  int last = -1;
  const auto limit = std::min(opening_limit, game.moves.size());
  for (std::size_t index = 0; index < limit; ++index) {
    const auto& move = game.moves[index];
    try {
      if (!theory->contains_move(move.fen_before, move.uci)) break;
      last = static_cast<int>(index + 1);
    } catch (...) {
      break;
    }
  }
  return last;
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Opening graph projection
// -----------------------------------------------------------------------------

OpeningGraphProjector::OpeningGraphProjector(
    Database& database, GraphStore& graph, DependencyTracker& dependencies,
    const OpeningTheoryProvider* theory)
    : database_(database), graph_(graph), dependencies_(dependencies),
      theory_(theory) {}

OpeningGraphProjectionReport OpeningGraphProjector::project_active_profile(
    const std::int64_t observed_at_ms) {
  OpeningGraphProjectionReport report;
  const auto profile = database_.active_profile();
  if (!profile) return report;
  const std::string profile_id = database_.player_profile_owner_id(profile->id);
  report.profile_id = profile_id;

  const auto source_rows = database_.player_profile_game_sources(profile_id);
  report.games_considered = source_rows.size();

  ProjectionAccumulator pending;
  std::vector<KnowledgeSourceRef> game_sources;
  std::optional<KnowledgeSourceRef> theory_source;
  if (theory_ != nullptr) {
    const std::string version = theory_->source_version();
    if (!version.empty() && version != "not-installed") {
      theory_source = KnowledgeSourceRef{
          .source_type = "opening_theory",
          .source_id = "global:opening_theory",
          .source_version = version,
      };
      const auto invalidated = dependencies_.invalidate_source_change(
          theory_source->source_type, theory_source->source_id,
          theory_source->source_version, observed_at_ms);
      report.invalidated_entries.insert(report.invalidated_entries.end(),
                                        invalidated.begin(), invalidated.end());
    }
  }
  std::map<std::string, std::vector<KnowledgeNode>> variations_by_endpoint;
  std::map<std::string, std::int64_t> opening_counts;
  std::map<std::string, KnowledgeNode> openings_by_id;

  // Full game moves are loaded only for games that already have a persisted
  // named opening. KCO classification remains the source of opening identity.
  for (const auto& source_row : source_rows) {
    const auto game_value = database_.game(source_row.game_id);
    if (!game_value || game_value->moves.empty() || !game_value->opening_name ||
        game_value->opening_name->empty() || !game_value->opening_ply ||
        *game_value->opening_ply <= 0) {
      continue;
    }
    const auto& game = *game_value;
    ++report.named_games;

    const std::size_t opening_limit = std::min<std::size_t>(
        game.moves.size(), static_cast<std::size_t>(*game.opening_ply));
    if (opening_limit == 0) continue;

    const auto source = game_source(game, opening_limit);
    game_sources.push_back(source);
    const auto invalidated = dependencies_.invalidate_source_change(
        source.source_type, source.source_id, source.source_version,
        observed_at_ms);
    report.invalidated_entries.insert(report.invalidated_entries.end(),
                                      invalidated.begin(), invalidated.end());

    const std::string name = *game.opening_name;
    const std::string eco = game.opening_eco.value_or("");
    const std::string family_name = opening_family(name);

    const auto family = make_node(
        KnowledgeNodeKind::kOpeningFamily,
        "family:" + family_name,
        {{"name", family_name}});
    const auto opening = make_node(
        KnowledgeNodeKind::kOpening,
        "opening:" + eco + ":" + name,
        {{"eco", eco}, {"name", name}, {"family", family_name}});

    std::ostringstream sequence;
    for (std::size_t index = 0; index < opening_limit; ++index) {
      if (index != 0) sequence << ' ';
      sequence << game.moves[index].uci;
    }
    const std::string sequence_text = sequence.str();
    const std::string endpoint_key = position_key_string(
        game.moves[opening_limit - 1].fen_after);
    const int theory_ply = theory_end_ply(theory_, game, opening_limit);

    KnowledgeProperties variation_properties{
        {"eco", eco},
        {"name", name},
        {"family", family_name},
        {"move_order_uci", sequence_text},
        {"ply_count", static_cast<std::int64_t>(opening_limit)},
        {"endpoint_position_key", endpoint_key},
    };
    if (theory_ply >= 0) {
      variation_properties.emplace("theory_end_ply",
                                   static_cast<std::int64_t>(theory_ply));
    }
    const auto variation = make_node(
        KnowledgeNodeKind::kVariation,
        "variation:" + eco + ":" + name + ":" +
            stable_fingerprint(sequence_text),
        std::move(variation_properties));

    const auto game_node = make_node(
        KnowledgeNodeKind::kGame, "game:" + game.id,
        {{"game_id", game.id}, {"profile_id", profile_id}});

    pending.node(family, source);
    pending.node(opening, source);
    pending.node(variation, source);
    if (theory_source && theory_ply >= 0) {
      pending.node(variation, *theory_source);
    }
    // Game is a shared routing identity. It is written here so edge endpoints
    // exist, but its ownership is not used to duplicate the game payload.
    graph_.upsert_node(game_node);

    pending.edge(make_edge(family, KnowledgeEdgeKind::kContains, opening,
                           {{"projection", std::string("opening_graph")}},
                           "opening_graph"),
                 source);
    pending.edge(make_edge(opening, KnowledgeEdgeKind::kContains, variation,
                           {{"projection", std::string("opening_graph")}},
                           "opening_graph"),
                 source);
    pending.edge(make_edge(game_node, KnowledgeEdgeKind::kPlays, variation,
                           {{"projection", std::string("opening_graph")}},
                           "opening_graph"),
                 source);

    std::optional<KnowledgeNode> previous_position;
    for (std::size_t index = 0; index < opening_limit; ++index) {
      const auto& move = game.moves[index];
      KnowledgeNode before;
      if (index == 0) {
        before = position_node(move.fen_before);
        pending.node(before, source);
      } else {
        before = *previous_position;
      }
      const auto after = position_node(move.fen_after);
      pending.node(after, source);
      pending.edge(make_edge(
                       before, KnowledgeEdgeKind::kPrecedes, after,
                       {{"projection", std::string("opening_graph")},
                        {"uci", move.uci},
                        {"san", move.san},
                        {"ply", static_cast<std::int64_t>(index + 1)}},
                       "opening_graph:" + move.uci),
                   source);
      previous_position = after;
    }

    if (previous_position) {
      pending.edge(make_edge(
                       variation, KnowledgeEdgeKind::kReaches,
                       *previous_position,
                       {{"projection", std::string("opening_graph")},
                        {"opening_endpoint", true}},
                       "opening_graph"),
                   source);
      pending.edge(make_edge(
                       game_node, KnowledgeEdgeKind::kReaches,
                       *previous_position,
                       {{"projection", std::string("opening_graph")},
                        {"opening_endpoint", true},
                        {"ply", static_cast<std::int64_t>(opening_limit)}},
                       "opening_graph"),
                   source);
      variations_by_endpoint[endpoint_key].push_back(variation);
    }

    openings_by_id[opening.id.value] = opening;
    ++opening_counts[opening.id.value];
  }

  const auto manifest = manifest_source(profile_id, game_sources);
  const auto manifest_invalidated = dependencies_.invalidate_source_change(
      manifest.source_type, manifest.source_id, manifest.source_version,
      observed_at_ms);
  report.invalidated_entries.insert(report.invalidated_entries.end(),
                                    manifest_invalidated.begin(),
                                    manifest_invalidated.end());

  // Different move-order Variation nodes that converge on the same canonical
  // endpoint are explicit transpositions. Use a deterministic star instead of
  // an O(n^2) clique while preserving graph connectivity between all orders.
  for (auto& [_, variations] : variations_by_endpoint) {
    std::sort(variations.begin(), variations.end(),
              [](const KnowledgeNode& left, const KnowledgeNode& right) {
                return left.id.value < right.id.value;
              });
    variations.erase(
        std::unique(variations.begin(), variations.end(),
                    [](const KnowledgeNode& left, const KnowledgeNode& right) {
                      return left.id == right.id;
                    }),
        variations.end());
    if (variations.size() < 2) continue;
    const auto& canonical = variations.front();
    for (std::size_t index = 1; index < variations.size(); ++index) {
      // The manifest is sufficient provenance for cross-game transposition
      // relations because the relation is derived from the current profile-wide
      // set of move orders, not from one authoritative game alone.
      pending.edge(make_edge(
                       variations[index], KnowledgeEdgeKind::kTranspositionOf,
                       canonical,
                       {{"projection", std::string("opening_graph")},
                        {"same_endpoint", true}},
                       "opening_graph"),
                   manifest);
      ++report.transposition_links;
    }
  }

  // Materialize aggregated player -> opening usage without recomputing any game
  // result statistic. This edge only records observed opening frequency.
  const auto player = make_node(KnowledgeNodeKind::kPlayer,
                                "profile:" + profile_id,
                                {{"profile_id", profile_id}});
  graph_.upsert_node(player);
  for (const auto& [opening_id, count] : opening_counts) {
    const auto found = openings_by_id.find(opening_id);
    if (found == openings_by_id.end()) continue;
    pending.edge(make_edge(
                     player, KnowledgeEdgeKind::kPlays, found->second,
                     {{"projection", std::string("opening_graph")},
                      {"sample_size", count}},
                     "opening_graph"),
                 manifest);
  }

  // Persist the new projection and attach both the profile manifest and the
  // exact contributing game sources to shared semantic entries.
  std::set<std::string> live_entries;
  for (const auto& [id, value] : pending.nodes()) {
    const auto sources = source_values(value.sources, manifest);
    const KnowledgeEntryRef entry{KnowledgeEntryKind::kNode, id};
    const bool graph_changed = graph_.upsert_node(value.node);
    const bool dependency_changed = !dependencies_.dependencies_match(entry, sources);
    if (dependency_changed) {
      dependencies_.replace_provenance(entry, sources, observed_at_ms);
      dependencies_.replace_dependencies(entry, sources, observed_at_ms);
    }
    live_entries.insert("node\n" + id);
    if (graph_changed) ++report.nodes_upserted;
    if (graph_changed || dependency_changed) report.changed_entries.push_back(entry);
  }
  for (const auto& [id, value] : pending.edges()) {
    const auto sources = source_values(value.sources, manifest);
    const KnowledgeEntryRef entry{KnowledgeEntryKind::kEdge, id};
    const bool graph_changed = graph_.upsert_edge(value.edge);
    const bool dependency_changed = !dependencies_.dependencies_match(entry, sources);
    if (dependency_changed) {
      dependencies_.replace_provenance(entry, sources, observed_at_ms);
      dependencies_.replace_dependencies(entry, sources, observed_at_ms);
    }
    live_entries.insert("edge\n" + id);
    if (graph_changed) ++report.edges_upserted;
    if (graph_changed || dependency_changed) report.changed_entries.push_back(entry);
  }

  // If the manifest changed, edges from the previous opening projection that no
  // longer exist are safe to remove. Nodes are intentionally retained because
  // they may already be shared by later graph layers; their stale dependency
  // state remains available for targeted refresh/diagnostics.
  for (const auto& invalidation : manifest_invalidated) {
    if (invalidation.entry.kind != KnowledgeEntryKind::kEdge) continue;
    if (live_entries.contains("edge\n" + invalidation.entry.id)) continue;
    if (graph_.remove_edge(KnowledgeEdgeId{invalidation.entry.id})) {
      report.removed_entries.push_back(invalidation.entry);
    }
  }

  return report;
}

}  // namespace kchess::knowledge
