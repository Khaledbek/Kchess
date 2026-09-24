#include "experts/context_evidence_experts.h"

#include <algorithm>
#include <iterator>
#include <nlohmann/json.hpp>

namespace kchess::ai {
namespace {

ExpertEvidence wrap(EvidenceSource source, const EvidenceItem& item,
                    std::vector<EvidenceNeed> satisfies,
                    bool objective_truth) {
  return ExpertEvidence{
      .source = source,
      .item = item,
      .satisfies = std::move(satisfies),
      .objective_truth = objective_truth,
  };
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Position / strategic evidence
// -----------------------------------------------------------------------------

std::vector<ExpertEvidence> PositionContextExpert::collect(
    std::span<const EvidenceItem> evidence) const {
  std::vector<ExpertEvidence> result;
  for (const auto& item : evidence) {
    switch (item.kind) {
      case EvidenceKind::position_features:
        result.push_back(wrap(
            EvidenceSource::position_features, item,
            {EvidenceNeed::piece_activity, EvidenceNeed::controlled_squares,
             EvidenceNeed::position_structure},
            true));
        break;
      case EvidenceKind::position_weaknesses:
      case EvidenceKind::weakness_exploitation:
      case EvidenceKind::strategic_plans:
        result.push_back(wrap(
            EvidenceSource::position_features, item,
            {EvidenceNeed::position_structure, EvidenceNeed::strategic_plans},
            false));
        break;
      default:
        break;
    }
  }
  return result;
}

// -----------------------------------------------------------------------------
// Section: Tactical evidence
// -----------------------------------------------------------------------------

std::vector<ExpertEvidence> TacticalContextExpert::collect(
    std::span<const EvidenceItem> evidence) const {
  std::vector<ExpertEvidence> result;
  for (const auto& item : evidence) {
    if (item.kind != EvidenceKind::tactical_motifs) continue;
    result.push_back(wrap(
        EvidenceSource::tactical_detector, item,
        {EvidenceNeed::threats, EvidenceNeed::tactical_motifs,
         EvidenceNeed::candidate_moves},
        false));
  }
  return result;
}

// -----------------------------------------------------------------------------
// Section: Opening evidence
// -----------------------------------------------------------------------------

std::vector<ExpertEvidence> OpeningContextExpert::collect(
    std::span<const EvidenceItem> evidence) const {
  std::vector<ExpertEvidence> result;
  for (const auto& item : evidence) {
    if (item.kind != EvidenceKind::opening && item.kind != EvidenceKind::theory) {
      continue;
    }
    result.push_back(wrap(
        EvidenceSource::opening_knowledge, item,
        {EvidenceNeed::opening_context, EvidenceNeed::historical_examples,
         EvidenceNeed::practical_candidates},
        item.kind == EvidenceKind::theory));
  }
  return result;
}

// -----------------------------------------------------------------------------
// Section: Player-profile evidence
// -----------------------------------------------------------------------------

std::vector<ExpertEvidence> ProfileContextExpert::collect(
    std::span<const EvidenceItem> evidence) const {
  std::vector<ExpertEvidence> result;
  for (const auto& item : evidence) {
    if (item.kind != EvidenceKind::user_profile) continue;
    result.push_back(wrap(
        EvidenceSource::player_profile, item,
        {EvidenceNeed::player_tendencies, EvidenceNeed::practical_candidates,
         EvidenceNeed::human_move_prediction},
        false));
    // Historical/profile retrieval currently shares the same native profile
    // evidence payload. Expose a second planner-source envelope rather than
    // duplicating persistence queries; the aggregator will keep one payload.
    result.push_back(wrap(
        EvidenceSource::game_history, item,
        {EvidenceNeed::historical_examples, EvidenceNeed::player_tendencies,
         EvidenceNeed::opening_context},
        false));
  }
  return result;
}

// -----------------------------------------------------------------------------
// Section: Unified evidence normalization
// -----------------------------------------------------------------------------

std::vector<ExpertEvidence> normalize_expert_evidence(
    std::span<const EvidenceItem> evidence) {
  std::vector<ExpertEvidence> result;
  const auto append_all = [&](std::vector<ExpertEvidence> values) {
    result.insert(result.end(), std::make_move_iterator(values.begin()),
                  std::make_move_iterator(values.end()));
  };

  append_all(PositionContextExpert{}.collect(evidence));
  append_all(TacticalContextExpert{}.collect(evidence));
  append_all(OpeningContextExpert{}.collect(evidence));
  append_all(ProfileContextExpert{}.collect(evidence));

  const auto already_wrapped = [&](const EvidenceItem& item) {
    return std::any_of(result.begin(), result.end(), [&](const ExpertEvidence& current) {
      return current.item.id == item.id && current.item.kind == item.kind &&
             current.item.payload == item.payload;
    });
  };

  for (const auto& item : evidence) {
    if (already_wrapped(item)) continue;
    switch (item.kind) {
      case EvidenceKind::cache:
      case EvidenceKind::existing_analysis:
        result.push_back(wrap(
            EvidenceSource::existing_analysis, item,
            {EvidenceNeed::candidate_moves, EvidenceNeed::move_evaluations,
             EvidenceNeed::best_reply, EvidenceNeed::principal_variation},
            true));
        break;
      case EvidenceKind::engine:
        result.push_back(wrap(
            EvidenceSource::engine, item,
            {EvidenceNeed::move_evaluations, EvidenceNeed::best_reply,
             EvidenceNeed::principal_variation, EvidenceNeed::forced_mate},
            true));
        break;
      case EvidenceKind::candidate_moves: {
        std::vector<EvidenceNeed> needs{
            EvidenceNeed::candidate_moves, EvidenceNeed::move_evaluations,
            EvidenceNeed::best_reply, EvidenceNeed::principal_variation,
            EvidenceNeed::move_comparison};
        const auto payload = nlohmann::json::parse(item.payload, nullptr, false);
        if (payload.is_object()) {
          const auto focus = payload.value("focus_kind", "");
          if (focus == "worst_move" || focus == "fastest_loss") {
            needs.push_back(EvidenceNeed::legal_moves);
            needs.push_back(EvidenceNeed::material_consequences);
          }
          if (focus == "fastest_loss") needs.push_back(EvidenceNeed::forced_mate);
        }
        result.push_back(wrap(EvidenceSource::engine, item, std::move(needs), true));
        break;
      }
      case EvidenceKind::human_model:
        result.push_back(wrap(
            EvidenceSource::human_model, item,
            {EvidenceNeed::human_move_prediction,
             EvidenceNeed::practical_candidates},
            false));
        break;
      case EvidenceKind::conversation:
        result.push_back(wrap(EvidenceSource::conversation, item,
                              {EvidenceNeed::conversation_reference}, false));
        break;
      case EvidenceKind::chess_concepts:
        result.push_back(wrap(
            EvidenceSource::chess_concepts, item,
            {EvidenceNeed::concept_explanation, EvidenceNeed::strategic_plans,
             EvidenceNeed::tactical_motifs},
            false));
        break;
      case EvidenceKind::practicality:
        result.push_back(wrap(
            EvidenceSource::human_model, item,
            {EvidenceNeed::practical_candidates}, false));
        break;
      case EvidenceKind::move_contrast:
        result.push_back(wrap(
            EvidenceSource::existing_analysis, item,
            {EvidenceNeed::move_comparison, EvidenceNeed::move_evaluations,
             EvidenceNeed::material_consequences},
            true));
        break;
      case EvidenceKind::position_features:
      case EvidenceKind::position_weaknesses:
      case EvidenceKind::weakness_exploitation:
      case EvidenceKind::theory:
      case EvidenceKind::opening:
      case EvidenceKind::user_profile:
      case EvidenceKind::tactical_motifs:
      case EvidenceKind::strategic_plans:
        // Already normalized by the dedicated adapters above.
        break;
    }
  }
  return result;
}


}  // namespace kchess::ai
