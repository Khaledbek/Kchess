#include "hybrid_retrieval.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <deque>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace kchess::knowledge {
namespace {

std::string lower_ascii(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
    return c < 128 ? static_cast<char>(std::tolower(c)) : static_cast<char>(c);
  });
  return text;
}

const std::string* text_property(const KnowledgeNode& node,
                                 std::string_view key) {
  const auto it = node.properties.find(std::string(key));
  if (it == node.properties.end()) return nullptr;
  return std::get_if<std::string>(&it->second);
}

bool string_property_equals(const KnowledgeNode& node, std::string_view key,
                            std::string_view expected) {
  const auto* value = text_property(node, key);
  return value != nullptr && lower_ascii(*value) == lower_ascii(std::string(expected));
}

bool has_property(const KnowledgeNode& node, std::string_view key) {
  return node.properties.find(std::string(key)) != node.properties.end();
}

std::optional<double> numeric_property(const KnowledgeNode& node,
                                       std::string_view key) {
  const auto it = node.properties.find(std::string(key));
  if (it == node.properties.end()) return std::nullopt;
  if (const auto* value = std::get_if<double>(&it->second)) return *value;
  if (const auto* value = std::get_if<std::int64_t>(&it->second)) {
    return static_cast<double>(*value);
  }
  return std::nullopt;
}

std::vector<std::string> lexical_terms(const HybridRetrievalRequest& request) {
  std::vector<std::string> result;
  auto append = [&](std::string value) {
    value = lower_ascii(std::move(value));
    if (value.size() < 2) return;
    if (std::find(result.begin(), result.end(), value) == result.end()) {
      result.push_back(std::move(value));
    }
  };

  for (const auto& entity : request.route.entities) append(entity.canonical_value);

  static constexpr std::array<std::string_view, 38> kStopWords{
      "the", "and", "for", "with", "from", "this", "that", "what", "which",
      "why", "how", "are", "was", "were", "have", "has", "mein", "meine",
      "meiner", "meinen", "warum", "wie", "welche", "welcher", "welches",
      "und", "oder", "mit", "von", "bei", "ich", "mir", "mich", "das",
      "die", "der", "den", "dem"};
  auto stop_word = [](std::string_view word) {
    return std::find(kStopWords.begin(), kStopWords.end(), word) != kStopWords.end();
  };

  std::string token;
  for (const unsigned char c : request.query_text) {
    if ((c < 128 && std::isalnum(c) != 0) || c >= 128 || c == '-' || c == '_') {
      token.push_back(c < 128 ? static_cast<char>(std::tolower(c))
                              : static_cast<char>(c));
      continue;
    }
    if (token.size() >= 3 && !stop_word(token)) append(token);
    token.clear();
  }
  if (token.size() >= 3 && !stop_word(token)) append(token);
  if (result.size() > 12) result.resize(12);
  return result;
}

void append_unique_node(std::vector<KnowledgeNode>& nodes, KnowledgeNode value,
                        std::size_t limit) {
  if (nodes.size() >= limit) return;
  const auto duplicate = std::find_if(nodes.begin(), nodes.end(), [&](const auto& item) {
    return item.id == value.id;
  });
  if (duplicate == nodes.end()) nodes.push_back(std::move(value));
}

void append_lookup(GraphStore& graph, std::vector<KnowledgeNode>& nodes,
                   KnowledgeNodeKind kind, std::string key, std::string value,
                   std::size_t limit) {
  if (nodes.size() >= limit || value.empty()) return;
  KnowledgeNodeLookup lookup;
  lookup.kind = kind;
  lookup.text_properties.emplace(std::move(key), std::move(value));
  lookup.limit = limit - nodes.size();
  for (auto& node : graph.find_nodes(lookup)) {
    append_unique_node(nodes, std::move(node), limit);
  }
}

std::vector<KnowledgeNode> entity_seed_nodes(GraphStore& graph,
                                             const HybridRetrievalRequest& request) {
  std::vector<KnowledgeNode> result;
  const auto limit = request.budgets.max_seed_nodes;
  for (const auto& entity : request.route.entities) {
    switch (entity.kind) {
      case KnowledgeQueryEntityKind::kOpeningName:
        append_lookup(graph, result, KnowledgeNodeKind::kOpening, "name",
                      entity.canonical_value, limit);
        append_lookup(graph, result, KnowledgeNodeKind::kOpeningFamily, "name",
                      entity.canonical_value, limit);
        append_lookup(graph, result, KnowledgeNodeKind::kVariation, "name",
                      entity.canonical_value, limit);
        break;
      case KnowledgeQueryEntityKind::kEcoCode:
        append_lookup(graph, result, KnowledgeNodeKind::kOpening, "eco",
                      entity.canonical_value, limit);
        append_lookup(graph, result, KnowledgeNodeKind::kVariation, "eco",
                      entity.canonical_value, limit);
        break;
      case KnowledgeQueryEntityKind::kTimeControl:
        append_lookup(graph, result, KnowledgeNodeKind::kTimeControl, "type",
                      entity.canonical_value, limit);
        break;
      case KnowledgeQueryEntityKind::kPlayerColor:
        append_lookup(graph, result, KnowledgeNodeKind::kColor, "color",
                      entity.canonical_value, limit);
        break;
      case KnowledgeQueryEntityKind::kResult:
        append_lookup(graph, result, KnowledgeNodeKind::kResultType, "outcome",
                      entity.canonical_value, limit);
        break;
      case KnowledgeQueryEntityKind::kTermination:
        append_lookup(graph, result, KnowledgeNodeKind::kTerminationType, "termination",
                      entity.canonical_value, limit);
        append_lookup(graph, result, KnowledgeNodeKind::kTerminationType, "type",
                      entity.canonical_value, limit);
        break;
      default:
        break;
    }
    if (result.size() >= limit) break;
  }
  return result;
}

bool statistic_matches_entities(const KnowledgeNode& node,
                                const std::vector<KnowledgeQueryEntity>& entities) {
  for (const auto& entity : entities) {
    bool applies = true;
    bool matches = false;
    switch (entity.kind) {
      case KnowledgeQueryEntityKind::kOpeningName:
        matches = string_property_equals(node, "opening_family", entity.canonical_value) ||
                  string_property_equals(node, "opening", entity.canonical_value);
        break;
      case KnowledgeQueryEntityKind::kEcoCode:
        matches = string_property_equals(node, "eco", entity.canonical_value);
        break;
      case KnowledgeQueryEntityKind::kTimeControl:
        matches = string_property_equals(node, "time_control", entity.canonical_value);
        break;
      case KnowledgeQueryEntityKind::kPlayerColor:
        matches = string_property_equals(node, "color", entity.canonical_value);
        break;
      case KnowledgeQueryEntityKind::kGamePhase:
        if (entity.canonical_value == "opening") {
          matches = string_property_equals(node, "scope", "opening_family") ||
                    string_property_equals(node, "scope", "opening_coverage");
        } else {
          applies = false;
        }
        break;
      case KnowledgeQueryEntityKind::kResult:
        if (entity.canonical_value == "win") {
          matches = has_property(node, "wins");
        } else if (entity.canonical_value == "loss") {
          matches = has_property(node, "losses");
        } else if (entity.canonical_value == "draw") {
          matches = has_property(node, "draws");
        } else {
          applies = false;
        }
        break;
      case KnowledgeQueryEntityKind::kTermination:
        matches = string_property_equals(node, "termination_type", entity.canonical_value);
        break;
      case KnowledgeQueryEntityKind::kTemporalScope:
        if (entity.canonical_value == "recent") {
            matches = string_property_equals(node, "scope", "recent_form") ||
                    string_property_equals(node, "scope", "recent");
        } else if (entity.canonical_value == "lifetime") {
          matches = string_property_equals(node, "scope", "overall") ||
                    string_property_equals(node, "scope", "lifetime") ||
                    string_property_equals(node, "scope", "accuracy");
        } else {
          // Historical comparison is represented by temporal graph/conflict
          // state, not by a dedicated StatisticsService snapshot.
          applies = false;
        }
        break;
      case KnowledgeQueryEntityKind::kStatisticMetric:
        if (entity.canonical_value == "accuracy") {
          matches = has_property(node, "average_accuracy") ||
                    string_property_equals(node, "statistic_key", "average_accuracy");
        } else if (entity.canonical_value == "rating") {
          matches = has_property(node, "rating");
        } else if (entity.canonical_value == "win_rate") {
          matches = has_property(node, "wins") || has_property(node, "winRate") ||
                    has_property(node, "winShare") || has_property(node, "scorePercent");
        } else if (entity.canonical_value == "frequency") {
          matches = has_property(node, "games") || has_property(node, "count") ||
                    has_property(node, "sample_size");
        } else if (entity.canonical_value == "score") {
          matches = has_property(node, "scorePercent");
        } else {
          applies = false;
        }
        break;
      default:
        applies = false;
        break;
    }
    if (applies && !matches) return false;
  }
  return true;
}

std::vector<KnowledgeNode> exact_statistic_nodes(GraphStore& graph,
                                                 const HybridRetrievalRequest& request,
                                                 std::size_t limit) {
  if (request.player_id.empty() || limit == 0) return {};
  std::vector<KnowledgeNode> candidates;
  KnowledgeNodeLookup stats;
  stats.kind = KnowledgeNodeKind::kStatistic;
  stats.text_properties.emplace("profile_id", request.player_id);
  stats.limit = 512;
  for (auto& node : graph.find_nodes(stats)) {
    if (statistic_matches_entities(node, request.route.entities)) {
      candidates.push_back(std::move(node));
    }
  }

  std::optional<std::string> metric;
  for (const auto& entity : request.route.entities) {
    if (entity.kind == KnowledgeQueryEntityKind::kStatisticMetric) {
      metric = entity.canonical_value;
      break;
    }
  }
  if (metric) {
    const auto score_for = [&](const KnowledgeNode& node) {
      if (*metric == "frequency") {
        return numeric_property(node, "games").value_or(
            numeric_property(node, "sample_size").value_or(0.0));
      }
      if (*metric == "win_rate") {
        return numeric_property(node, "winRate").value_or(
            numeric_property(node, "winShare").value_or(
                numeric_property(node, "scorePercent").value_or(0.0)));
      }
      if (*metric == "score") {
        return numeric_property(node, "scorePercent").value_or(0.0);
      }
      if (*metric == "accuracy") {
        return numeric_property(node, "average_accuracy").value_or(0.0);
      }
      return 0.0;
    };
    std::stable_sort(candidates.begin(), candidates.end(), [&](const auto& lhs, const auto& rhs) {
      return score_for(lhs) > score_for(rhs);
    });
  }
  if (candidates.size() > limit) candidates.resize(limit);

  if (candidates.size() < limit) {
    bool wants_rating = false;
    for (const auto& entity : request.route.entities) {
      if (entity.kind == KnowledgeQueryEntityKind::kStatisticMetric &&
          entity.canonical_value == "rating") {
        wants_rating = true;
        break;
      }
    }
    if (wants_rating) {
      const auto rating_id = make_knowledge_node_id(
          to_string(KnowledgeNodeKind::kRatingPeriod),
          "profile:" + request.player_id + ":rating:lifetime");
      if (auto rating = graph.node(rating_id)) {
        append_unique_node(candidates, std::move(*rating), limit);
      }
    }
  }
  return candidates;
}

bool belongs_to_player_history(GraphStore& graph, const KnowledgeNodeId& position,
                               std::string_view player_id) {
  if (player_id.empty()) return true;
  for (const auto& step : graph.adjacent(position, GraphDirection::kIncoming,
                                         KnowledgeEdgeKind::kReaches, 64)) {
    if (step.node.kind != KnowledgeNodeKind::kGame) continue;
    const auto* owner = text_property(step.node, "profile_id");
    if (owner != nullptr && *owner == player_id) return true;
  }
  return false;
}

bool eligible_for_player_scope(const KnowledgeNode& node,
                               const HybridRetrievalRequest& request) {
  if (!request.route.requires_player_scope || request.player_id.empty()) return true;
  const auto* owner = text_property(node, "profile_id");
  return owner == nullptr || *owner == request.player_id;
}

std::size_t traversal_depth_for(const HybridRetrievalRequest& request) {
  const bool complex = request.route.intent == KnowledgeQueryIntent::kRelationship ||
      request.route.intent == KnowledgeQueryIntent::kCausalAnalysis ||
      request.route.intent == KnowledgeQueryIntent::kEvidence;
  return complex ? request.budgets.complex_hop_depth : request.budgets.hop_depth;
}

void merge_node_candidate(std::vector<HybridNodeCandidate>& out,
                          const KnowledgeNode& node,
                          const HybridRetrievalSignals& signals) {
  auto it = std::find_if(out.begin(), out.end(), [&](const auto& value) {
    return value.node.id == node.id;
  });
  if (it == out.end()) {
    out.push_back({node, signals});
    return;
  }
  it->signals.exact_statistics |= signals.exact_statistics;
  it->signals.graph |= signals.graph;
  it->signals.position_similarity |= signals.position_similarity;
  it->signals.seed |= signals.seed;
  if (signals.graph_distance &&
      (!it->signals.graph_distance || *signals.graph_distance < *it->signals.graph_distance)) {
    it->signals.graph_distance = signals.graph_distance;
  }
  if (signals.position_score &&
      (!it->signals.position_score || *signals.position_score > *it->signals.position_score)) {
    it->signals.position_score = signals.position_score;
  }
}

void merge_chunk_candidate(std::vector<HybridChunkCandidate>& out,
                           KnowledgeChunk chunk,
                           const HybridRetrievalSignals& signals,
                           std::size_t max_candidates,
                           bool* budget_exhausted) {
  auto it = std::find_if(out.begin(), out.end(), [&](const auto& value) {
    return value.chunk.metadata.id == chunk.metadata.id;
  });
  if (it != out.end()) {
    it->signals.exact_statistics |= signals.exact_statistics;
    it->signals.graph |= signals.graph;
    it->signals.lexical |= signals.lexical;
    it->signals.vector |= signals.vector;
    if (signals.lexical_score &&
        (!it->signals.lexical_score || *signals.lexical_score > *it->signals.lexical_score)) {
      it->signals.lexical_score = signals.lexical_score;
    }
    if (signals.vector_score &&
        (!it->signals.vector_score || *signals.vector_score > *it->signals.vector_score)) {
      it->signals.vector_score = signals.vector_score;
    }
    return;
  }
  if (out.size() >= max_candidates) {
    if (budget_exhausted != nullptr) *budget_exhausted = true;
    return;
  }
  out.push_back({std::move(chunk), signals});
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Hybrid retrieval
// -----------------------------------------------------------------------------

HybridRetrievalEngine::HybridRetrievalEngine(
    GraphStore& graph, ChunkRegistry& chunks,
    const PositionSimilarityIndex* position_similarity)
    : graph_(graph), chunks_(chunks),
      position_similarity_(position_similarity) {}

HybridRetrievalResult HybridRetrievalEngine::retrieve(
    const HybridRetrievalRequest& request) const {
  HybridRetrievalResult result;
  const auto& budgets = request.budgets;
  if (budgets.max_seed_nodes == 0) return result;

  std::vector<KnowledgeNode> seeds = entity_seed_nodes(graph_, request);
  seeds.erase(std::remove_if(seeds.begin(), seeds.end(), [&](const auto& node) {
    return !eligible_for_player_scope(node, request);
  }), seeds.end());

  std::vector<KnowledgeNode> exact_nodes;
  if (request.route.channels.exact_statistics && !request.player_id.empty()) {
    if (request.authoritative_nodes.empty())
      exact_nodes = exact_statistic_nodes(graph_, request, budgets.max_seed_nodes);
    for (const auto& node : exact_nodes) {
      HybridRetrievalSignals signals;
      signals.exact_statistics = true;
      merge_node_candidate(result.nodes, node, signals);
      append_unique_node(seeds, node, budgets.max_seed_nodes);
    }
  }

  // Live service facts use the same candidate/ranking/packet path as persisted
  // nodes. This keeps cold and partially prepared profiles answerable without
  // writing a graph or starting embeddings/analysis in a foreground question.
  for (const auto& candidate : request.authoritative_nodes) {
    if (!eligible_for_player_scope(candidate.node, request) ||
        !knowledge_node_matches_scope(candidate.node, request.route)) continue;
    result.nodes.push_back(candidate);
    if (request.route.channels.graph && seeds.size() < budgets.max_seed_nodes && graph_.node(candidate.node.id))
      append_unique_node(seeds, candidate.node, budgets.max_seed_nodes);
  }

  if (seeds.empty() && request.route.requires_player_scope && !request.player_id.empty()) {
    append_lookup(graph_, seeds, KnowledgeNodeKind::kPlayer, "profile_id",
                  request.player_id, budgets.max_seed_nodes);
  }
  result.seed_budget_exhausted = seeds.size() >= budgets.max_seed_nodes;
  for (const auto& seed : seeds) {
    result.seed_nodes.push_back(seed.id);
    HybridRetrievalSignals signals;
    signals.seed = true;
    signals.graph = request.route.channels.graph;
    signals.graph_distance = 0;
    merge_node_candidate(result.nodes, seed, signals);
  }

  std::vector<KnowledgeEntryRef> graph_entries;
  graph_entries.reserve(budgets.max_expanded_nodes + seeds.size());
  for (const auto& seed : seeds) {
    graph_entries.push_back({KnowledgeEntryKind::kNode, seed.id.value});
  }

  if (request.route.channels.graph && !seeds.empty() && budgets.max_expanded_nodes > 0) {
    struct PendingNode {
      KnowledgeNodeId id;
      std::size_t depth{0};
    };
    const auto max_depth = traversal_depth_for(request);
    std::deque<PendingNode> pending;
    std::unordered_set<std::string> visited;
    for (const auto& seed : seeds) {
      if (!eligible_for_player_scope(seed, request)) continue;
      if (visited.insert(seed.id.value).second) pending.push_back({seed.id, 0});
    }

    while (!pending.empty() && result.expanded_nodes < budgets.max_expanded_nodes) {
      const auto current = pending.front();
      pending.pop_front();
      if (current.depth >= max_depth) continue;
      const auto remaining = budgets.max_expanded_nodes - result.expanded_nodes;
      for (const auto& step : graph_.adjacent(current.id, GraphDirection::kBoth,
                                              std::nullopt, remaining)) {
        if (!eligible_for_player_scope(step.node, request)) continue;
        if (!visited.insert(step.node.id.value).second) continue;
        const auto depth = current.depth + 1;
        ++result.expanded_nodes;
        HybridRetrievalSignals signals;
        signals.graph = true;
        signals.graph_distance = depth;
        merge_node_candidate(result.nodes, step.node, signals);
        auto routed_step = step;
        routed_step.depth = depth;
        result.graph_steps.push_back(std::move(routed_step));
        graph_entries.push_back({KnowledgeEntryKind::kNode, step.node.id.value});
        graph_entries.push_back({KnowledgeEntryKind::kEdge, step.edge.id.value});
        if (depth < max_depth) pending.push_back({step.node.id, depth});
        if (result.expanded_nodes >= budgets.max_expanded_nodes) break;
      }
    }
    result.traversal_budget_exhausted =
        result.expanded_nodes >= budgets.max_expanded_nodes;
  }

  if (request.route.channels.graph && !request.player_id.empty() &&
      !graph_entries.empty() && budgets.max_graph_chunks > 0 &&
      budgets.max_candidate_chunks > 0) {
    for (auto& chunk : chunks_.linked_chunks(
             request.player_id, graph_entries,
             std::min(budgets.max_graph_chunks, budgets.max_candidate_chunks))) {
      HybridRetrievalSignals signals;
      signals.graph = true;
      merge_chunk_candidate(result.chunks, std::move(chunk), signals,
                            budgets.max_candidate_chunks,
                            &result.chunk_budget_exhausted);
    }
  }

  if (request.route.channels.lexical && !request.player_id.empty() &&
      budgets.max_lexical_chunks > 0 && budgets.max_candidate_chunks > 0) {
    const auto terms = lexical_terms(request);
    for (auto& hit : chunks_.lexical_search(
             request.player_id, terms,
             std::min(budgets.max_lexical_chunks, budgets.max_candidate_chunks))) {
      HybridRetrievalSignals signals;
      signals.lexical = true;
      signals.lexical_score = hit.lexical_score;
      merge_chunk_candidate(result.chunks, std::move(hit.chunk), signals,
                            budgets.max_candidate_chunks,
                            &result.chunk_budget_exhausted);
    }
  }

  if (request.route.channels.position_similarity && position_similarity_ != nullptr &&
      request.current_fen && !request.current_fen->empty() &&
      budgets.max_position_candidates > 0) {
    for (const auto& match : position_similarity_->similar_to_fen(
             *request.current_fen, budgets.max_position_candidates)) {
      if (request.route.requires_player_scope &&
          !belongs_to_player_history(graph_, match.position_node_id, request.player_id)) {
        continue;
      }
      const auto node = graph_.node(match.position_node_id);
      if (!node) continue;
      HybridRetrievalSignals signals;
      signals.position_similarity = true;
      signals.position_score = match.score;
      merge_node_candidate(result.nodes, *node, signals);
    }
  }

  const auto eligible = [&](const KnowledgeNode& node) {
    if (!eligible_for_player_scope(node, request) ||
        !knowledge_node_matches_scope(node, request.route)) return false;
    // Service snapshots supersede stale persisted statistics for this query.
    if (!request.authoritative_nodes.empty() &&
        (node.kind == KnowledgeNodeKind::kStatistic || node.kind == KnowledgeNodeKind::kRatingPeriod)) {
      return std::any_of(request.authoritative_nodes.begin(), request.authoritative_nodes.end(),
          [&](const auto& live) { return live.node.id == node.id; });
    }
    return true;
  };
  result.nodes.erase(std::remove_if(result.nodes.begin(), result.nodes.end(),
      [&](const auto& candidate) { return !eligible(candidate.node); }), result.nodes.end());
  result.chunks.erase(std::remove_if(result.chunks.begin(), result.chunks.end(),
      [&](const auto& candidate) {
        const auto links = chunks_.graph_links(candidate.chunk.metadata.id);
        return !std::any_of(links.begin(), links.end(), [&](const auto& link) {
          if (link.entry.kind != KnowledgeEntryKind::kNode) return false;
          const auto node = graph_.node({link.entry.id});
          return node && eligible(*node);
        });
      }), result.chunks.end());
  return result;
}

}  // namespace kchess::knowledge
