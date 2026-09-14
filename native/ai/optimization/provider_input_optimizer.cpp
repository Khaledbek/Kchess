#include "provider_input_optimizer.h"

#include <algorithm>
#include <optional>
#include <set>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include "../dto/query_plan.h"

namespace kchess::ai {
namespace {

// -----------------------------------------------------------------------------
// Section: Evidence priority and trimming
// -----------------------------------------------------------------------------

int priority(EvidenceKind kind) {
  switch (kind) {
    case EvidenceKind::candidate_moves:
    case EvidenceKind::move_contrast:
      return -1;
    case EvidenceKind::engine:
    case EvidenceKind::existing_analysis:
      return 0;
    case EvidenceKind::position_features:
    case EvidenceKind::tactical_motifs:
    case EvidenceKind::position_weaknesses:
    case EvidenceKind::strategic_plans:
      return 1;
    case EvidenceKind::opening:
    case EvidenceKind::theory:
    case EvidenceKind::user_profile:
    case EvidenceKind::practicality:
      return 2;
    default:
      return 3;
  }
}

std::size_t evidence_budget(ResponseDepth depth, std::size_t context_tokens,
                            const QueryPlan* plan, std::size_t profile_target) {
  const std::size_t base = depth == ResponseDepth::concise
                               ? 500
                               : depth == ResponseDepth::detailed ? 1400 : 900;
  const bool compact_concept = plan &&
      (plan->intent == CoachIntent::chess_concept ||
       plan->intent == CoachIntent::chess_rules ||
       plan->intent == CoachIntent::chess_history);
  const std::size_t total = depth == ResponseDepth::concise
      ? 800
      : depth == ResponseDepth::detailed ? 2500 : compact_concept ? 800 : 1500;
  if (plan && plan->needs_profile && profile_target > 0) {
    const auto objective_reserve = plan->needs_position ? 600U : 0U;
    const auto target = std::min<std::size_t>(4000, profile_target + objective_reserve);
    return context_tokens >= 4500 ? 0 : std::min(target, 4500 - context_tokens);
  }
  return context_tokens >= total ? 0 : std::min(base, total - context_tokens);
}

std::size_t estimated_tokens(std::string_view text) {
  return (text.size() + 3) / 4;
}

std::size_t item_cost(const EvidenceItem& item) {
  return estimated_tokens(item.payload) + estimated_tokens(item.id) + 12;
}

bool chunk_matches_scope_value(const nlohmann::json& chunk,
                               std::string_view key,
                               const std::string& expected) {
  if (!chunk.is_object() || !chunk.contains("scope") ||
      !chunk["scope"].is_object()) {
    return false;
  }
  const auto& scope = chunk["scope"];
  return scope.contains(std::string(key)) && scope[std::string(key)].is_string() &&
      scope[std::string(key)].get<std::string>() == expected;
}

// -----------------------------------------------------------------------------
// Section: Atomic profile chunk selection
// -----------------------------------------------------------------------------

std::optional<EvidenceItem> fit_profile(const EvidenceItem& item,
                                        std::size_t budget) {
  using json = nlohmann::json;
  if (budget == 0) return std::nullopt;

  auto context = json::parse(item.payload, nullptr, false);
  if (!context.is_object() || context.value("schema", json{}) != "profile.context.v3" ||
      !context.contains("chunks") || !context["chunks"].is_array()) {
    return std::nullopt;
  }
  EvidenceItem fitted = item;
  const auto required_groups = context.value("requiredChunkGroups", json::array());
  // Native selection requirements are checked here; the LLM does not need the
  // duplicated ID lists or internal budget hints in its input.
  context.erase("requiredChunkGroups");
  context.erase("providerTokenBudget");
  fitted.payload = context.dump();
  if (item_cost(fitted) <= budget) return fitted;
  const auto original_chunks = context["chunks"];
  context["chunks"] = json::array();
  if (!context.contains("limitations") || !context["limitations"].is_array()) {
    context["limitations"] = json::array();
  }
  context["limitations"].push_back("provider_context_budget");

  const auto original_skipped = context.value("skippedChunkCount", json{});
  const std::size_t skipped_before = original_skipped.is_number_unsigned()
      ? original_skipped.get<std::size_t>() : 0;

  auto serialize = [&] {
    context["skippedChunkCount"] = skipped_before + original_chunks.size() -
        context["chunks"].size();
    fitted.payload = context.dump();
  };

  std::set<std::size_t> selected_indexes;
  const auto try_add = [&](std::size_t index) {
    if (index >= original_chunks.size() || selected_indexes.contains(index)) return false;
    const auto& chunk = original_chunks[index];
    if (!chunk.is_object()) return false;
    if (chunk.contains("data") && chunk["data"].is_object()) {
      for (const auto* key : {"fromNodeId", "toNodeId"}) {
        const auto ref = chunk["data"].value(key, json{});
        if (ref.is_string() && !std::any_of(context["chunks"].begin(), context["chunks"].end(),
            [&](const auto& selected) { return selected.value("nodeId", json{}) == ref; })) return false;
      }
    }
    const auto parent = chunk.value("supportsNodeId", json{});
    if (parent.is_string() && !parent.get_ref<const std::string&>().empty()) {
      const bool parent_selected = std::any_of(
          context["chunks"].begin(), context["chunks"].end(), [&](const auto& selected) {
            return selected.is_object() && selected.value("nodeId", json{}) == parent;
          });
      if (!parent_selected) return false;
    }
    context["chunks"].push_back(chunk);
    serialize();
    if (item_cost(fitted) > budget) {
      context["chunks"].erase(context["chunks"].end() - 1);
      serialize();
      return false;
    }
    selected_indexes.insert(index);
    return true;
  };

  // Comparison requests must preserve at least one chunk for each side before
  // optional chunks consume the provider budget.
  const auto requested = context.value("requestedScope", json::object());
  for (const auto& group : required_groups) {
    if (!group.is_array()) continue;
    for (const auto& id : group) {
      const auto found = std::find_if(original_chunks.begin(), original_chunks.end(),
          [&](const auto& chunk) { return chunk.value("nodeId", json{}) == id; });
      if (found == original_chunks.end()) continue;
      if (found->contains("data") && (*found)["data"].is_object()) {
        for (const auto* key : {"fromNodeId", "toNodeId"}) {
          const auto ref = (*found)["data"].value(key, json{});
          if (!ref.is_string()) continue;
          for (std::size_t i = 0; i < original_chunks.size(); ++i)
            if (original_chunks[i].value("nodeId", json{}) == ref) { try_add(i); break; }
        }
      }
      const auto index = static_cast<std::size_t>(found - original_chunks.begin());
      if (selected_indexes.contains(index) || try_add(index)) break;
    }
  }
  // Each requested topic survives trimming, not only comparison axes.
  if (requested.contains("topics") && requested["topics"].is_array()) {
    for (const auto& topic : requested["topics"]) {
      if (!topic.is_string()) continue;
      for (std::size_t i = 0; i < original_chunks.size(); ++i) {
        if (chunk_matches_scope_value(original_chunks[i], "topic", topic.get<std::string>()) && try_add(i)) break;
      }
    }
  }
  const bool compare_scopes = requested.value("compareScopes", false);
  if (compare_scopes) {
    const auto reserve_axis = [&](const char* request_key, std::string_view chunk_key) {
      if (!requested.contains(request_key) || !requested[request_key].is_array()) return;
      for (const auto& value : requested[request_key]) {
        if (!value.is_string()) continue;
        const auto expected = value.get<std::string>();
        for (std::size_t i = 0; i < original_chunks.size(); ++i) {
          if (chunk_matches_scope_value(original_chunks[i], chunk_key, expected) &&
              try_add(i)) {
            break;
          }
        }
      }
    };
    reserve_axis("timeControls", "timeControl");
    reserve_axis("phases", "phase");
    reserve_axis("playerColors", "playerColor");
  }

  for (std::size_t i = 0; i < original_chunks.size(); ++i) try_add(i);
  // A supporting example may sort before its parent. Retry once after parent
  // observations have been selected so linked deep evidence can still fit.
  for (std::size_t i = 0; i < original_chunks.size(); ++i) try_add(i);
  serialize();

  bool required_scope_preserved = true;
  for (const auto& group : required_groups) {
    if (!group.is_array() || !std::any_of(group.begin(), group.end(), [&](const auto& id) {
      return std::any_of(context["chunks"].begin(), context["chunks"].end(),
          [&](const auto& chunk) { return chunk.value("nodeId", json{}) == id; });
    })) required_scope_preserved = false;
  }
  // If all chunks carrying a previously supplied topic are removed, the
  // reduced packet cannot inherit the original answerability verdict.
  for (const auto* axis : {"topic", "timeControl", "phase", "playerColor"}) {
    const char* request_axis = std::string_view(axis) == "topic" ? "topics" :
        std::string_view(axis) == "timeControl" ? "timeControls" :
        std::string_view(axis) == "phase" ? "phases" : "playerColors";
    if (!requested.contains(request_axis) || !requested[request_axis].is_array()) continue;
    for (const auto& value : requested[request_axis]) {
      if (!value.is_string()) continue;
      const auto expected = value.get<std::string>();
      const auto matches = [&](const auto& chunk) { return chunk_matches_scope_value(chunk, axis, expected); };
      if (std::any_of(original_chunks.begin(), original_chunks.end(), matches) &&
          !std::any_of(context["chunks"].begin(), context["chunks"].end(), matches)) required_scope_preserved = false;
    }
  }
  if (compare_scopes) {
    const auto selected_has = [&](std::string_view key, const std::string& expected) {
      return std::any_of(context["chunks"].begin(), context["chunks"].end(),
                         [&](const auto& chunk) {
                           return chunk_matches_scope_value(chunk, key, expected);
                         });
    };
    if (requested.contains("timeControls") && requested["timeControls"].is_array()) {
      for (const auto& value : requested["timeControls"]) {
        if (value.is_string() && !selected_has("timeControl", value.get<std::string>())) {
          required_scope_preserved = false;
        }
      }
    }
    if (requested.contains("phases") && requested["phases"].is_array() &&
        requested["phases"].size() > 1) {
      for (const auto& value : requested["phases"]) {
        if (value.is_string() && !selected_has("phase", value.get<std::string>())) {
          required_scope_preserved = false;
        }
      }
    }
    if (requested.contains("playerColors") && requested["playerColors"].is_array()) {
      for (const auto& value : requested["playerColors"]) {
        if (value.is_string() && !selected_has("playerColor", value.get<std::string>())) {
          required_scope_preserved = false;
        }
      }
    }
  }

  const bool source_complete = context.value("scopeComplete", false);
  const bool available = !context["chunks"].empty() && source_complete &&
      required_scope_preserved;
  context["contextStatus"] = available ? "available" : "insufficient_evidence";
  if (!required_scope_preserved) {
    context["limitations"].push_back("provider_budget_scope_incomplete");
    context["scopeComplete"] = false;
  }
  serialize();

  if (context["chunks"].empty()) fitted.confidence = 0.0;
  if (item_cost(fitted) > budget) return std::nullopt;
  return fitted;
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Provider evidence selection
// -----------------------------------------------------------------------------

std::vector<EvidenceItem> optimize_provider_evidence(
    const std::vector<EvidenceItem>& evidence,
    ResponseDepth depth,
    std::size_t context_tokens,
    const QueryPlan* plan,
    ProviderInputOptimizationStats* stats) {
  ProviderInputOptimizationStats local_stats;
  local_stats.input_items = evidence.size();
  std::vector<const EvidenceItem*> ordered;
  ordered.reserve(evidence.size());
  for (const auto& item : evidence) {
    if (item.payload.empty()) {
      ++local_stats.empty_items;
      continue;
    }
    local_stats.estimated_input_tokens += item_cost(item);
    if (plan && item.kind == EvidenceKind::user_profile &&
        (!plan->needs_profile || plan->query_family == QueryFamily::general_chess)) {
      ++local_stats.profile_filtered_items;
      continue;
    }
    const bool duplicate = std::any_of(
        ordered.begin(), ordered.end(), [&](const EvidenceItem* existing) {
          return existing->kind == item.kind && existing->id == item.id &&
              existing->payload == item.payload &&
              existing->confidence == item.confidence;
        });
    if (duplicate) {
      ++local_stats.exact_duplicate_items;
      continue;
    }
    ordered.push_back(&item);
  }
  std::stable_sort(ordered.begin(), ordered.end(), [plan](const auto* a, const auto* b) {
    const auto rank = [plan](EvidenceKind kind) {
      return plan && plan->needs_profile && kind == EvidenceKind::user_profile
          ? -2 : priority(kind);
    };
    return rank(a->kind) < rank(b->kind);
  });

  std::size_t profile_target = 0;
  for (const auto* item : ordered) {
    if (item->kind != EvidenceKind::user_profile) continue;
    const auto profile = nlohmann::json::parse(item->payload, nullptr, false);
    if (!profile.is_object()) continue;
    const auto declared = profile.value("providerTokenBudget", std::size_t{0});
    profile_target = std::min<std::size_t>(4000, std::min(declared, item_cost(*item)));
  }
  const std::size_t budget = evidence_budget(depth, context_tokens, plan, profile_target);
  local_stats.evidence_budget_tokens = budget;
  std::size_t used = 0;
  std::vector<EvidenceItem> output;
  for (const auto* original : ordered) {
    if (used >= budget) break;
    std::optional<EvidenceItem> fitted;
    const EvidenceItem* item = original;
    if (item->kind == EvidenceKind::user_profile) {
      // Mixed position+personal questions must reserve room for objective board
      // evidence; personal context never consumes the whole evidence budget.
      const std::size_t remaining = budget - used;
      const std::size_t profile_budget = plan && plan->needs_position
          ? remaining - std::min<std::size_t>(600, remaining / 3) : remaining;
      fitted = fit_profile(*item, profile_budget);
      if (!fitted) continue;
      item = &*fitted;
    }
    const std::size_t cost = item_cost(*item);
    if (cost > budget - used) continue;
    output.push_back(*item);
    used += cost;
  }
  local_stats.selected_items = output.size();
  local_stats.estimated_selected_tokens = used;
  if (stats != nullptr) *stats = local_stats;
  return output;
}

}  // namespace kchess::ai
