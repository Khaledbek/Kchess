#include "evidence_packet_builder.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
#include <string_view>

namespace kchess::knowledge {
namespace {

// -----------------------------------------------------------------------------
// Section: Packet policy helpers
// -----------------------------------------------------------------------------

constexpr std::size_t kCharsPerEstimatedToken = 4;
constexpr std::size_t kMaxNodeProperties = 10;
constexpr std::size_t kMaxNodeContentChars = 720;
constexpr double kLowConfidenceThreshold = 0.45;
constexpr double kLowCoverageThreshold = 0.35;
constexpr double kLowFreshnessThreshold = 0.30;
constexpr std::size_t kMaxUncertaintyDetailTokens = 64;
constexpr std::size_t kMaxPacketQueryTokens = 200;

std::size_t estimated_tokens(std::string_view text) {
  return (text.size() + kCharsPerEstimatedToken - 1) /
         kCharsPerEstimatedToken;
}

std::string trim_utf8_to_tokens(std::string text, const std::size_t tokens) {
  const std::size_t max_chars = tokens * kCharsPerEstimatedToken;
  if (text.size() <= max_chars) return text;
  if (max_chars == 0) return {};
  std::size_t end = max_chars;
  while (end > 0 && end < text.size() &&
         (static_cast<unsigned char>(text[end]) & 0xc0) == 0x80) {
    --end;
  }
  text.resize(end);
  if (end >= 3) {
    text.resize(end - 3);
    text += "...";
  }
  return text;
}

double clamp01(const double value) {
  if (!std::isfinite(value)) return 0.0;
  return std::clamp(value, 0.0, 1.0);
}

bool relevant_recent_context(const KnowledgeEvidencePacketRequest& request) {
  if (request.execution_plan.prefer_recent ||
      request.route.intent == KnowledgeQueryIntent::kTrend) {
    return true;
  }
  for (const auto& entity : request.route.entities) {
    if (entity.kind == KnowledgeQueryEntityKind::kTemporalScope &&
        (entity.canonical_value == "recent" ||
         entity.canonical_value == "current")) {
      return true;
    }
  }
  return false;
}

KnowledgeEvidencePacketBudget packet_budget_for(
    const KnowledgeEvidencePacketRequest& request) {
  KnowledgeEvidencePacketBudget budget;
  switch (request.route.intent) {
    case KnowledgeQueryIntent::kExactStatistic:
      budget.max_tokens = 600;
      budget.max_facts = 6;
      budget.max_observations = 2;
      budget.max_evidence = 4;
      break;
    case KnowledgeQueryIntent::kRelationship:
      budget.max_tokens = 1800;
      break;
    case KnowledgeQueryIntent::kCausalAnalysis:
      budget.max_tokens = request.execution_plan.complex_query ? 3000 : 2200;
      budget.max_facts = 10;
      budget.max_observations = 10;
      break;
    case KnowledgeQueryIntent::kTrend:
      budget.max_tokens = 1800;
      budget.max_observations = 10;
      break;
    case KnowledgeQueryIntent::kEvidence:
      budget.max_tokens = 2200;
      budget.max_evidence = 10;
      break;
    case KnowledgeQueryIntent::kSimilarity:
    case KnowledgeQueryIntent::kCurrentPosition:
      budget.max_tokens = 1600;
      break;
    case KnowledgeQueryIntent::kGeneralPersonal:
      budget.max_tokens = request.execution_plan.complex_query ? 2600 : 1500;
      break;
    case KnowledgeQueryIntent::kGeneralChess:
      budget.max_tokens = 1000;
      break;
    case KnowledgeQueryIntent::kUnknown:
      budget.max_tokens = 1200;
      break;
  }
  if (request.route.profile_scope.compare_scopes ||
      std::find(request.route.profile_scope.topics.begin(), request.route.profile_scope.topics.end(),
                "rating") != request.route.profile_scope.topics.end()) {
    budget.max_tokens = std::max<std::size_t>(budget.max_tokens,
        400 + 180 * std::min<std::size_t>(24, request.ranked.nodes.size()));
    budget.max_facts = 24;
  }
  budget.max_tokens = std::min<std::size_t>(budget.max_tokens, 4000);
  return budget;
}

std::string property_value_text(const KnowledgePropertyValue& value) {
  if (const auto* item = std::get_if<bool>(&value)) return *item ? "true" : "false";
  if (const auto* item = std::get_if<std::int64_t>(&value)) {
    return std::to_string(*item);
  }
  if (const auto* item = std::get_if<double>(&value)) {
    std::ostringstream out;
    out << std::setprecision(5) << *item;
    return out.str();
  }
  if (const auto* item = std::get_if<std::string>(&value)) return *item;
  return {};
}

std::string compact_node_content(const KnowledgeNode& node) {
  std::ostringstream out;
  out << "kind=" << to_string(node.kind)
      << "; assertion=" << to_string(node.assertion_kind);
  std::size_t emitted = 0;
  for (const auto& [key, value] : node.properties) {
    if (emitted >= kMaxNodeProperties) break;
    const auto rendered = property_value_text(value);
    if (rendered.empty()) continue;
    out << "; " << key << '=' << rendered;
    ++emitted;
  }
  auto text = out.str();
  if (text.size() > kMaxNodeContentChars) {
    text.resize(kMaxNodeContentChars - 3);
    text += "...";
  }
  return text;
}

std::optional<KnowledgeQualityMetrics> node_quality(
    const KnowledgeQualityStore* store, const KnowledgeNodeId& id) {
  if (store == nullptr) return std::nullopt;
  return store->quality({KnowledgeEntryKind::kNode, id.value});
}

void apply_quality(KnowledgeEvidencePacketItem& item,
                   const std::optional<KnowledgeQualityMetrics>& quality) {
  if (!quality) return;
  item.confidence = clamp01(quality->confidence);
  item.coverage = clamp01(quality->coverage);
  if (quality->freshness) item.freshness = clamp01(*quality->freshness);
}

void apply_chunk_quality(KnowledgeEvidencePacketItem& item,
                         const KnowledgeChunkMetadata& metadata) {
  if (metadata.confidence) item.confidence = clamp01(*metadata.confidence);
  if (metadata.coverage) item.coverage = clamp01(*metadata.coverage);
  if (metadata.freshness) item.freshness = clamp01(*metadata.freshness);
}

std::vector<KnowledgeSourceRef> sources_for_node(
    DependencyTracker& dependencies, const KnowledgeNodeId& id) {
  std::vector<KnowledgeSourceRef> sources;
  for (const auto& record : dependencies.provenance_for(
           {KnowledgeEntryKind::kNode, id.value})) {
    if (record.source.valid()) sources.push_back(record.source);
  }
  return sources;
}

void append_uncertainty(std::vector<KnowledgePacketUncertainty>& out,
                        const KnowledgeEvidencePacketBudget& budget,
                        std::string code, std::string detail,
                        std::optional<KnowledgeEntryRef> entry = std::nullopt) {
  if (out.size() >= budget.max_uncertainties) return;
  for (const auto& existing : out) {
    if (existing.code == code && existing.entry == entry) return;
  }
  detail = trim_utf8_to_tokens(std::move(detail), kMaxUncertaintyDetailTokens);
  out.push_back({std::move(code), std::move(detail), std::move(entry)});
}

void add_quality_uncertainties(const KnowledgeEvidencePacketItem& item,
                               KnowledgeEvidencePacket& packet) {
  const auto entry = item.graph_entries.empty()
                         ? std::optional<KnowledgeEntryRef>{}
                         : std::optional<KnowledgeEntryRef>{item.graph_entries.front()};
  if (item.confidence && *item.confidence < kLowConfidenceThreshold) {
    append_uncertainty(packet.uncertainties, packet.budget, "low_confidence",
                       "Selected knowledge has low confidence.", entry);
  }
  if (item.coverage && *item.coverage < kLowCoverageThreshold) {
    append_uncertainty(packet.uncertainties, packet.budget, "low_coverage",
                       "Selected knowledge has limited evidence coverage.", entry);
  }
  if (item.freshness && *item.freshness < kLowFreshnessThreshold) {
    append_uncertainty(packet.uncertainties, packet.budget, "stale_evidence",
                       "Selected knowledge may be outdated.", entry);
  }
}

KnowledgePacketSection linked_chunk_section(GraphStore& graph,
                                            ChunkRegistry& chunks,
                                            const KnowledgeChunk& chunk) {
  bool has_fact = false;
  bool has_observation = false;
  bool has_hypothesis = false;
  for (const auto& link : chunks.graph_links(chunk.metadata.id)) {
    if (link.entry.kind != KnowledgeEntryKind::kNode) {
      if (link.entry.kind == KnowledgeEntryKind::kEdge) has_fact = true;
      continue;
    }
    const auto node = graph.node({link.entry.id});
    if (!node) continue;
    switch (node->assertion_kind) {
      case KnowledgeAssertionKind::kFact:
        has_fact = true;
        break;
      case KnowledgeAssertionKind::kObservation:
        has_observation = true;
        break;
      case KnowledgeAssertionKind::kHypothesis:
        has_hypothesis = true;
        break;
    }
  }
  if (has_hypothesis && !has_fact && !has_observation) {
    return KnowledgePacketSection::kUncertainty;
  }
  if (has_observation && !has_fact) return KnowledgePacketSection::kObservation;
  if (has_fact && !has_observation) return KnowledgePacketSection::kFact;
  // Ambiguous or unlinked semantic chunks remain evidence rather than being
  // promoted to a stronger assertion class.
  return KnowledgePacketSection::kEvidence;
}

std::vector<KnowledgeEntryRef> chunk_entries(ChunkRegistry& chunks,
                                             const KnowledgeChunkId& id) {
  std::vector<KnowledgeEntryRef> entries;
  for (const auto& link : chunks.graph_links(id)) entries.push_back(link.entry);
  return entries;
}

std::size_t packet_content_tokens(const KnowledgeEvidencePacket& packet) {
  std::size_t tokens = 150;  // provider envelope, requested scope and limitations
  for (const auto* section : {&packet.facts, &packet.observations,
                              &packet.evidence}) {
    for (const auto& item : *section) {
      tokens += estimated_tokens(item.topic) + estimated_tokens(item.id) + 80;
      if (item.properties.empty()) tokens += estimated_tokens(item.content);
      else for (const auto& [key, value] : item.properties)
        tokens += estimated_tokens(key) + estimated_tokens(property_value_text(value)) + 2;
    }
  }
  for (const auto& item : packet.uncertainties) {
    tokens += estimated_tokens(item.code) + estimated_tokens(item.detail);
  }
  if (packet.current_position) tokens += estimated_tokens(*packet.current_position);
  if (packet.recent_context) tokens += estimated_tokens(*packet.recent_context);
  return tokens;
}

bool append_item_with_budget(KnowledgeEvidencePacket& packet,
                             KnowledgeEvidencePacketItem item) {
  std::vector<KnowledgeEvidencePacketItem>* target = &packet.evidence;
  std::size_t limit = packet.budget.max_evidence;
  switch (item.section) {
    case KnowledgePacketSection::kFact:
      target = &packet.facts;
      limit = packet.budget.max_facts;
      break;
    case KnowledgePacketSection::kObservation:
      target = &packet.observations;
      limit = packet.budget.max_observations;
      break;
    case KnowledgePacketSection::kEvidence:
      break;
    case KnowledgePacketSection::kUncertainty:
      return false;
  }
  if (target->size() >= limit) return false;

  const std::size_t used = packet_content_tokens(packet);
  if (used >= packet.budget.max_tokens) {
    packet.token_budget_exhausted = true;
    return false;
  }
  constexpr std::size_t kUncertaintyReserveTokens = 72;
  if (item.content.empty()) return false;
  target->push_back(std::move(item));
  if (packet_content_tokens(packet) + kUncertaintyReserveTokens > packet.budget.max_tokens) {
    target->pop_back();
    packet.token_budget_exhausted = true;
    return false;
  }
  return true;
}

void add_trace(KnowledgeEvidencePacket& packet, KnowledgeEntryRef entry,
               std::vector<KnowledgeSourceRef> sources,
               const HybridRetrievalSignals& signals, const double score,
               std::optional<KnowledgeChunkId> chunk_id = std::nullopt) {
  packet.source_trace.push_back({std::move(entry), std::move(sources),
                                 std::move(chunk_id), signals, clamp01(score)});
}

void append_node_item(const RankedKnowledgeNode& ranked,
                      DependencyTracker& dependencies,
                      const KnowledgeQualityStore* quality_store,
                      KnowledgeEvidencePacket& packet) {
  KnowledgeEvidencePacketItem item;
  item.id = ranked.candidate.node.id.value;
  item.topic = std::string(to_string(ranked.candidate.node.kind));
  item.content = compact_node_content(ranked.candidate.node);
  item.retrieval_score = clamp01(ranked.score);
  item.graph_entries.push_back(
      {KnowledgeEntryKind::kNode, ranked.candidate.node.id.value});
  item.sources = ranked.candidate.sources.empty()
      ? sources_for_node(dependencies, ranked.candidate.node.id) : ranked.candidate.sources;
  item.properties = ranked.candidate.node.properties;
  if (ranked.candidate.signals.position_similarity &&
      ranked.candidate.signals.position_score) {
    item.properties["positionSimilarityScore"] =
        static_cast<double>(*ranked.candidate.signals.position_score);
  }
  item.node_kind = ranked.candidate.node.kind;
  if (ranked.candidate.sources.empty())
    apply_quality(item, node_quality(quality_store, ranked.candidate.node.id));

  switch (ranked.candidate.node.assertion_kind) {
    case KnowledgeAssertionKind::kFact:
      item.section = KnowledgePacketSection::kFact;
      break;
    case KnowledgeAssertionKind::kObservation:
      item.section = KnowledgePacketSection::kObservation;
      break;
    case KnowledgeAssertionKind::kHypothesis:
      append_uncertainty(
          packet.uncertainties, packet.budget, "hypothesis",
          item.content,
          KnowledgeEntryRef{KnowledgeEntryKind::kNode,
                            ranked.candidate.node.id.value});
      add_trace(packet,
                {KnowledgeEntryKind::kNode, ranked.candidate.node.id.value},
                item.sources, ranked.candidate.signals, ranked.score);
      return;
  }

  const auto sources = item.sources;
  if (append_item_with_budget(packet, item)) {
    add_quality_uncertainties(item, packet);
    add_trace(packet,
              {KnowledgeEntryKind::kNode, ranked.candidate.node.id.value},
              sources, ranked.candidate.signals, ranked.score);
  }
}

void append_chunk_item(const RankedKnowledgeChunk& ranked, GraphStore& graph,
                       ChunkRegistry& chunks, KnowledgeEvidencePacket& packet) {
  KnowledgeEvidencePacketItem item;
  item.id = ranked.candidate.chunk.metadata.id.value;
  item.topic = ranked.candidate.chunk.metadata.topic;
  item.content = ranked.candidate.chunk.content;
  item.retrieval_score = clamp01(ranked.score);
  item.section = linked_chunk_section(graph, chunks, ranked.candidate.chunk);
  item.graph_entries = chunk_entries(chunks, ranked.candidate.chunk.metadata.id);
  if (item.graph_entries.size() == 1 && item.graph_entries.front().kind == KnowledgeEntryKind::kNode) {
    if (const auto node = graph.node({item.graph_entries.front().id})) {
      item.properties = node->properties;
      item.node_kind = node->kind;
    }
  }
  item.sources = ranked.candidate.chunk.sources;
  apply_chunk_quality(item, ranked.candidate.chunk.metadata);

  if (item.section == KnowledgePacketSection::kUncertainty) {
    append_uncertainty(
        packet.uncertainties, packet.budget, "hypothesis_chunk", item.content,
        KnowledgeEntryRef{KnowledgeEntryKind::kChunk,
                          ranked.candidate.chunk.metadata.id.value});
    add_trace(packet,
              {KnowledgeEntryKind::kChunk,
               ranked.candidate.chunk.metadata.id.value},
              item.sources, ranked.candidate.signals, ranked.score,
              ranked.candidate.chunk.metadata.id);
    return;
  }

  const auto sources = item.sources;
  if (append_item_with_budget(packet, item)) {
    add_quality_uncertainties(item, packet);
    add_trace(packet,
              {KnowledgeEntryKind::kChunk,
               ranked.candidate.chunk.metadata.id.value},
              sources, ranked.candidate.signals, ranked.score,
              ranked.candidate.chunk.metadata.id);
  }
}


void sort_packet_items(std::vector<KnowledgeEvidencePacketItem>& items) {
  std::stable_sort(items.begin(), items.end(),
                   [](const KnowledgeEvidencePacketItem& lhs,
                      const KnowledgeEvidencePacketItem& rhs) {
                     if (lhs.retrieval_score != rhs.retrieval_score) {
                       return lhs.retrieval_score > rhs.retrieval_score;
                     }
                     return false;  // preserve authoritative ranking ties
                   });
}

void filter_source_trace_to_packet(KnowledgeEvidencePacket& packet) {
  std::set<std::string> ids;
  const auto collect = [&](const std::vector<KnowledgeEvidencePacketItem>& items) {
    for (const auto& item : items) ids.insert(item.id);
  };
  collect(packet.facts);
  collect(packet.observations);
  collect(packet.evidence);
  packet.evidence.erase(std::remove_if(packet.evidence.begin(), packet.evidence.end(), [&](const auto& item) {
    if (item.topic != "relationship") return false;
    for (const auto* key : {"fromNodeId", "toNodeId"}) {
      const auto found = item.properties.find(key);
      const auto* ref = found == item.properties.end() ? nullptr : std::get_if<std::string>(&found->second);
      if (!ref || !ids.contains(*ref)) { ids.erase(item.id); return true; }
    }
    return false;
  }), packet.evidence.end());
  packet.graph_trace.erase(std::remove_if(packet.graph_trace.begin(), packet.graph_trace.end(),
      [&](const auto& step) { return !ids.contains(step.edge_id.value); }), packet.graph_trace.end());
  for (const auto& uncertainty : packet.uncertainties) {
    if (uncertainty.entry) ids.insert(uncertainty.entry->id);
  }
  packet.source_trace.erase(
      std::remove_if(packet.source_trace.begin(), packet.source_trace.end(),
                     [&](const KnowledgePacketSourceTrace& trace) {
                       return ids.find(trace.entry.id) == ids.end();
                     }),
      packet.source_trace.end());
}

void enforce_packet_token_budget(KnowledgeEvidencePacket& packet,
                                 const KnowledgeScopeRelevance current_board) {
  sort_packet_items(packet.facts);
  sort_packet_items(packet.observations);
  sort_packet_items(packet.evidence);

  auto over_budget = [&] {
    return packet_content_tokens(packet) > packet.budget.max_tokens;
  };
  while (over_budget()) {
    packet.token_budget_exhausted = true;
    if (packet.evidence.size() > 1) {
      packet.evidence.pop_back();
      continue;
    }
    if (packet.observations.size() > 1) {
      packet.observations.pop_back();
      continue;
    }
    if (packet.facts.size() > 1) {
      packet.facts.pop_back();
      continue;
    }
    if (packet.uncertainties.size() > 1) {
      packet.uncertainties.pop_back();
      continue;
    }
    if (packet.recent_context) {
      packet.recent_context.reset();
      continue;
    }
    if (estimated_tokens(packet.query) > 48) {
      packet.query = trim_utf8_to_tokens(std::move(packet.query), 48);
      continue;
    }
    if (packet.current_position &&
        current_board != KnowledgeScopeRelevance::kRequired) {
      packet.current_position.reset();
      continue;
    }
    // Required board context and the last selected evidence item are preserved.
    // Their producer-level limits make this branch exceptional; keep the
    // exhaustion flag explicit rather than silently deleting required context.
    break;
  }
  filter_source_trace_to_packet(packet);
}

KnowledgeAnswerability assess_answerability(
    const KnowledgeEvidencePacketRequest& request,
    const KnowledgeEvidencePacket& packet) {
  KnowledgeAnswerability out;
  out.current_position_satisfied =
      request.route.current_board != KnowledgeScopeRelevance::kRequired ||
      packet.current_position.has_value();

  double score_sum = 0.0;
  std::size_t score_count = 0;
  std::size_t sourced_items = 0;
  const auto collect = [&](const std::vector<KnowledgeEvidencePacketItem>& items) {
    for (const auto& item : items) {
      score_sum += clamp01(item.retrieval_score);
      ++score_count;
      if (!item.sources.empty()) ++sourced_items;
    }
  };
  collect(packet.facts);
  collect(packet.observations);
  collect(packet.evidence);

  for (const auto& trace : packet.source_trace) {
    out.has_exact_support = out.has_exact_support || trace.retrieval_signals.exact_statistics;
    out.has_source_trace = out.has_source_trace || !trace.sources.empty();
  }
  out.has_graph_support = !packet.graph_trace.empty();
  const auto supplied = [&](const KnowledgeQueryRoute& scope) {
    for (const auto* section : {&packet.facts, &packet.observations, &packet.evidence})
      for (const auto& item : *section) {
        KnowledgeNode node;
        node.kind = item.node_kind;
        node.properties = item.properties;
        if (knowledge_node_matches_scope(node, scope)) return true;
      }
    return false;
  };
  const auto check_axis = [&](const auto& values, auto member, auto& missing) {
    for (const auto& value : values) {
      auto scoped = request.route;
      scoped.profile_scope.*member = {value};
      if (!supplied(scoped)) missing.push_back(value);
    }
  };
  check_axis(request.route.profile_scope.topics, &ai::ProfileQueryScope::topics, out.missing_topics);
  check_axis(request.route.profile_scope.time_controls, &ai::ProfileQueryScope::time_controls, out.missing_time_controls);
  check_axis(request.route.profile_scope.phases, &ai::ProfileQueryScope::phases, out.missing_phases);
  check_axis(request.route.profile_scope.player_colors, &ai::ProfileQueryScope::player_colors, out.missing_player_colors);
  if (!out.missing_topics.empty() || !out.missing_time_controls.empty() ||
      !out.missing_phases.empty() || !out.missing_player_colors.empty()) out.reasons.push_back("requested_scope_missing");
  if (request.route.profile_scope.needs_endgame_material_type) {
    bool material = false;
    for (const auto* section : {&packet.facts, &packet.observations, &packet.evidence})
      for (const auto& item : *section)
        material |= item.node_kind == KnowledgeNodeKind::kEndgameType || item.properties.contains("endgame_type");
    if (!material) out.reasons.push_back("endgame_material_evidence_missing");
  }

  const std::size_t material_count =
      packet.facts.size() + packet.observations.size() + packet.evidence.size();
  const double retrieval_confidence =
      score_count == 0 ? 0.0 : score_sum / static_cast<double>(score_count);
  const double source_fraction =
      material_count == 0
          ? 0.0
          : static_cast<double>(sourced_items) /
                static_cast<double>(material_count);
  out.confidence = clamp01(0.72 * retrieval_confidence + 0.28 * source_fraction);

  if (!out.current_position_satisfied) {
    out.reasons.push_back("required_current_position_missing");
  }
  if (material_count == 0) out.reasons.push_back("no_retrieved_material");

  switch (request.route.intent) {
    case KnowledgeQueryIntent::kExactStatistic:
      if (!out.has_exact_support) out.reasons.push_back("exact_support_missing");
      break;
    case KnowledgeQueryIntent::kRelationship:
    case KnowledgeQueryIntent::kCausalAnalysis:
      if (!out.has_graph_support) out.reasons.push_back("graph_support_missing");
      if (material_count < 2) out.reasons.push_back("insufficient_causal_evidence");
      break;
    case KnowledgeQueryIntent::kEvidence:
      if (!out.has_source_trace) out.reasons.push_back("source_trace_missing");
      break;
    case KnowledgeQueryIntent::kTrend:
      if (!std::any_of(packet.observations.begin(), packet.observations.end(), [](const auto& item) {
            return item.node_kind == KnowledgeNodeKind::kTrend || item.properties.contains("trend");
          })) {
        out.reasons.push_back("recent_evidence_missing");
      }
      break;
    case KnowledgeQueryIntent::kSimilarity:
    case KnowledgeQueryIntent::kCurrentPosition:
    case KnowledgeQueryIntent::kGeneralPersonal:
    case KnowledgeQueryIntent::kGeneralChess:
    case KnowledgeQueryIntent::kUnknown:
      break;
  }

  out.answerable = out.reasons.empty() && material_count > 0;
  return out;
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Public packet contract
// -----------------------------------------------------------------------------

std::string_view to_string(const KnowledgePacketSection section) noexcept {
  switch (section) {
    case KnowledgePacketSection::kFact:
      return "fact";
    case KnowledgePacketSection::kObservation:
      return "observation";
    case KnowledgePacketSection::kEvidence:
      return "evidence";
    case KnowledgePacketSection::kUncertainty:
      return "uncertainty";
  }
  return "evidence";
}

EvidencePacketBuilder::EvidencePacketBuilder(
    GraphStore& graph, ChunkRegistry& chunks, DependencyTracker& dependencies,
    const KnowledgeQualityStore* quality_store)
    : graph_(graph),
      chunks_(chunks),
      dependencies_(dependencies),
      quality_store_(quality_store) {}

KnowledgeEvidencePacket EvidencePacketBuilder::build(
    const KnowledgeEvidencePacketRequest& request) const {
  KnowledgeEvidencePacket packet;
  packet.budget = packet_budget_for(request);
  packet.query = trim_utf8_to_tokens(request.query_text, kMaxPacketQueryTokens);

  // Current board context is admitted only when Update-118 routing says it is
  // relevant. Merely having a board open is not sufficient.
  if (request.current_position &&
      request.route.current_board != KnowledgeScopeRelevance::kNone) {
    packet.current_position = trim_utf8_to_tokens(
        *request.current_position, packet.budget.current_position_tokens);
  }
  if (request.recent_context && relevant_recent_context(request)) {
    packet.recent_context = trim_utf8_to_tokens(
        *request.recent_context, packet.budget.recent_context_tokens);
  }

  const auto represented = [&](const std::string& id) {
    for (const auto* section : {&packet.facts, &packet.observations, &packet.evidence})
      for (const auto& item : *section)
        for (const auto& entry : item.graph_entries) if (entry.id == id) return true;
    return false;
  };
  const auto packet_id = [&](const std::string& graph_id) {
    for (const auto* section : {&packet.facts, &packet.observations, &packet.evidence})
      for (const auto& item : *section)
        for (const auto& entry : item.graph_entries) if (entry.id == graph_id) return item.id;
    return std::string{};
  };
  const auto append_node = [&](const RankedKnowledgeNode& ranked) {
    if (ranked.candidate.sources.empty() && dependencies_.is_invalidated(
        {KnowledgeEntryKind::kNode, ranked.candidate.node.id.value})) return;
    if (!represented(ranked.candidate.node.id.value))
      append_node_item(ranked, dependencies_, quality_store_, packet);
  };
  for (const auto& topic : request.route.profile_scope.topics) {
    auto scoped = request.route;
    scoped.profile_scope.topics = {topic};
    for (const auto& ranked : request.ranked.nodes) {
      if (!knowledge_node_matches_scope(ranked.candidate.node, scoped)) continue;
      append_node(ranked);
      break;
    }
  }
  // Reserve every requested comparison cell before optional examples, then
  // prefer fresh exact facts. One graph assertion is never sent twice through
  // its node and its semantic chunk.
  if (request.route.profile_scope.compare_scopes) {
    const auto values = [](const std::vector<std::string>& axis) {
      return axis.empty() ? std::vector<std::string>{""} : axis;
    };
    for (const auto& tc : values(request.route.profile_scope.time_controls))
      for (const auto& color : values(request.route.profile_scope.player_colors))
        for (const auto& phase : values(request.route.profile_scope.phases)) {
          auto cell = request.route;
          if (!tc.empty()) cell.profile_scope.time_controls = {tc};
          if (!color.empty()) cell.profile_scope.player_colors = {color};
          if (!phase.empty()) cell.profile_scope.phases = {phase};
          for (const auto& ranked : request.ranked.nodes) {
            if (!knowledge_node_matches_scope(ranked.candidate.node, cell)) continue;
            append_node(ranked);
            break;
          }
        }
  }
  for (const auto& ranked : request.ranked.nodes) {
    if (ranked.candidate.signals.exact_statistics) append_node(ranked);
  }
  for (const auto& ranked : request.ranked.chunks) {
    const auto entries = chunk_entries(chunks_, ranked.candidate.chunk.metadata.id);
    if (dependencies_.is_invalidated({KnowledgeEntryKind::kChunk, ranked.candidate.chunk.metadata.id.value}) ||
        std::any_of(entries.begin(), entries.end(), [&](const auto& entry) { return dependencies_.is_invalidated(entry); })) continue;
    if (!entries.empty() && std::all_of(entries.begin(), entries.end(),
        [&](const auto& entry) { return represented(entry.id); })) continue;
    append_chunk_item(ranked, graph_, chunks_, packet);
  }
  for (const auto& ranked : request.ranked.nodes) append_node(ranked);

  // Only actual relationships between supplied assertions may support a
  // relationship answer. An arbitrary traversal hit is not causal evidence.
  for (const auto& step : request.retrieval.graph_steps) {
    if (dependencies_.is_invalidated({KnowledgeEntryKind::kEdge, step.edge.id.value})) continue;
    if (!represented(step.edge.from.value) || !represented(step.edge.to.value)) continue;
    KnowledgeEvidencePacketItem relation;
    relation.id = step.edge.id.value;
    relation.topic = "relationship";
    relation.section = KnowledgePacketSection::kEvidence;
    relation.content = std::string(to_string(step.edge.kind));
    relation.properties = {{"fromNodeId", packet_id(step.edge.from.value)}, {"toNodeId", packet_id(step.edge.to.value)},
        {"relation", std::string(to_string(step.edge.kind))}, {"causationEstablished", false}};
    relation.graph_entries = {{KnowledgeEntryKind::kEdge, step.edge.id.value}};
    for (const auto& source : dependencies_.provenance_for(relation.graph_entries.front()))
      if (source.source.valid()) relation.sources.push_back(source.source);
    if (relation.sources.empty() || !append_item_with_budget(packet, relation)) continue;
    packet.graph_trace.push_back({step.edge.id, step.node.id, step.edge.kind, step.depth});
  }

  if (request.retrieval.seed_budget_exhausted) {
    append_uncertainty(packet.uncertainties, packet.budget,
                       "seed_budget_exhausted",
                       "Graph seed budget limited candidate discovery.");
  }
  if (request.retrieval.traversal_budget_exhausted) {
    append_uncertainty(packet.uncertainties, packet.budget,
                       "traversal_budget_exhausted",
                       "Graph traversal budget limited relationship discovery.");
  }
  if (request.retrieval.chunk_budget_exhausted || packet.token_budget_exhausted) {
    append_uncertainty(packet.uncertainties, packet.budget,
                       "context_budget_exhausted",
                       "Additional relevant evidence existed outside the bounded context.");
  }

  enforce_packet_token_budget(packet, request.route.current_board);
  packet.estimated_tokens = packet_content_tokens(packet);
  if (packet.estimated_tokens > packet.budget.max_tokens) {
    packet.token_budget_exhausted = true;
  }
  packet.answerability = assess_answerability(request, packet);
  return packet;
}

}  // namespace kchess::knowledge
