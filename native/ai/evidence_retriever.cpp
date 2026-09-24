#include "evidence_retriever.h"

#include <algorithm>
#include <string_view>
#include <utility>

#include "chess/move.h"

#include "concepts/concept_retriever.h"

namespace kchess::ai {
namespace {

// -----------------------------------------------------------------------------
// Section: Retrieval policy
// -----------------------------------------------------------------------------

bool requested(const QueryPlan& plan, EvidenceKind kind) {
  return std::find(plan.evidence.begin(), plan.evidence.end(), kind) !=
         plan.evidence.end();
}

std::string_view stable_id(EvidenceKind kind) {
  switch (kind) {
    case EvidenceKind::cache:
      return "cache.lookup.v1";
    case EvidenceKind::existing_analysis:
      return "analysis.existing.v1";
    case EvidenceKind::theory:
      return "theory.lookup.v1";
    case EvidenceKind::opening:
      return "opening.lookup.v1";
    case EvidenceKind::user_profile:
      return "profile.context.v3";
    case EvidenceKind::engine:
      return "engine.analysis.v1";
    case EvidenceKind::human_model:
      return "human_model.elo_bot.v1";
    case EvidenceKind::conversation:
      return "conversation.session.v1";
    default:
      return {};
  }
}

bool append_source(std::vector<EvidenceItem>& items,
                   const EvidenceSourceHook& source,
                   EvidenceKind kind,
                   const CoachRequest& request,
                   const QueryPlan& plan) {
  if (!source || !requested(plan, kind)) return false;
  auto item = source(request, plan);
  if (!item.has_value() || item->payload.empty()) return false;
  item->kind = kind;
  item->id = std::string(stable_id(kind));
  item->confidence = std::clamp(item->confidence, 0.0, 1.0);
  items.push_back(std::move(*item));
  return true;
}

bool append_profile_source(std::vector<EvidenceItem>& items,
                           const ProfileEvidenceSource& source,
                           const CoachRequest& request,
                           const QueryPlan& plan ) {
  if (!source || !plan.needs_profile || plan.query_family == QueryFamily::general_chess ||
      !requested(plan, EvidenceKind::user_profile)) return false;
  auto item = source(request, plan);
  if (!item.has_value() || item->payload.empty()) return false;
  item->kind = EvidenceKind::user_profile;
  item->id = std::string(stable_id(EvidenceKind::user_profile));
  item->confidence = std::clamp(item->confidence, 0.0, 1.0);
  items.push_back(std::move(*item));
  return true;
}

void append_conversation(std::vector<EvidenceItem>& items,
                         const QueryPlan& plan,
                         const CoachContext& context) {
  if (!requested(plan, EvidenceKind::conversation) ||
      context.session_summary.empty()) {
    return;
  }
  items.push_back({
      .id = std::string(stable_id(EvidenceKind::conversation)),
      .kind = EvidenceKind::conversation,
      .payload = context.session_summary,
      .confidence = 1.0,
  });
}

void append_concepts(std::vector<EvidenceItem>& items,
                     const CoachRequest& request,
                     const QueryPlan& plan ) {
  if (!requested(plan, EvidenceKind::chess_concepts)) return;
  auto item = ConceptRetriever{}.evidence(request.user_text, plan.concept_limit);
  if (item.confidence > 0.0) items.push_back(std::move(item));
}

std::vector<std::string> legal_pv_prefix(
    const std::string& fen, const CandidateLineInput& line) {
  if (line.pv_uci.empty() || line.pv_uci.front() != line.move_uci) return {};

  std::vector<std::string> result;
  result.reserve(line.pv_uci.size());
  std::string current_fen = fen;
  for (const auto& move : line.pv_uci) {
    try {
      const auto applied = kchess::apply_legal_uci_move(current_fen, move);
      result.push_back(move);
      current_fen = applied.fen_after;
    } catch (...) {
      break;
    }
  }
  return result;
}

std::optional<CandidateLineInput> legal_candidate_line(
    const CandidateLineInput& line, const std::string& fen) {
  if (line.move_uci.empty()) return std::nullopt;
  try {
    (void)kchess::apply_legal_uci_move(fen, line.move_uci);
  } catch (...) {
    return std::nullopt;
  }

  CandidateLineInput sanitized = line;
  sanitized.pv_uci = legal_pv_prefix(fen, line);
  return sanitized;
}

CandidateMoveSnapshot position_bound_candidates(
    const CandidateMoveSnapshot& snapshot,
    const std::optional<std::string>& position_fen) {
  if (!position_fen.has_value()) return snapshot;

  CandidateMoveSnapshot sanitized;
  sanitized.root_lines.reserve(snapshot.root_lines.size());
  for (const auto& line : snapshot.root_lines) {
    if (auto legal = legal_candidate_line(line, *position_fen)) {
      const auto duplicate = std::find_if(
          sanitized.root_lines.begin(), sanitized.root_lines.end(),
          [&](const CandidateLineInput& existing) {
            return existing.move_uci == legal->move_uci;
          });
      if (duplicate == sanitized.root_lines.end()) {
        sanitized.root_lines.push_back(std::move(*legal));
      }
    }
  }
  if (snapshot.user_move_line.has_value()) {
    sanitized.user_move_line =
        legal_candidate_line(*snapshot.user_move_line, *position_fen);
  }
  return sanitized;
}

bool append_candidates(RetrievedEvidence& result,
                       const CandidateMoveSnapshot& snapshot,
                       const CoachRequest& request,
                       const QueryPlan& plan) {
  CandidateMoveSystem candidates;
  const auto reviewed_move = request.verdict_challenge &&
          request.verdict_challenge->challenged_move_uci
      ? request.verdict_challenge->challenged_move_uci
      : request.user_move_uci;
  auto set = candidates.build(
      position_bound_candidates(snapshot, request.position_fen),
      reviewed_move, plan.analysis_mode);
  if (!set.best.has_value() && !set.focus.has_value()) return false;
  result.items.push_back(candidates.evidence(set));
  result.candidate_moves = std::move(set);
  return true;
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Evidence retrieval
// -----------------------------------------------------------------------------

EvidenceRetriever::EvidenceRetriever(EvidenceSources sources)
    : sources_(std::move(sources)) {}

std::optional<EvidenceItem> EvidenceRetriever::completed_move_analysis(
    const CoachRequest& request, const std::string& original_fen) const {
  if (!sources_.completed_move_analysis) return std::nullopt;
  return sources_.completed_move_analysis(request, original_fen);
}

RetrievedEvidence EvidenceRetriever::retrieve(
    const CoachRequest& request,
    const QueryPlan& plan,
    const CoachContext& context) const {
  RetrievedEvidence result;

  // Cache and persisted analysis always get the first opportunity to ground a
  // request before a fresh engine search is considered.
  append_source(result.items, sources_.cache, EvidenceKind::cache, request,
                plan);
  append_source(result.items, sources_.existing_analysis,
                EvidenceKind::existing_analysis, request, plan);
  const bool requires_complete_root_extreme_search =
      plan.analysis_mode == PositionAnalysisMode::worst_move ||
      plan.analysis_mode == PositionAnalysisMode::fastest_loss;
  if (!requires_complete_root_extreme_search &&
      !request.verdict_challenge.has_value() &&
      requested(plan, EvidenceKind::candidate_moves) &&
      sources_.existing_candidates) {
    if (auto snapshot = sources_.existing_candidates(request, plan)) {
      append_candidates(result, *snapshot, request, plan);
    }
  }

  result.existing_analysis_coverage =
      ExistingAnalysisExpert{}.inspect(plan, result.items, result.candidate_moves);

  const bool direct_current_board_extreme =
      plan.analysis_mode_explicit &&
      (plan.analysis_mode == PositionAnalysisMode::best_move ||
       plan.analysis_mode == PositionAnalysisMode::worst_move);
  if (!direct_current_board_extreme) {
    append_conversation(result.items, plan, context);
    append_concepts(result.items, request, plan);

    append_source(result.items, sources_.theory, EvidenceKind::theory, request,
                  plan);
    append_source(result.items, sources_.opening, EvidenceKind::opening, request,
                  plan);
    append_profile_source(result.items, sources_.user_profile, request, plan);
    if (plan.needs_profile && plan.query_family != QueryFamily::general_chess &&
        requested(plan, EvidenceKind::user_profile) &&
        sources_.practicality_player) {
      result.practicality_player = sources_.practicality_player(request, plan);
    }
  }

  result.engine_budget = EngineBudgetSystem{}.decide(
      request, plan, result.items,
      result.existing_analysis_coverage.satisfied_needs);
  if (result.engine_budget.should_query()) {
    QueryPlan engine_plan = plan;
    engine_plan.engine_budget = result.engine_budget.effective;

    bool used_candidate_source = false;
    if (sources_.engine_candidates) {
      auto bundle = sources_.engine_candidates(request, engine_plan);
      if (bundle.has_value()) {
        used_candidate_source = true;
        auto engine_item = std::move(bundle->engine_evidence);
        if (!engine_item.payload.empty()) {
          engine_item.kind = EvidenceKind::engine;
          engine_item.id = std::string(stable_id(EvidenceKind::engine));
          engine_item.confidence = std::clamp(engine_item.confidence, 0.0, 1.0);
          result.items.push_back(std::move(engine_item));
        }

        if (requested(plan, EvidenceKind::candidate_moves) &&
            !result.candidate_moves.has_value()) {
          append_candidates(result, bundle->candidate_snapshot, request, plan);
        }
      }
    }
    if (!used_candidate_source) {
      append_source(result.items, sources_.engine, EvidenceKind::engine, request,
                    engine_plan);
    }
  }

  return result;
}

}  // namespace kchess::ai
