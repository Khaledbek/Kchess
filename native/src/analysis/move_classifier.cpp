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

// -----------------------------------------------------------------------------
// Section: Evaluation helpers
// -----------------------------------------------------------------------------

double clamp_probability(const double value) {
  return std::clamp(value, 0.0, 1.0);
}

bool valid_expected(const std::optional<double>& value) {
  return value.has_value() && std::isfinite(*value)
      && *value >= 0.0 && *value <= 1.0;
}

std::optional<int> nonnegative_cp_difference(
    const std::optional<int>& better,
    const std::optional<int>& worse) {
  if (!better.has_value() || !worse.has_value()) return std::nullopt;
  return std::max(0, *better - *worse);
}

bool within_cp_limit(const std::optional<int>& loss, const int limit) {
  return !loss.has_value() || *loss <= limit;
}

bool within_expected_limit(const std::optional<double>& loss, const double limit) {
  return !loss.has_value() || *loss <= limit;
}

int piece_value_cp(const Stockfish::Piece piece) {
  if (piece == Stockfish::NO_PIECE) return 0;
  switch (Stockfish::type_of(piece)) {
    case Stockfish::PAWN: return 100;
    case Stockfish::KNIGHT: return 320;
    case Stockfish::BISHOP: return 330;
    case Stockfish::ROOK: return 500;
    case Stockfish::QUEEN: return 900;
    case Stockfish::KING:
    case Stockfish::NO_PIECE_TYPE: return 0;
  }
  return 0;
}

int material_balance(
    const Stockfish::Position& position,
    const Stockfish::Color perspective) {
  int result = 0;
  for (Stockfish::Square square = Stockfish::SQ_A1;
       square <= Stockfish::SQ_H8;
       ++square) {
    const auto piece = position.piece_on(square);
    if (piece == Stockfish::NO_PIECE) continue;
    const int value = piece_value_cp(piece);
    result += Stockfish::color_of(piece) == perspective ? value : -value;
  }
  return result;
}

Stockfish::Move find_uci_move(
    const Stockfish::Position& position,
    const std::string& uci) {
  for (const auto move : Stockfish::MoveList<Stockfish::LEGAL>(position)) {
    if (Stockfish::UCIEngine::move(move, false) == uci) return move;
  }
  return Stockfish::Move::none();
}

bool same_mate_value(
    const std::optional<int>& best,
    const std::optional<int>& played) {
  if (!best.has_value() && !played.has_value()) return true;
  if (!best.has_value() || !played.has_value()) return false;
  return *best == *played;
}

bool equivalent_to_best(
    const MoveClassifierInput& input,
    const MoveClassifierConfig& config,
    const std::optional<double>& loss,
    const std::optional<int>& cp_loss) {
  if (input.played_is_best) return true;

  if (input.best_mate_in.has_value() || input.played_mate_in.has_value()) {
    if (!same_mate_value(input.best_mate_in, input.played_mate_in)) return false;
  }

  if (!within_expected_limit(loss, config.equivalent_loss)) return false;
  if (!within_cp_limit(cp_loss, config.equivalent_cp_loss)) return false;

  // If both numerical signals are absent, rank alone cannot prove equality.
  return loss.has_value() || cp_loss.has_value()
      || (input.best_mate_in.has_value() && input.played_mate_in.has_value());
}

bool result_band_drop(const double best, const double alternative) {
  return (best >= 0.75 && alternative < 0.60)
      || (best >= 0.60 && alternative < 0.45)
      || (best >= 0.50 && alternative < 0.35)
      || (best >= 0.35 && alternative < 0.20);
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Evaluation conversion
// -----------------------------------------------------------------------------

std::optional<double> expected_score_side_to_move(
    const PositionEvaluation& evaluation) {
  if (evaluation.mate_in.has_value()) {
    return *evaluation.mate_in > 0 ? 1.0 : 0.0;
  }
  if (evaluation.wdl.has_value()) {
    const double total = evaluation.wdl->wins
        + evaluation.wdl->draws
        + evaluation.wdl->losses;
    if (total > 0.0) {
      return clamp_probability(
          (evaluation.wdl->wins + 0.5 * evaluation.wdl->draws) / total);
    }
  }
  if (evaluation.evaluation_cp.has_value()) {
    // Lichess' published empirical CP -> winning-chance slope. WDL is preferred
    // when Stockfish provides it, but this keeps old/cache rows comparable.
    constexpr double kLichessWinChanceSlope = 0.00368208;
    return clamp_probability(
        1.0 / (1.0 + std::exp(
            -kLichessWinChanceSlope * *evaluation.evaluation_cp)));
  }
  return std::nullopt;
}

std::optional<double> expected_score_mover_after_move(
    const PositionEvaluation& evaluation) {
  const auto side_to_move = expected_score_side_to_move(evaluation);
  return side_to_move.has_value()
      ? std::optional<double>(1.0 - *side_to_move)
      : std::nullopt;
}

std::optional<double> expected_score_loss(
    const std::optional<double>& best,
    const std::optional<double>& played) {
  if (!valid_expected(best) || !valid_expected(played)) return std::nullopt;
  return std::clamp(*best - *played, 0.0, 1.0);
}

// -----------------------------------------------------------------------------
// Section: Move classification
// -----------------------------------------------------------------------------

MoveCategory classify_move(
    const MoveClassifierInput& input,
    const MoveClassifierConfig& config) {
  // There is no decision when exactly one legal move exists. Forced is more
  // specific than Theory/Best and therefore wins the priority ladder.
  if (input.legal_move_count == 1) return MoveCategory::forced;
  if (input.theory) return MoveCategory::theory;

  const auto loss = expected_score_loss(
      input.best_expected_score, input.played_expected_score);
  const auto cp_loss = nonnegative_cp_difference(
      input.best_evaluation_cp, input.played_evaluation_cp);
  const auto second_cp_gap = nonnegative_cp_difference(
      input.best_evaluation_cp, input.second_best_evaluation_cp);

  const bool best_forces_mate = input.best_mate_in.has_value()
      && *input.best_mate_in > 0;
  const bool played_forces_mate = input.played_mate_in.has_value()
      && *input.played_mate_in > 0;
  const bool second_forces_mate = input.second_best_mate_in.has_value()
      && *input.second_best_mate_in > 0;
  const bool unique_forced_mate = best_forces_mate && !second_forces_mate;

  // Stockfish's final bestmove remains authoritative even when an independent
  // after-position search is missing/noisy. This invariant prevents the engine
  // recommendation from ever receiving a negative quality label.
  if (input.played_is_best
      && !loss.has_value()
      && !cp_loss.has_value()
      && !input.best_mate_in.has_value()) {
    return MoveCategory::best;
  }

  const bool equivalent_best = equivalent_to_best(input, config, loss, cp_loss);
  const bool candidate_best = input.played_is_best || equivalent_best;

  const bool has_second_expected = valid_expected(input.best_expected_score)
      && valid_expected(input.second_best_expected_score);
  const double second_expected_gap = has_second_expected
      ? std::max(0.0,
          *input.best_expected_score - *input.second_best_expected_score)
      : 0.0;
  const bool second_is_equivalent = has_second_expected
      && second_expected_gap <= config.equivalent_loss
      && within_cp_limit(second_cp_gap, config.equivalent_cp_loss)
      && same_mate_value(input.best_mate_in, input.second_best_mate_in);
  const bool second_result_band_drop = has_second_expected
      && result_band_drop(
          *input.best_expected_score, *input.second_best_expected_score);
  // Prefer practical outcome separation whenever WDL/expected-score data is
  // available. A raw CP gap is only a fallback: +10 vs +8 is a large CP gap
  // but not a critical decision when both moves still win trivially.
  const bool critical_gap = !second_is_equivalent
      && (unique_forced_mate
          || second_result_band_drop
          || (has_second_expected
              && second_expected_gap >= config.critical_gap)
          || (!has_second_expected
              && second_cp_gap.has_value()
              && *second_cp_gap >= config.critical_cp_gap));

  if (candidate_best) {
    const bool verified_piece_sacrifice = input.sacrifice_against_best_defense
        && input.sacrifice_piece_value_cp >= config.brilliant_piece_min_cp
        && input.sacrifice_net_material_loss_cp
            >= config.brilliant_net_sacrifice_min_cp;
    const bool sound_advantage = played_forces_mate
        || (valid_expected(input.played_expected_score)
            && *input.played_expected_score >= config.brilliant_min_best_score)
        || (input.played_evaluation_cp.has_value()
            && *input.played_evaluation_cp >= config.brilliant_min_best_cp);
    const bool already_completely_decided = !played_forces_mate
        && valid_expected(input.best_expected_score)
        && *input.best_expected_score >= config.brilliant_decided_score_ceiling;
    if (!input.was_in_check_before_move
        && !already_completely_decided
        && input.legal_move_count > 1
        && verified_piece_sacrifice
        && sound_advantage) {
      return MoveCategory::brilliant;
    }

    // Critical is intentionally rank-1 strict. If another move is genuinely
    // equivalent, the decision was not unique and both moves are simply Best.
    if (input.played_is_best && critical_gap) {
      return MoveCategory::critical;
    }
    return MoveCategory::best;
  }

  // A newly created forced mate for the opponent is always a Blunder. This is
  // independent of whether the side was already -3, -8, etc.; if mate was not
  // forced before the move and is forced afterwards, the move hung the game.
  if (input.allowed_forced_mate) return MoveCategory::blunder;

  // Missing one's own forced mate is a missed opportunity, not the same concept
  // as hanging mate to the opponent.
  if (input.missed_forced_mate) return MoveCategory::miss;

  const bool meaningful_eval_drop =
      (loss.has_value() && *loss >= config.mistake_loss)
      || (cp_loss.has_value() && *cp_loss >= config.mistake_cp_loss);
  const bool severe_eval_drop =
      (loss.has_value() && *loss >= config.blunder_loss)
      || (cp_loss.has_value() && *cp_loss >= config.blunder_cp_loss);

  const bool material_blunder = input.material_loss_cp
          >= config.blunder_material_loss_cp
      && severe_eval_drop;
  const bool catastrophic_result_flip = loss.has_value()
      && *loss >= config.blunder_severe_loss
      && cp_loss.has_value()
      && *cp_loss >= config.blunder_cp_loss;
  const bool result_collapsed_to_lost = valid_expected(input.best_expected_score)
      && valid_expected(input.played_expected_score)
      && *input.best_expected_score >= 0.45
      && *input.played_expected_score <= 0.15
      && cp_loss.has_value()
      && *cp_loss >= 150;
  if (material_blunder || catastrophic_result_flip || result_collapsed_to_lost) {
    return MoveCategory::blunder;
  }

  const int material_opportunity_gap = std::max(
      0, input.best_material_gain_cp - input.played_material_gain_cp);
  const bool had_real_advantage = best_forces_mate
      || (input.best_evaluation_cp.has_value()
          && *input.best_evaluation_cp >= config.miss_min_best_cp)
      || (valid_expected(input.best_expected_score)
          && *input.best_expected_score >= 0.58);
  const bool opportunity_was_lost =
      (loss.has_value() && *loss >= config.miss_loss)
      || (cp_loss.has_value() && *cp_loss >= config.miss_cp_loss)
      || material_opportunity_gap >= config.miss_material_opportunity_cp;
  const bool no_material_self_damage = input.material_loss_cp
      < config.mistake_material_loss_cp;

  const bool material_mistake = input.material_loss_cp
          >= config.mistake_material_loss_cp
      && meaningful_eval_drop;
  const bool outside_top_four = input.played_rank >= 5;
  const bool result_band_worsened = valid_expected(input.best_expected_score)
      && valid_expected(input.played_expected_score)
      && result_band_drop(
          *input.best_expected_score, *input.played_expected_score);
  const bool ranked_positional_mistake = outside_top_four
      && meaningful_eval_drop
      && result_band_worsened;

  // Concrete self-inflicted material damage beats a positive root rank. A
  // shallow rank by itself must never hide a pawn/piece drop that the
  // continuation search has already proven.
  if (material_mistake) return MoveCategory::mistake;

  // Rank-aware positive classes. This is deliberately evaluated before the
  // generic Miss rule when the move is genuinely inside Stockfish's top four:
  // ranks 2/3/4 with only a light score deviation are Excellent/Good/Okay, as
  // opposed to being called a Miss merely because the best position was good.
  const bool excellent_quality = within_expected_limit(loss, config.excellent_loss)
      && within_cp_limit(cp_loss, config.excellent_cp_loss);
  const bool good_quality = within_expected_limit(loss, config.good_loss)
      && within_cp_limit(cp_loss, config.good_cp_loss);
  const bool okay_quality = within_expected_limit(loss, config.okay_loss)
      && within_cp_limit(cp_loss, config.okay_cp_loss);
  const bool missed_material_tactic = material_opportunity_gap
      >= config.miss_material_opportunity_cp;

  if (!missed_material_tactic) {
    if (input.played_rank == 2 && excellent_quality) return MoveCategory::excellent;
    if (input.played_rank == 3 && good_quality) return MoveCategory::good;
    if (input.played_rank == 4 && okay_quality) return MoveCategory::okay;
  }

  // A Miss is specifically "there was something significantly better and I
  // failed to cash it in". This covers +2 -> +1 without material self-damage,
  // missed tactical material, and winning continuations whose score drop is
  // too large to qualify for the positive top-four labels above.
  if (had_real_advantage
      && opportunity_was_lost
      && no_material_self_damage) {
    return MoveCategory::miss;
  }

  if (ranked_positional_mistake) return MoveCategory::mistake;

  // Fallback for old caches or positions where a reliable MultiPV rank was not
  // available. Equivalent moves were already promoted to Best above.
  if (excellent_quality) return MoveCategory::excellent;
  if (good_quality) return MoveCategory::good;
  if (okay_quality) return MoveCategory::okay;

  // Two independent signals (outside top four + meaningful evaluation loss)
  // are enough for a non-material Mistake when the exact tactical consequence
  // lies beyond the short material PV horizon.
  if (outside_top_four && meaningful_eval_drop) return MoveCategory::mistake;

  // Avoid inventing a harsh label from one noisy number alone. A move that is
  // not proven to be a concrete error remains Okay rather than being punished
  // purely because one score crossed a threshold.
  return MoveCategory::okay;
}

std::string move_category_name(const MoveCategory category) {
  switch (category) {
    case MoveCategory::theory: return "theory";
    case MoveCategory::forced: return "forced";
    case MoveCategory::brilliant: return "brilliant";
    case MoveCategory::critical: return "critical";
    case MoveCategory::best: return "best";
    case MoveCategory::excellent: return "excellent";
    case MoveCategory::good: return "good";
    case MoveCategory::okay: return "okay";
    case MoveCategory::miss: return "miss";
    case MoveCategory::mistake: return "mistake";
    case MoveCategory::blunder: return "blunder";
    case MoveCategory::unknown: return "unknown";
  }
  return "unknown";
}

// -----------------------------------------------------------------------------
// Section: Position evidence
// -----------------------------------------------------------------------------

bool root_move_is_tactical(
    const std::string& fen_before,
    const std::string& uci_move) {
  if (uci_move.empty()) return false;
  initialize_stockfish_runtime();
  std::deque<Stockfish::StateInfo> states(1);
  Stockfish::Position position;
  position.set(fen_before, false, &states.back());
  const auto move = find_uci_move(position, uci_move);
  if (move == Stockfish::Move::none()) return false;
  return position.capture(move)
      || position.gives_check(move)
      || move.type_of() == Stockfish::PROMOTION;
}

VerifiedSacrifice root_move_sacrifice_evidence(
    const std::string& fen_before,
    const std::vector<std::string>& principal_variation) {
  if (principal_variation.size() < 2) return {};

  initialize_stockfish_runtime();
  std::deque<Stockfish::StateInfo> states(1);
  Stockfish::Position position;
  position.set(fen_before, false, &states.back());
  const auto mover = position.side_to_move();
  const int before = material_balance(position, mover);

  const auto played_move = find_uci_move(position, principal_variation[0]);
  if (played_move == Stockfish::Move::none()) return {};
  states.emplace_back();
  position.do_move(played_move, states.back(), nullptr);
  const int after_played_move = material_balance(position, mover);

  // The opponent's best reply must actually take the piece offered by this root
  // move. This prevents unrelated quiet moves from inheriting a sacrifice that
  // was already available against some other hanging piece.
  const auto offered_square = played_move.to_sq();
  const auto offered_piece = position.piece_on(offered_square);
  if (offered_piece == Stockfish::NO_PIECE
      || Stockfish::color_of(offered_piece) != mover) {
    return {};
  }

  const auto best_reply = find_uci_move(position, principal_variation[1]);
  if (best_reply == Stockfish::Move::none()
      || !position.capture(best_reply)
      || best_reply.type_of() == Stockfish::EN_PASSANT
      || best_reply.to_sq() != offered_square) {
    return {};
  }

  states.emplace_back();
  position.do_move(best_reply, states.back(), nullptr);
  const int after_best_reply = material_balance(position, mover);
  const int net_material_given = std::max(0, before - after_best_reply);
  const int material_taken_on_reply = std::max(
      0, after_played_move - after_best_reply);

  return {
      .verified = material_taken_on_reply > 0 && net_material_given > 0,
      .offered_piece_value_cp = piece_value_cp(offered_piece),
      .net_material_loss_cp = net_material_given,
  };
}

bool root_move_sacrifice_against_best_defense(
    const std::string& fen_before,
    const std::vector<std::string>& principal_variation,
    const int minimum_material_loss_cp) {
  if (minimum_material_loss_cp <= 0) return false;
  const auto evidence = root_move_sacrifice_evidence(
      fen_before, principal_variation);
  return evidence.verified
      && evidence.net_material_loss_cp >= minimum_material_loss_cp;
}

PvMaterialEvidence pv_material_evidence(
    const std::string& fen_before,
    const std::vector<std::string>& principal_variation,
    const int max_plies) {
  PvMaterialEvidence evidence;
  if (principal_variation.empty() || max_plies <= 0) return evidence;

  initialize_stockfish_runtime();
  std::deque<Stockfish::StateInfo> states(1);
  Stockfish::Position position;
  position.set(fen_before, false, &states.back());
  const auto mover = position.side_to_move();
  const int before = material_balance(position, mover);
  int last_delta = 0;

  const int limit = std::min(
      max_plies, static_cast<int>(principal_variation.size()));
  for (int ply = 0; ply < limit; ++ply) {
    const auto move = find_uci_move(position, principal_variation[ply]);
    if (move == Stockfish::Move::none()) break;
    states.emplace_back();
    position.do_move(move, states.back(), nullptr);
    last_delta = material_balance(position, mover) - before;
    evidence.maximum_gain_cp = std::max(evidence.maximum_gain_cp, last_delta);
    evidence.maximum_loss_cp = std::max(evidence.maximum_loss_cp, -last_delta);
  }
  evidence.final_delta_cp = last_delta;
  return evidence;
}

bool material_sacrifice_in_pv(
    const std::string& fen_before,
    const std::vector<std::string>& principal_variation,
    const int max_plies) {
  if (principal_variation.size() < 2 || max_plies < 2) return false;
  const auto evidence = pv_material_evidence(
      fen_before, principal_variation, max_plies);
  return evidence.maximum_loss_cp >= 250;
}

PositionContext position_context(const std::string& fen) {
  initialize_stockfish_runtime();
  std::deque<Stockfish::StateInfo> states(1);
  Stockfish::Position position;
  position.set(fen, false, &states.back());
  return {
      .legal_move_count = static_cast<int>(
          Stockfish::MoveList<Stockfish::LEGAL>(position).size()),
      .in_check = static_cast<bool>(position.checkers()),
  };
}

}  // namespace kchess
