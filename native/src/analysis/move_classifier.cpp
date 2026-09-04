#include "analysis/move_classifier.h"

#include <algorithm>
#include <cmath>
#include <deque>
#include <stdexcept>

#include "engine/stockfish_runtime.h"
#include "movegen.h"
#include "position.h"
#include "uci.h"

namespace kchess {
namespace {

double clamp_probability(const double value) { return std::clamp(value, 0.0, 1.0); }

std::optional<int> nonnegative_cp_difference(
    const std::optional<int>& better, const std::optional<int>& worse) {
  if (!better.has_value() || !worse.has_value()) return std::nullopt;
  return std::max(0, *better - *worse);
}

int material_balance(const Stockfish::Position& position, const Stockfish::Color perspective) {
  constexpr int values[] = {0, 100, 320, 330, 500, 900, 0, 0};
  int result = 0;
  for (Stockfish::Square square = Stockfish::SQ_A1; square <= Stockfish::SQ_H8; ++square) {
    const auto piece = position.piece_on(square);
    if (piece == Stockfish::NO_PIECE) continue;
    const int value = values[Stockfish::type_of(piece)];
    result += Stockfish::color_of(piece) == perspective ? value : -value;
  }
  return result;
}

Stockfish::Move find_uci_move(const Stockfish::Position& position, const std::string& uci) {
  for (const auto move : Stockfish::MoveList<Stockfish::LEGAL>(position)) {
    if (Stockfish::UCIEngine::move(move, false) == uci) return move;
  }
  return Stockfish::Move::none();
}

}  // namespace

std::optional<double> expected_score_side_to_move(const PositionEvaluation& evaluation) {
  if (evaluation.mate_in.has_value()) return *evaluation.mate_in > 0 ? 1.0 : 0.0;
  if (evaluation.wdl.has_value()) {
    const double total = evaluation.wdl->wins + evaluation.wdl->draws + evaluation.wdl->losses;
    if (total > 0.0) {
      return clamp_probability((evaluation.wdl->wins + 0.5 * evaluation.wdl->draws) / total);
    }
  }
  if (evaluation.evaluation_cp.has_value()) {
    // A smooth, bounded CP fallback; WDL and mate always take precedence.
    return clamp_probability(1.0 / (1.0 + std::exp(-*evaluation.evaluation_cp / 400.0)));
  }
  return std::nullopt;
}

std::optional<double> expected_score_mover_after_move(const PositionEvaluation& evaluation) {
  const auto side_to_move = expected_score_side_to_move(evaluation);
  return side_to_move.has_value() ? std::optional<double>(1.0 - *side_to_move) : std::nullopt;
}

std::optional<double> expected_score_loss(
    const std::optional<double>& best, const std::optional<double>& played) {
  if (!best.has_value() || !played.has_value()
      || !std::isfinite(*best) || !std::isfinite(*played)) {
    return std::nullopt;
  }
  return std::clamp(*best - *played, 0.0, 1.0);
}

MoveCategory classify_move(
    const MoveClassifierInput& input, const MoveClassifierConfig& config) {
  if (input.theory) return MoveCategory::theory;

  const auto loss = expected_score_loss(input.best_expected_score, input.played_expected_score);
  if (!loss.has_value()) return MoveCategory::unknown;

  const double best = *input.best_expected_score;
  const double played = *input.played_expected_score;
  if (!std::isfinite(best) || !std::isfinite(played)
      || best < 0.0 || best > 1.0 || played < 0.0 || played > 1.0) {
    return MoveCategory::unknown;
  }

  const bool has_second = input.second_best_expected_score.has_value()
      && std::isfinite(*input.second_best_expected_score)
      && *input.second_best_expected_score >= 0.0
      && *input.second_best_expected_score <= 1.0;
  const double second = has_second ? *input.second_best_expected_score : best;
  const double gap = has_second ? std::max(0.0, best - second) : 0.0;
  const bool has_real_alternative = input.legal_move_count > 1 && has_second;
  const auto played_cp_loss = nonnegative_cp_difference(
      input.best_evaluation_cp, input.played_evaluation_cp);
  const auto second_cp_gap = nonnegative_cp_difference(
      input.best_evaluation_cp, input.second_best_evaluation_cp);

  // Best is intentionally rank-strict. WDL can saturate at 1000/0/0 or
  // 0/0/1000 in clearly decided positions, making objectively different root
  // moves appear to have exactly the same expected score. Such alternatives
  // are Excellent at most; only Stockfish's actual rank-1 move is Best.

  // Brilliant is exceptional, but it must not depend on a fragile exact-rank
  // match from one particular search depth. A verified sacrifice that is
  // objectively near-best can still be Brilliant when it preserves a strong
  // result and has concrete tactical justification. A centipawn guard prevents
  // saturated WDL from making a materially worse sacrifice look near-best.
  const bool cp_near_best = !played_cp_loss.has_value() || *played_cp_loss <= 35;
  const bool near_best = input.played_is_best
      || (*loss <= config.excellent_loss && cp_near_best);
  const bool uniquely_strong_best = has_real_alternative
      && (gap >= config.brilliant_gap
          || (second_cp_gap.has_value() && *second_cp_gap >= config.critical_cp_gap));
  const bool brilliant_eligible = near_best
      && !input.was_in_check_before_move
      && input.legal_move_count > 1
      && best >= config.brilliant_min_best_score
      && played >= config.brilliant_min_best_score
      && input.material_sacrifice
      && (input.forces_nontrivial_mate || uniquely_strong_best);
  if (brilliant_eligible) return MoveCategory::brilliant;

  // "Critical" is the app's Great-move bucket. It is reserved for the exact
  // engine move when the runner-up is materially worse in expected-score
  // terms. The old classifier additionally required coarse result-band
  // crossings, which caused clear only-move / great-move situations to be
  // labelled plain Best.
  const bool critical_eligible = input.played_is_best
      && has_real_alternative
      && best >= config.critical_min_best_score
      && (gap >= config.critical_gap
          || (second_cp_gap.has_value() && *second_cp_gap >= config.critical_cp_gap));
  if (critical_eligible) return MoveCategory::critical;

  if (input.played_is_best) return MoveCategory::best;

  // Miss is a concrete missed opportunity, not a synonym for every bad move.
  // Missing a forced mate qualifies. Otherwise require evidence that there was
  // a unique tactical move and that most of the opportunity was actually lost.
  const bool missed_tactical_opportunity = input.only_move_tactical
      && best >= config.miss_best_score
      && played <= config.miss_played_ceiling
      && *loss >= config.miss_loss;
  if (input.missed_forced_mate || missed_tactical_opportunity) {
    return MoveCategory::miss;
  }

  // Blunder requires either a very large expected-score loss or a practical
  // outcome collapse. In positions that were already essentially lost, a
  // further small deterioration is capped at Mistake so the move list does not
  // become a wall of red labels caused by engine noise.
  const bool had_meaningful_chances = best >= config.blunder_min_best_score;
  const bool outcome_collapse = (best >= 0.70 && played <= 0.45)
      || (best >= 0.45 && played <= 0.20);
  const bool severe_blunder = *loss >= config.blunder_severe_loss;
  const bool outcome_blunder = *loss >= config.blunder_outcome_loss && outcome_collapse;
  const bool newly_allowed_mate = input.allowed_forced_mate && had_meaningful_chances;
  if (had_meaningful_chances && (severe_blunder || outcome_blunder || newly_allowed_mate)) {
    return MoveCategory::blunder;
  }
  // Even in an already lost position, voluntarily allowing a new forced mate
  // is not an Excellent move. Keep the old anti-inflation rule (no automatic
  // Blunder without meaningful chances), but floor the verdict at Mistake.
  if (input.allowed_forced_mate) return MoveCategory::mistake;

  // WDL alone becomes too coarse once a position is strongly won/lost. Use
  // centipawn loss as a second guard for ordinary quality buckets so a move
  // cannot stay Excellent merely because both alternatives round to the same
  // practical WDL. Mate-only lines have no CP value and keep the WDL path.
  const bool cp_excellent = !played_cp_loss.has_value()
      || *played_cp_loss <= config.excellent_cp_loss;
  const bool cp_okay = !played_cp_loss.has_value()
      || *played_cp_loss <= config.okay_cp_loss;
  if (*loss <= config.excellent_loss && cp_excellent) return MoveCategory::excellent;
  if (*loss <= config.okay_loss && cp_okay) return MoveCategory::okay;
  return MoveCategory::mistake;
}

std::string move_category_name(const MoveCategory category) {
  switch (category) {
    case MoveCategory::theory: return "theory";
    case MoveCategory::brilliant: return "brilliant";
    case MoveCategory::critical: return "critical";
    case MoveCategory::best: return "best";
    case MoveCategory::excellent: return "excellent";
    case MoveCategory::okay: return "okay";
    case MoveCategory::miss: return "miss";
    case MoveCategory::mistake: return "mistake";
    case MoveCategory::blunder: return "blunder";
    case MoveCategory::unknown: return "unknown";
  }
  return "unknown";
}

bool material_sacrifice_in_pv(
    const std::string& fen_before,
    const std::vector<std::string>& principal_variation,
    const int max_plies) {
  if (principal_variation.size() < 2 || max_plies < 2) return false;
  initialize_stockfish_runtime();
  std::deque<Stockfish::StateInfo> states(1);
  Stockfish::Position position;
  position.set(fen_before, false, &states.back());
  const auto mover = position.side_to_move();
  const int before = material_balance(position, mover);
  const int limit = std::min(max_plies, static_cast<int>(principal_variation.size()));
  for (int ply = 0; ply < limit; ++ply) {
    const auto move = find_uci_move(position, principal_variation[ply]);
    if (move == Stockfish::Move::none()) return false;
    states.emplace_back();
    position.do_move(move, states.back(), nullptr);
    // Inspect only after the opponent has had a chance to accept the sacrifice.
    if (ply % 2 == 1 && before - material_balance(position, mover) >= 250) return true;
  }
  return false;
}

PositionContext position_context(const std::string& fen) {
  initialize_stockfish_runtime();
  std::deque<Stockfish::StateInfo> states(1);
  Stockfish::Position position;
  position.set(fen, false, &states.back());
  return {
      .legal_move_count = static_cast<int>(Stockfish::MoveList<Stockfish::LEGAL>(position).size()),
      .in_check = static_cast<bool>(position.checkers()),
  };
}

}  // namespace kchess
