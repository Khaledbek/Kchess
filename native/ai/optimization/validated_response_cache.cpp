#include "validated_response_cache.h"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string_view>

#include "../model_versions.h"

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

void append_evidence_plan(std::ostringstream& out, const EvidencePlan& plan) {
  append_number(out, plan.sources.size());
  for (const auto source : plan.sources) {
    append_number(out, static_cast<int>(source));
  }
  append_number(out, plan.needs.size());
  for (const auto need : plan.needs) {
    append_number(out, static_cast<int>(need));
  }
  append_number(out, static_cast<int>(plan.perspective));
  append_number(out, static_cast<int>(plan.scope));
  append_number(out, static_cast<int>(plan.depth));
  append_number(out, static_cast<int>(plan.freshness));
  append_number(out, static_cast<int>(plan.interaction));
  append_number(out, static_cast<int>(plan.elo_target));
  append_number(out, static_cast<int>(plan.priority));
  append_number(out, plan.confidence);
  append_number(out, plan.allow_optional_sources ? 1 : 0);
}

void append_teaching_plan(std::ostringstream& out, const TeachingPlan& plan) {
  append_field(out, plan.objective_id);
  append_field(out, plan.delivery_mode);
  append_number(out, plan.reveal_level);
  append_number(out, plan.hint_level);
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
    const std::string_view provider_cache_identity, const LLMProviderRequest& request,
    const std::vector<EvidenceItem>& validation_evidence) {
  std::ostringstream out;
  out << std::setprecision(17);
  append_field(out, provider_cache_identity);
  append_field(out, ModelVersionRegistry::version_of("CoachPrompt"));
  append_field(out, ModelVersionRegistry::version_of("CoachValidator"));
  append_field(out, request.response_schema_version);
  append_field(out, request.context_schema_version);
  append_field(out, request.locale);
  append_number(out, static_cast<int>(request.intent));
  append_number(out, static_cast<int>(request.mode));
  append_number(out, static_cast<int>(request.depth));
  append_field(out, request.user_text);
  append_field(out, request.interaction.request_kind);
  append_field(out, request.interaction.answer_intent);
  append_number(out, request.chess_verdict.has_value() ? 1 : 0);
  if (request.chess_verdict) {
    const auto& verdict = *request.chess_verdict;
    append_number(out, verdict.authoritative ? 1 : 0);
    append_field(out, verdict.position_scope);
    append_number(out, static_cast<int>(verdict.position_verdict));
    append_number(out, static_cast<int>(verdict.move_verdict));
    append_optional(out, verdict.evaluated_move_uci);
    append_number(out, verdict.white_evaluation_cp.has_value() ? 1 : 0);
    if (verdict.white_evaluation_cp) append_number(out, *verdict.white_evaluation_cp);
    append_number(out, verdict.white_mate_in.has_value() ? 1 : 0);
    if (verdict.white_mate_in) append_number(out, *verdict.white_mate_in);
    append_number(out, verdict.move_loss_cp.has_value() ? 1 : 0);
    if (verdict.move_loss_cp) append_number(out, *verdict.move_loss_cp);
    append_number(out, verdict.move_loss_expected_score.has_value() ? 1 : 0);
    if (verdict.move_loss_expected_score) {
      append_number(out, *verdict.move_loss_expected_score);
    }
    append_field(out, verdict.basis);
  }
  append_number(out, request.verdict_review.has_value() ? 1 : 0);
  if (request.verdict_review) {
    const auto& review = *request.verdict_review;
    append_number(out, review.active ? 1 : 0);
    append_number(out, static_cast<int>(review.outcome));
    append_optional(out, review.challenged_move_uci);
    append_field(out, review.trigger);
    append_number(out, static_cast<int>(review.previous_verdict.position_verdict));
    append_number(out, static_cast<int>(review.previous_verdict.move_verdict));
    append_field(out, review.previous_verdict.basis);
  }
  append_number(out, request.interaction.requested_actions.size());
  for (const auto& action : request.interaction.requested_actions) append_field(out, action);
  append_number(out, request.interaction.elo.has_value() ? 1 : 0);
  if (request.interaction.elo) append_number(out, *request.interaction.elo);
  append_optional(out, request.interaction.color);
  append_optional(out, request.interaction.game_mode);
  append_optional(out, request.interaction.time_control);
  append_optional(out, request.position_fen);
  append_optional(out, request.player_color);
  append_optional(out, request.hint_move_uci);
  append_optional(out, request.teaching_target);
  append_teaching_plan(out, request.teaching_plan);
  append_field(out, request.session_summary);
  append_field(out, request.pgn_excerpt);
  append_number(out, request.input_token_budget);
  append_number(out, request.automatic_turn ? 1 : 0);
  append_number(out, request.move_attribution.has_value() ? 1 : 0);
  if (request.move_attribution) {
    const auto& move = *request.move_attribution;
    append_optional(out, move.previous_fen);
    append_optional(out, move.current_fen);
    append_optional(out, move.played_move_uci);
    append_optional(out, move.mover_color);
    append_optional(out, move.learner_color);
    append_field(out, move.mover_role);
    append_optional(out, move.classification);
    append_number(out, move.expected_score_before.has_value() ? 1 : 0);
    if (move.expected_score_before) append_number(out, *move.expected_score_before);
    append_number(out, move.expected_score_played.has_value() ? 1 : 0);
    if (move.expected_score_played) append_number(out, *move.expected_score_played);
    append_number(out, move.expected_score_loss.has_value() ? 1 : 0);
    if (move.expected_score_loss) append_number(out, *move.expected_score_loss);
    append_number(out, move.verified_learner_move ? 1 : 0);
  }
  append_number(out, request.has_verified_learner_feedback ? 1 : 0);
  append_number(out, static_cast<int>(request.query_family));
  append_number(out, static_cast<int>(request.analysis_mode));
  append_number(out, request.analysis_mode_explicit ? 1 : 0);
  append_evidence_plan(out, request.evidence_plan);
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
    const std::string_view provider_cache_identity, const LLMProviderRequest& request,
    const std::vector<EvidenceItem>& validation_evidence) const {
  auto value = key_for(provider_cache_identity, request, validation_evidence);
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
