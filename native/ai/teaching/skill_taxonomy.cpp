#include "skill_taxonomy.h"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <nlohmann/json.hpp>

namespace kchess::ai {
namespace {

using json = nlohmann::json;

std::optional<TeachingSkill> tactical_skill(
    const std::vector<EvidenceItem>& evidence) {
  std::optional<std::pair<double, std::string>> best;
  for (const auto& item : evidence) {
    if (item.kind != EvidenceKind::tactical_motifs || item.payload.empty()) {
      continue;
    }
    try {
      const auto payload = json::parse(item.payload);
      for (const auto& motif : payload.value("motifs", json::array())) {
        if (!motif.is_object()) continue;
        const auto kind = motif.value("kind", std::string{});
        const double confidence = motif.value("confidence", 0.0);
        // Match the native claim-validation threshold. Lower-confidence
        // heuristics may still be explained, but they must not become a
        // persistent learner skill label.
        if (kind.empty() || confidence < 0.85) continue;
        if (!best || confidence > best->first ||
            (confidence == best->first && kind < best->second)) {
          best = std::pair{confidence, kind};
        }
      }
    } catch (...) {
      // Evidence parsing failure must fall back to the coarse skill rather
      // than affecting the Coach answer.
    }
  }
  if (!best) return std::nullopt;
  return TeachingSkill{.id = "tactics." + best->second,
                       .family = "tactics"};
}

std::optional<TeachingSkill> strategic_skill(
    const CoachRequest& request, const std::vector<EvidenceItem>& evidence) {
  if (!request.player_color) return std::nullopt;
  const std::string preferred_side =
      *request.player_color == "black" ? "by_black" : "by_white";
  for (const auto& item : evidence) {
    if (item.kind != EvidenceKind::strategic_plans || item.payload.empty()) {
      continue;
    }
    try {
      const auto payload = json::parse(item.payload);
      const auto plans = payload.value(preferred_side, json::array());
      if (!plans.is_array() || plans.empty() || !plans.front().is_object()) {
        continue;
      }
      const auto theme = plans.front().value("theme", std::string{});
      if (!theme.empty()) {
        return TeachingSkill{.id = "strategy." + theme,
                             .family = "strategy"};
      }
    } catch (...) {
      // Fall through to the stable coarse skill.
    }
  }
  return std::nullopt;
}

TeachingSkill fallback_skill(const CoachIntent topic) {
  switch (topic) {
    case CoachIntent::tactic:
      return {.id = "tactics.pattern_recognition", .family = "tactics"};
    case CoachIntent::opening:
      return {.id = "opening.decision", .family = "opening"};
    case CoachIntent::endgame:
      return {.id = "endgame.decision", .family = "endgame"};
    case CoachIntent::plan:
      return {.id = "strategy.plan_choice", .family = "strategy"};
    case CoachIntent::position:
      return {.id = "strategy.position_priority", .family = "strategy"};
    case CoachIntent::player_development:
      return {.id = "training.personal_priority", .family = "training"};
    case CoachIntent::chess_concept:
      return {.id = "knowledge.concept_application", .family = "knowledge"};
    case CoachIntent::move_explanation:
    case CoachIntent::training:
    case CoachIntent::follow_up:
    case CoachIntent::game_review:
    case CoachIntent::unknown:
    default:
      return {.id = "calculation.candidate_selection",
              .family = "calculation"};
  }
}

}  // namespace

TeachingSkill resolve_teaching_skill(
    const CoachRequest& request, const CoachIntent topic,
    const std::vector<EvidenceItem>& evidence) {
  // Preserve the lesson's routed subject. Specific board evidence may refine a
  // tactical or generic decision exercise, but it must not silently turn an
  // opening/endgame lesson into a different curriculum topic.
  if (topic == CoachIntent::tactic || topic == CoachIntent::move_explanation ||
      topic == CoachIntent::training || topic == CoachIntent::position ||
      topic == CoachIntent::game_review || topic == CoachIntent::follow_up) {
    if (const auto tactical = tactical_skill(evidence)) return *tactical;
  }

  if (topic == CoachIntent::plan || topic == CoachIntent::position ||
      topic == CoachIntent::training || topic == CoachIntent::game_review) {
    if (const auto strategic = strategic_skill(request, evidence)) {
      return *strategic;
    }
  }

  return fallback_skill(topic);
}

}  // namespace kchess::ai
