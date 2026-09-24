#include "hypothesis_manager.h"

#include <algorithm>
#include <map>
#include <string>

namespace kchess::ai {
namespace {

std::string hypothesis_status(const double confidence) {
  if (confidence >= 0.85) return "confirmed";
  if (confidence >= 0.70) return "probable";
  if (confidence >= 0.50) return "possible";
  return "tentative";
}

int counterevidence_games(
    const PlayerPattern& pattern,
    const std::vector<ProfileGameEvidence>& games) {
  int clean = 0;
  for (const auto& game : games) {
    if (pattern.time_control != "all" &&
        profile_time_control_id(game.time_control) != pattern.time_control) {
      continue;
    }
    int analyzed = game.classifications.analyzed_moves;
    int errors = game.classifications.miss + game.classifications.mistake +
                 game.classifications.blunder;
    if (pattern.id == "blunder_control") errors = game.classifications.blunder;
    if (pattern.id == "missed_opportunities") errors = game.classifications.miss;
    if (pattern.phase != "all") {
      analyzed = 0;
      errors = 0;
      for (const auto& phase : game.phases) {
        if (profile_phase_id(phase.phase) == pattern.phase) {
          analyzed = phase.analyzed_moves;
          errors = phase.miss + phase.mistake + phase.blunder;
          break;
        }
      }
    }
    if (analyzed >= 8 && errors == 0) ++clean;
  }
  return clean;
}

}  // namespace

std::vector<ProfileHypothesis> HypothesisManager::update(
    const std::vector<PlayerPattern>& patterns,
    const std::vector<ProfileGameEvidence>& games,
    const std::int64_t now_seconds,
    const std::optional<ChessProfile>& previous) const {
  std::map<std::string, ProfileHypothesis> previous_by_pattern;
  if (previous) {
    for (const auto& hypothesis : previous->hypotheses) {
      previous_by_pattern[hypothesis.pattern_id] = hypothesis;
    }
  }

  std::vector<ProfileHypothesis> result;
  for (const auto& pattern : patterns) {
    if (pattern.type != "weakness") continue;
    const int against = counterevidence_games(pattern, games);
    double confidence = pattern.confidence;
    // Counterexamples matter, but they should correct the hypothesis gradually
    // rather than erase a well-supported recurring pattern after one clean game.
    confidence = std::clamp(confidence - std::min(0.24, against * 0.015), 0.0, 1.0);
    if (const auto found = previous_by_pattern.find(pattern.id);
        found != previous_by_pattern.end()) {
      confidence = std::clamp(0.75 * confidence + 0.25 * found->second.confidence, 0.0, 1.0);
    }
    result.push_back(ProfileHypothesis{
        .id = "hypothesis." + pattern.id,
        .pattern_id = pattern.id,
        .status = hypothesis_status(confidence),
        .confidence = confidence,
        .evidence_for = pattern.occurrences,
        .evidence_against = against,
        .updated_at = now_seconds,
    });
  }
  std::sort(result.begin(), result.end(), [](const ProfileHypothesis& a,
                                             const ProfileHypothesis& b) {
    return a.confidence > b.confidence;
  });
  return result;
}

}  // namespace kchess::ai
