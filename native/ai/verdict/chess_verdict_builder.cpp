#include "chess_verdict_builder.h"

#include <algorithm>
#include <cmath>
#include <string_view>
#include <utility>

namespace kchess::ai {
namespace {

bool white_to_move(const std::optional<std::string>& fen) {
  if (!fen.has_value()) return true;
  const auto first_space = fen->find(' ');
  return first_space == std::string::npos || first_space + 1 >= fen->size() ||
      (*fen)[first_space + 1] == 'w';
}

PositionVerdict position_verdict_from_white_score(
    const std::optional<int>& evaluation_cp,
    const std::optional<int>& mate_in) {
  if (mate_in.has_value()) {
    if (*mate_in > 0) return PositionVerdict::white_winning;
    if (*mate_in < 0) return PositionVerdict::black_winning;
  }
  if (!evaluation_cp.has_value()) return PositionVerdict::unknown;
  const int cp = *evaluation_cp;
  if (cp >= 300) return PositionVerdict::white_winning;
  if (cp >= 100) return PositionVerdict::white_clearly_better;
  if (cp >= 35) return PositionVerdict::white_slightly_better;
  if (cp > -35) return PositionVerdict::equal;
  if (cp > -100) return PositionVerdict::black_slightly_better;
  if (cp > -300) return PositionVerdict::black_clearly_better;
  return PositionVerdict::black_winning;
}

MoveVerdict mapped_native_classification(const std::optional<std::string>& value) {
  if (!value.has_value()) return MoveVerdict::unknown;
  const auto& category = *value;
  if (category == "best" || category == "critical" || category == "brilliant" ||
      category == "theory" || category == "forced") {
    return MoveVerdict::best;
  }
  if (category == "excellent") return MoveVerdict::excellent;
  if (category == "good") return MoveVerdict::good;
  if (category == "okay") return MoveVerdict::playable;
  if (category == "inaccuracy" || category == "miss") return MoveVerdict::inaccuracy;
  if (category == "mistake") return MoveVerdict::mistake;
  if (category == "blunder") return MoveVerdict::blunder;
  return MoveVerdict::unknown;
}

const CandidateMove* candidate_for_move(
    const CandidateMoveSet& set, const std::string_view move) {
  const auto matches = [move](const std::optional<CandidateMove>& candidate) {
    return candidate.has_value() && candidate->move_uci == move;
  };
  if (matches(set.best)) return &*set.best;
  if (matches(set.focus)) return &*set.focus;
  if (matches(set.user_move)) return &*set.user_move;
  const auto found = std::find_if(
      set.alternatives.begin(), set.alternatives.end(),
      [move](const CandidateMove& candidate) { return candidate.move_uci == move; });
  return found == set.alternatives.end() ? nullptr : &*found;
}

const CandidateMove* evaluated_candidate(
    const CoachRequest& request,
    const QueryPlan& plan,
    const CandidateMoveSet& set) {
  if ((plan.analysis_mode == PositionAnalysisMode::worst_move ||
       plan.analysis_mode == PositionAnalysisMode::fastest_loss) &&
      set.focus.has_value()) {
    return &*set.focus;
  }
  if (request.verdict_challenge &&
      request.verdict_challenge->challenged_move_uci) {
    if (const auto* candidate = candidate_for_move(
            set, *request.verdict_challenge->challenged_move_uci)) {
      return candidate;
    }
  }
  if (request.user_move_uci.has_value()) {
    if (const auto* candidate = candidate_for_move(set, *request.user_move_uci)) {
      return candidate;
    }
  }
  if (request.hint_move_uci.has_value()) {
    if (const auto* candidate = candidate_for_move(set, *request.hint_move_uci)) {
      return candidate;
    }
  }
  if (plan.analysis_mode == PositionAnalysisMode::best_move && set.best.has_value()) {
    return &*set.best;
  }
  return nullptr;
}

std::optional<int> mover_cp_loss(
    const CandidateMove& best,
    const CandidateMove& evaluated,
    const bool root_is_white) {
  if (!best.evaluation_cp.has_value() || !evaluated.evaluation_cp.has_value()) {
    return std::nullopt;
  }
  const int raw = root_is_white
      ? *best.evaluation_cp - *evaluated.evaluation_cp
      : *evaluated.evaluation_cp - *best.evaluation_cp;
  return std::max(0, raw);
}

std::optional<double> expected_score_loss(
    const CandidateMove& best, const CandidateMove& evaluated) {
  if (!best.expected_score.has_value() || !evaluated.expected_score.has_value()) {
    return std::nullopt;
  }
  return std::clamp(*best.expected_score - *evaluated.expected_score, 0.0, 1.0);
}

MoveVerdict classify_loss(
    const std::optional<int>& cp_loss,
    const std::optional<double>& score_loss,
    const PositionVerdict resulting_position,
    const bool mover_is_white) {
  const bool mover_losing = mover_is_white
      ? resulting_position == PositionVerdict::black_winning
      : resulting_position == PositionVerdict::white_winning;
  if (mover_losing &&
      ((cp_loss.has_value() && *cp_loss >= 300) ||
       (score_loss.has_value() && *score_loss >= 0.20))) {
    return MoveVerdict::losing;
  }

  const int cp = cp_loss.value_or(0);
  const double loss = score_loss.value_or(0.0);
  if ((!cp_loss.has_value() || cp <= 12) &&
      (!score_loss.has_value() || loss <= 0.006)) {
    return MoveVerdict::best;
  }
  if ((!cp_loss.has_value() || cp <= 70) &&
      (!score_loss.has_value() || loss <= 0.035)) {
    return MoveVerdict::excellent;
  }
  if ((!cp_loss.has_value() || cp <= 120) &&
      (!score_loss.has_value() || loss <= 0.065)) {
    return MoveVerdict::good;
  }
  if ((!cp_loss.has_value() || cp <= 180) &&
      (!score_loss.has_value() || loss <= 0.10)) {
    return MoveVerdict::playable;
  }
  if ((cp_loss.has_value() && cp > 400) ||
      (score_loss.has_value() && loss > 0.25)) {
    return MoveVerdict::blunder;
  }
  if ((cp_loss.has_value() && cp > 260) ||
      (score_loss.has_value() && loss > 0.15)) {
    return MoveVerdict::mistake;
  }
  if ((cp_loss.has_value() && cp > 180) ||
      (score_loss.has_value() && loss > 0.10)) {
    return MoveVerdict::inaccuracy;
  }
  if (cp_loss.has_value() || score_loss.has_value()) return MoveVerdict::playable;
  return MoveVerdict::unknown;
}

}  // namespace

std::optional<ChessVerdictContract> build_chess_verdict(
    const CoachRequest& request,
    const QueryPlan& plan,
    const std::optional<CandidateMoveSet>& candidates) {
  ChessVerdictContract verdict;
  const bool mover_is_white = white_to_move(request.position_fen);

  // Completed-move classifications are already native adjudications. Preserve
  // them rather than reclassifying the same move from a thinner candidate set.
  const MoveVerdict classified = mapped_native_classification(
      request.move_attribution && request.move_attribution->classification
          ? request.move_attribution->classification
          : request.user_move_classification);
  if (classified != MoveVerdict::unknown) {
    verdict.move_verdict = classified;
    verdict.authoritative = true;
    verdict.basis = "native_move_classification";
    if (request.move_attribution && request.move_attribution->played_move_uci) {
      verdict.evaluated_move_uci = request.move_attribution->played_move_uci;
    } else if (request.user_move_uci) {
      verdict.evaluated_move_uci = request.user_move_uci;
    }
  }

  if (!candidates.has_value()) {
    return verdict.authoritative
        ? std::optional<ChessVerdictContract>{std::move(verdict)}
        : std::nullopt;
  }

  const auto& set = *candidates;
  const CandidateMove* evaluated = evaluated_candidate(request, plan, set);

  // The best candidate's score is the root-position score. Extreme searches
  // intentionally omit the best root move, so they can bind only the selected
  // move/result, not a current-position verdict.
  if (set.best.has_value()) {
    verdict.white_evaluation_cp = set.best->evaluation_cp;
    verdict.white_mate_in = set.best->mate_in;
    verdict.position_verdict = position_verdict_from_white_score(
        set.best->evaluation_cp, set.best->mate_in);
    verdict.position_scope = "current_position";
    if (verdict.position_verdict != PositionVerdict::unknown) {
      verdict.authoritative = true;
      if (verdict.basis.empty()) verdict.basis = "engine_root_evaluation";
    }
  }

  if (evaluated != nullptr) {
    verdict.evaluated_move_uci = evaluated->move_uci;
    const auto resulting_position = position_verdict_from_white_score(
        evaluated->evaluation_cp, evaluated->mate_in);

    if (plan.analysis_mode == PositionAnalysisMode::worst_move ||
        plan.analysis_mode == PositionAnalysisMode::fastest_loss) {
      verdict.position_scope = "after_evaluated_move";
      verdict.position_verdict = resulting_position;
      verdict.white_evaluation_cp = evaluated->evaluation_cp;
      verdict.white_mate_in = evaluated->mate_in;
      // Complete-root selection proves extremeness, but does not by itself
      // prove a classifier label against the true best move.
      verdict.move_verdict = resulting_position ==
              (mover_is_white ? PositionVerdict::black_winning
                              : PositionVerdict::white_winning)
          ? MoveVerdict::losing
          : MoveVerdict::unknown;
      verdict.authoritative = true;
      verdict.basis = plan.analysis_mode == PositionAnalysisMode::fastest_loss
          ? "complete_root_fastest_loss"
          : "complete_root_worst_move";
    } else if (set.best.has_value()) {
      verdict.position_scope = "after_evaluated_move";
      verdict.position_verdict = resulting_position;
      verdict.white_evaluation_cp = evaluated->evaluation_cp;
      verdict.white_mate_in = evaluated->mate_in;
      verdict.move_loss_cp = mover_cp_loss(*set.best, *evaluated, mover_is_white);
      verdict.move_loss_expected_score = expected_score_loss(*set.best, *evaluated);
      if (verdict.move_verdict == MoveVerdict::unknown) {
        verdict.move_verdict = evaluated->move_uci == set.best->move_uci
            ? MoveVerdict::best
            : classify_loss(verdict.move_loss_cp,
                            verdict.move_loss_expected_score,
                            resulting_position,
                            mover_is_white);
      }
      verdict.authoritative = verdict.authoritative ||
          verdict.move_verdict != MoveVerdict::unknown;
      if (verdict.basis.empty()) verdict.basis = "engine_candidate_comparison";
    }
  }

  return verdict.authoritative
      ? std::optional<ChessVerdictContract>{std::move(verdict)}
      : std::nullopt;
}

}  // namespace kchess::ai
