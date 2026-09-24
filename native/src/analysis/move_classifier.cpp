#include "analysis/move_classifier.h"

#include <algorithm>
#include <array>
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

template<Stockfish::PieceType Type>
int material_difference(
    const Stockfish::Position& position,
    const Stockfish::Color perspective) {
  return piece_value_cp(Stockfish::make_piece(perspective, Type))
      * (position.count<Type>(perspective) - position.count<Type>(~perspective));
}

int material_balance(
    const Stockfish::Position& position,
    const Stockfish::Color perspective) {
  // Stockfish maintains these counts through captures, promotions and undo.
  // Reuse them instead of scanning 64 squares after every PV move. Kings have
  // zero material value, exactly as in the previous square-based sum.
  return material_difference<Stockfish::PAWN>(position, perspective)
      + material_difference<Stockfish::KNIGHT>(position, perspective)
      + material_difference<Stockfish::BISHOP>(position, perspective)
      + material_difference<Stockfish::ROOK>(position, perspective)
      + material_difference<Stockfish::QUEEN>(position, perspective);
}

Stockfish::Move find_uci_move(
    const Stockfish::Position& position,
    const std::string& uci) {
  for (const auto move : Stockfish::MoveList<Stockfish::LEGAL>(position)) {
    if (Stockfish::UCIEngine::move(move, false) == uci) return move;
  }
  return Stockfish::Move::none();
}

bool equivalent_to_best(
    const MoveAnalysisFacts& input,
    const MoveClassifierConfig& config,
    const std::optional<double>& loss,
    const std::optional<int>& cp_loss) {
  if (input.played_is_best) return true;

  if (input.best_mate_in.has_value() || input.played_mate_in.has_value()) {
    if (!mate_distance_equivalent(input.best_mate_in, input.played_mate_in)) return false;
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

std::optional<int> mate_distance_loss(
    const std::optional<int>& best_mate_in,
    const std::optional<int>& played_mate_in) {
  if (!best_mate_in.has_value() || !played_mate_in.has_value()) {
    return std::nullopt;
  }
  if (*best_mate_in == 0 || *played_mate_in == 0) return std::nullopt;
  if ((*best_mate_in > 0) != (*played_mate_in > 0)) return std::nullopt;

  if (*best_mate_in > 0) {
    // When winning, reaching mate sooner is better. A shorter played mate than
    // the captured rank-1 distance is treated as no loss (snapshot noise).
    return std::max(0, *played_mate_in - *best_mate_in);
  }

  // When already being mated, surviving longer is better. -M10 is therefore
  // better than -M3 from the mover's perspective.
  return std::max(0, std::abs(*best_mate_in) - std::abs(*played_mate_in));
}

bool mate_distance_equivalent(
    const std::optional<int>& best_mate_in,
    const std::optional<int>& played_mate_in,
    const int tolerance) {
  if (!best_mate_in.has_value() && !played_mate_in.has_value()) return true;
  const auto loss = mate_distance_loss(best_mate_in, played_mate_in);
  return loss.has_value() && *loss <= std::max(0, tolerance);
}

MoveSeverity classify_move_severity(
    const std::optional<double>& best_expected,
    const std::optional<double>& played_expected,
    const std::optional<int>& best_evaluation_cp,
    const std::optional<int>& played_evaluation_cp,
    const MoveSeverityConfig& config) {
  const auto loss = expected_score_loss(best_expected, played_expected);
  if (loss.has_value()) {
    const bool catastrophic = *loss >= config.blunder_expected_loss;
    const bool result_band_collapse = valid_expected(best_expected)
        && valid_expected(played_expected)
        && *best_expected >= config.result_band_best_floor
        && *played_expected < config.result_band_played_ceiling
        && *loss >= config.result_band_blunder_loss;
    const bool collapsed_to_lost = valid_expected(best_expected)
        && valid_expected(played_expected)
        && *best_expected >= config.collapse_best_floor
        && *played_expected <= config.collapse_played_ceiling
        && *loss >= config.result_band_blunder_loss;
    if (catastrophic || result_band_collapse || collapsed_to_lost) {
      return MoveSeverity::blunder;
    }
    if (*loss >= config.mistake_expected_loss) {
      return MoveSeverity::mistake;
    }
    if (*loss >= config.inaccuracy_expected_loss) {
      return MoveSeverity::inaccuracy;
    }
    return MoveSeverity::none;
  }

  // Compatibility fallback only. If normalized outcome data exists, a large
  // raw CP swing is deliberately ignored for severity because saturated won or
  // lost positions can have huge CP differences without a comparable result
  // change.
  const auto cp_loss = nonnegative_cp_difference(
      best_evaluation_cp, played_evaluation_cp);
  if (!cp_loss.has_value()) return MoveSeverity::none;
  if (*cp_loss >= config.fallback_blunder_cp_loss) return MoveSeverity::blunder;
  if (*cp_loss >= config.fallback_mistake_cp_loss) return MoveSeverity::mistake;
  if (*cp_loss >= config.fallback_inaccuracy_cp_loss) return MoveSeverity::inaccuracy;
  return MoveSeverity::none;
}

// -----------------------------------------------------------------------------
// Section: Move classification
// -----------------------------------------------------------------------------

MoveCategory classify_move(
    const MoveAnalysisFacts& input,
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
      && mate_distance_equivalent(input.best_mate_in, input.second_best_mate_in);
  const bool second_result_band_drop = has_second_expected
      && result_band_drop(
          *input.best_expected_score, *input.second_best_expected_score);
  const bool has_second_cp = second_cp_gap.has_value();
  const bool expected_separates = has_second_expected
      && second_expected_gap >= config.critical_gap;
  const bool cp_separates = has_second_cp
      && *second_cp_gap >= config.critical_cp_gap;
  const auto second_mate_distance_loss = mate_distance_loss(
      input.best_mate_in, input.second_best_mate_in);
  const bool mate_distance_separates = second_mate_distance_loss.has_value()
      && *second_mate_distance_loss >= 4;
  // Great/Critical is an isolated rank-1 move, not merely "rank 1 exists".
  // If both normalized outcome value and CP are available they must agree that
  // rank 2 belongs to a worse cluster. This avoids inflated Great labels in
  // saturated won/lost positions while retaining a 0.5-pawn CP fallback for
  // legacy/incomplete WDL samples.
  const bool numeric_cluster_separation = has_second_expected && has_second_cp
      ? (expected_separates && cp_separates)
      : (expected_separates || cp_separates);
  const bool critical_gap = !second_is_equivalent
      && (unique_forced_mate
          || mate_distance_separates
          || second_result_band_drop
          || numeric_cluster_separation);

  if (candidate_best) {
    const bool direct_piece_sacrifice = input.sacrifice_against_best_defense
        && input.sacrifice_piece_value_cp >= config.brilliant_piece_min_cp
        && input.sacrifice_net_material_loss_cp
            >= config.brilliant_net_sacrifice_min_cp;
    const bool ignored_piece_exposure = input.material_exposure_present
        && input.material_exposure_piece_value_cp >= config.brilliant_piece_min_cp
        && input.material_exposure_net_loss_cp
            >= config.brilliant_net_sacrifice_min_cp;
    const bool real_material_concession = direct_piece_sacrifice
        || ignored_piece_exposure;
    const bool pv_compensation = input.played_material_fully_recovered
        || input.played_material_net_gain_after_pv_cp > 0
        || input.played_material_concession_cp
            <= input.played_material_recovered_cp;
    const bool sound_advantage = played_forces_mate
        || (valid_expected(input.played_expected_score)
            && *input.played_expected_score >= config.brilliant_min_best_score)
        || (input.played_evaluation_cp.has_value()
            && *input.played_evaluation_cp >= config.brilliant_min_best_cp);
    // A best/near-best move that consciously leaves a >= minor-piece tactical
    // capture available is itself compensation evidence: Stockfish has already
    // judged that accepting the material does not refute the move. Deeper PV
    // recovery/gain strengthens the proof when the best line actually enters
    // the material sequence.
    const bool compensated_concession = real_material_concession
        && (sound_advantage || pv_compensation);
    const bool already_completely_decided = !played_forces_mate
        && valid_expected(input.best_expected_score)
        && *input.best_expected_score >= config.brilliant_decided_score_ceiling;
    if (!input.was_in_check_before_move
        && !already_completely_decided
        && input.legal_move_count > 1
        && compensated_concession
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

  // Centipawns and WDL saturate around forced mate. When both moves preserve
  // the same terminal result, classify precision by mate distance instead of
  // pretending M1, M3 and M10 are numerically identical.
  if (const auto mate_loss = mate_distance_loss(
          input.best_mate_in, input.played_mate_in);
      mate_loss.has_value()) {
    if (*mate_loss <= 1) return MoveCategory::best;
    if (*mate_loss <= 3) return MoveCategory::excellent;
    if (*mate_loss <= 6) return MoveCategory::good;
    return MoveCategory::okay;
  }

  const auto severity = classify_move_severity(
      input.best_expected_score,
      input.played_expected_score,
      input.best_evaluation_cp,
      input.played_evaluation_cp);
  if (severity == MoveSeverity::blunder) return MoveCategory::blunder;

  const int material_opportunity_gap = std::max(
      0, input.best_material_gain_cp - input.played_material_gain_cp);
  const bool had_real_advantage = best_forces_mate
      || (valid_expected(input.best_expected_score)
          ? *input.best_expected_score >= 0.58
          : (input.best_evaluation_cp.has_value()
              && *input.best_evaluation_cp >= config.miss_min_best_cp));
  const bool opportunity_was_lost =
      (loss.has_value()
          ? *loss >= config.miss_loss
          : (cp_loss.has_value() && *cp_loss >= config.miss_cp_loss))
      || material_opportunity_gap >= config.miss_material_opportunity_cp;
  const bool no_material_self_damage = input.material_loss_cp
      < config.mistake_material_loss_cp;

  const bool outside_top_five = input.played_rank >= 6;

  // Positive labels are quality clusters, not fixed rank numbers. Equivalent
  // alternatives were already promoted to Best above. Any known top-five move
  // just outside that band can be Excellent when its objective loss is still
  // light; lower positive classes then describe progressively wider clusters.
  const bool excellent_quality = within_expected_limit(loss, config.excellent_loss)
      && within_cp_limit(cp_loss, config.excellent_cp_loss);
  const bool good_quality = within_expected_limit(loss, config.good_loss)
      && within_cp_limit(cp_loss, config.good_cp_loss);
  const bool okay_quality = within_expected_limit(loss, config.okay_loss)
      && within_cp_limit(cp_loss, config.okay_cp_loss);
  const bool missed_material_tactic = material_opportunity_gap
      >= config.miss_material_opportunity_cp;
  const bool known_top_five = input.played_rank >= 2 && input.played_rank <= 5;

  if (!missed_material_tactic && known_top_five) {
    if (excellent_quality) return MoveCategory::excellent;
    if (good_quality) return MoveCategory::good;
    if (okay_quality) return MoveCategory::okay;
  }

  // A Miss is specifically "there was something significantly better and I
  // failed to cash it in". This covers +2 -> +1 without material self-damage,
  // missed tactical material, and winning continuations whose score drop is
  // too large to qualify for the positive top-five cluster labels above.
  if (had_real_advantage
      && opportunity_was_lost
      && no_material_self_damage) {
    return MoveCategory::miss;
  }

  if (severity == MoveSeverity::mistake) return MoveCategory::mistake;
  if (severity == MoveSeverity::inaccuracy) return MoveCategory::inaccuracy;

  // Fallback for old caches or positions where a reliable MultiPV rank was not
  // available. Equivalent moves were already promoted to Best above.
  if (excellent_quality) return MoveCategory::excellent;
  if (good_quality) return MoveCategory::good;
  if (okay_quality) return MoveCategory::okay;

  // Rank alone is never enough for a harsh label. The shared severity contract
  // above must prove a meaningful outcome deterioration first.
  (void)outside_top_five;

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
    case MoveCategory::inaccuracy: return "inaccuracy";
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

MaterialExposureEvidence root_move_material_exposure_evidence(
    const std::string& fen_before,
    const std::string& uci_move) {
  MaterialExposureEvidence evidence;
  if (uci_move.empty()) return evidence;

  initialize_stockfish_runtime();
  std::deque<Stockfish::StateInfo> states(1);
  Stockfish::Position position;
  position.set(fen_before, false, &states.back());
  const auto mover = position.side_to_move();

  const auto root_move = find_uci_move(position, uci_move);
  if (root_move == Stockfish::Move::none()) return evidence;

  const auto moved_to = root_move.to_sq();

  // Remember the pieces that existed before the move so an exposure of some
  // other piece can be distinguished from the piece that actually moved.
  std::array<Stockfish::Piece, 64> before_pieces{};
  for (int square = 0; square < 64; ++square) {
    before_pieces[square] = position.piece_on(Stockfish::Square(square));
  }

  states.emplace_back();
  position.do_move(root_move, states.back(), nullptr);

  // After the root move it is the opponent's turn, so legal captures are a
  // conservative definition of material that is genuinely attackable now.
  // For each capture, estimate the immediate net concession. If the capturing
  // piece can be recaptured at once, count only the favorable exchange margin;
  // otherwise count the full value of the victim. Deeper recovery belongs to
  // the PV compensation pass in Update 3.
  for (const auto reply : Stockfish::MoveList<Stockfish::LEGAL>(position)) {
    if (!position.capture(reply)) continue;

    const auto target = reply.to_sq();
    Stockfish::Piece victim = position.piece_on(target);
    int victim_value = piece_value_cp(victim);
    if (reply.type_of() == Stockfish::EN_PASSANT) {
      victim_value = 100;
    }
    if (victim_value <= 0) continue;
    if (victim != Stockfish::NO_PIECE && Stockfish::color_of(victim) != mover) {
      continue;
    }

    Stockfish::StateInfo reply_state;
    position.do_move(reply, reply_state, nullptr);

    const auto capturing_piece = position.piece_on(target);
    const int capturing_value = piece_value_cp(capturing_piece);
    bool can_recapture = false;
    for (const auto recapture : Stockfish::MoveList<Stockfish::LEGAL>(position)) {
      if (!position.capture(recapture)) continue;
      if (recapture.to_sq() == target) {
        can_recapture = true;
        break;
      }
    }
    position.undo_move(reply);

    const int estimated_net = can_recapture
        ? std::max(0, victim_value - capturing_value)
        : victim_value;
    if (estimated_net <= 0) continue;

    const bool stronger = !evidence.present
        || estimated_net > evidence.estimated_net_loss_cp
        || (estimated_net == evidence.estimated_net_loss_cp
            && victim_value > evidence.exposed_piece_value_cp);
    if (!stronger) continue;

    const int target_index = static_cast<int>(target);
    const auto before_piece = before_pieces[target_index];
    evidence.present = true;
    evidence.immediately_unprotected = !can_recapture;
    evidence.other_piece_than_mover = target != moved_to;
    evidence.piece_was_already_on_square = target != moved_to
        && before_piece != Stockfish::NO_PIECE
        && Stockfish::color_of(before_piece) == mover;
    evidence.exposed_piece_value_cp = victim_value;
    evidence.estimated_net_loss_cp = estimated_net;
    evidence.opponent_capture_uci = Stockfish::UCIEngine::move(reply, false);
  }

  return evidence;
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
    const int loss = std::max(0, -last_delta);
    if (loss > evidence.maximum_loss_cp) {
      evidence.maximum_loss_cp = loss;
      evidence.maximum_loss_ply = ply + 1;
    }
  }
  evidence.final_delta_cp = last_delta;
  evidence.final_net_loss_cp = std::max(0, -last_delta);
  evidence.final_net_gain_cp = std::max(0, last_delta);
  evidence.recovered_from_maximum_loss_cp = std::max(
      0, evidence.maximum_loss_cp + last_delta);
  evidence.fully_recovered = evidence.maximum_loss_cp > 0
      && evidence.final_delta_cp >= 0;
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
