#include "validated_response_cache.h"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string_view>

#include "../models/model_versions.h"

namespace kchess::ai {
namespace {

void append_field(std::ostringstream& out, std::string_view value) {
  out << value.size() << ':' << value << ';';
}

template <typename T>
void append_number(std::ostringstream& out, const T value) {
  out << value << ';';
}

void append_strings(std::ostringstream& out,
                    const std::vector<std::string>& values) {
  append_number(out, values.size());
  for (const auto& value : values) append_field(out, value);
}

void append_scope(std::ostringstream& out, const ProfileQueryScope& scope) {
  append_strings(out, scope.topics);
  append_strings(out, scope.time_controls);
  append_strings(out, scope.phases);
  append_strings(out, scope.player_colors);
  append_number(out, scope.needs_endgame_material_type ? 1 : 0);
  append_number(out, scope.wants_proof ? 1 : 0);
  append_number(out, scope.compare_scopes ? 1 : 0);
}

void append_teaching_plan(std::ostringstream& out, const TeachingPlan& plan) {
  append_field(out, plan.objective_id);
  append_field(out, plan.delivery_mode);
  append_number(out, plan.reveal_level);
  append_number(out, plan.ask_question ? 1 : 0);
  append_number(out, plan.max_recommendations);
  append_number(out, plan.max_concepts);
  append_field(out, plan.skill_id);
  append_field(out, plan.skill_family);
  append_field(out, plan.target);
}

void append_optional(std::ostringstream& out,
                     const std::optional<std::string>& value) {
  append_number(out, value.has_value() ? 1 : 0);
  if (value) append_field(out, *value);
}

}  // namespace

ValidatedResponseCache::ValidatedResponseCache(const std::size_t capacity)
    : capacity_(std::max<std::size_t>(1, capacity)) {}

std::string ValidatedResponseCache::key_for(
    const std::string_view provider_id, const LLMProviderRequest& request,
    const std::vector<EvidenceItem>& validation_evidence) {
  std::ostringstream out;
  out << std::setprecision(17);
  append_field(out, provider_id);
  append_field(out, ModelVersionRegistry::version_of("CoachPrompt"));
  append_field(out, request.response_schema_version);
  append_field(out, request.locale);
  append_number(out, static_cast<int>(request.intent));
  append_number(out, static_cast<int>(request.mode));
  append_number(out, static_cast<int>(request.depth));
  append_field(out, request.user_text);
  append_optional(out, request.position_fen);
  append_optional(out, request.player_color);
  append_optional(out, request.hint_move_uci);
  append_optional(out, request.teaching_target);
  append_teaching_plan(out, request.teaching_plan);
  append_field(out, request.session_summary);
  append_field(out, request.pgn_excerpt);
  append_number(out, request.input_token_budget);
  append_number(out, request.automatic_turn ? 1 : 0);
  append_number(out, request.has_verified_learner_feedback ? 1 : 0);
  append_number(out, static_cast<int>(request.query_family));
  append_number(out, request.needs_profile ? 1 : 0);
  append_scope(out, request.profile_scope);
  append_number(out, request.evidence.size());
  for (const auto& item : request.evidence) {
    append_field(out, item.id);
    append_number(out, static_cast<int>(item.kind));
    append_field(out, item.payload);
    append_number(out, item.confidence);
  }
  append_number(out, validation_evidence.size());
  for (const auto& item : validation_evidence) {
    append_field(out, item.id);
    append_number(out, static_cast<int>(item.kind));
    append_field(out, item.payload);
    append_number(out, item.confidence);
  }
  return out.str();
}

PreparedValidatedResponseCacheKey ValidatedResponseCache::prepare_key(
    const std::string_view provider_id, const LLMProviderRequest& request,
    const std::vector<EvidenceItem>& validation_evidence) const {
  auto value = key_for(provider_id, request, validation_evidence);
  {
    std::lock_guard lock(mutex_);
    ++key_builds_;
    key_bytes_ += value.size();
  }
  return PreparedValidatedResponseCacheKey{.value = std::move(value)};
}

std::optional<StructuredCoachContent> ValidatedResponseCache::lookup(
    const PreparedValidatedResponseCacheKey& key) const {
  std::lock_guard lock(mutex_);
  ++requests_;
  const auto it = entries_.find(key.value);
  if (it == entries_.end()) return std::nullopt;
  ++hits_;
  return it->second.content;
}

void ValidatedResponseCache::store(
    const PreparedValidatedResponseCacheKey& key,
    const StructuredCoachContent& content) const {
  if (content.empty()) return;
  std::lock_guard lock(mutex_);
  auto [it, inserted] = entries_.emplace(key.value, Entry{content});
  if (!inserted) {
    it->second.content = content;
    return;
  }
  order_.push_back(key.value);
  ++stores_;
  while (entries_.size() > capacity_ && !order_.empty()) {
    const auto oldest = std::move(order_.front());
    order_.pop_front();
    if (entries_.erase(oldest) != 0) ++evictions_;
  }
}

ValidatedResponseCacheStats ValidatedResponseCache::stats() const {
  std::lock_guard lock(mutex_);
  return ValidatedResponseCacheStats{
      .requests = requests_,
      .hits = hits_,
      .stores = stores_,
      .evictions = evictions_,
      .entries = entries_.size(),
      .capacity = capacity_,
      .key_builds = key_builds_,
      .key_bytes = key_bytes_,
  };
}

}  // namespace kchess::ai
