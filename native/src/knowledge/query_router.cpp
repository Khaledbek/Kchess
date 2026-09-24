#include "query_router.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <string_view>

namespace kchess::knowledge {
namespace {

std::string lower_ascii(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
    return c < 128 ? static_cast<char>(std::tolower(c)) : static_cast<char>(c);
  });
  return text;
}

template <std::size_t N>
bool contains_any(std::string_view text,
                  const std::array<std::string_view, N>& terms) {
  return std::any_of(terms.begin(), terms.end(), [&](std::string_view term) {
    return text.find(term) != std::string_view::npos;
  });
}

bool has_entity(const std::vector<KnowledgeQueryEntity>& entities,
                KnowledgeQueryEntityKind kind) {
  return std::any_of(entities.begin(), entities.end(), [&](const auto& entity) {
    return entity.kind == kind;
  });
}

bool is_current_position_intent(ai::CoachIntent intent) {
  return intent == ai::CoachIntent::position ||
      intent == ai::CoachIntent::move_explanation ||
      intent == ai::CoachIntent::plan || intent == ai::CoachIntent::tactic ||
      intent == ai::CoachIntent::game_review;
}

KnowledgeQueryIntent classify_knowledge_intent(
    std::string_view text, const ai::QueryPlan& plan,
    const std::vector<KnowledgeQueryEntity>& entities) {
  constexpr std::array causal_terms{
      std::string_view{"why"}, std::string_view{"warum"},
      std::string_view{"reason"}, std::string_view{"cause"},
      std::string_view{"ursache"}, std::string_view{"führt dazu"},
      std::string_view{"lead to"}, std::string_view{"because"},
  };
  constexpr std::array relationship_terms{
      std::string_view{"which openings lead"}, std::string_view{"welche openings führen"},
      std::string_view{"welche eröffnung führt"}, std::string_view{"connected"},
      std::string_view{"zusammenhang"}, std::string_view{"correlat"},
      std::string_view{"transition"}, std::string_view{"übergeh"},
      std::string_view{"transpose"}, std::string_view{"transposition"},
  };
  constexpr std::array trend_terms{
      std::string_view{"improv"}, std::string_view{"verbesser"},
      std::string_view{"declin"}, std::string_view{"verschlechter"},
      std::string_view{"trend"}, std::string_view{"recent vs"},
      std::string_view{"aktuell vs"}, std::string_view{"früher"},
      std::string_view{"historisch"},
  };
  constexpr std::array similarity_terms{
      std::string_view{"similar position"}, std::string_view{"similar positions"},
      std::string_view{"ähnliche stellung"}, std::string_view{"ähnlichen stellung"},
      std::string_view{"same structure"}, std::string_view{"gleiche struktur"},
  };
  constexpr std::array evidence_terms{
      std::string_view{"show evidence"}, std::string_view{"show proof"},
      std::string_view{"belege"}, std::string_view{"beweis"},
      std::string_view{"welche partien"}, std::string_view{"which games"},
  };
  constexpr std::array exact_terms{
      std::string_view{"how many"}, std::string_view{"wie viele"},
      std::string_view{"how often"}, std::string_view{"wie oft"},
      std::string_view{"most common"}, std::string_view{"am häufigsten"},
      std::string_view{"win rate"}, std::string_view{"siegquote"},
      std::string_view{"average"}, std::string_view{"durchschnitt"},
      std::string_view{"rating"}, std::string_view{"elo"},
      std::string_view{"accuracy"}, std::string_view{"genauigkeit"},
      std::string_view{"statistics"}, std::string_view{"statistik"},
  };

  if (contains_any(text, similarity_terms)) return KnowledgeQueryIntent::kSimilarity;
  if (contains_any(text, causal_terms)) return KnowledgeQueryIntent::kCausalAnalysis;
  if (contains_any(text, relationship_terms)) return KnowledgeQueryIntent::kRelationship;
  if (contains_any(text, trend_terms) ||
      has_entity(entities, KnowledgeQueryEntityKind::kTemporalScope)) {
    return KnowledgeQueryIntent::kTrend;
  }
  if (plan.profile_scope.wants_proof || contains_any(text, evidence_terms)) {
    return KnowledgeQueryIntent::kEvidence;
  }
  if (contains_any(text, exact_terms) ||
      has_entity(entities, KnowledgeQueryEntityKind::kStatisticMetric)) {
    return KnowledgeQueryIntent::kExactStatistic;
  }
  if (plan.needs_position &&
      (is_current_position_intent(plan.intent) ||
       plan.query_family == ai::QueryFamily::position)) {
    return KnowledgeQueryIntent::kCurrentPosition;
  }
  if (plan.query_family == ai::QueryFamily::personal_chess || plan.needs_profile) {
    return KnowledgeQueryIntent::kGeneralPersonal;
  }
  if (plan.query_family == ai::QueryFamily::general_chess) {
    return KnowledgeQueryIntent::kGeneralChess;
  }
  return KnowledgeQueryIntent::kUnknown;
}

KnowledgeRetrievalChannels channels_for(KnowledgeQueryIntent intent,
                                         const ai::QueryPlan& plan) {
  KnowledgeRetrievalChannels out;
  switch (intent) {
    case KnowledgeQueryIntent::kExactStatistic:
      out.exact_statistics = true;
      out.graph = true;
      out.lexical = true;
      break;
    case KnowledgeQueryIntent::kRelationship:
      out.graph = true;
      out.lexical = true;
      out.vector = true;
      break;
    case KnowledgeQueryIntent::kCausalAnalysis:
      out.exact_statistics = true;
      out.graph = true;
      out.lexical = true;
      out.vector = true;
      break;
    case KnowledgeQueryIntent::kTrend:
      out.exact_statistics = true;
      out.graph = true;
      out.lexical = true;
      break;
    case KnowledgeQueryIntent::kEvidence:
      out.exact_statistics = true;
      out.graph = true;
      out.lexical = true;
      break;
    case KnowledgeQueryIntent::kSimilarity:
      out.graph = true;
      out.lexical = true;
      out.vector = true;
      out.position_similarity = plan.needs_position;
      break;
    case KnowledgeQueryIntent::kCurrentPosition:
      out.graph = true;
      out.lexical = true;
      out.vector = true;
      out.position_similarity = true;
      break;
    case KnowledgeQueryIntent::kGeneralPersonal:
      out.exact_statistics = true;
      out.graph = true;
      out.lexical = true;
      out.vector = true;
      break;
    case KnowledgeQueryIntent::kGeneralChess:
      // The personal Knowledge Graph is not a replacement for the existing
      // concept/theory retrieval path. Lexical/vector may later use explicitly
      // global chunks, but no player graph traversal is required here.
      out.lexical = true;
      out.vector = true;
      break;
    case KnowledgeQueryIntent::kUnknown:
      out.lexical = true;
      break;
  }
  return out;
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Public routing
// -----------------------------------------------------------------------------

bool knowledge_node_matches_scope(const KnowledgeNode& node,
                                  const KnowledgeQueryRoute& route) {
  const auto text = [&](std::string_view key) {
    const auto it = node.properties.find(std::string(key));
    if (it == node.properties.end()) return std::string{};
    const auto* value = std::get_if<std::string>(&it->second);
    return value ? lower_ascii(*value) : std::string{};
  };
  // Containers are traversal routes, never personal assertions by themselves.
  if (node.kind == KnowledgeNodeKind::kPlayer || node.kind == KnowledgeNodeKind::kAccount ||
      node.kind == KnowledgeNodeKind::kProvider || node.kind == KnowledgeNodeKind::kColor ||
      node.kind == KnowledgeNodeKind::kTimeControl || node.kind == KnowledgeNodeKind::kKnowledgeGap)
    return false;
  const auto& scope = route.profile_scope;
  const auto axis = [&](const std::vector<std::string>& values, const std::string& actual) {
    return values.empty() || std::find(values.begin(), values.end(), actual) != values.end();
  };
  if (!axis(scope.time_controls, text("time_control")) ||
      !axis(scope.player_colors, text("player_color").empty() ? text("color") : text("player_color")))
    return false;
  std::string surface = lower_ascii(std::string(to_string(node.kind)));
  for (const auto key : {"topic", "pattern_id", "pattern", "behavior", "skill", "phase", "statistic_key"})
    surface += " " + text(key);
  const auto contains = [&](const std::string& value) { return surface.find(value) != std::string::npos; };
  const auto topic = [&](const std::string& value) {
    if (value == "rating") return node.properties.contains("latestRecordedGameRating");
    if (value == "statistics") return node.kind == KnowledgeNodeKind::kStatistic;
    if (value == "opening") return contains("opening") || node.kind == KnowledgeNodeKind::kVariation ||
        node.kind == KnowledgeNodeKind::kRepertoirePattern;
    if (value == "play_style" || value == "strengths" || value == "weaknesses" ||
        value == "improvement" || value == "training") {
      return node.kind == KnowledgeNodeKind::kStatistic || node.kind == KnowledgeNodeKind::kStrength ||
          node.kind == KnowledgeNodeKind::kWeakness || node.kind == KnowledgeNodeKind::kBehavior ||
          node.kind == KnowledgeNodeKind::kHabit || node.kind == KnowledgeNodeKind::kStylePattern ||
          node.kind == KnowledgeNodeKind::kTrend || node.kind == KnowledgeNodeKind::kHypothesis ||
          node.kind == KnowledgeNodeKind::kConversionPattern || node.kind == KnowledgeNodeKind::kDefensePattern ||
          node.kind == KnowledgeNodeKind::kRecoveryPattern || node.kind == KnowledgeNodeKind::kTimeManagementPattern;
    }
    return contains(value);
  };
  if (!scope.phases.empty()) {
    const auto explicit_phase = text("phase");
    if (!explicit_phase.empty()) {
      if (!axis(scope.phases, explicit_phase)) return false;
    } else if (!std::any_of(scope.phases.begin(), scope.phases.end(), topic)) {
      return false;
    }
  }
  if (!scope.topics.empty() && !std::any_of(scope.topics.begin(), scope.topics.end(), topic)) return false;
  // Specific content and a general goal compose, e.g. endgame weaknesses.
  bool has_content = false, content_match = false;
  for (const auto& value : scope.topics) {
    if (value == "play_style" || value == "strengths" || value == "weaknesses" ||
        value == "improvement" || value == "training" || value == "statistics") continue;
    has_content = true;
    content_match |= topic(value);
  }
  if (has_content && !content_match) return false;
  const auto entity_axis = [&](KnowledgeQueryEntityKind kind, const auto& matches) {
    bool requested = false, found = false;
    for (const auto& entity : route.entities) if (entity.kind == kind) {
      requested = true;
      found |= matches(lower_ascii(entity.canonical_value));
    }
    return !requested || found;
  };
  if (!entity_axis(KnowledgeQueryEntityKind::kEcoCode, [&](const auto& value) {
    return text("eco") == value || text("opening_eco") == value;
  })) return false;
  if (!entity_axis(KnowledgeQueryEntityKind::kOpeningName, [&](const auto& value) {
    return text("name") == value || text("opening_name") == value || text("opening_family") == value;
  })) return false;
  if (!entity_axis(KnowledgeQueryEntityKind::kTemporalScope, [&](const auto& value) {
    if (value != "recent") return true;
    const auto it = node.properties.find("recent");
    const auto* recent = it == node.properties.end() ? nullptr : std::get_if<bool>(&it->second);
    return text("temporal_scope") == "recent" || text("scope") == "recent" ||
        text("scope") == "recent_form" || (recent && *recent);
  })) return false;
  return true;
}

KnowledgeQueryRoute KnowledgeQueryRouter::route(
    std::string_view query_text, const ai::QueryPlan& plan,
    bool current_position_available) const {
  KnowledgeQueryRoute out;
  out.query_family = plan.query_family;
  out.coach_intent = plan.intent;
  out.profile_scope = plan.profile_scope;
  out.current_position_available = current_position_available;
  out.entities = entity_extractor_.extract(query_text, plan);

  const std::string lower = lower_ascii(std::string(query_text));
  out.intent = classify_knowledge_intent(lower, plan, out.entities);
  out.channels = channels_for(out.intent, plan);

  const bool explicit_current_position = has_entity(
      out.entities, KnowledgeQueryEntityKind::kPositionReference);
  const bool plan_requires_position = plan.needs_position &&
      (is_current_position_intent(plan.intent) ||
       plan.query_family == ai::QueryFamily::position);

  // Availability is not relevance. This prevents a board that happens to be
  // open from taking over a historical/profile query. Explicit user reference
  // or the already-authoritative QueryPlan is required before the board is
  // treated as retrieval scope.
  if (plan_requires_position || explicit_current_position) {
    out.current_board = KnowledgeScopeRelevance::kRequired;
  } else if (current_position_available &&
             out.intent == KnowledgeQueryIntent::kSimilarity) {
    out.current_board = KnowledgeScopeRelevance::kOptional;
  } else {
    out.current_board = KnowledgeScopeRelevance::kNone;
    out.channels.position_similarity = false;
  }

  const bool personal_scope = plan.needs_profile ||
      plan.query_family == ai::QueryFamily::personal_chess ||
      plan.profile_scope.compare_scopes;
  const bool historical_intent =
      out.intent == KnowledgeQueryIntent::kExactStatistic ||
      out.intent == KnowledgeQueryIntent::kRelationship ||
      out.intent == KnowledgeQueryIntent::kCausalAnalysis ||
      out.intent == KnowledgeQueryIntent::kTrend ||
      out.intent == KnowledgeQueryIntent::kEvidence ||
      out.intent == KnowledgeQueryIntent::kGeneralPersonal;

  if (personal_scope && historical_intent) {
    out.historical_profile = KnowledgeScopeRelevance::kRequired;
  } else if (personal_scope) {
    out.historical_profile = KnowledgeScopeRelevance::kOptional;
  } else {
    out.historical_profile = KnowledgeScopeRelevance::kNone;
  }
  out.requires_player_scope =
      out.historical_profile != KnowledgeScopeRelevance::kNone;

  // Never let the presence of the board broaden a general/personal historical
  // request. Conversely, mixed position+personal queries may legitimately need
  // both scopes because QueryPlan.needs_position and needs_profile are
  // independent flags.
  if (out.current_board == KnowledgeScopeRelevance::kNone &&
      out.intent != KnowledgeQueryIntent::kGeneralChess) {
    out.channels.position_similarity = false;
  }

  out.confidence = std::clamp(plan.routing_confidence, 0.0, 1.0);
  if (out.intent != KnowledgeQueryIntent::kUnknown) {
    out.confidence = std::max(out.confidence, 0.80);
  }
  if (explicit_current_position && !current_position_available) {
    out.confidence = std::min(out.confidence, 0.70);
  }
  return out;
}

std::string_view to_string(KnowledgeQueryIntent intent) noexcept {
  switch (intent) {
    case KnowledgeQueryIntent::kUnknown: return "unknown";
    case KnowledgeQueryIntent::kExactStatistic: return "exact_statistic";
    case KnowledgeQueryIntent::kRelationship: return "relationship";
    case KnowledgeQueryIntent::kCausalAnalysis: return "causal_analysis";
    case KnowledgeQueryIntent::kTrend: return "trend";
    case KnowledgeQueryIntent::kEvidence: return "evidence";
    case KnowledgeQueryIntent::kSimilarity: return "similarity";
    case KnowledgeQueryIntent::kCurrentPosition: return "current_position";
    case KnowledgeQueryIntent::kGeneralPersonal: return "general_personal";
    case KnowledgeQueryIntent::kGeneralChess: return "general_chess";
  }
  return "unknown";
}

std::string_view to_string(KnowledgeScopeRelevance relevance) noexcept {
  switch (relevance) {
    case KnowledgeScopeRelevance::kNone: return "none";
    case KnowledgeScopeRelevance::kOptional: return "optional";
    case KnowledgeScopeRelevance::kRequired: return "required";
  }
  return "none";
}

}  // namespace kchess::knowledge
