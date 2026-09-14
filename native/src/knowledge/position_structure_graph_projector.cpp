#include "position_structure_graph_projector.h"

#include "position_similarity.h"

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ai/position/position_features.h"
#include "persistence/database.h"
#include "theory/position_key.h"

namespace kchess::knowledge {
namespace {

// -----------------------------------------------------------------------------
// Section: Stable identities and signatures
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
  edge.id = make_knowledge_edge_id(from.id, to_string(kind), to.id,
                                   discriminator);
  edge.properties = std::move(properties);
  return edge;
}

KnowledgeNode position_node(const std::string& fen) {
  const auto key = position_key_string(fen);
  return make_node(KnowledgeNodeKind::kPosition, "position:" + key,
                   {{"position_key", key}});
}

std::string join_ints(const std::vector<int>& values) {
  std::ostringstream out;
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i != 0) out << ',';
    out << values[i];
  }
  return out.str();
}

std::string piece_counts_key(const ai::PieceCountFeatures& p) {
  std::ostringstream out;
  out << p.pawns << 'p' << p.knights << 'n' << p.bishops << 'b' << p.rooks
      << 'r' << p.queens << 'q';
  return out.str();
}

int center_pawn_count(const ai::PositionFeatures& features) {
  constexpr int kD4 = 27;
  constexpr int kE4 = 28;
  constexpr int kD5 = 35;
  constexpr int kE5 = 36;
  int count = 0;
  const auto in_center = [](int square) {
    return square == kD4 || square == kE4 || square == kD5 || square == kE5;
  };
  for (const int square : features.white.strategic.pawns.pawn_squares) {
    if (in_center(square)) ++count;
  }
  for (const int square : features.black.strategic.pawns.pawn_squares) {
    if (in_center(square)) ++count;
  }
  return count;
}

std::string center_state(const ai::PositionFeatures& features) {
  const int center = center_pawn_count(features);
  if (center >= 3) return "closed";
  if (center <= 1 && features.open_files.size() >= 3) return "open";
  return "semi_open";
}

std::string endgame_type(const ai::PositionFeatures& f) {
  const int queens = f.white.pieces.queens + f.black.pieces.queens;
  const int rooks = f.white.pieces.rooks + f.black.pieces.rooks;
  const int bishops = f.white.pieces.bishops + f.black.pieces.bishops;
  const int knights = f.white.pieces.knights + f.black.pieces.knights;
  const int non_pawn = queens + rooks + bishops + knights;
  if (non_pawn > 6 && queens > 0) return {};
  if (queens == 0 && rooks == 0 && bishops == 0 && knights == 0) {
    return "king_and_pawn";
  }
  if (queens == 0 && rooks > 0 && bishops == 0 && knights == 0) {
    return "rook";
  }
  if (queens == 0 && rooks == 0 && (bishops + knights) > 0) {
    return "minor_piece";
  }
  if (queens == 0 && rooks > 0 && (bishops + knights) > 0) {
    return "rook_and_minor";
  }
  if (queens > 0 && non_pawn <= 6) return "queen";
  return "mixed";
}

std::string king_safety_class(const ai::KingSafetyFeatures& king) {
  if (king.pawn_shield >= 2 && king.exposed_files == 0 &&
      king.attacked_zone_squares <= 2) {
    return "sheltered";
  }
  if (king.exposed_files >= 2 || king.attacked_zone_squares >= 5 ||
      king.enemy_attackers >= 3) {
    return "exposed";
  }
  return "mixed";
}

KnowledgeNode pawn_structure_node(const ai::PositionFeatures& f) {
  const auto& w = f.white.strategic.pawns;
  const auto& b = f.black.strategic.pawns;
  const std::string signature = "w:" + join_ints(w.pawn_squares) + "|b:" +
                                join_ints(b.pawn_squares);
  return make_node(
      KnowledgeNodeKind::kPawnStructure,
      "pawn_structure:" + stable_fingerprint(signature),
      {{"white_pawn_squares", join_ints(w.pawn_squares)},
       {"black_pawn_squares", join_ints(b.pawn_squares)},
       {"white_isolated", static_cast<std::int64_t>(w.isolated_pawns)},
       {"black_isolated", static_cast<std::int64_t>(b.isolated_pawns)},
       {"white_doubled", static_cast<std::int64_t>(w.doubled_pawns)},
       {"black_doubled", static_cast<std::int64_t>(b.doubled_pawns)}});
}

KnowledgeNode material_node(const ai::PositionFeatures& f) {
  const std::string key = "w:" + piece_counts_key(f.white.pieces) + "|b:" +
                          piece_counts_key(f.black.pieces);
  return make_node(
      KnowledgeNodeKind::kMaterialConfiguration, "material:" + key,
      {{"white_material_cp", static_cast<std::int64_t>(f.white.material_cp)},
       {"black_material_cp", static_cast<std::int64_t>(f.black.material_cp)},
       {"white_counts", piece_counts_key(f.white.pieces)},
       {"black_counts", piece_counts_key(f.black.pieces)}});
}

KnowledgeNode piece_configuration_node(const ai::PositionFeatures& f) {
  std::ostringstream key;
  key << "w:" << piece_counts_key(f.white.pieces) << ":dev"
      << f.white.minor_pieces_off_home << ":k"
      << f.white.strategic.king.king_square << "|b:"
      << piece_counts_key(f.black.pieces) << ":dev"
      << f.black.minor_pieces_off_home << ":k"
      << f.black.strategic.king.king_square;
  return make_node(
      KnowledgeNodeKind::kPieceConfiguration,
      "piece_configuration:" + stable_fingerprint(key.str()),
      {{"white_minor_developed",
        static_cast<std::int64_t>(f.white.minor_pieces_off_home)},
       {"black_minor_developed",
        static_cast<std::int64_t>(f.black.minor_pieces_off_home)},
       {"white_king_square",
        static_cast<std::int64_t>(f.white.strategic.king.king_square)},
       {"black_king_square",
        static_cast<std::int64_t>(f.black.strategic.king.king_square)}});
}

KnowledgeNode king_safety_node(const ai::KingSafetyFeatures& king,
                               std::string_view color) {
  const auto cls = king_safety_class(king);
  std::ostringstream key;
  key << color << ':' << cls << ":shield" << king.pawn_shield << ":files"
      << king.exposed_files << ":zone" << king.attacked_zone_squares
      << ":attackers" << king.enemy_attackers;
  return make_node(
      KnowledgeNodeKind::kKingSafetyPattern,
      "king_safety:" + stable_fingerprint(key.str()),
      {{"color", std::string(color)},
       {"class", cls},
       {"pawn_shield", static_cast<std::int64_t>(king.pawn_shield)},
       {"exposed_files", static_cast<std::int64_t>(king.exposed_files)},
       {"attacked_zone_squares",
        static_cast<std::int64_t>(king.attacked_zone_squares)},
       {"enemy_attackers", static_cast<std::int64_t>(king.enemy_attackers)}});
}

KnowledgeNode center_node(const ai::PositionFeatures& f) {
  const auto state = center_state(f);
  return make_node(KnowledgeNodeKind::kPositionFamily, "center_state:" + state,
                   {{"family_type", std::string("center_state")},
                    {"center_state", state},
                    {"center_pawns",
                     static_cast<std::int64_t>(center_pawn_count(f))},
                    {"open_files",
                     static_cast<std::int64_t>(f.open_files.size())}});
}

KnowledgeNode middlegame_node(const ai::PositionFeatures& f) {
  const auto state = center_state(f);
  const auto& w = f.white.strategic.pawns;
  const auto& b = f.black.strategic.pawns;
  std::ostringstream key;
  key << state << "|wi" << w.isolated_pawns << "|bi" << b.isolated_pawns
      << "|wd" << w.doubled_pawns << "|bd" << b.doubled_pawns
      << "|of" << f.open_files.size();
  return make_node(
      KnowledgeNodeKind::kMiddlegameStructure,
      "middlegame:" + stable_fingerprint(key.str()),
      {{"center_state", state},
       {"open_files", static_cast<std::int64_t>(f.open_files.size())},
       {"white_isolated", static_cast<std::int64_t>(w.isolated_pawns)},
       {"black_isolated", static_cast<std::int64_t>(b.isolated_pawns)},
       {"white_doubled", static_cast<std::int64_t>(w.doubled_pawns)},
       {"black_doubled", static_cast<std::int64_t>(b.doubled_pawns)}});
}

KnowledgeNode endgame_node(std::string_view type) {
  return make_node(KnowledgeNodeKind::kEndgameType,
                   "endgame_type:" + std::string(type),
                   {{"endgame_type", std::string(type)}});
}

KnowledgeSourceRef game_source(const GameRecord& game) {
  std::ostringstream canonical;
  canonical << game.id << '|';
  if (!game.moves.empty()) canonical << game.moves.front().fen_before << ';';
  for (std::size_t i = 0; i < game.moves.size(); ++i) {
    canonical << i << ':' << game.moves[i].uci << ':' << game.moves[i].fen_after
              << ';';
  }
  return {.source_type = "games_db_positions",
          .source_id = "game:" + game.id,
          .source_version = "fnv1a64:" + stable_fingerprint(canonical.str())};
}

KnowledgeSourceRef manifest_source(
    const std::string& profile_id,
    const std::vector<KnowledgeSourceRef>& game_sources) {
  std::vector<std::string> parts;
  parts.reserve(game_sources.size());
  for (const auto& source : game_sources) {
    parts.push_back(source.source_id + "@" + source.source_version);
  }
  std::sort(parts.begin(), parts.end());
  std::ostringstream canonical;
  for (const auto& part : parts) canonical << part << ';';
  return {.source_type = "position_structure_manifest",
          .source_id = "profile:" + profile_id,
          .source_version = "fnv1a64:" + stable_fingerprint(canonical.str())};
}

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

void project_position(ProjectionAccumulator& pending, const std::string& fen,
                      const KnowledgeSourceRef& source,
                      ai::PositionFeatureExtractor& extractor,
                      GraphStore& graph, PositionSimilarityIndex* position_similarity,
                      const std::int64_t observed_at_ms,
                      PositionStructureProjectionReport& report) {
  ++report.positions_considered;
  const auto features = extractor.extract(fen);
  const auto position = position_node(fen);
  // Position vectors are a second, deterministic feature space. The canonical
  // Position node is made visible before vector upsert so vector ownership can
  // be validated without storing FEN inside the graph itself.
  if (position_similarity != nullptr) {
    graph.upsert_node(position);
    (void)position_similarity->upsert_position(
        position.id, fen, observed_at_ms);
  }
  const auto pawn = pawn_structure_node(features);
  const auto material = material_node(features);
  const auto pieces = piece_configuration_node(features);
  const auto white_king = king_safety_node(features.white.strategic.king, "white");
  const auto black_king = king_safety_node(features.black.strategic.king, "black");
  const auto center = center_node(features);

  const auto add_structure = [&](const KnowledgeNode& structure,
                                 std::string_view discriminator) {
    pending.node(structure, source);
    pending.edge(make_edge(position, KnowledgeEdgeKind::kHasStructure,
                           structure,
                           {{"projection", std::string("position_structure_graph")}},
                           std::string("position_structure_graph:") +
                               std::string(discriminator)),
                 source);
  };

  pending.node(position, source);
  add_structure(pawn, "pawn");
  add_structure(material, "material");
  add_structure(pieces, "pieces");
  add_structure(white_king, "king_white");
  add_structure(black_king, "king_black");
  add_structure(center, "center");

  const auto ending = endgame_type(features);
  if (!ending.empty()) {
    add_structure(endgame_node(ending), "endgame");
  } else {
    add_structure(middlegame_node(features), "middlegame");
  }
  ++report.positions_projected;
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Position/structure graph projection
// -----------------------------------------------------------------------------

PositionStructureGraphProjector::PositionStructureGraphProjector(
    Database& database, GraphStore& graph, DependencyTracker& dependencies,
    PositionSimilarityIndex* position_similarity)
    : database_(database), graph_(graph), dependencies_(dependencies),
      position_similarity_(position_similarity) {}

PositionStructureProjectionReport
PositionStructureGraphProjector::project_active_profile(
    const std::int64_t observed_at_ms) {
  PositionStructureProjectionReport report;
  const auto profile = database_.active_profile();
  if (!profile) return report;
  const std::string profile_id = database_.player_profile_owner_id(profile->id);
  report.profile_id = profile_id;

  const auto source_rows = database_.player_profile_game_sources(profile_id);
  report.games_considered = source_rows.size();

  ProjectionAccumulator pending;
  ai::PositionFeatureExtractor extractor;
  std::vector<KnowledgeSourceRef> sources;

  for (const auto& source_row : source_rows) {
    const auto game_value = database_.game(source_row.game_id);
    if (!game_value || game_value->moves.empty()) continue;
    const auto& game = *game_value;
    const auto source = game_source(game);
    sources.push_back(source);

    const auto invalidated = dependencies_.invalidate_source_change(
        source.source_type, source.source_id, source.source_version,
        observed_at_ms);
    report.invalidated_entries.insert(report.invalidated_entries.end(),
                                      invalidated.begin(), invalidated.end());

    project_position(pending, game.moves.front().fen_before, source, extractor,
                     graph_, position_similarity_, observed_at_ms, report);
    for (const auto& move : game.moves) {
      project_position(pending, move.fen_after, source, extractor, graph_,
                       position_similarity_, observed_at_ms, report);
    }
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
