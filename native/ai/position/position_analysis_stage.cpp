#include "position_analysis_stage.h"

#include <algorithm>
#include <utility>

namespace kchess::ai {
namespace {

bool requested(const QueryPlan& plan, const EvidenceKind kind) {
  return std::find(plan.evidence.begin(), plan.evidence.end(), kind) !=
         plan.evidence.end();
}

}  // namespace

PositionAnalysisStage::PositionAnalysisStage(const std::size_t cache_capacity)
    : cache_capacity_(std::max<std::size_t>(1, cache_capacity)) {}

std::optional<PositionAnalysisStage::CacheEntry> PositionAnalysisStage::lookup(
    const std::string& fen) const {
  std::lock_guard lock(cache_mutex_);
  const auto found = cache_.find(fen);
  if (found == cache_.end()) return std::nullopt;
  touch_locked(fen);
  return found->second;
}

void PositionAnalysisStage::touch_locked(const std::string& fen) const {
  const auto existing = std::find(recency_.begin(), recency_.end(), fen);
  if (existing != recency_.end()) recency_.erase(existing);
  recency_.push_back(fen);
}

void PositionAnalysisStage::store(const std::string& fen,
                                  const CacheEntry& entry) const {
  std::lock_guard lock(cache_mutex_);
  cache_[fen] = entry;
  touch_locked(fen);
  while (cache_.size() > cache_capacity_ && !recency_.empty()) {
    const auto victim = std::move(recency_.front());
    recency_.pop_front();
    if (victim == fen) continue;
    cache_.erase(victim);
    ++cache_evictions_;
  }
}

PositionAnalysisResult PositionAnalysisStage::analyze(
    const CoachRequest& request, const QueryPlan& plan,
    std::vector<EvidenceItem>& evidence) const {
  PositionAnalysisResult result;
  const bool wants_features = requested(plan, EvidenceKind::position_features);
  const bool wants_weaknesses = requested(plan, EvidenceKind::position_weaknesses);
  const bool wants_exploitation =
      requested(plan, EvidenceKind::weakness_exploitation);
  const bool wants_plans = requested(plan, EvidenceKind::strategic_plans);
  const bool wants_tactics = requested(plan, EvidenceKind::tactical_motifs);
  const bool wants_practicality = requested(plan, EvidenceKind::practicality);
  if ((!wants_features && !wants_weaknesses && !wants_exploitation &&
       !wants_plans && !wants_tactics && !wants_practicality) ||
      !request.position_fen.has_value()) {
    return result;
  }

  result.cache_requested = true;
  const auto& fen = *request.position_fen;
  CacheEntry entry;
  if (auto cached = lookup(fen)) entry = std::move(*cached);

  bool any_hit = false;
  bool all_from_cache = true;
  const auto require = [&](const bool needed, const bool cached) {
    if (!needed) return;
    any_hit = any_hit || cached;
    all_from_cache = all_from_cache && cached;
  };

  const bool needs_features = wants_features || wants_weaknesses ||
      wants_exploitation || wants_plans || wants_practicality;
  require(needs_features, entry.features.has_value());
  require(wants_weaknesses || wants_exploitation || wants_plans,
          entry.weaknesses.has_value());
  require(wants_exploitation || wants_plans, entry.exploitation.has_value());
  require(wants_plans, entry.plans.has_value());
  require(wants_tactics, entry.tactics.has_value());

  if (needs_features && !entry.features.has_value()) {
    entry.features = PositionFeatureExtractor{}.extract(fen);
    all_from_cache = false;
  }

  if ((wants_weaknesses || wants_exploitation || wants_plans) &&
      !entry.weaknesses.has_value()) {
    entry.weaknesses = WeaknessAnalyzer{}.analyze(*entry.features);
    all_from_cache = false;
  }
  if ((wants_exploitation || wants_plans) && !entry.exploitation.has_value()) {
    entry.exploitation = ExploitationPlanner{}.plan(*entry.weaknesses);
    all_from_cache = false;
  }
  if (wants_plans && !entry.plans.has_value()) {
    entry.plans = PlanGenerator{}.generate(
        *entry.features, *entry.weaknesses, *entry.exploitation);
    all_from_cache = false;
  }
  if (wants_tactics && !entry.tactics.has_value()) {
    entry.tactics = TacticalDetector{}.analyze(fen);
    all_from_cache = false;
  }

  if (wants_features && entry.features.has_value()) {
    evidence.push_back(PositionFeatureExtractor{}.evidence(*entry.features));
  }
  if (wants_weaknesses && entry.weaknesses.has_value()) {
    evidence.push_back(WeaknessAnalyzer{}.evidence(*entry.weaknesses));
  }
  if (wants_exploitation && entry.exploitation.has_value()) {
    evidence.push_back(ExploitationPlanner{}.evidence(*entry.exploitation));
  }
  if (wants_plans && entry.plans.has_value()) {
    evidence.push_back(PlanGenerator{}.evidence(*entry.plans));
  }
  if (wants_tactics && entry.tactics.has_value()) {
    evidence.push_back(TacticalDetector{}.evidence(*entry.tactics));
  }

  result.artifacts.features = entry.features;
  result.artifacts.tactics = entry.tactics;
  result.cache_any_hit = any_hit;
  result.cache_full_hit = all_from_cache;
  store(fen, entry);

  {
    std::lock_guard lock(cache_mutex_);
    ++cache_requests_;
    cache_any_hits_ += result.cache_any_hit ? 1 : 0;
    cache_full_hits_ += result.cache_full_hit ? 1 : 0;
    cache_misses_ += result.cache_any_hit ? 0 : 1;
  }
  return result;
}

PositionAnalysisCacheStats PositionAnalysisStage::cache_stats() const {
  std::lock_guard lock(cache_mutex_);
  return PositionAnalysisCacheStats{
      .requests = cache_requests_,
      .any_hits = cache_any_hits_,
      .full_hits = cache_full_hits_,
      .misses = cache_misses_,
      .evictions = cache_evictions_,
      .entries = cache_.size(),
      .capacity = cache_capacity_,
  };
}

}  // namespace kchess::ai
