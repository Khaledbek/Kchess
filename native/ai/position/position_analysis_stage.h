#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "../dto/coach_request.h"
#include "../dto/evidence.h"
#include "../dto/query_plan.h"
#include "exploitation_planner.h"
#include "plan_generator.h"
#include "position_features.h"
#include "tactical_detector.h"
#include "weakness_analyzer.h"

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Reusable deterministic position intelligence
// -----------------------------------------------------------------------------

struct PositionAnalysisArtifacts {
  std::optional<PositionFeatures> features;
  std::optional<TacticalAnalysis> tactics;
};

struct PositionAnalysisResult {
  PositionAnalysisArtifacts artifacts;
  bool cache_requested{false};
  bool cache_any_hit{false};
  bool cache_full_hit{false};
};

struct PositionAnalysisCacheStats {
  std::uint64_t requests{0};
  std::uint64_t any_hits{0};
  std::uint64_t full_hits{0};
  std::uint64_t misses{0};
  std::uint64_t evictions{0};
  std::size_t entries{0};
  std::size_t capacity{0};
};

class PositionAnalysisStage {
 public:
  explicit PositionAnalysisStage(std::size_t cache_capacity = 128);

  [[nodiscard]] PositionAnalysisResult analyze(
      const CoachRequest& request, const QueryPlan& plan,
      std::vector<EvidenceItem>& evidence) const;

  [[nodiscard]] PositionAnalysisCacheStats cache_stats() const;

 private:
  struct CacheEntry {
    std::optional<PositionFeatures> features;
    std::optional<PositionWeaknesses> weaknesses;
    std::optional<PositionExploitationPlans> exploitation;
    std::optional<PositionPlans> plans;
    std::optional<TacticalAnalysis> tactics;
  };

  [[nodiscard]] std::optional<CacheEntry> lookup(const std::string& fen) const;
  void store(const std::string& fen, const CacheEntry& entry) const;
  void touch_locked(const std::string& fen) const;

  std::size_t cache_capacity_{128};
  mutable std::mutex cache_mutex_;
  mutable std::unordered_map<std::string, CacheEntry> cache_;
  mutable std::deque<std::string> recency_;
  mutable std::uint64_t cache_requests_{0};
  mutable std::uint64_t cache_any_hits_{0};
  mutable std::uint64_t cache_full_hits_{0};
  mutable std::uint64_t cache_misses_{0};
  mutable std::uint64_t cache_evictions_{0};
};

}  // namespace kchess::ai
