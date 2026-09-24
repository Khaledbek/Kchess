#include "coach_priority_engine.h"

#include <algorithm>
#include <cmath>

namespace kchess::ai {
namespace {

double trainability(const std::string& pattern_id) {
  if (pattern_id == "time_pressure_errors") return 0.95;
  if (pattern_id == "calculation_consistency") return 0.95;
  if (pattern_id == "missed_opportunities") return 0.90;
  if (pattern_id == "blunder_control") return 0.90;
  if (pattern_id == "opening_errors") return 0.82;
  if (pattern_id == "endgame_errors") return 0.86;
  return 0.75;
}

}  // namespace

std::vector<CoachPriority> CoachPriorityEngine::rank(
    const std::vector<PlayerPattern>& patterns) const {
  struct Ranked {
    std::string id;
    double score{0.0};
  };
  std::vector<Ranked> ranked;
  for (const auto& pattern : patterns) {
    if (pattern.type != "weakness" || pattern.confidence < 0.35) continue;
    const double frequency = std::clamp(
        std::log1p(static_cast<double>(pattern.occurrences)) / std::log(16.0), 0.0, 1.0);
    const double recency = pattern.recent ? 1.0 : 0.45;
    const double trend = pattern.trend == "worsening"
        ? 1.0
        : (pattern.trend == "improving" ? 0.25 : 0.55);
    const double score =
        0.30 * pattern.confidence +
        0.24 * pattern.severity +
        0.16 * frequency +
        0.10 * recency +
        0.08 * trend +
        0.12 * trainability(pattern.id);
    ranked.push_back(Ranked{.id = pattern.id, .score = std::clamp(score, 0.0, 1.0)});
  }
  std::sort(ranked.begin(), ranked.end(), [](const Ranked& a, const Ranked& b) {
    return a.score > b.score;
  });

  std::vector<CoachPriority> result;
  for (std::size_t index = 0; index < ranked.size() && index < 3; ++index) {
    result.push_back(CoachPriority{
        .pattern_id = ranked[index].id,
        .role = index == 0 ? "main" : "secondary",
        .score = ranked[index].score,
    });
  }
  return result;
}

}  // namespace kchess::ai
