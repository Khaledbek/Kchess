#include "game_relevance_scorer.h"

#include <algorithm>
#include <cmath>

namespace kchess::ai {
namespace {

double clamp01(const double value) { return std::clamp(value, 0.0, 1.0); }

double recency_weight(const std::int64_t played_at, const std::int64_t now_seconds) {
  if (played_at <= 0 || now_seconds <= played_at) return 1.0;
  const double age_days = static_cast<double>(now_seconds - played_at) / 86400.0;
  if (age_days < 30.0) return 1.0;
  if (age_days < 90.0) return 0.86;
  if (age_days < 180.0) return 0.68;
  if (age_days < 365.0) return 0.46;
  return 0.26;
}

}  // namespace

GameRelevanceScore GameRelevanceScorer::score(
    const ProfileGameEvidence& game,
    const std::int64_t now_seconds) const {
  GameRelevanceScore result;
  result.game_id = game.game_id;
  result.recency = recency_weight(game.played_at, now_seconds);

  const int analyzed = game.classifications.analyzed_moves;
  const int major_errors = game.classifications.miss + game.classifications.mistake +
                           game.classifications.blunder;
  const double major_error_rate = analyzed > 0
      ? static_cast<double>(major_errors) / static_cast<double>(analyzed)
      : 0.0;
  result.error_signal = clamp01(major_error_rate * 5.0);
  if (game.maximum_expected_score_loss.has_value()) {
    result.error_signal = std::max(
        result.error_signal, clamp01(*game.maximum_expected_score_loss / 0.35));
  }

  result.analysis_value = game.analysis_complete
      ? 1.0
      : (analyzed > 0 ? 0.7 : (game.accuracy.has_value() ? 0.45 : 0.15));

  result.diversity_value = 0.0;
  if (game.time_control != ProfileTimeControl::other) result.diversity_value += 0.3;
  if (!game.opening_eco.empty() || !game.opening_name.empty()) result.diversity_value += 0.25;
  if (game.provider_outcome == "loss") result.diversity_value += 0.2;
  else if (game.provider_outcome == "draw") result.diversity_value += 0.12;
  if (game.move_count >= 60) result.diversity_value += 0.25;
  result.diversity_value = clamp01(result.diversity_value);

  // Existing analysis receives a meaningful bonus because it gives the initial
  // profile high-value evidence without spending Stockfish time again.
  result.score = clamp01(
      0.34 * result.recency +
      0.30 * result.analysis_value +
      0.22 * result.error_signal +
      0.14 * result.diversity_value);
  return result;
}

}  // namespace kchess::ai
