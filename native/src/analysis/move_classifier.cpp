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
  if (!best.has_value() || !played.has_value()) return std::nullopt;
  return std::clamp(*best - *played, 0.0, 1.0);
}

MoveCategory classify_move(
    const MoveClassifierInput& input, const MoveClassifierConfig& config) {
  if (input.theory) return MoveCategory::theory;
  const auto loss = expected_score_loss(input.best_expected_score, input.played_expected_score);
  if (!loss.has_value()) return MoveCategory::unknown;

  const double best = *input.best_expected_score;
  const double played = *input.played_expected_score;
  const double gap = input.second_best_expected_score.has_value()
      ? std::max(0.0, best - *input.second_best_expected_score)
      : 0.0;
  const bool practical_best = input.played_is_best || *loss <= config.best_loss;
  const bool has_real_alternative = input.legal_move_count > 1
      && input.second_best_expected_score.has_value();

  // Brilliant is intentionally exceptional. A move made while in check is
  // never Brilliant: even a difficult king reply is a defensive obligation,
  // not a sacrifice award. It must be Stockfish's exact first choice, have a
  // genuine alternative, preserve meaningful chances, separate clearly from
  // the runner-up and contain a verified near-term material sacrifice.
  const bool brilliant_eligible = input.played_is_best
      && !input.was_in_check_before_move
      && has_real_alternative
      && best >= config.brilliant_min_best_score
      && played >= config.brilliant_min_best_score
      && gap >= config.brilliant_gap
      && input.material_sacrifice;
  if (brilliant_eligible) return MoveCategory::brilliant;

  // Critical means the position genuinely hinges on the choice. A plain best
  // move in a quiet position is still Best. We require a large MultiPV gap and
  // either a result-band drop for the next-best move or an exceptionally large
  // gap. Forced single replies are excluded. Multiple-reply check positions
  // may still be Critical, but never Brilliant.
  const bool second_drops_result_band = input.second_best_expected_score.has_value()
      && ((best >= 0.70 && *input.second_best_expected_score < 0.50)
          || (best >= 0.45 && *input.second_best_expected_score < 0.25));
  const bool critical_eligible = practical_best
      && has_real_alternative
      && best >= config.critical_min_best_score
      && gap >= config.critical_gap
      && (second_drops_result_band || gap >= 0.30);
  if (critical_eligible) return MoveCategory::critical;

  if (practical_best) return MoveCategory::best;

  // Miss is reserved for a concrete opportunity, not merely a generic bad
  // move from a good position. Missing a forced mate always qualifies. The
  // non-mate form requires a unique tactical best move (large MultiPV gap)
  // and losing most of the winning/drawing opportunity.
  const bool missed_tactical_opportunity = input.only_move_tactical
      && best >= config.miss_best_score
      && played <= config.miss_played_ceiling
      && *loss >= config.miss_loss;
  if (input.missed_forced_mate || missed_tactical_opportunity) {
    return MoveCategory::miss;
  }

  // Blunder is also conservative. In an already bad position (best expected
  // score below blunder_min_best_score), further deterioration is capped at
  // Mistake. Otherwise a move must either lose an enormous amount of expected
  // score or cross a major practical result band. This prevents games that are
  // already lost from accumulating a wall of Blunders.
  const bool had_meaningful_chances = best >= config.blunder_min_best_score;
  const bool outcome_collapse = (best >= 0.70 && played <= 0.45)
      || (best >= 0.45 && played <= 0.20);
  const bool severe_blunder = *loss >= config.blunder_severe_loss;
  const bool outcome_blunder = *loss >= config.blunder_outcome_loss && outcome_collapse;
  if (had_meaningful_chances
      && (severe_blunder || outcome_blunder
          || (input.allowed_forced_mate && *loss >= config.miss_loss))) {
    return MoveCategory::blunder;
  }

  if (*loss > config.okay_loss) return MoveCategory::mistake;
  if (*loss <= config.excellent_loss) return MoveCategory::excellent;
  if (*loss <= config.okay_loss) return MoveCategory::okay;
  return MoveCategory::unknown;
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
