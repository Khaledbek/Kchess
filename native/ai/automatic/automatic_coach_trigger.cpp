#include "automatic_coach_trigger.h"

#include <algorithm>
#include <set>

#include "../position/position_features.h"
#include "../position/tactical_detector.h"

namespace kchess::ai {
namespace {

// -----------------------------------------------------------------------------
// Section: Trigger helpers
// -----------------------------------------------------------------------------

enum class PositionPhase { opening, middlegame, endgame };

void add_reason(AutomaticCoachDecision& decision,
                AutomaticCoachReason reason, double priority) {
  if (std::find(decision.reasons.begin(), decision.reasons.end(), reason) ==
      decision.reasons.end()) {
    decision.reasons.push_back(reason);
  }
  decision.priority = std::max(decision.priority, priority);
}

std::optional<PositionPhase> phase_for(const std::optional<std::string>& fen) {
  if (!fen || fen->empty()) return std::nullopt;
  try {
    const auto features = PositionFeatureExtractor{}.extract(*fen);
    const int total_material =
        features.white.material_cp + features.black.material_cp;
    const int developed_minors = features.white.minor_pieces_off_home +
                                 features.black.minor_pieces_off_home;
    if (total_material <= 4600) return PositionPhase::endgame;
    if (total_material >= 6500 && developed_minors < 6) {
      return PositionPhase::opening;
    }
    return PositionPhase::middlegame;
  } catch (...) {
    return std::nullopt;
  }
}

std::set<TacticalMotifKind> confident_motifs(
    const std::optional<std::string>& fen) {
  std::set<TacticalMotifKind> kinds;
  if (!fen || fen->empty()) return kinds;
  try {
    for (const auto& motif : TacticalDetector{}.analyze(*fen).motifs) {
      if (motif.confidence >= 0.85) kinds.insert(motif.kind);
    }
  } catch (...) {
  }
  return kinds;
}

bool has_new_motif(const AutomaticCoachEvent& event) {
  const auto before = confident_motifs(event.previous_fen);
  const auto after = confident_motifs(event.current_fen);
  return std::any_of(after.begin(), after.end(), [&](const auto kind) {
    return !before.contains(kind);
  });
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Automatic coaching policy
// -----------------------------------------------------------------------------

AutomaticCoachDecision AutomaticCoachTrigger::decide(
    const AutomaticCoachEvent& event) const {
  AutomaticCoachDecision decision;
  const double loss = event.expected_score_loss.value_or(0.0);

  if (event.classification == "blunder") {
    add_reason(decision, AutomaticCoachReason::blunder, 1.0);
  } else if (event.classification == "brilliant") {
    add_reason(decision, AutomaticCoachReason::brilliant, 0.92);
  } else if (event.classification == "mistake") {
    add_reason(decision, AutomaticCoachReason::mistake, 0.90);
  } else if (event.classification == "miss") {
    add_reason(decision, AutomaticCoachReason::missed_tactic, 0.88);
  } else if (event.classification == "okay" && loss >= 0.065) {
    // KChess has no separate Inaccuracy label. A materially weaker Okay move
    // is the narrow automatic-coach equivalent without changing classification.
    add_reason(decision, AutomaticCoachReason::inaccuracy, 0.68);
  }

  // Expected-score loss is derived from Stockfish WDL and therefore gives the
  // trigger a stable WDL-shift signal without running an additional search.
  if (loss >= 0.15) {
    add_reason(decision, AutomaticCoachReason::large_wdl_shift, 0.82);
  }

  const auto before_phase = phase_for(event.previous_fen);
  const auto after_phase = phase_for(event.current_fen);
  if (before_phase && after_phase && *before_phase != *after_phase) {
    add_reason(decision, AutomaticCoachReason::phase_transition, 0.55);
  }

  if (has_new_motif(event)) {
    add_reason(decision, AutomaticCoachReason::new_motif, 0.65);
  }

  if (event.repeated_personal_mistake) {
    add_reason(decision, AutomaticCoachReason::repeated_personal_mistake, 0.78);
  }

  decision.trigger = event.previous_fen.has_value() && event.current_fen.has_value();
  return decision;
}

std::string_view automatic_coach_reason_name(
    const AutomaticCoachReason reason) noexcept {
  switch (reason) {
    case AutomaticCoachReason::brilliant: return "brilliant";
    case AutomaticCoachReason::inaccuracy: return "inaccuracy";
    case AutomaticCoachReason::mistake: return "mistake";
    case AutomaticCoachReason::blunder: return "blunder";
    case AutomaticCoachReason::missed_tactic: return "missed_tactic";
    case AutomaticCoachReason::new_motif: return "new_motif";
    case AutomaticCoachReason::large_wdl_shift: return "large_wdl_shift";
    case AutomaticCoachReason::phase_transition: return "phase_transition";
    case AutomaticCoachReason::repeated_personal_mistake:
      return "repeated_personal_mistake";
  }
  return "unknown";
}

}  // namespace kchess::ai
