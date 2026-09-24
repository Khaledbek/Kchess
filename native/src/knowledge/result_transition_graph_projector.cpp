#include "result_transition_graph_projector.h"

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

#include "chess/pgn.h"
#include "persistence/database.h"
#include "services/termination.h"
#include "theory/position_key.h"

namespace kchess::knowledge {
namespace {

// -----------------------------------------------------------------------------
// Section: Stable identities and compact source versions
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
  edge.id = make_knowledge_edge_id(from.id, to_string(kind), to.id,
                                   discriminator);
  edge.properties = std::move(properties);
  return edge;
}

std::string position_key_string(const std::string& fen) {
  const auto key = stockfish_position_key(fen);
  std::ostringstream out;
  out << "sf18:" << std::hex << std::setfill('0') << std::setw(16) << key;
  return out.str();
}

KnowledgeNode position_node(const std::string& fen) {
  const auto key = position_key_string(fen);
  return make_node(KnowledgeNodeKind::kPosition, "position:" + key,
                   {{"position_key", key}});
}

KnowledgeNode game_node(const std::string& profile_id,
                        const std::string& game_id) {
  return make_node(KnowledgeNodeKind::kGame, "game:" + game_id,
                   {{"game_id", game_id}, {"profile_id", profile_id}});
}

KnowledgeNode result_node(std::string_view outcome) {
  return make_node(KnowledgeNodeKind::kResultType,
                   "result_type:" + std::string(outcome),
                   {{"outcome", std::string(outcome)}});
}

KnowledgeNode termination_node(std::string_view termination) {
  return make_node(KnowledgeNodeKind::kTerminationType,
                   "termination_type:" + std::string(termination),
                   {{"termination", std::string(termination)}});
}

KnowledgeSourceRef game_source(const PlayerProfileGameSourceRow& source,
                               const GameRecord& game,
                               const std::vector<PlayerProfileMoveSourceRow>& moves) {
  std::ostringstream canonical;
  canonical << source.game_id << '|' << source.source_version << '|'
            << source.provider_outcome << '|' << source.player_color << '|'
            << game.result << '|' << termination_bucket(game.pgn, game.result)
            << '|' << game.time_control << '|';
  for (const auto& move : moves) {
    canonical << move.ply << ':' << move.classification << ':';
    if (move.expected_score_before) canonical << *move.expected_score_before;
    canonical << ':';
    if (move.expected_score_played) canonical << *move.expected_score_played;
    canonical << ':';
    if (move.expected_score_loss) canonical << *move.expected_score_loss;
    canonical << ';';
  }
  const auto clocks = extract_mainline_clock_millis(game.pgn, game.moves.size());
  for (std::size_t i = 0; i < clocks.size(); ++i) {
    if (clocks[i]) canonical << i << '=' << *clocks[i] << ';';
  }
  return {.source_type = "result_transition_game",
          .source_id = "game:" + source.game_id,
          .source_version = "fnv1a64:" + stable_fingerprint(canonical.str())};
}

KnowledgeSourceRef manifest_source(
    const std::string& profile_id,
    const std::vector<KnowledgeSourceRef>& game_sources) {
  std::vector<std::string> values;
  values.reserve(game_sources.size());
  for (const auto& source : game_sources) {
    values.push_back(source.source_id + "@" + source.source_version);
  }
  std::sort(values.begin(), values.end());
  std::ostringstream canonical;
  for (const auto& value : values) canonical << value << ';';
  return {.source_type = "result_transition_manifest",
          .source_id = "profile:" + profile_id,
          .source_version = "fnv1a64:" + stable_fingerprint(canonical.str())};
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
    const auto id = value.id.value;
    auto [it, inserted] = nodes_.emplace(
        id, PendingNode{.node = value, .sources = {}});
    if (!inserted) it->second.node = std::move(value);
    it->second.sources[source_key(source)] = source;
  }

  void edge(KnowledgeEdge value, const KnowledgeSourceRef& source) {
    const auto id = value.id.value;
    auto [it, inserted] = edges_.emplace(
        id, PendingEdge{.edge = value, .sources = {}});
    if (!inserted) it->second.edge = std::move(value);
    it->second.sources[source_key(source)] = source;
  }

  const auto& nodes() const { return nodes_; }
  const auto& edges() const { return edges_; }

 private:
  std::map<std::string, PendingNode> nodes_;
  std::map<std::string, PendingEdge> edges_;
};

std::vector<KnowledgeSourceRef> source_values(
    const std::map<std::string, KnowledgeSourceRef>& values,
    const KnowledgeSourceRef& manifest) {
  auto merged = values;
  merged[source_key(manifest)] = manifest;
  std::vector<KnowledgeSourceRef> result;
  result.reserve(merged.size());
  for (const auto& [_, source] : merged) result.push_back(source);
  return result;
}

// -----------------------------------------------------------------------------
// Section: Existing-graph structure lookup
// -----------------------------------------------------------------------------

std::optional<KnowledgeNode> structure_for_position(
    const GraphStore& graph, const std::string& fen,
    KnowledgeNodeKind desired_kind) {
  const auto position = position_node(fen);
  for (const auto& step : graph.adjacent(position.id, GraphDirection::kOutgoing,
                                         KnowledgeEdgeKind::kHasStructure, 32)) {
    if (step.node.kind == desired_kind) return step.node;
  }
  return std::nullopt;
}

std::optional<KnowledgeNode> first_structure_after(
    const GraphStore& graph, const GameRecord& game, std::size_t start,
    KnowledgeNodeKind desired_kind) {
  for (std::size_t i = start; i < game.moves.size(); ++i) {
    if (const auto structure =
            structure_for_position(graph, game.moves[i].fen_after, desired_kind)) {
      return structure;
    }
  }
  return std::nullopt;
}

// -----------------------------------------------------------------------------
// Section: Time and analysis-derived facts
// -----------------------------------------------------------------------------

std::optional<std::int64_t> initial_clock_millis(std::string_view tc) {
  if (tc.empty() || tc == "-") return std::nullopt;
  const auto plus = tc.find('+');
  const auto pipe = tc.find('|');
  const auto end = std::min(plus == std::string_view::npos ? tc.size() : plus,
                            pipe == std::string_view::npos ? tc.size() : pipe);
  try {
    const auto seconds = std::stoll(std::string(tc.substr(0, end)));
    if (seconds <= 0) return std::nullopt;
    return seconds * 1000;
  } catch (...) {
    return std::nullopt;
  }
}

struct TimeTroubleObservation {
  bool present{false};
  std::int64_t minimum_clock_ms{0};
  int affected_moves{0};
};

TimeTroubleObservation time_trouble(const GameRecord& game,
                                    std::string_view player_color) {
  TimeTroubleObservation result;
  const auto clocks = extract_mainline_clock_millis(game.pgn, game.moves.size());
  const auto initial = initial_clock_millis(game.time_control);
  const std::int64_t threshold = initial
      ? std::max<std::int64_t>(10000, *initial / 10)
      : 30000;
  std::optional<std::int64_t> minimum;
  for (std::size_t i = 0; i < game.moves.size() && i < clocks.size(); ++i) {
    if (game.moves[i].side_to_move != player_color || !clocks[i]) continue;
    if (!minimum || *clocks[i] < *minimum) minimum = *clocks[i];
    if (*clocks[i] <= threshold) ++result.affected_moves;
  }
  if (minimum) result.minimum_clock_ms = *minimum;
  result.present = result.affected_moves >= 2;
  return result;
}

KnowledgeNode time_pattern_node(const TimeTroubleObservation& time) {
  const std::string severity = time.minimum_clock_ms <= 10000 ? "severe" : "low_clock";
  return make_node(KnowledgeNodeKind::kTimeManagementPattern,
                   "time_management:time_trouble:" + severity,
                   {{"pattern", std::string("time_trouble")},
                    {"severity", severity}});
}

KnowledgeNode conversion_pattern_node(int attempts, int failures) {
  const std::string state = failures == 0 ? "held_advantage" : "dropped_advantage";
  return make_node(KnowledgeNodeKind::kConversionPattern,
                   "conversion:" + state,
                   {{"pattern", state},
                    {"attempts", static_cast<std::int64_t>(attempts)},
                    {"failures", static_cast<std::int64_t>(failures)}});
}

KnowledgeNode defense_pattern_node(int attempts, int saves) {
  const std::string state = saves > 0 ? "resourceful_defense" : "under_pressure";
  return make_node(KnowledgeNodeKind::kDefensePattern,
                   "defense:" + state,
                   {{"pattern", state},
                    {"attempts", static_cast<std::int64_t>(attempts)},
                    {"saves", static_cast<std::int64_t>(saves)}});
}

void project_analysis_patterns(ProjectionAccumulator& pending,
                               const KnowledgeNode& game,
                               const std::vector<PlayerProfileMoveSourceRow>& moves,
                               const KnowledgeSourceRef& source,
                               ResultTransitionProjectionReport& report) {
  int conversion_attempts = 0;
  int conversion_failures = 0;
  int defense_attempts = 0;
  int defensive_saves = 0;

  for (const auto& move : moves) {
    if (!move.expected_score_before || !move.expected_score_played ||
        move.fen_before.empty()) {
      continue;
    }
    const double before = *move.expected_score_before;
    const double played = *move.expected_score_played;
    const auto position = position_node(move.fen_before);
    pending.node(position, source);

    if (before >= 0.75) {
      ++conversion_attempts;
      if (played <= 0.55) {
        ++conversion_failures;
        pending.edge(make_edge(game, KnowledgeEdgeKind::kMissedWin, position,
                               {{"ply", static_cast<std::int64_t>(move.ply)},
                                {"expected_score_before", before},
                                {"expected_score_played", played},
                                {"projection", std::string("result_transition_graph")}},
                               "result_transition_graph:missed_win:" +
                                   std::to_string(move.ply)),
                     source);
        ++report.missed_wins;
      }
    }
    if (before <= 0.25) {
      ++defense_attempts;
      if (played >= 0.45) {
        ++defensive_saves;
        pending.edge(make_edge(game, KnowledgeEdgeKind::kSavedFrom, position,
                               {{"ply", static_cast<std::int64_t>(move.ply)},
                                {"expected_score_before", before},
                                {"expected_score_played", played},
                                {"projection", std::string("result_transition_graph")}},
                               "result_transition_graph:saved_loss:" +
                                   std::to_string(move.ply)),
                     source);
        ++report.saved_losses;
      }
    }
  }

  if (conversion_attempts > 0) {
    const auto pattern = conversion_pattern_node(conversion_attempts,
                                                  conversion_failures);
    pending.node(pattern, source);
    pending.edge(make_edge(game, KnowledgeEdgeKind::kContains, pattern,
                           {{"projection", std::string("result_transition_graph")},
                            {"attempts", static_cast<std::int64_t>(conversion_attempts)},
                            {"failures", static_cast<std::int64_t>(conversion_failures)}},
                           "result_transition_graph:conversion"),
                 source);
  }
  if (defense_attempts > 0) {
    const auto pattern = defense_pattern_node(defense_attempts, defensive_saves);
    pending.node(pattern, source);
    pending.edge(make_edge(game, KnowledgeEdgeKind::kContains, pattern,
                           {{"projection", std::string("result_transition_graph")},
                            {"attempts", static_cast<std::int64_t>(defense_attempts)},
                            {"saves", static_cast<std::int64_t>(defensive_saves)}},
                           "result_transition_graph:defense"),
                 source);
  }
}

void project_phase_transitions(ProjectionAccumulator& pending,
                               const GraphStore& graph,
                               const GameRecord& game,
                               const KnowledgeSourceRef& source) {
  const std::size_t opening_end = game.opening_ply && *game.opening_ply > 0
      ? std::min<std::size_t>(game.moves.size(), *game.opening_ply)
      : 0;

  std::optional<KnowledgeNode> middlegame;
  if (opening_end < game.moves.size()) {
    middlegame = first_structure_after(graph, game, opening_end,
                                       KnowledgeNodeKind::kMiddlegameStructure);
  }
  const auto endgame = first_structure_after(graph, game, opening_end,
                                              KnowledgeNodeKind::kEndgameType);

  if (game.opening_name && !game.opening_name->empty() && middlegame) {
    const auto opening = make_node(
        KnowledgeNodeKind::kOpening,
        "opening:" + game.opening_eco.value_or("") + ":" + *game.opening_name,
        {{"eco", game.opening_eco.value_or("")}, {"name", *game.opening_name}});
    pending.node(opening, source);
    pending.node(*middlegame, source);
    pending.edge(make_edge(opening, KnowledgeEdgeKind::kTransitionsTo,
                           *middlegame,
                           {{"projection", std::string("result_transition_graph")}},
                           "result_transition_graph:opening_to_middlegame"),
                 source);
  }
  if (middlegame && endgame) {
    pending.node(*middlegame, source);
    pending.node(*endgame, source);
    pending.edge(make_edge(*middlegame, KnowledgeEdgeKind::kTransitionsTo,
                           *endgame,
                           {{"projection", std::string("result_transition_graph")}},
                           "result_transition_graph:middlegame_to_endgame"),
                 source);
  }
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Result/time/transition graph projection
// -----------------------------------------------------------------------------

ResultTransitionGraphProjector::ResultTransitionGraphProjector(
    Database& database, GraphStore& graph, DependencyTracker& dependencies)
    : database_(database), graph_(graph), dependencies_(dependencies) {}

ResultTransitionProjectionReport
ResultTransitionGraphProjector::project_active_profile(
    const std::int64_t observed_at_ms) {
  ResultTransitionProjectionReport report;
  const auto profile = database_.active_profile();
  if (!profile) return report;
  const std::string profile_id = database_.player_profile_owner_id(profile->id);
  report.profile_id = profile_id;

  const auto source_rows = database_.player_profile_game_sources(profile_id);
  report.games_considered = source_rows.size();
  ProjectionAccumulator pending;
  std::vector<KnowledgeSourceRef> sources;

  for (const auto& source_row : source_rows) {
    const auto stored_game = database_.game(source_row.game_id);
    if (!stored_game) continue;
    const auto moves = database_.player_profile_move_sources(
        source_row.game_id, source_row.player_color);
    const auto source = game_source(source_row, *stored_game, moves);
    sources.push_back(source);

    const auto invalidated = dependencies_.invalidate_source_change(
        source.source_type, source.source_id, source.source_version,
        observed_at_ms);
    report.invalidated_entries.insert(report.invalidated_entries.end(),
                                      invalidated.begin(), invalidated.end());

    const auto game = game_node(profile_id, source_row.game_id);
    const auto outcome = result_node(source_row.provider_outcome);
    const auto termination = termination_node(
        termination_bucket(stored_game->pgn, stored_game->result));
    pending.node(game, source);
    pending.node(outcome, source);
    pending.node(termination, source);

    pending.edge(make_edge(game, KnowledgeEdgeKind::kHasStatistics, outcome,
                           {{"projection", std::string("result_transition_graph")}},
                           "result_transition_graph:outcome"),
                 source);

    KnowledgeEdgeKind termination_edge = KnowledgeEdgeKind::kExplainedBy;
    if (source_row.provider_outcome == "loss") {
      termination_edge = KnowledgeEdgeKind::kLostBy;
    } else if (source_row.provider_outcome == "win") {
      termination_edge = KnowledgeEdgeKind::kWonBy;
    }
    pending.edge(make_edge(game, termination_edge, termination,
                           {{"projection", std::string("result_transition_graph")}},
                           "result_transition_graph:termination"),
                 source);

    const auto time = time_trouble(*stored_game, source_row.player_color);
    if (time.present) {
      const auto pattern = time_pattern_node(time);
      pending.node(pattern, source);
      pending.edge(make_edge(game, KnowledgeEdgeKind::kContains, pattern,
                             {{"projection", std::string("result_transition_graph")},
                              {"minimum_clock_ms", time.minimum_clock_ms},
                              {"affected_moves", static_cast<std::int64_t>(time.affected_moves)}},
                             "result_transition_graph:time_trouble"),
                   source);
      if (source_row.provider_outcome == "loss") {
        pending.edge(make_edge(pattern, KnowledgeEdgeKind::kContributesTo,
                               termination,
                               {{"projection", std::string("result_transition_graph")}},
                               "result_transition_graph:time_to_loss"),
                     source);
      }
      ++report.time_trouble_games;
    }

    project_analysis_patterns(pending, game, moves, source, report);
    project_phase_transitions(pending, graph_, *stored_game, source);
    ++report.games_projected;
  }

  const auto manifest = manifest_source(profile_id, sources);
  const auto manifest_invalidated = dependencies_.invalidate_source_change(
      manifest.source_type, manifest.source_id, manifest.source_version,
      observed_at_ms);
  report.invalidated_entries.insert(report.invalidated_entries.end(),
                                    manifest_invalidated.begin(),
                                    manifest_invalidated.end());

  std::set<std::string> live_entries;
  for (const auto& [id, value] : pending.nodes()) {
    const auto provenance = source_values(value.sources, manifest);
    const KnowledgeEntryRef entry{KnowledgeEntryKind::kNode, id};
    const bool graph_changed = graph_.upsert_node(value.node);
    const bool dependency_changed = !dependencies_.dependencies_match(entry, provenance);
    if (dependency_changed) {
      dependencies_.replace_provenance(entry, provenance, observed_at_ms);
      dependencies_.replace_dependencies(entry, provenance, observed_at_ms);
    }
    live_entries.insert("node\n" + id);
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
    live_entries.insert("edge\n" + id);
    if (graph_changed) ++report.edges_upserted;
    if (graph_changed || dependency_changed) report.changed_entries.push_back(entry);
  }

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
