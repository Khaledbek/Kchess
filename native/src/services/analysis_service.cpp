#include "services/analysis_service.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <unordered_set>
#include <vector>

#include <nlohmann/json.hpp>

#include "analysis/accuracy.h"
#include "analysis/move_classifier.h"
#include "analysis/move_classifier_sf19.h"
#include "chess/move.h"
#include "chess/position_view.h"
#include "core/settings_registry.h"
#include "diagnostics/logger.h"
#include "engine/stockfish_factory.h"

namespace kchess {
namespace {

// -----------------------------------------------------------------------------
// Section: Analysis policies and helpers
// -----------------------------------------------------------------------------

constexpr int kEngineThreadHardCap = 32;
constexpr int kLiveStableIterations = 3;
constexpr int kLiveStableEvalToleranceCp = 15;
constexpr int kPreanalysisStableIterations = 3;
constexpr int kPreanalysisStableEvalToleranceCp = 10;
constexpr int kClassificationMultiPv = 4;

bool uses_stockfish19_classifier(const std::string& engine_version) {
  return engine_version.rfind("Stockfish 19", 0) == 0;
}

int classifier_version_for_engine(const std::string& engine_version) {
  return uses_stockfish19_classifier(engine_version)
      ? MoveClassifierSf19Config::version
      : MoveClassifierConfig::version;
}

// -----------------------------------------------------------------------------
// Section: Stockfish 19 evaluation-bar projection
// -----------------------------------------------------------------------------

std::optional<int> sf19_evaluation_bar_white_permille(
    const EngineLine& line, const bool side_to_move_is_white) {
  if (!line.wdl.has_value()) return std::nullopt;

  const int white_wins = side_to_move_is_white
      ? line.wdl->wins
      : line.wdl->losses;
  const int white_draws = line.wdl->draws;
  const int white_losses = side_to_move_is_white
      ? line.wdl->losses
      : line.wdl->wins;
  const int total = white_wins + white_draws + white_losses;
  if (total <= 0) return std::nullopt;

  // SF19 already exposes a material-calibrated WDL model. Use its expected
  // White score for the bar geometry instead of the legacy linear cp scale.
  // SF18 deliberately keeps the old projection.
  const long long doubled_white_score =
      2LL * white_wins + static_cast<long long>(white_draws);
  const long long numerator = 1000LL * doubled_white_score;
  const long long denominator = 2LL * total;
  const int rounded = static_cast<int>((numerator + denominator / 2) / denominator);
  return std::clamp(rounded, 0, 1000);
}

constexpr double kPreanalysisBoundaryMargin = 0.02;
constexpr double kPreanalysisGapMargin = 0.04;

PositionEvaluation evaluation_of(const EngineLine& line);

bool near_threshold(const double value, const double threshold, const double margin) {
  return std::abs(value - threshold) <= margin;
}

bool difficult_preanalysis_position(
    const std::string& fen,
    const std::string& played_move,
    const AnalysisResult& result) {
  if (result.lines.empty()) return false;

  // If Stockfish's final bestmove differs from the last completed MultiPV-1
  // info line, the root is still classification-sensitive. Spend the full
  // preparation budget rather than publishing a harsh label from a mixed
  // iteration snapshot.
  if (!result.best_move.empty() && result.best_move != "(none)"
      && result.best_move != "0000"
      && !result.lines.front().best_move().empty()
      && result.lines.front().best_move() != result.best_move) {
    return true;
  }

  const MoveClassifierConfig classifier;
  const auto best_expected = expected_score_side_to_move(evaluation_of(result.lines.front()));
  std::optional<double> second_expected;
  if (result.lines.size() > 1) {
    second_expected = expected_score_side_to_move(evaluation_of(result.lines[1]));
  }

  const EngineLine* played_line = nullptr;
  for (const auto& line : result.lines) {
    if (line.best_move() == played_move) {
      played_line = &line;
      break;
    }
  }

  // Mate and verified sacrifices are exactly the positions where a shallow
  // convergence decision can damage Great/Brilliant/Miss classification.
  if (std::any_of(result.lines.begin(), result.lines.end(), [](const EngineLine& line) {
        return line.mate_in.has_value();
      })) {
    return true;
  }
  if (played_line != nullptr
      && material_sacrifice_in_pv(fen, played_line->moves, 8)) {
    return true;
  }

  if (!best_expected.has_value()) return false;

  if (second_expected.has_value()) {
    const double gap = std::max(0.0, *best_expected - *second_expected);
    if (gap >= classifier.brilliant_gap - kPreanalysisGapMargin
        || near_threshold(gap, classifier.critical_gap, kPreanalysisGapMargin)) {
      return true;
    }
  }
  if (result.lines.size() > 1
      && result.lines.front().evaluation_cp.has_value()
      && result.lines[1].evaluation_cp.has_value()) {
    const int cp_gap = std::max(
        0, *result.lines.front().evaluation_cp - *result.lines[1].evaluation_cp);
    if (cp_gap >= classifier.critical_cp_gap - 30) return true;
  }

  if (played_line == nullptr) {
    // The played move falling outside the scout candidates is already an
    // uncertainty signal: full configured MultiPV should verify it.
    return !played_move.empty();
  }

  const auto played_expected = expected_score_side_to_move(evaluation_of(*played_line));
  const auto loss = expected_score_loss(best_expected, played_expected);
  if (!loss.has_value()) return false;

  // Spend the full minimum budget near category boundaries, where a few
  // centipawns can change the user-visible label.
  return near_threshold(*loss, classifier.excellent_loss, kPreanalysisBoundaryMargin)
      || near_threshold(*loss, classifier.good_loss, kPreanalysisBoundaryMargin)
      || near_threshold(*loss, classifier.okay_loss, kPreanalysisBoundaryMargin)
      || near_threshold(*loss, classifier.miss_loss, kPreanalysisBoundaryMargin)
      || near_threshold(*loss, classifier.blunder_outcome_loss, kPreanalysisBoundaryMargin)
      || near_threshold(*loss, classifier.blunder_severe_loss, kPreanalysisBoundaryMargin);
}

int preanalysis_early_stop_min_depth(const AppSettings& settings) {
  // The user's minimum depth remains the hard ceiling for preparation. Quiet
  // positions may stop only after a substantial search and several stable
  // iterations. Depths below 12 are cheap enough that early-stop bookkeeping
  // is not worthwhile.
  if (settings.depth < 12) return settings.depth;
  return std::max(8, settings.depth - 6);
}

int maximum_worker_threads() {
  const unsigned hardware = std::thread::hardware_concurrency();
  if (hardware <= 1) return 1;
  // A single Stockfish worker may use at most half of the machine's logical
  // CPUs. The hard cap keeps the value inside Kchess' persisted setting range.
  const int half = std::max(1, static_cast<int>(hardware / 2U));
  return std::clamp(half, 1, kEngineThreadHardCap);
}

int live_refinement_threads(const AppSettings& settings) {
  return std::clamp(settings.threads, 1, maximum_worker_threads());
}

int live_early_stop_min_depth(const AppSettings& settings) {
  // Convergence needs one baseline iteration plus kLiveStableIterations
  // successful comparisons. Leave enough room for all of them to complete
  // strictly before the hard maximum depth. With the default 12..18 range,
  // this starts at depth 14, allowing stable depths 15, 16 and 17 to finish
  // the search early instead of making an early stop mathematically impossible.
  const int hard_max_depth = settings.depth;
  const int minimum_depth = std::min(settings.min_analysis_depth, hard_max_depth);
  const int latest_useful_start =
      hard_max_depth - kLiveStableIterations - 1;
  if (latest_useful_start < minimum_depth) {
    // There is not enough depth headroom to prove stability before max depth.
    // Starting at the maximum preserves the normal hard-depth behavior.
    return hard_max_depth;
  }

  const int preferred_start = std::max(16, settings.min_analysis_depth + 4);
  return std::clamp(preferred_start, minimum_depth, latest_useful_start);
}

std::int64_t unix_time_seconds() {
  return std::chrono::duration_cast<std::chrono::seconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

void validate_token(const std::string& value, const char* field) {
  if (value.empty() || value.size() > 128) {
    throw std::invalid_argument(std::string(field) + " must contain 1-128 characters");
  }
  if (std::any_of(value.begin(), value.end(), [](const unsigned char character) {
        return std::iscntrl(character) != 0;
      })) {
    throw std::invalid_argument(std::string(field) + " contains control characters");
  }
}

std::string escape_json(const std::string& input) {
  std::ostringstream output;
  for (const unsigned char character : input) {
    switch (character) {
      case '"': output << "\\\""; break;
      case '\\': output << "\\\\"; break;
      case '\b': output << "\\b"; break;
      case '\f': output << "\\f"; break;
      case '\n': output << "\\n"; break;
      case '\r': output << "\\r"; break;
      case '\t': output << "\\t"; break;
      default:
        if (character < 0x20) {
          output << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                 << static_cast<int>(character) << std::dec;
        } else {
          output << character;
        }
    }
  }
  return output.str();
}

void append_optional_int(std::ostringstream& json, const std::optional<int>& value) {
  if (value.has_value()) json << *value;
  else json << "null";
}

void append_optional_double(
    std::ostringstream& json, const std::optional<double>& value) {
  if (value.has_value()) json << std::fixed << std::setprecision(6) << *value;
  else json << "null";
}

std::string lowercase(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char character) {
    return static_cast<char>(std::tolower(character));
  });
  return value;
}

PositionEvaluation evaluation_of(const EngineLine& line) {
  return {
      .wdl = line.wdl,
      .evaluation_cp = line.evaluation_cp,
      .mate_in = line.mate_in,
  };
}

bool usable_engine_move(const std::string& move) {
  return !move.empty() && move != "(none)" && move != "0000";
}

const EngineLine* line_for_move(
    const std::vector<EngineLine>& lines, const std::string& move) {
  if (!usable_engine_move(move)) return nullptr;
  const auto found = std::find_if(lines.begin(), lines.end(), [&](const EngineLine& line) {
    return line.best_move() == move;
  });
  return found == lines.end() ? nullptr : &*found;
}

std::string ranked_best_move(
    const std::vector<EngineLine>& lines,
    const std::string& fallback) {
  // The published rank-1 PV is the atomic best-move truth for both SF18 and
  // SF19. A final bestmove callback can arrive after the last complete MultiPV
  // report and therefore belong to a newer root ordering than the scores stored
  // in `lines`. Mixing that callback with an older PV is exactly how the board
  // arrow and move classification could disagree. Prefer rank 1 whenever a
  // complete published line exists; the callback is only a terminal fallback.
  if (!lines.empty()) {
    const auto move = lines.front().best_move();
    if (usable_engine_move(move)) return move;
  }
  return usable_engine_move(fallback) ? fallback : std::string{};
}

const EngineLine* ranked_best_line(
    const std::vector<EngineLine>& lines, const std::string& final_best_move) {
  // If Stockfish emitted a usable final bestmove, only the line for that exact
  // move is evidence for Great/Brilliant/expected-loss calculations. Falling
  // back to a stale MultiPV-1 line here can pair the recommendation from one
  // iteration with the score of another move. In that rare incomplete-snapshot
  // case we keep the recommendation authoritative and let classification fall
  // back to plain Best rather than fabricating a severity/special label.
  if (usable_engine_move(final_best_move)) {
    return line_for_move(lines, final_best_move);
  }
  return lines.empty() ? nullptr : &lines.front();
}

const EngineLine* ranked_second_line(
    const std::vector<EngineLine>& lines, const std::string& best_move) {
  for (const auto& line : lines) {
    if (line.best_move().empty() || line.best_move() == best_move) continue;
    return &line;
  }
  return nullptr;
}

struct MoverEvaluationSample {
  std::optional<double> expected;
  std::optional<int> evaluation_cp;
  std::optional<int> mate_in;
  int depth{0};

  bool has_score() const {
    return expected.has_value() || evaluation_cp.has_value() || mate_in.has_value();
  }
};

MoverEvaluationSample root_sample(const EngineLine* line) {
  if (line == nullptr) return {};
  return {
      .expected = expected_score_side_to_move(evaluation_of(*line)),
      .evaluation_cp = line->evaluation_cp,
      .mate_in = line->mate_in,
      .depth = line->depth,
  };
}

MoverEvaluationSample after_sample(
    const std::vector<EngineLine>* after_lines,
    const std::string& fen_after,
    const int fallback_depth) {
  if (after_lines != nullptr && !after_lines->empty()) {
    const auto& line = after_lines->front();
    MoverEvaluationSample sample{
        .expected = expected_score_mover_after_move(evaluation_of(line)),
        .evaluation_cp = line.evaluation_cp.has_value()
            ? std::optional<int>(-*line.evaluation_cp)
            : std::nullopt,
        .mate_in = std::nullopt,
        .depth = line.depth,
    };
    if (line.mate_in.has_value()) {
      if (*line.mate_in == 0) {
        // The adapter uses mate=0 for a position where the opponent is already
        // checkmated. From the mover's perspective that is a completed mate.
        sample.mate_in = 1;
        sample.evaluation_cp.reset();
        if (sample.depth <= 0) sample.depth = fallback_depth;
      } else {
        sample.mate_in = -*line.mate_in;
      }
    } else if (line.depth == 0 && line.wdl.has_value()
               && line.wdl->draws > 0
               && line.wdl->wins == 0
               && line.wdl->losses == 0) {
      // Preserve the previous Accuracy V3 terminal-draw marker. Accuracy does
      // not consume WDL, so a stalemate needs an explicit neutral CP sample.
      sample.evaluation_cp = 0;
      sample.depth = fallback_depth;
    }
    return sample;
  }

  const auto context = position_context(fen_after);
  if (context.legal_move_count != 0) return {};
  if (context.in_check) {
    return {
        .expected = 1.0,
        .evaluation_cp = std::nullopt,
        .mate_in = 1,
        .depth = fallback_depth,
    };
  }
  return {
      .expected = 0.5,
      .evaluation_cp = 0,
      .mate_in = std::nullopt,
      .depth = fallback_depth,
  };
}

MoverEvaluationSample resolved_played_sample(
    const bool played_is_best,
    const MoverEvaluationSample& best,
    const MoverEvaluationSample& root,
    const MoverEvaluationSample& after) {
  // SF18 compatibility invariant: an engine-best move keeps the root quality
  // unless its caller explicitly rejects that bestmove first. SF19 performs
  // that stronger verification below before using this legacy resolver.
  if (played_is_best && best.has_score()) return best;
  if (!root.has_score()) return after;
  if (!after.has_score()) return root;

  // A same-root score is the cleanest apples-to-apples comparison. Trust the
  // resulting-position search only when it is materially deeper, or when it
  // proves a mate that the root line did not yet see.
  if (after.depth >= root.depth + AccuracyConfig{}.deeper_result_margin) return after;
  if (!root.mate_in.has_value() && after.mate_in.has_value()) return after;
  return root;
}

struct Sf19BestMoveVerification {
  bool contradicted{false};
  bool confirmed_for_brilliant{false};
};

Sf19BestMoveVerification verify_sf19_bestmove(
    const MoverEvaluationSample& best,
    const MoverEvaluationSample& after) {
  Sf19BestMoveVerification result;
  if (!best.has_score() || !after.has_score()) return result;

  const MoveClassifierSf19Config config;
  const auto best_value = accuracy_decision_value({
      .evaluation_cp = best.evaluation_cp,
      .mate_in = best.mate_in,
      .depth = best.depth,
  });
  const auto after_value = accuracy_decision_value({
      .evaluation_cp = after.evaluation_cp,
      .mate_in = after.mate_in,
      .depth = after.depth,
  });

  const std::optional<double> value_drop =
      best_value.has_value() && after_value.has_value()
      ? std::optional<double>(std::max(0.0, *best_value - *after_value))
      : std::nullopt;
  const std::optional<int> cp_drop =
      best.evaluation_cp.has_value() && after.evaluation_cp.has_value()
      ? std::optional<int>(std::max(0, *best.evaluation_cp - *after.evaluation_cp))
      : std::nullopt;

  const bool comparable_depth = after.depth > 0
      && after.depth + config.bestmove_verification_depth_slack >= best.depth;
  const bool credible_hard_check = after.depth >= config.bestmove_hard_min_after_depth;

  const bool newly_losing_mate = after.mate_in.has_value()
      && *after.mate_in < 0
      && (!best.mate_in.has_value() || *best.mate_in >= 0);
  const bool lost_forced_mate = best.mate_in.has_value()
      && *best.mate_in > 0
      && after.mate_in.has_value()
      && *after.mate_in <= 0;
  const bool mate_contradiction = newly_losing_mate || lost_forced_mate;

  const bool comparable_value_contradiction = comparable_depth
      && value_drop.has_value()
      && *value_drop >= config.bestmove_contradiction_value_loss;
  const bool comparable_cp_contradiction = comparable_depth
      && cp_drop.has_value()
      && *cp_drop >= config.bestmove_contradiction_cp_loss;
  const bool hard_value_contradiction = credible_hard_check
      && value_drop.has_value()
      && *value_drop >= config.bestmove_hard_contradiction_value_loss;
  const bool hard_cp_contradiction = credible_hard_check
      && cp_drop.has_value()
      && *cp_drop >= config.bestmove_hard_contradiction_cp_loss;

  result.contradicted = mate_contradiction
      || comparable_value_contradiction
      || comparable_cp_contradiction
      || hard_value_contradiction
      || hard_cp_contradiction;

  // Brilliant is a deliberately conservative special label. A sacrifice must
  // survive an independent after-position search that is reasonably close in
  // depth and agrees with the root evaluation. A missing or much shallower
  // confirmation can still be Best/Critical, but never Brilliant.
  const bool confirmation_depth = after.depth > 0
      && after.depth + config.brilliant_confirmation_depth_slack >= best.depth;
  const bool mate_compatible = !best.mate_in.has_value()
      || !after.mate_in.has_value()
      || ((*best.mate_in > 0) == (*after.mate_in > 0)
          && (*best.mate_in < 0) == (*after.mate_in < 0));
  const bool value_compatible = !value_drop.has_value()
      || *value_drop <= config.brilliant_confirmation_value_loss;
  const bool cp_compatible = !cp_drop.has_value()
      || *cp_drop <= config.brilliant_confirmation_cp_loss;
  const bool has_confirmation_score = value_drop.has_value()
      || cp_drop.has_value()
      || (best.mate_in.has_value() && after.mate_in.has_value());
  result.confirmed_for_brilliant = !result.contradicted
      && confirmation_depth
      && mate_compatible
      && value_compatible
      && cp_compatible
      && has_confirmation_score;
  return result;
}

std::string fnv1a_hex(const std::string& value) {
  std::uint64_t hash = 14695981039346656037ULL;
  for (const unsigned char character : value) {
    hash ^= character;
    hash *= 1099511628211ULL;
  }
  std::ostringstream result;
  result << std::hex << std::setw(16) << std::setfill('0') << hash;
  return result.str();
}

std::string position_cache_engine_identity(
    const ChessEngine& engine, const bool adaptive_early_stop) {
  // A cache entry is only reusable with the exact engine build, NNUE network(s)
  // and convergence policy. The engine adapter owns the evaluator identity so
  // Stockfish versions cannot accidentally share a cache namespace.
  return engine.cache_identity()
      + "|adaptive=" + (adaptive_early_stop ? "1" : "0");
}

std::string canonical_position_cache_fen(const std::string& fen) {
  // The fullmove counter does not affect the legal position or Stockfish's
  // search. Keep the halfmove clock because it affects the fifty-move rule.
  std::istringstream input(fen);
  std::vector<std::string> fields;
  std::string field;
  while (input >> field) fields.push_back(field);
  if (fields.size() < 5) return fen;
  std::ostringstream result;
  for (std::size_t index = 0; index < 5; ++index) {
    if (index != 0) result << ' ';
    result << fields[index];
  }
  return result.str();
}

std::string variation_position_cache_key(
    const std::string& fen,
    const AppSettings& settings,
    const std::string& engine_cache_identity) {
  // MultiPV is intentionally not part of the key. A wider result can satisfy
  // a later two-line classifier lookup; callers verify the available line
  // count before reusing it. Threads/hash do not change requested quality.
  // Engine/NNUE identity is included even though the map is also cleared on an
  // engine switch; this makes cross-engine reuse impossible by construction.
  return engine_cache_identity + "|" + canonical_position_cache_fen(fen)
      + "|depth=" + std::to_string(settings.depth)
      + "|time=" + std::to_string(settings.time_limit_seconds);
}

struct MoveAssessment {
  MoveCategory category{MoveCategory::unknown};
  std::string recommended_move_uci;
  std::optional<double> best_expected;
  std::optional<double> played_expected;
  std::optional<double> second_expected;
  std::optional<double> loss;
  AccuracyMove accuracy;
};

MoveAssessment assess_move(
    const std::string& fen_before,
    const std::string& played_move,
    const std::string& fen_after,
    const std::vector<EngineLine>* before_lines,
    const std::string* before_best_move,
    const std::vector<EngineLine>* after_lines,
    const TheoryMoveInfo& theory,
    const bool use_sf19_classifier) {
  MoveAssessment assessment;
  const auto context = position_context(fen_before);
  assessment.accuracy.theory = theory.is_theory;
  assessment.accuracy.legal_move_count = context.legal_move_count;

  const EngineLine* best_line = nullptr;
  const EngineLine* second_line = nullptr;
  const EngineLine* played_line = nullptr;
  if (before_lines != nullptr && !before_lines->empty()) {
    const std::string empty_best_move;
    const auto& final_best_move = before_best_move != nullptr
        ? *before_best_move
        : empty_best_move;
    assessment.recommended_move_uci = ranked_best_move(
        *before_lines, final_best_move);
    best_line = ranked_best_line(*before_lines, assessment.recommended_move_uci);
    second_line = ranked_second_line(*before_lines, assessment.recommended_move_uci);
    played_line = line_for_move(*before_lines, played_move);
  }

  // There is no meaningful ranking choice with one legal move. For SF19 the
  // final bestmove callback is normally authoritative, but it must not override
  // a completed post-move verification that proves a large opposite swing.
  // This matters when a late MultiPV reshuffle leaves the callback/root snapshot
  // inconsistent: without this guard a losing sacrifice could be labelled
  // Brilliant merely because its UCI move matched the stale final callback.
  const bool engine_marked_best = context.legal_move_count == 1
      || (usable_engine_move(assessment.recommended_move_uci)
          && played_move == assessment.recommended_move_uci);

  const auto best_sample = root_sample(best_line);
  const auto second_sample = root_sample(second_line);
  auto played_root_sample = root_sample(played_line);
  if (engine_marked_best && !played_root_sample.has_score()) {
    played_root_sample = best_sample;
  }
  const auto played_after_sample = after_sample(after_lines, fen_after, best_sample.depth);

  Sf19BestMoveVerification sf19_verification;
  if (use_sf19_classifier && engine_marked_best && context.legal_move_count > 1) {
    sf19_verification = verify_sf19_bestmove(best_sample, played_after_sample);
  }
  const bool sf19_bestmove_contradicted = use_sf19_classifier
      && sf19_verification.contradicted;

  const bool played_is_best = engine_marked_best && !sf19_bestmove_contradicted;
  assessment.accuracy.played_is_best = played_is_best;

  int played_rank = 0;
  if (played_is_best) {
    played_rank = 1;
  } else if (!sf19_bestmove_contradicted
             && played_line != nullptr && played_line->rank > 0) {
    played_rank = played_line->rank;
  } else if (before_lines != nullptr
             && static_cast<int>(before_lines->size()) >= kClassificationMultiPv) {
    // A completed four-line root search that does not contain the played move
    // proves only "outside top four"; do not fabricate an exact rank 5.
    played_rank = kClassificationMultiPv + 1;
  }

  const auto played_sample = sf19_bestmove_contradicted
      ? played_after_sample
      : resolved_played_sample(
          played_is_best, best_sample, played_root_sample, played_after_sample);

  assessment.best_expected = best_sample.expected;
  assessment.played_expected = played_sample.expected;
  assessment.second_expected = second_sample.expected;
  assessment.loss = expected_score_loss(
      assessment.best_expected, assessment.played_expected);

  VerifiedSacrifice sacrifice;
  if (played_line != nullptr && played_line->best_move() == played_move) {
    sacrifice = root_move_sacrifice_evidence(fen_before, played_line->moves);
  } else if (played_is_best && best_line != nullptr
             && best_line->best_move() == played_move) {
    sacrifice = root_move_sacrifice_evidence(fen_before, best_line->moves);
  }

  std::vector<std::string> played_pv;
  if (played_line != nullptr && played_line->best_move() == played_move) {
    played_pv = played_line->moves;
  } else if (!played_move.empty()) {
    played_pv.push_back(played_move);
    if (after_lines != nullptr && !after_lines->empty()) {
      const auto& continuation = after_lines->front().moves;
      played_pv.insert(played_pv.end(), continuation.begin(), continuation.end());
    }
  }
  const auto played_material = pv_material_evidence(fen_before, played_pv, 6);
  const auto best_material = best_line != nullptr
      ? pv_material_evidence(fen_before, best_line->moves, 6)
      : PvMaterialEvidence{};

  const MoveClassifierConfig classifier;
  bool unique_by_expected = false;
  if (best_sample.expected.has_value() && second_sample.expected.has_value()) {
    unique_by_expected = *best_sample.expected - *second_sample.expected
        >= classifier.miss_unique_gap;
  }
  const bool unique_by_cp = best_sample.evaluation_cp.has_value()
      && second_sample.evaluation_cp.has_value()
      && *best_sample.evaluation_cp - *second_sample.evaluation_cp
          >= classifier.critical_cp_gap;
  const bool unique_forced_mate = best_sample.mate_in.has_value()
      && *best_sample.mate_in > 0
      && (!second_sample.mate_in.has_value() || *second_sample.mate_in <= 0);
  const bool only_move_tactical = usable_engine_move(assessment.recommended_move_uci)
      && root_move_is_tactical(fen_before, assessment.recommended_move_uci)
      && (unique_by_expected || unique_by_cp || unique_forced_mate);

  const bool missed_forced_mate = !played_is_best
      && best_sample.mate_in.has_value() && *best_sample.mate_in > 0
      && (!played_sample.mate_in.has_value() || *played_sample.mate_in <= 0);
  const bool allowed_forced_mate = !played_is_best
      && played_sample.mate_in.has_value() && *played_sample.mate_in < 0
      && (!best_sample.mate_in.has_value() || *best_sample.mate_in >= 0);

  const MoveClassifierInput classifier_input{
      .theory = theory.is_theory,
      .played_is_best = played_is_best,
      .sacrifice_against_best_defense = sacrifice.verified,
      .only_move_tactical = only_move_tactical,
      .missed_forced_mate = missed_forced_mate,
      .allowed_forced_mate = allowed_forced_mate,
      .was_in_check_before_move = context.in_check,
      .legal_move_count = context.legal_move_count,
      .played_rank = played_rank,
      .sacrifice_piece_value_cp = sacrifice.offered_piece_value_cp,
      .sacrifice_net_material_loss_cp = sacrifice.net_material_loss_cp,
      .material_loss_cp = played_material.maximum_loss_cp,
      .best_material_gain_cp = best_material.maximum_gain_cp,
      .played_material_gain_cp = played_material.maximum_gain_cp,
      .best_expected_score = best_sample.expected,
      .played_expected_score = played_sample.expected,
      .second_best_expected_score = second_sample.expected,
      .best_evaluation_cp = best_sample.evaluation_cp,
      .played_evaluation_cp = played_sample.evaluation_cp,
      .second_best_evaluation_cp = second_sample.evaluation_cp,
      .best_mate_in = best_sample.mate_in,
      .played_mate_in = played_sample.mate_in,
      .second_best_mate_in = second_sample.mate_in,
  };
  if (use_sf19_classifier) {
    assessment.category = classify_move_sf19({
        .common = classifier_input,
        .best_move_verified_after = sf19_verification.confirmed_for_brilliant,
        .played_final_material_delta_cp = played_material.final_delta_cp,
        .best_final_material_delta_cp = best_material.final_delta_cp,
    });
  } else {
    // Stockfish 18 stays byte-for-byte on the established V11 classifier.
    assessment.category = classify_move(classifier_input);
  }

  assessment.accuracy.best = {
      .evaluation_cp = best_sample.evaluation_cp,
      .mate_in = best_sample.mate_in,
      .depth = best_sample.depth,
  };
  assessment.accuracy.played_root = {
      .evaluation_cp = played_root_sample.evaluation_cp,
      .mate_in = played_root_sample.mate_in,
      .depth = played_root_sample.depth,
  };
  assessment.accuracy.played_after = {
      .evaluation_cp = played_after_sample.evaluation_cp,
      .mate_in = played_after_sample.mate_in,
      .depth = played_after_sample.depth,
  };
  assessment.accuracy.second_best = {
      .evaluation_cp = second_sample.evaluation_cp,
      .mate_in = second_sample.mate_in,
      .depth = second_sample.depth,
  };
  return assessment;
}

MoveCategory classify_variation_move(
    const std::string& fen_before,
    const std::string& played_move,
    const std::string& fen_after,
    const AnalysisResult& before,
    const AnalysisResult& after,
    const TheoryMoveInfo& theory,
    const bool use_sf19_classifier) {
  return assess_move(
      fen_before, played_move, fen_after,
      &before.lines, &before.best_move, &after.lines, theory,
      use_sf19_classifier).category;
}

}  // namespace

AnalysisService::AnalysisService(
    Database& database, OpeningTheoryProvider& opening_theory)
    : database_(database), opening_theory_(&opening_theory) {}

AnalysisService::~AnalysisService() {
  std::vector<std::shared_ptr<AnalysisJob>> analysis_jobs;
  {
    std::lock_guard lock(jobs_mutex_);
    for (const auto& [game_id, job] : jobs_) {
      (void)game_id;
      job->cancel_requested = true;
      job->engine->cancel();
      analysis_jobs.push_back(job);
    }
  }
  for (const auto& job : analysis_jobs) {
    if (job->worker.joinable()) job->worker.join();
  }

  std::vector<std::shared_ptr<AnalysisJob>> refinement_jobs;
  {
    std::lock_guard lock(refinement_jobs_mutex_);
    for (const auto& [game_id, job] : refinement_jobs_) {
      (void)game_id;
      job->cancel_requested = true;
      job->engine->cancel();
      refinement_jobs.push_back(job);
    }
  }
  for (const auto& job : refinement_jobs) {
    if (job->worker.joinable()) job->worker.join();
  }

  stop_all_variation_jobs(true);
}

void AnalysisService::set_opening_theory_provider(
    OpeningTheoryProvider& opening_theory) noexcept {
  opening_theory_ = &opening_theory;
}

void AnalysisService::clear_engine_cache() {
  database_.clear_global_position_cache();
  diagnostics::info("cache", "global position cache cleared");
}

void AnalysisService::cancel_jobs_for_games(
    const std::vector<std::string>& game_ids) {
  std::vector<std::shared_ptr<AnalysisJob>> cancelled;
  {
    std::lock_guard lock(jobs_mutex_);
    for (const auto& game_id : game_ids) {
      const auto found = jobs_.find(game_id);
      if (found == jobs_.end()) continue;
      found->second->cancel_requested = true;
      found->second->engine->cancel();
      cancelled.push_back(found->second);
      jobs_.erase(found);
    }
  }
  for (const auto& job : cancelled) {
    if (job->worker.joinable()) job->worker.join();
  }

  std::vector<std::shared_ptr<AnalysisJob>> refinements;
  {
    std::lock_guard lock(refinement_jobs_mutex_);
    for (const auto& game_id : game_ids) {
      const auto found = refinement_jobs_.find(game_id);
      if (found == refinement_jobs_.end()) continue;
      found->second->cancel_requested = true;
      found->second->engine->cancel();
      refinements.push_back(found->second);
      refinement_jobs_.erase(found);
    }
  }
  for (const auto& job : refinements) {
    if (job->worker.joinable()) job->worker.join();
  }
}

AppSettings AnalysisService::preanalysis_budget_settings() const {
  auto budget = database_.settings();
  // min_analysis_depth is the maximum depth of the preparation pass.  Do not
  // copy the live maximum into budget.depth.  MultiPV, time, threads and hash
  // remain exactly as configured by the user.
  budget.depth = std::clamp(
      budget.min_analysis_depth, AppSettings::min_depth, AppSettings::max_depth);
  budget.threads = std::clamp(budget.threads, 1, maximum_worker_threads());
  return budget;
}

AnalysisRequest AnalysisService::analysis_request(
    const std::string& fen, const AppSettings& settings) const {
  return AnalysisRequest{
      .fen = fen,
      .depth = settings.depth,
      .multi_pv = settings.multi_pv,
      .threads = settings.threads,
      .hash_mb = settings.hash_mb,
      .time_limit_seconds = settings.time_limit_seconds,
      .search_moves = {},
  };
}

AnalysisRequest AnalysisService::preanalysis_request(
    const std::string& fen, const AppSettings& budget) const {
  // Kept separate from live refinement on purpose. Later scheduler stages may
  // choose cheaper per-position requests, but no pre-analysis request may ever
  // exceed this user-selected budget. SF19 preparation additionally requires
  // one complete exact MultiPV iteration before a position can be persisted.
  // Live refinement is audited separately. SF19 sideline roots opt into the
  // same exact-snapshot contract explicitly in start_variation_job_json(); the
  // SF18 adapter ignores the flag entirely.
  auto request = analysis_request(fen, budget);
  request.require_exact_multipv_snapshot =
      normalize_stockfish_engine_id(budget.engine_id) == kStockfish19Id;
  return request;
}

std::string AnalysisService::analysis_config_hash(const AppSettings& settings) const {
  const auto engine = create_stockfish_engine(settings.engine_id);
  // Classifier/Accuracy/Book versions remain independent from the engine
  // settings and are checked by rebuild_classification().
  std::string config = engine->version();
  // Only settings marked cache_relevant participate. Visual/app-only settings
  // therefore never force an expensive Stockfish re-analysis.
  for (const auto& descriptor : kSettingsRegistry) {
    if (!descriptor.cache_relevant) continue;
    config += "|" + std::string(descriptor.cache_token) + "=";
    if (descriptor.key == kDepthSetting.key) {
      config += std::to_string(settings.depth);
    } else if (descriptor.key == kMultiPvSetting.key) {
      config += std::to_string(settings.multi_pv);
    } else if (descriptor.key == kTimeLimitSetting.key) {
      config += std::to_string(settings.time_limit_seconds);
    } else if (descriptor.key == kThreadsSetting.key) {
      config += std::to_string(settings.threads);
    } else if (descriptor.key == kHashMbSetting.key) {
      config += std::to_string(settings.hash_mb);
    } else if (descriptor.key == kAdaptiveEarlyStopSetting.key) {
      config += settings.adaptive_early_stop ? "1" : "0";
    }
  }
  // Position model v2 stores the engine result for the position BEFORE a
  // move at the same public ply index, plus one synthetic final-position slot.
  // Including this token prevents legacy after-move caches from being reused.
  config += "|positionModel=before-after-v2";
  config += "|classification=unknown-phase2";
  return "phase7-" + fnv1a_hex(config);
}

std::optional<PersistedAnalysis> AnalysisService::reusable_analysis(
    const std::string& game_id,
    const AppSettings& settings,
    const int requested_ply) const {
  // A deeper/higher-quality saved run is authoritative for lower requests.
  // Resource-only changes (threads/hash) must not duplicate a game's saved
  // analysis.  Database compatibility still enforces engine version, depth,
  // MultiPV, time budget and strict-vs-adaptive semantics.
  const auto engine = create_stockfish_engine(settings.engine_id);
  return database_.compatible_analysis(
      game_id, engine->version(), settings, requested_ply);
}

const char* AnalysisService::job_state_name(const AnalysisJobState state) noexcept {
  switch (state) {
    case AnalysisJobState::queued: return "queued";
    case AnalysisJobState::running: return "running";
    case AnalysisJobState::cancelling: return "cancelling";
    case AnalysisJobState::cancelled: return "cancelled";
    case AnalysisJobState::completed: return "completed";
    case AnalysisJobState::failed: return "failed";
  }
  return "failed";
}

AnalysisService::AnalysisJobState AnalysisService::persisted_job_state(
    const PersistedAnalysis& analysis) noexcept {
  if (analysis.status == "complete") return AnalysisJobState::completed;
  if (analysis.status == "cancelled") return AnalysisJobState::cancelled;
  if (analysis.status == "error") return AnalysisJobState::failed;
  return AnalysisJobState::running;
}

PersistedAnalysis AnalysisService::stable_live_classification_snapshot(
    const std::string& game_id,
    const int ply,
    PersistedAnalysis analysis,
    const AnalysisJob* live_job) const {
  // Maximum-depth engine rows are persisted position-by-position so the eval
  // bar/PV can update live.  Classifications, however, are a game-wide
  // coherent snapshot because adjacent positions affect each move.  Until
  // the refinement queue finishes and rebuild_classification() commits the
  // complete maximum-depth snapshot, keep showing the last published
  // pre-analysis classification instead of exposing partial/null categories.
  if (live_job == nullptr
      || live_job->state.load() == AnalysisJobState::completed
      || live_job->published_classification_config_hash.empty()) {
    return analysis;
  }

  const auto published = database_.analysis(
      game_id, live_job->published_classification_config_hash, ply);
  if (!published.has_value()) return analysis;

  analysis.classification = published->classification;
  analysis.classifier_version = published->classifier_version;
  analysis.recommended_move = published->recommended_move;
  analysis.expected_score_before = published->expected_score_before;
  analysis.expected_score_best = published->expected_score_best;
  analysis.expected_score_played = published->expected_score_played;
  analysis.expected_score_loss = published->expected_score_loss;
  analysis.theory = published->theory;

  // Keep the live run's engine-depth metadata, but freeze all classification
  // counters/accuracy values to the same published snapshot as the move label.
  const int live_engine_depth = analysis.summary.engine_depth;
  analysis.summary = published->summary;
  analysis.summary.engine_depth = live_engine_depth;
  return analysis;
}

std::string AnalysisService::analysis_json(
    const std::string& game_id, const PersistedAnalysis& analysis,
    const AnalysisJob* live_job) const {
  std::ostringstream json;
  const auto job_state = live_job != nullptr
      ? live_job->state.load()
      : persisted_job_state(analysis);
  const int live_current_slot = live_job != nullptr
      ? live_job->current_position_slot.load()
      : analysis.latest_ply;
  const int live_completed_moves = live_job != nullptr
      ? std::max(analysis.completed_plies, live_job->completed_moves.load())
      : analysis.completed_plies;

  json << "{\"gameId\":\"" << escape_json(game_id) << "\",\"status\":\""
       << escape_json(analysis.status) << "\",\"jobState\":\""
       << job_state_name(job_state) << "\",\"completedPlies\":"
       << live_completed_moves << ",\"totalPlies\":" << analysis.total_plies
       << ",\"progress\":";
  if (analysis.total_plies <= 0) json << '0';
  else json << std::fixed << std::setprecision(3)
            << static_cast<double>(live_completed_moves) / analysis.total_plies;
  const int live_depth = live_job != nullptr && live_job->engine != nullptr
      ? live_job->engine->current_depth()
      : 0;
  bool quality_complete = false;
  try {
    const auto current_settings = database_.settings();
    const auto compatible = database_.compatible_analysis(
        game_id, analysis.engine_version, current_settings, analysis.latest_ply);
    quality_complete = analysis.latest_ply >= 0
        && compatible.has_value()
        && compatible->config_hash == analysis.config_hash
        && (!analysis.lines.empty()
            || (analysis.classification.has_value()
                && *analysis.classification == MoveCategory::theory));
  } catch (...) {
  }
  json << ",\"currentPly\":" << live_current_slot
       << ",\"liveDepth\":" << live_depth
       << ",\"qualityComplete\":" << (quality_complete ? "true" : "false")
       << ",\"bestMove\":\"" << escape_json(analysis.best_move)
       << "\",\"engineVersion\":\"" << escape_json(analysis.engine_version)
       << "\",\"configHash\":\"" << escape_json(analysis.config_hash)
       << "\",\"error\":";
  if (analysis.error.empty()) json << "null";
  else json << '"' << escape_json(analysis.error) << '"';
  json << ",\"recommendedMove\":\"" << escape_json(analysis.recommended_move)
       << "\",\"classification\":";
  if (analysis.classification.has_value()) {
    json << '"' << move_category_name(*analysis.classification) << '"';
  } else {
    json << "null";
  }
  json << ",\"classifierVersion\":" << analysis.classifier_version
       << ",\"expectedScoreBefore\":";
  append_optional_double(json, analysis.expected_score_before);
  json << ",\"expectedScoreBest\":";
  append_optional_double(json, analysis.expected_score_best);
  json << ",\"expectedScorePlayed\":";
  append_optional_double(json, analysis.expected_score_played);
  json << ",\"expectedScoreLoss\":";
  append_optional_double(json, analysis.expected_score_loss);
  json << ",\"theory\":";
  if (analysis.theory.has_value()) {
    json << "{\"games\":" << analysis.theory->games
         << ",\"whiteWins\":" << analysis.theory->white_wins
         << ",\"draws\":" << analysis.theory->draws
         << ",\"blackWins\":" << analysis.theory->black_wins << '}';
  } else {
    json << "null";
  }
  std::string analyzed_fen;
  if (const auto game = database_.game(game_id); game.has_value()) {
    analyzed_fen = game->starting_fen;
    if (!game->moves.empty() && analysis.latest_ply > 0) {
      const auto move_index = std::min(
          static_cast<std::size_t>(analysis.latest_ply - 1),
          game->moves.size() - 1);
      analyzed_fen = game->moves[move_index].fen_after;
    }
  }
  json << ",\"analyzedFen\":\"" << escape_json(analyzed_fen) << '\"';
  const bool line_is_white = analyzed_fen.empty() || white_to_move(analyzed_fen);
  json << ",\"lines\":[";
  const auto line_count = std::min(
      analysis.lines.size(), static_cast<std::size_t>(database_.settings().multi_pv));
  for (std::size_t index = 0; index < line_count; ++index) {
    if (index != 0) json << ',';
    const auto& line = analysis.lines[index];
    json << "{\"rank\":" << line.rank << ",\"depth\":" << line.depth
         << ",\"evaluationCp\":";
    append_optional_int(json, line.evaluation_cp.has_value()
        ? std::optional<int>(line_is_white ? *line.evaluation_cp : -*line.evaluation_cp)
        : std::nullopt);
    json << ",\"mateIn\":";
    append_optional_int(json, line.mate_in.has_value()
        ? std::optional<int>(line_is_white ? *line.mate_in : -*line.mate_in)
        : std::nullopt);
    json << ",\"wdl\":";
    if (line.wdl.has_value()) {
      json << "{\"wins\":" << (line_is_white ? line.wdl->wins : line.wdl->losses)
           << ",\"draws\":" << line.wdl->draws
           << ",\"losses\":" << (line_is_white ? line.wdl->losses : line.wdl->wins)
           << '}';
    } else {
      json << "null";
    }
    json << ",\"evaluationBarWhitePermille\":";
    append_optional_int(
        json, uses_stockfish19_classifier(analysis.engine_version)
            ? sf19_evaluation_bar_white_permille(line, line_is_white)
            : std::nullopt);
    json << ",\"nodes\":" << line.nodes << ",\"moves\":[";
    for (std::size_t move_index = 0; move_index < line.moves.size(); ++move_index) {
      if (move_index != 0) json << ',';
      json << '"' << escape_json(line.moves[move_index]) << '"';
    }
    json << "]}";
  }
  json << ']';
  {
    const auto& summary = analysis.summary;
    std::string profile_side = "unknown";
    const auto profile = database_.active_profile();
    const auto game = database_.game(game_id);
    if (profile.has_value() && game.has_value()) {
      const auto display_name = lowercase(profile->display_name);
      const auto provider_name = profile->provider_username.has_value()
          ? lowercase(*profile->provider_username) : std::string{};
      const auto matches = [&](const std::string& player_name) {
        const auto normalized = lowercase(player_name);
        return normalized == display_name
            || (!provider_name.empty() && normalized == provider_name);
      };
      if (matches(game->white_name) && !matches(game->black_name)) {
        profile_side = "white";
      } else if (matches(game->black_name) && !matches(game->white_name)) {
        profile_side = "black";
      }
    }
    auto append_player = [&](const PlayerAnalysisSummary& player) {
      json << "{\"theory\":" << player.theory
           << ",\"forced\":" << player.forced
           << ",\"brilliant\":" << player.brilliant << ",\"critical\":" << player.critical
           << ",\"best\":" << player.best
           << ",\"excellent\":" << player.excellent << ",\"good\":" << player.good
           << ",\"okay\":" << player.okay
           << ",\"miss\":" << player.miss << ",\"mistake\":" << player.mistake
           << ",\"blunder\":" << player.blunder << ",\"totalMoves\":"
           << player.total_moves << ",\"analyzedMoves\":" << player.analyzed_moves
           << ",\"localAccuracy\":";
      append_optional_double(json, player.local_accuracy);
      json << '}';
    };
    json << ",\"summary\":{\"profileSide\":\"" << profile_side
         << "\",\"classifierVersion\":" << summary.classifier_version
         << ",\"accuracyAlgorithmVersion\":" << summary.accuracy_algorithm_version
         << ",\"openingBookVersion\":\"" << escape_json(summary.opening_book_version)
         << "\",\"engineDepth\":" << summary.engine_depth
         << ",\"engineVersion\":\"" << escape_json(analysis.engine_version)
         << "\",\"white\":";
    append_player(summary.white);
    json << ",\"black\":";
    append_player(summary.black);
    json << '}';
  }
  json << '}';
  return json.str();
}

void AnalysisService::rebuild_classification(
    const std::string& game_id,
    const std::string& config_hash,
    const bool force,
    const int through_ply) {
  const std::string book_version = opening_theory_->source_version();
  const auto run_metadata = database_.analysis(game_id, config_hash);
  const std::string engine_version = run_metadata.has_value()
      ? run_metadata->engine_version
      : std::string{};
  const bool use_sf19_classifier = uses_stockfish19_classifier(engine_version);
  const int classifier_version = classifier_version_for_engine(engine_version);
  if (!force && database_.classification_is_current(
          game_id, config_hash, classifier_version,
          AccuracyConfig::version, book_version)) {
    return;
  }
  const auto game = database_.game(game_id);
  if (!game.has_value()) throw std::runtime_error("Game not found during classification");
  std::vector<MoveClassificationRecord> records;
  std::vector<AccuracyMove> white_moves;
  std::vector<AccuracyMove> black_moves;
  const auto book_metadata = opening_theory_->metadata();
  for (const auto& move : game->moves) {
    if (through_ply >= 0 && move.ply_index > through_ply) break;
    // Position slot i is the position before move i; slot i+1 is the
    // resulting position after that move.  This makes move 0 identical to
    // every other move and gives it a real Stockfish "before" evaluation.
    const auto before = database_.analysis(game_id, config_hash, move.ply_index);
    const auto after = database_.analysis(game_id, config_hash, move.ply_index + 1);
    TheoryMoveInfo theory;
    if (book_metadata.max_ply > 0
        && static_cast<std::uint32_t>(move.ply_index) < book_metadata.max_ply) {
      theory = opening_theory_->lookup(move.fen_before, move.uci);
    }
    const auto assessment = assess_move(
        move.fen_before,
        move.uci,
        move.fen_after,
        before.has_value() ? &before->lines : nullptr,
        before.has_value() ? &before->best_move : nullptr,
        after.has_value() ? &after->lines : nullptr,
        theory,
        use_sf19_classifier);

    std::string recommended_move = assessment.recommended_move_uci;
    if (usable_engine_move(recommended_move)) {
      recommended_move = apply_legal_uci_move(move.fen_before, recommended_move).san;
    } else {
      recommended_move.clear();
    }

    records.push_back({
        .ply = move.ply_index,
        .classification = assessment.category,
        .classifier_version = classifier_version,
        .expected_score_before = assessment.best_expected,
        .expected_score_best = assessment.best_expected,
        .expected_score_played = assessment.played_expected,
        .expected_score_loss = assessment.loss,
        .recommended_move = recommended_move,
        .theory = theory,
    });
    {
      std::ostringstream log_message;
      log_message << "game=" << game_id << " ply=" << move.ply_index
                  << " category=" << move_category_name(assessment.category);
      if (assessment.loss.has_value()) {
        log_message << " loss=" << std::fixed << std::setprecision(4)
                    << *assessment.loss;
      }
      diagnostics::debug("classifier", log_message.str());
    }
    auto& accuracy_moves = move.side_to_move == "black" ? black_moves : white_moves;
    accuracy_moves.push_back(assessment.accuracy);
  }
  database_.persist_classifications(
      game_id, config_hash, records, game_accuracy(white_moves), game_accuracy(black_moves),
      classifier_version, AccuracyConfig::version, book_version,
      through_ply < 0);
}

void AnalysisService::run_analysis(
    const std::string& game_id,
    const std::string& config_hash,
    AppSettings settings,
    std::vector<std::string> positions,
    std::vector<int> completed_position_slots,
    const std::shared_ptr<AnalysisJob>& job,
    const int preferred_slot) noexcept {
  try {
    if (job->cancel_requested) {
      job->state = AnalysisJobState::cancelled;
      database_.set_analysis_status(game_id, config_hash, "cancelled");
      job->finished = true;
      return;
    }
    job->state = AnalysisJobState::running;
    {
      std::ostringstream log_message;
      log_message << "game=" << game_id << " started positions=" << positions.size()
                  << " resumedSlots=" << completed_position_slots.size()
                  << " preanalysisBudgetDepth=" << settings.depth
                  << " preanalysisBudgetMultiPv=" << settings.multi_pv
                  << " threads=" << settings.threads
                  << " hashMb=" << settings.hash_mb
                  << " time=" << settings.time_limit_seconds;
      diagnostics::info("analysis", log_message.str());
    }
    const std::string cache_engine_identity = position_cache_engine_identity(
        *job->engine, settings.adaptive_early_stop);
    bool engine_started = false;

    std::unordered_set<int> completed_slots(
        completed_position_slots.begin(), completed_position_slots.end());
    const bool is_fen_position_only = positions.size() == 1;

    // For PGN preparation, slot i is the position before move i. Keep the
    // actually played move beside that slot so the adaptive scheduler can
    // decide whether the cheap two-line root search already contains enough
    // information for classification.
    std::vector<std::string> played_move_by_slot(positions.size());
    if (!is_fen_position_only) {
      if (const auto game = database_.game(game_id); game.has_value()) {
        for (const auto& move : game->moves) {
          if (move.ply_index < 0
              || move.ply_index >= static_cast<int>(played_move_by_slot.size())) {
            continue;
          }
          played_move_by_slot[static_cast<std::size_t>(move.ply_index)] = move.uci;
        }
      }
    }

    auto contiguous_completed_moves = [&]() {
      if (is_fen_position_only) {
        return completed_slots.contains(0) ? 1 : 0;
      }
      // A PGN move i is fully analyzable only when both its pre-move slot i
      // and post-move slot i+1 exist. Therefore N completed moves require
      // the contiguous position prefix 0..N.
      int next_slot = 0;
      while (next_slot < static_cast<int>(positions.size())
             && completed_slots.contains(next_slot)) {
        ++next_slot;
      }
      return std::max(0, next_slot - 1);
    };

    job->completed_moves = contiguous_completed_moves();

    // Preparation-pass optimization: Theory moves need no Stockfish work of
    // their own. Skip only position slots whose adjacent moves are both
    // Theory, so the boundary position required by the first non-Theory move
    // is still analyzed normally. This is book-driven, never based on a fixed
    // opening move count, so an early amateur deviation immediately resumes
    // normal engine analysis.
    std::vector<int> theory_skipped_slots;
    if (!is_fen_position_only) {
      if (const auto game = database_.game(game_id); game.has_value()) {
        std::vector<bool> theory_moves(game->moves.size(), false);
        const auto metadata = opening_theory_->metadata();
        for (std::size_t index = 0; index < game->moves.size(); ++index) {
          const auto& move = game->moves[index];
          if (metadata.max_ply > 0
              && static_cast<std::uint32_t>(move.ply_index) < metadata.max_ply) {
            theory_moves[index] =
                opening_theory_->lookup(move.fen_before, move.uci).is_theory;
          }
        }
        for (int slot = 0; slot < static_cast<int>(positions.size()); ++slot) {
          const bool previous_requires_engine = slot > 0
              && !theory_moves[static_cast<std::size_t>(slot - 1)];
          const bool next_requires_engine = slot < static_cast<int>(theory_moves.size())
              && !theory_moves[static_cast<std::size_t>(slot)];
          if (!previous_requires_engine && !next_requires_engine) {
            theory_skipped_slots.push_back(slot);
          }
        }
      }
    }

    for (const int slot : theory_skipped_slots) {
      if (completed_slots.contains(slot)) continue;
      const std::string cache_fen = canonical_position_cache_fen(positions[slot]);
      AnalysisResult skipped_result;
      // Reuse a previous checkpoint when one exists, but do not run Stockfish
      // merely to populate an eval for a move that is classified as Theory.
      if (settings.use_global_analysis_cache) {
        if (auto checkpoint = database_.best_position_checkpoint(
                cache_fen, cache_engine_identity, settings.depth, settings.multi_pv);
            checkpoint.has_value()) {
          skipped_result = std::move(*checkpoint);
        }
      }
      completed_slots.insert(slot);
      // A Theory classification still needs an authoritative move_analysis row.
      // Without this empty placeholder persist_classifications() has nothing to
      // update, so the move disappears from both the UI badge and Theory totals.
      database_.persist_engine_result(
          game_id, config_hash, slot, contiguous_completed_moves(),
          skipped_result, unix_time_seconds());
    }
    job->completed_moves = contiguous_completed_moves();
    if (!theory_skipped_slots.empty()) {
      rebuild_classification(game_id, config_hash, true, job->completed_moves - 1);
    }

    std::vector<int> analysis_order;
    analysis_order.reserve(positions.size());
    const auto push_slot = [&](const int slot) {
      if (slot < 0 || slot >= static_cast<int>(positions.size())) return;
      if (std::find(analysis_order.begin(), analysis_order.end(), slot) == analysis_order.end()) {
        analysis_order.push_back(slot);
      }
    };
    if (preferred_slot >= 0) {
      push_slot(preferred_slot);
      push_slot(preferred_slot + 1);
    }
    for (int slot = 0; slot < static_cast<int>(positions.size()); ++slot) push_slot(slot);

    for (const int ply : analysis_order) {
      if (completed_slots.contains(ply)) continue;
      job->current_position_slot = ply;

      if (job->cancel_requested) {
        job->state = AnalysisJobState::cancelled;
        database_.set_analysis_status(game_id, config_hash, "cancelled");
        job->engine->stop();
        job->finished = true;
        return;
      }

      const std::string cache_fen = canonical_position_cache_fen(positions[ply]);
      const auto context = position_context(positions[ply]);
      auto classification_cache_settings = settings;
      if (context.legal_move_count > 0) {
        classification_cache_settings.multi_pv = std::max(
            settings.multi_pv,
            std::min(context.legal_move_count, kClassificationMultiPv));
      }
      std::optional<AnalysisResult> result;
      if (settings.use_global_analysis_cache) {
        result = database_.compatible_position_analysis(
            cache_fen, cache_engine_identity, classification_cache_settings);
      }
      if (result.has_value()) {
        diagnostics::debug(
            "cache", "game=" + game_id + " slot=" + std::to_string(ply) + " hit");
      } else {
        diagnostics::debug(
            "cache", "game=" + game_id + " slot=" + std::to_string(ply)
                + (settings.use_global_analysis_cache ? " miss" : " disabled"));
        if (context.legal_move_count == 0) {
          // Terminal checkmate/stalemate positions have no candidate move and
          // therefore need no Stockfish search. Persist an empty terminal
          // result so the final played move can still be classified from the
          // board state.
          result = AnalysisResult{};
        } else {
          if (!engine_started) {
            job->engine->start();
            job->engine->new_game();
            engine_started = true;
          }

          // Adaptive preparation stage 1: always obtain the best two root
          // alternatives for classification, even when the UI only displays one
          // PV. Extra classifier lines stay hidden from the UI and do not change
          // the user's visible MultiPV preference.
          auto scout_settings = settings;
          scout_settings.multi_pv = std::max(
              1, std::min(context.legal_move_count, 2));

          std::optional<AnalysisResult> scout;
          if (settings.use_global_analysis_cache) {
            scout = database_.compatible_position_analysis(
                cache_fen, cache_engine_identity, scout_settings);
          }
          if (!scout.has_value()) {
            auto scout_request = preanalysis_request(positions[ply], scout_settings);
            scout_request.multi_pv = scout_settings.multi_pv;
            if (scout_settings.depth >= 12) {
              scout_request.dynamic_early_stop = settings.adaptive_early_stop;
              scout_request.early_stop_min_depth =
                  preanalysis_early_stop_min_depth(scout_settings);
              scout_request.early_stop_stable_iterations =
                  kPreanalysisStableIterations;
              scout_request.early_stop_eval_tolerance_cp =
                  kPreanalysisStableEvalToleranceCp;
              scout_request.early_stop_require_cp_scores = true;
            }
            scout = job->engine->analyze(scout_request);
            if (scout->interrupted) {
              if (job->cancel_requested) {
                job->state = AnalysisJobState::cancelled;
                database_.set_analysis_status(game_id, config_hash, "cancelled");
                job->engine->stop();
                job->finished = true;
                return;
              }
              continue;
            }
            if (settings.use_global_analysis_cache) {
              database_.persist_position_analysis(
                  cache_fen, cache_engine_identity, scout_settings, *scout,
                  unix_time_seconds());
            }
          }

          bool needs_full_lines = false;
          const std::string played_move = ply >= 0
                  && ply < static_cast<int>(played_move_by_slot.size())
              ? played_move_by_slot[static_cast<std::size_t>(ply)]
              : std::string{};
          bool played_is_in_scout = !played_move.empty() && std::any_of(
              scout->lines.begin(), scout->lines.end(), [&](const EngineLine& line) {
                return line.best_move() == played_move;
              });

          // Adaptive preparation stage 2: when the played move is outside the
          // two scout candidates, do not immediately repeat the whole root
          // search with the user's larger MultiPV. Ask Stockfish to search only
          // the played root move at the scout's reached depth. This gives the
          // classifier a same-position score at a fraction of the cost while
          // preserving the existing full verification for genuinely difficult
          // tactical/boundary positions below.
          if (!played_move.empty() && !played_is_in_scout) {
            auto played_request = preanalysis_request(positions[ply], settings);
            played_request.depth = std::max(1, scout->reached_depth);
            played_request.multi_pv = 1;
            played_request.dynamic_early_stop = false;
            played_request.search_moves = {played_move};
            auto played_result = job->engine->analyze(played_request);
            if (played_result.interrupted) {
              if (job->cancel_requested) {
                job->state = AnalysisJobState::cancelled;
                database_.set_analysis_status(game_id, config_hash, "cancelled");
                job->engine->stop();
                job->finished = true;
                return;
              }
              continue;
            }

            const bool targeted_played_move = !played_result.lines.empty()
                && played_result.lines.front().best_move() == played_move;
            if (targeted_played_move) {
              auto played_line = std::move(played_result.lines.front());
              // searchmoves gives a score, not a trustworthy global rank. Keep
              // rank=0 until the dedicated top-four resolution search below.
              played_line.rank = 0;
              scout->lines.push_back(std::move(played_line));
              scout->nodes = std::max(scout->nodes, played_result.nodes);
              played_is_in_scout = true;
              diagnostics::debug(
                  "analysis", "game=" + game_id + " slot=" + std::to_string(ply)
                      + " targetedPlayedMove=1 move=" + played_move
                      + " depth=" + std::to_string(played_request.depth));
            } else {
              // A legal game move should always survive a searchmoves root
              // restriction. Fall back to the previous full-MultiPV path if
              // Stockfish ever cannot return that targeted line.
              needs_full_lines = true;
              diagnostics::debug(
                  "analysis", "game=" + game_id + " slot=" + std::to_string(ply)
                      + " targetedPlayedMove=0 fallbackFull=1 move=" + played_move);
            }
          }

          // Resolve ranks 3/4 explicitly when the played move was outside the
          // top-two scout. This is classification-only MultiPV: the UI still
          // renders at most settings.multi_pv lines. If the move is absent from
          // the top four, append its targeted score with rank 5 meaning "5+".
          if (!played_move.empty() && !std::any_of(
                  scout->lines.begin(), scout->lines.end(), [&](const EngineLine& line) {
                    return line.rank > 0 && line.best_move() == played_move;
                  })) {
            const int classification_multi_pv = std::max(
                1, std::min(context.legal_move_count, kClassificationMultiPv));
            if (classification_multi_pv > scout_settings.multi_pv) {
              const EngineLine* targeted_line = line_for_move(scout->lines, played_move);
              auto rank_settings = settings;
              rank_settings.depth = std::max(1, scout->reached_depth);
              rank_settings.multi_pv = classification_multi_pv;
              auto rank_request = preanalysis_request(positions[ply], rank_settings);
              rank_request.depth = rank_settings.depth;
              rank_request.multi_pv = rank_settings.multi_pv;
              rank_request.dynamic_early_stop = false;
              auto ranked = job->engine->analyze(rank_request);
              if (ranked.interrupted) {
                if (job->cancel_requested) {
                  job->state = AnalysisJobState::cancelled;
                  database_.set_analysis_status(game_id, config_hash, "cancelled");
                  job->engine->stop();
                  job->finished = true;
                  return;
                }
                continue;
              }
              const bool played_is_top_four = std::any_of(
                  ranked.lines.begin(), ranked.lines.end(), [&](const EngineLine& line) {
                    return line.best_move() == played_move;
                  });
              if (!played_is_top_four && targeted_line != nullptr) {
                auto outside_line = *targeted_line;
                outside_line.rank = kClassificationMultiPv + 1;
                ranked.lines.push_back(std::move(outside_line));
              }
              scout = std::move(ranked);
              if (settings.use_global_analysis_cache) {
                database_.persist_position_analysis(
                    cache_fen, cache_engine_identity, rank_settings, *scout,
                    unix_time_seconds());
              }
            }
          }

          const bool difficult_position = difficult_preanalysis_position(
              positions[ply], played_move, *scout);
          // Step 5: difficult classifications receive the user's full minimum
          // budget. This is intentionally selective: quiet, clear positions
          // keep the cheap scout result, while tactical/mate/sacrifice and
          // boundary cases are verified without early termination.
          const int required_classification_multi_pv = std::max(
              1, std::min(context.legal_move_count, kClassificationMultiPv));
          const bool needs_full_verification = difficult_position
              && (scout->converged_early
                  || static_cast<int>(scout->lines.size()) < required_classification_multi_pv
                  || scout->reached_depth < settings.depth);

          if (needs_full_lines || needs_full_verification) {
            auto full_settings = settings;
            full_settings.multi_pv = std::max(
                settings.multi_pv, required_classification_multi_pv);
            auto full_request = preanalysis_request(positions[ply], full_settings);
            full_request.multi_pv = std::max(
                1, std::min(full_request.multi_pv, context.legal_move_count));
            // Deliberately leave dynamic_early_stop disabled. The whole point
            // of this second pass is to settle a user-visible classification
            // that the cheap scout could not establish confidently.
            result = job->engine->analyze(full_request);
            if (result->interrupted) {
              if (job->cancel_requested) {
                job->state = AnalysisJobState::cancelled;
                database_.set_analysis_status(game_id, config_hash, "cancelled");
                job->engine->stop();
                job->finished = true;
                return;
              }
              continue;
            }
            if (settings.use_global_analysis_cache) {
              database_.persist_position_analysis(
                  cache_fen, cache_engine_identity, full_settings, *result,
                  unix_time_seconds());
            }
            diagnostics::debug(
                "analysis", "game=" + game_id + " slot=" + std::to_string(ply)
                    + " adaptiveLines=" + std::to_string(scout_settings.multi_pv)
                    + "->" + std::to_string(full_request.multi_pv)
                    + " difficult=" + (difficult_position ? "1" : "0"));
          } else {
            result = std::move(scout);
            diagnostics::debug(
                "analysis", "game=" + game_id + " slot=" + std::to_string(ply)
                    + " adaptiveLines=" + std::to_string(scout_settings.multi_pv)
                    + " difficult=" + (difficult_position ? "1" : "0"));
          }
        }
      }

      completed_slots.insert(ply);
      const int completed_moves = contiguous_completed_moves();
      job->completed_moves = completed_moves;
      database_.persist_engine_result(
          game_id, config_hash, ply, completed_moves, *result, unix_time_seconds());

      diagnostics::debug(
          "analysis", "game=" + game_id + " slot=" + std::to_string(ply)
              + " completedMoves=" + std::to_string(completed_moves));

      if (!is_fen_position_only && preferred_slot >= 0
          && completed_slots.contains(preferred_slot)
          && completed_slots.contains(preferred_slot + 1)) {
        rebuild_classification(game_id, config_hash, true, preferred_slot);
      } else if (!is_fen_position_only && completed_moves > 0) {
        rebuild_classification(
            game_id, config_hash, true, completed_moves - 1);
      }
    }
    rebuild_classification(game_id, config_hash, true);
    database_.set_analysis_status(game_id, config_hash, "complete");
    // This completed run supersedes every older/lower saved run for the game.
    // Position-cache entries remain available independently.
    database_.prune_game_analyses_except(game_id, config_hash);
    job->state = AnalysisJobState::completed;
    diagnostics::info("analysis", "game=" + game_id + " completed");
    job->current_position_slot = static_cast<int>(positions.size()) - 1;
    job->engine->stop();
    job->finished = true;
  } catch (const std::exception& error) {
    const bool cancelled = job->cancel_requested;
    if (cancelled) {
      diagnostics::info("analysis", "game=" + game_id + " cancelled");
    } else {
      diagnostics::error("analysis", "game=" + game_id + " failed: " + error.what());
    }
    job->state = cancelled ? AnalysisJobState::cancelled : AnalysisJobState::failed;
    try {
      database_.set_analysis_status(
          game_id, config_hash, cancelled ? "cancelled" : "error",
          cancelled ? std::string{} : error.what());
    } catch (...) {
    }
    job->engine->stop();
    job->finished = true;
  } catch (...) {
    diagnostics::error("analysis", "game=" + game_id + " failed: unknown error");
    job->state = job->cancel_requested
        ? AnalysisJobState::cancelled
        : AnalysisJobState::failed;
    try {
      database_.set_analysis_status(game_id, config_hash, "error", "Unknown engine error");
    } catch (...) {
    }
    job->engine->stop();
    job->finished = true;
  }
}


void AnalysisService::run_refinement_queue(
    const std::string& game_id,
    const std::string& config_hash,
    AppSettings settings,
    std::vector<std::string> positions,
    std::vector<int> completed_position_slots,
    const std::shared_ptr<AnalysisJob>& job) noexcept {
  try {
    job->state = AnalysisJobState::running;
    // There is exactly one live Stockfish instance. Respect the user-selected
    // Hash value here as well; the Engine settings must describe both
    // preparation and live refinement. Threads are still capped to a safe
    // per-worker share of the machine so the UI/OS remains responsive.
    settings.hash_mb = std::clamp(
        settings.hash_mb, kHashMbSetting.min_int, kHashMbSetting.max_int);
    settings.threads = live_refinement_threads(settings);

    const std::string cache_engine_identity = position_cache_engine_identity(
        *job->engine, settings.adaptive_early_stop);
    job->engine->start();
    job->engine->new_game();

    std::unordered_set<int> completed_slots(
        completed_position_slots.begin(), completed_position_slots.end());
    const bool is_fen_position_only = positions.size() == 1;

    auto contiguous_completed_moves = [&]() {
      if (is_fen_position_only) return completed_slots.contains(0) ? 1 : 0;
      int next_slot = 0;
      while (next_slot < static_cast<int>(positions.size())
             && completed_slots.contains(next_slot)) {
        ++next_slot;
      }
      return std::max(0, next_slot - 1);
    };
    job->completed_moves = contiguous_completed_moves();

    // Theory moves do not need maximum-depth engine work: the classifier
    // returns Theory directly. A position slot can be skipped only when every
    // adjacent move that depends on it is theory; this preserves the boundary
    // evaluation needed for the first non-theory move after the opening.
    std::vector<int> theory_skipped_slots;
    if (!is_fen_position_only) {
      if (const auto game = database_.game(game_id); game.has_value()) {
        std::vector<bool> theory_moves(game->moves.size(), false);
        const auto metadata = opening_theory_->metadata();
        for (std::size_t index = 0; index < game->moves.size(); ++index) {
          const auto& move = game->moves[index];
          if (metadata.max_ply > 0
              && static_cast<std::uint32_t>(move.ply_index) < metadata.max_ply) {
            theory_moves[index] =
                opening_theory_->lookup(move.fen_before, move.uci).is_theory;
          }
        }
        for (int slot = 0; slot < static_cast<int>(positions.size()); ++slot) {
          const bool previous_requires_engine = slot > 0
              && !theory_moves[static_cast<std::size_t>(slot - 1)];
          const bool next_requires_engine = slot < static_cast<int>(theory_moves.size())
              && !theory_moves[static_cast<std::size_t>(slot)];
          if (!previous_requires_engine && !next_requires_engine) {
            theory_skipped_slots.push_back(slot);
          }
        }
      }
    }

    for (const int slot : theory_skipped_slots) {
      if (!completed_slots.insert(slot).second) continue;
      // Copy the best already-calculated minimum/checkpoint result into this
      // maximum run when available. This costs no Stockfish time but keeps the
      // eval bar populated while the move itself remains Theory.
      const std::string cache_fen = canonical_position_cache_fen(positions[slot]);
      AnalysisResult skipped_result;
      if (auto checkpoint = database_.best_position_checkpoint(
              cache_fen, cache_engine_identity, settings.depth, settings.multi_pv);
          checkpoint.has_value()) {
        skipped_result = std::move(*checkpoint);
      }
      database_.persist_engine_result(
          game_id, config_hash, slot, contiguous_completed_moves(),
          skipped_result, unix_time_seconds());
    }
    job->completed_moves = contiguous_completed_moves();
    auto next_slot = [&]() -> int {
      if (positions.empty()) return -1;
      int focus = job->requested_position_slot.load();
      focus = std::clamp(focus, 0, static_cast<int>(positions.size()) - 1);

      // Priority: selected position, position after the selected move, all
      // following positions, then walk backwards. Recomputed after every
      // completed/interrupted search, so a new selection takes effect at once.
      if (!completed_slots.contains(focus)) return focus;
      if (focus + 1 < static_cast<int>(positions.size())
          && !completed_slots.contains(focus + 1)) {
        return focus + 1;
      }
      for (int slot = focus + 2; slot < static_cast<int>(positions.size()); ++slot) {
        if (!completed_slots.contains(slot)) return slot;
      }
      for (int slot = focus - 1; slot >= 0; --slot) {
        if (!completed_slots.contains(slot)) return slot;
      }
      return -1;
    };

    while (!job->cancel_requested) {
      const int slot = next_slot();
      if (slot < 0) break;

      job->current_position_slot = slot;
      const auto generation = job->target_generation.load();
      const std::string cache_fen = canonical_position_cache_fen(positions[slot]);
      std::optional<AnalysisResult> result;

      if (settings.use_global_analysis_cache) {
        result = database_.compatible_position_analysis(
            cache_fen, cache_engine_identity, settings);
      }

      if (!result.has_value()) {
        try {
          auto request = analysis_request(positions[slot], settings);
          request.dynamic_early_stop = settings.adaptive_early_stop;
          request.early_stop_min_depth = live_early_stop_min_depth(settings);
          request.early_stop_stable_iterations = kLiveStableIterations;
          request.early_stop_eval_tolerance_cp = kLiveStableEvalToleranceCp;
          result = job->engine->analyze(request);
        } catch (const std::exception&) {
          // A retarget can arrive before Stockfish emitted its first PV. That
          // is a normal preemption, not a failed analysis job.
          if (!job->cancel_requested && job->target_generation.load() != generation) {
            continue;
          }
          throw;
        }
      }

      if (!result.has_value()) continue;

      if (result->interrupted) {
        if (!result->lines.empty() && result->reached_depth > 0) {
          // Keep an interrupted search as a lightweight position checkpoint,
          // not as a completed maximum-analysis row. This preserves correct
          // completion semantics (including time-limited searches) while a
          // later visit can immediately show the best depth already reached.
          auto checkpoint_settings = settings;
          checkpoint_settings.depth = std::max(1, result->reached_depth);
          database_.persist_position_analysis(
              cache_fen, cache_engine_identity, checkpoint_settings,
              *result, unix_time_seconds());
        }
        if (job->cancel_requested) break;
        continue;
      }

      if (result->converged_early) {
        diagnostics::debug(
            "analysis", "game=" + game_id + " slot=" + std::to_string(slot)
                + " converged early at depth=" + std::to_string(result->reached_depth));
      }

      if (settings.use_global_analysis_cache) {
        database_.persist_position_analysis(
            cache_fen, cache_engine_identity, settings, *result, unix_time_seconds());
      }
      completed_slots.insert(slot);
      const int completed_moves = contiguous_completed_moves();
      job->completed_moves = completed_moves;
      database_.persist_engine_result(
          game_id, config_hash, slot, completed_moves, *result, unix_time_seconds());

      // Do not publish a partially recomputed classification here. Engine
      // results remain live, while move categories are published once, after
      // every position in this refinement queue has finished.
    }

    if (job->cancel_requested) {
      job->state = AnalysisJobState::cancelled;
      database_.set_analysis_status(game_id, config_hash, "cancelled");
      diagnostics::info("analysis", "game=" + game_id + " refinement worker cancelled");
    } else {
      rebuild_classification(game_id, config_hash, true);
      database_.set_analysis_status(game_id, config_hash, "complete");
      database_.prune_game_analyses_except(game_id, config_hash);
      job->state = AnalysisJobState::completed;
      diagnostics::info("analysis", "game=" + game_id + " refinement queue completed");
    }
  } catch (const std::exception& error) {
    const bool cancelled = job->cancel_requested;
    job->state = cancelled ? AnalysisJobState::cancelled : AnalysisJobState::failed;
    try {
      database_.set_analysis_status(
          game_id, config_hash, cancelled ? "cancelled" : "error",
          cancelled ? std::string{} : error.what());
    } catch (...) {
    }
    if (!cancelled) {
      diagnostics::error(
          "analysis", "game=" + game_id + " refinement worker failed: " + error.what());
    }
  } catch (...) {
    job->state = job->cancel_requested
        ? AnalysisJobState::cancelled
        : AnalysisJobState::failed;
    try {
      database_.set_analysis_status(
          game_id, config_hash, job->cancel_requested ? "cancelled" : "error",
          job->cancel_requested ? std::string{} : "Unknown refinement worker error");
    } catch (...) {
    }
  }

  job->current_position_slot = -1;
  job->engine->stop();
  job->finished = true;
}

std::string AnalysisService::start_analysis_json(const std::string& game_id) {
  validate_token(game_id, "game id");
  const auto game = database_.game(game_id);
  if (!game.has_value()) throw std::runtime_error("Game not found");
  std::vector<std::string> positions;
  if (game->kind == "fen") {
    positions.push_back(game->starting_fen);
  } else {
    // Slot 0 is the true position before the first move.  Each following
    // slot is the result after one played half-move, so move i is always
    // evaluated from slots i -> i+1.
    positions.reserve(game->moves.size() + 1);
    positions.push_back(game->starting_fen);
    for (const auto& move : game->moves) {
      positions.push_back(move.fen_after);
    }
  }
  if (positions.empty()) throw std::invalid_argument("Game has no analyzable positions");

  // A main-line analysis owns the Stockfish work slot. If a sideline engine
  // is still resident, release it before starting or resuming this worker.
  stop_all_variation_jobs(true);

  const auto settings = preanalysis_budget_settings();
  const auto version_probe = create_stockfish_engine(settings.engine_id);
  const auto config_hash = analysis_config_hash(settings);
  if (auto reusable = reusable_analysis(game_id, settings);
      reusable.has_value() && reusable->status == "complete") {
    diagnostics::info("cache", "game=" + game_id + " complete analysis reused");
    rebuild_classification(game_id, reusable->config_hash);
    database_.prune_game_analyses_except(game_id, reusable->config_hash);
    return analysis_json(game_id, database_.analysis(
        game_id, reusable->config_hash).value());
  }
  version_probe->validate_available();
  const int public_total_plies = game->kind == "fen"
      ? 1
      : static_cast<int>(game->moves.size());
  auto persisted = database_.prepare_analysis(
      game_id, config_hash, version_probe->version(), public_total_plies,
      settings.depth, settings.multi_pv, settings.time_limit_seconds,
      settings.adaptive_early_stop);
  if (persisted.status == "complete") {
    rebuild_classification(game_id, config_hash);
    return analysis_json(game_id, database_.analysis(game_id, config_hash).value());
  }

  {
    std::lock_guard lock(jobs_mutex_);
    auto existing = jobs_.find(game_id);
    if (existing != jobs_.end()
        && (existing->second->finished || existing->second->config_hash != config_hash)) {
      if (!existing->second->finished) {
        existing->second->cancel_requested = true;
        existing->second->engine->cancel();
      }
      if (existing->second->worker.joinable()) existing->second->worker.join();
      jobs_.erase(existing);
      existing = jobs_.end();
    }
    if (existing == jobs_.end()) {
      auto job = std::make_shared<AnalysisJob>();
      job->engine = create_stockfish_engine(settings.engine_id);
      job->config_hash = config_hash;
      // Resume is position-exact, not progress-counter based. Persisted
      // engine rows survive cancellation/errors, and only missing position
      // slots are sent to Stockfish on the next start.
      auto completed_position_slots =
          database_.analyzed_position_slots(game_id, config_hash);
      if (!completed_position_slots.empty()) {
        diagnostics::info(
            "analysis", "game=" + game_id + " resuming cachedSlots="
                + std::to_string(completed_position_slots.size()));
      }
      job->worker = std::thread(
          [this, game_id, config_hash, settings, positions = std::move(positions),
           completed_position_slots = std::move(completed_position_slots), job]() mutable {
            run_analysis(
                game_id, config_hash, settings, std::move(positions),
                std::move(completed_position_slots), job);
          });
      jobs_.emplace(game_id, std::move(job));
    }
  }
  return analysis_json(game_id, persisted);
}

std::string AnalysisService::analysis_status_json(const std::string& game_id) {
  validate_token(game_id, "game id");
  std::optional<PersistedAnalysis> result;
  std::shared_ptr<AnalysisJob> live_job;
  {
    std::lock_guard lock(jobs_mutex_);
    const auto job = jobs_.find(game_id);
    if (job != jobs_.end()) {
      live_job = job->second;
      result = database_.analysis(game_id, job->second->config_hash);
    }
  }
  if (!result.has_value()) {
    const auto minimum_settings = preanalysis_budget_settings();
    result = reusable_analysis(game_id, minimum_settings);
  }
  if (!result.has_value()) throw std::runtime_error("Analysis job not found");
  return analysis_json(game_id, *result, live_job.get());
}

std::string AnalysisService::move_analysis_status_json(const std::string& game_id, const int ply) {
  validate_token(game_id, "game id");
  if (ply < 0) throw std::invalid_argument("ply must be non-negative");

  // Prefer a maximum-quality live refinement when that position already has
  // a result. Keep the refinement job attached even before the first maximum
  // result is persisted, so callers continue polling and can see live depth.
  std::shared_ptr<AnalysisJob> refinement_job;
  {
    std::lock_guard lock(refinement_jobs_mutex_);
    const auto job = refinement_jobs_.find(game_id);
    if (job != refinement_jobs_.end()) {
      refinement_job = job->second;
      if (auto refined = database_.analysis(game_id, refinement_job->config_hash, ply);
          refined.has_value() && (!refined->lines.empty() || refined->latest_ply == ply)) {
        auto visible = stable_live_classification_snapshot(
            game_id, ply, std::move(*refined), refinement_job.get());
        return analysis_json(game_id, visible, refinement_job.get());
      }
    }
  }

  auto maximum_settings = database_.settings();
  if (auto refined = reusable_analysis(game_id, maximum_settings, ply);
      refined.has_value() && (!refined->lines.empty() || refined->latest_ply == ply)) {
    auto visible = stable_live_classification_snapshot(
        game_id, ply, std::move(*refined), refinement_job.get());
    return analysis_json(game_id, visible, refinement_job.get());
  }

  std::optional<PersistedAnalysis> result;
  std::shared_ptr<AnalysisJob> live_job;
  {
    std::lock_guard lock(jobs_mutex_);
    const auto job = jobs_.find(game_id);
    if (job != jobs_.end()) {
      live_job = job->second;
      result = database_.analysis(game_id, job->second->config_hash, ply);
    }
  }
  if (!result.has_value()) {
    const auto minimum_settings = preanalysis_budget_settings();
    result = reusable_analysis(game_id, minimum_settings, ply);
  }
  if (!result.has_value() && refinement_job != nullptr) {
    result = database_.analysis(game_id, refinement_job->config_hash, ply);
  }
  if (!result.has_value()) throw std::runtime_error("Analysis job not found");

  if (refinement_job != nullptr) {
    live_job = refinement_job;
    const auto game = database_.game(game_id);
    if (game.has_value()) {
      std::string fen;
      if (game->kind == "fen") {
        if (ply == 0) fen = game->starting_fen;
      } else if (ply == 0) {
        fen = game->starting_fen;
      } else if (ply <= static_cast<int>(game->moves.size())) {
        fen = game->moves[static_cast<std::size_t>(ply - 1)].fen_after;
      }
      if (!fen.empty()) {
        const auto requested = database_.settings();
        int visible_depth = result->lines.empty() ? 0 : result->lines.front().depth;

        // SF19 live refinement previously exposed only liveDepth while keeping
        // the old pre-analysis PV/best move on screen. That made an old arrow
        // look like it belonged to the deeper live search. Once SF19 has one
        // complete exact MultiPV iteration for this very slot, publish that
        // coherent snapshot immediately. SF18 deliberately keeps its existing
        // persisted-checkpoint behavior.
        if (refinement_job->engine->id() == "stockfish19"
            && refinement_job->state.load() == AnalysisJobState::running
            && refinement_job->current_position_slot.load() == ply) {
          auto live = refinement_job->engine->current_result();
          if (!live.lines.empty() && live.reached_depth > visible_depth) {
            result->best_move = live.best_move;
            result->lines = std::move(live.lines);
            result->latest_ply = ply;
            visible_depth = result->lines.front().depth;
          }
        }

        const auto checkpoint = database_.best_position_checkpoint(
            canonical_position_cache_fen(fen),
            position_cache_engine_identity(
                *refinement_job->engine, requested.adaptive_early_stop),
            requested.depth, requested.multi_pv);
        if (checkpoint.has_value() && !checkpoint->lines.empty()
            && checkpoint->lines.front().depth > visible_depth) {
          result->best_move = checkpoint->best_move;
          result->lines = checkpoint->lines;
          result->latest_ply = ply;
        }
      }
    }
  }
  if (refinement_job != nullptr && live_job == refinement_job) {
    *result = stable_live_classification_snapshot(
        game_id, ply, std::move(*result), refinement_job.get());
  }
  return analysis_json(game_id, *result, live_job.get());
}

std::string AnalysisService::start_move_refinement_json(
    const std::string& game_id, const int ply) {
  validate_token(game_id, "game id");
  if (ply < 0) throw std::invalid_argument("ply must be non-negative");
  const auto game = database_.game(game_id);
  if (!game.has_value()) throw std::runtime_error("Game not found");

  // Main-line refinement and sideline analysis share the same live-analysis
  // budget. Returning to the main line first stops the temporary sideline
  // search so Stockfish never competes with itself for CPU or RAM.
  stop_all_variation_jobs(true);

  auto settings = database_.settings();
  if (settings.depth <= settings.min_analysis_depth) {
    return move_analysis_status_json(game_id, ply);
  }

  std::vector<std::string> positions;
  if (game->kind == "fen") {
    positions.push_back(game->starting_fen);
  } else {
    positions.reserve(game->moves.size() + 1);
    positions.push_back(game->starting_fen);
    for (const auto& move : game->moves) {
      positions.push_back(move.fen_after);
    }
  }
  if (ply >= static_cast<int>(positions.size())) {
    throw std::invalid_argument("ply exceeds game position count");
  }

  const auto version_probe = create_stockfish_engine(settings.engine_id);
  version_probe->validate_available();
  if (auto reusable = reusable_analysis(game_id, settings, ply);
      reusable.has_value() && reusable->status == "complete") {
    rebuild_classification(game_id, reusable->config_hash);
    database_.prune_game_analyses_except(game_id, reusable->config_hash);
    return analysis_json(game_id, *reusable);
  }
  const auto config_hash = analysis_config_hash(settings);
  const int public_total_plies = game->kind == "fen"
      ? 1
      : static_cast<int>(game->moves.size());
  auto persisted = database_.prepare_analysis(
      game_id, config_hash, version_probe->version(), public_total_plies,
      settings.depth, settings.multi_pv, settings.time_limit_seconds,
      settings.adaptive_early_stop);

  // The minimum/background pass must never compete with live refinement for
  // CPU/RAM. The first user-driven refinement takes ownership of Stockfish.
  std::shared_ptr<AnalysisJob> minimum_job;
  {
    std::lock_guard lock(jobs_mutex_);
    const auto existing = jobs_.find(game_id);
    if (existing != jobs_.end()) {
      minimum_job = existing->second;
      if (!minimum_job->finished) {
        minimum_job->cancel_requested = true;
        minimum_job->engine->cancel();
      }
      jobs_.erase(existing);
    }
  }
  if (minimum_job && minimum_job->worker.joinable()) minimum_job->worker.join();

  // Only one live-refinement worker is allowed globally. Close a worker from
  // another game before activating this game. For the same game/config, keep
  // the existing worker and merely retarget it -- no thread/engine churn.
  std::vector<std::shared_ptr<AnalysisJob>> stale_jobs;
  std::shared_ptr<AnalysisJob> active;
  {
    std::lock_guard lock(refinement_jobs_mutex_);
    for (auto it = refinement_jobs_.begin(); it != refinement_jobs_.end();) {
      const bool same_job = it->first == game_id
          && it->second->config_hash == config_hash
          && !it->second->finished;
      if (same_job) {
        active = it->second;
        ++it;
        continue;
      }
      auto stale = it->second;
      if (!stale->finished) {
        stale->cancel_requested = true;
        stale->engine->cancel();
      }
      stale_jobs.push_back(stale);
      it = refinement_jobs_.erase(it);
    }
  }
  for (const auto& stale : stale_jobs) {
    if (stale->worker.joinable()) stale->worker.join();
  }

  if (active != nullptr) {
    active->requested_position_slot = ply;
    active->target_generation.fetch_add(1);
    // Preempt the old position immediately. The worker persists any partial
    // PV it already has and recomputes its queue around the newly selected ply.
    const int current = active->current_position_slot.load();
    if (current >= 0 && current != ply) active->engine->cancel();

    return move_analysis_status_json(game_id, ply);
  }

  // If the whole maximum run is already complete, there is no worker to start.
  if (persisted.status == "complete") {
    if (auto ready = database_.analysis(game_id, config_hash, ply); ready.has_value()) {
      return analysis_json(game_id, *ready);
    }
    return analysis_json(game_id, persisted);
  }

  auto job = std::make_shared<AnalysisJob>();
  job->engine = create_stockfish_engine(settings.engine_id);
  job->config_hash = config_hash;
  if (auto published = reusable_analysis(game_id, preanalysis_budget_settings());
      published.has_value()) {
    job->published_classification_config_hash = published->config_hash;
  } else {
    job->published_classification_config_hash =
        analysis_config_hash(preanalysis_budget_settings());
  }
  job->requested_position_slot = ply;
  job->target_generation = 1;
  auto completed_position_slots = database_.analyzed_position_slots(game_id, config_hash);
  job->worker = std::thread(
      [this, game_id, config_hash, settings, positions = std::move(positions),
       completed_position_slots = std::move(completed_position_slots), job]() mutable {
        run_refinement_queue(
            game_id, config_hash, settings, std::move(positions),
            std::move(completed_position_slots), job);
      });
  {
    std::lock_guard lock(refinement_jobs_mutex_);
    refinement_jobs_[game_id] = job;
  }

  return move_analysis_status_json(game_id, ply);
}

void AnalysisService::cancel_analysis(const std::string& game_id) {
  validate_token(game_id, "game id");
  std::vector<std::shared_ptr<AnalysisJob>> jobs;
  {
    std::lock_guard lock(jobs_mutex_);
    const auto found = jobs_.find(game_id);
    if (found != jobs_.end()) jobs.push_back(found->second);
  }
  {
    std::lock_guard lock(refinement_jobs_mutex_);
    const auto found = refinement_jobs_.find(game_id);
    if (found != refinement_jobs_.end()) jobs.push_back(found->second);
  }
  if (jobs.empty()) throw std::runtime_error("Analysis job not found");
  for (const auto& job : jobs) {
    job->state = AnalysisJobState::cancelling;
    job->cancel_requested = true;
    job->engine->cancel();
  }
}

void AnalysisService::delete_analysis(const std::string& game_id) {
  validate_token(game_id, "game id");
  if (!database_.game(game_id).has_value()) {
    throw std::runtime_error("Game not found");
  }

  std::vector<std::shared_ptr<AnalysisJob>> stale_jobs;
  {
    std::lock_guard lock(jobs_mutex_);
    const auto found = jobs_.find(game_id);
    if (found != jobs_.end()) {
      stale_jobs.push_back(found->second);
      jobs_.erase(found);
    }
  }
  {
    std::lock_guard lock(refinement_jobs_mutex_);
    const auto found = refinement_jobs_.find(game_id);
    if (found != refinement_jobs_.end()) {
      stale_jobs.push_back(found->second);
      refinement_jobs_.erase(found);
    }
  }
  for (const auto& job : stale_jobs) {
    if (!job->finished) {
      job->state = AnalysisJobState::cancelling;
      job->cancel_requested = true;
      job->engine->cancel();
    }
  }
  for (const auto& job : stale_jobs) {
    if (job->worker.joinable()) job->worker.join();
  }

  database_.delete_game_analyses(game_id);
  diagnostics::info("analysis", "game=" + game_id + " saved analysis deleted");
}


void AnalysisService::stop_all_mainline_analysis_jobs() noexcept {
  std::vector<std::shared_ptr<AnalysisJob>> stale_jobs;
  {
    std::lock_guard lock(jobs_mutex_);
    for (auto& [game_id, job] : jobs_) {
      (void)game_id;
      if (!job->finished) {
        job->state = AnalysisJobState::cancelling;
        job->cancel_requested = true;
        job->engine->cancel();
      }
      stale_jobs.push_back(job);
    }
    jobs_.clear();
  }
  {
    std::lock_guard lock(refinement_jobs_mutex_);
    for (auto& [game_id, job] : refinement_jobs_) {
      (void)game_id;
      if (!job->finished) {
        job->state = AnalysisJobState::cancelling;
        job->cancel_requested = true;
        job->engine->cancel();
      }
      stale_jobs.push_back(job);
    }
    refinement_jobs_.clear();
  }
  for (const auto& stale : stale_jobs) {
    if (stale->worker.joinable()) stale->worker.join();
  }
}

void AnalysisService::stop_all_variation_jobs(const bool stop_engine) noexcept {
  std::vector<std::shared_ptr<VariationJob>> stale_jobs;
  std::shared_ptr<ChessEngine> engine_to_stop;
  {
    std::lock_guard lock(variation_jobs_mutex_);
    for (auto& [job_id, job] : variation_jobs_) {
      (void)job_id;
      if (!job->finished) {
        job->cancel_requested = true;
        job->engine->cancel();
      }
      stale_jobs.push_back(job);
    }
    variation_jobs_.clear();
    if (stop_engine) {
      variation_position_results_.clear();
      engine_to_stop = std::move(variation_engine_);
    }
  }
  for (const auto& stale : stale_jobs) {
    if (stale->worker.joinable()) stale->worker.join();
  }
  if (engine_to_stop != nullptr) engine_to_stop->stop();
}

void AnalysisService::prepare_for_engine_change() noexcept {
  // Engine selection is an execution concern only. Keep every persisted
  // analysis/cache row intact so switching back to a previously used engine
  // can still reuse its compatible results. Only live workers must be stopped
  // before the setting changes; otherwise a worker created with the previous
  // engine could continue while later status/cache lookups observe the new
  // engine setting.
  stop_all_mainline_analysis_jobs();
  stop_all_variation_jobs(true);
}

void AnalysisService::reap_finished_variation_jobs() {
  std::vector<std::shared_ptr<VariationJob>> stale_jobs;
  {
    std::lock_guard lock(variation_jobs_mutex_);
    for (auto iterator = variation_jobs_.begin(); iterator != variation_jobs_.end();) {
      if (iterator->second->finished) {
        stale_jobs.push_back(iterator->second);
        iterator = variation_jobs_.erase(iterator);
      } else {
        ++iterator;
      }
    }
  }
  for (const auto& stale : stale_jobs) {
    if (stale->worker.joinable()) stale->worker.join();
  }
}

std::string AnalysisService::start_variation_analysis_json(
    const std::string& fen, const std::string& uci) {
  return start_variation_job_json(fen, uci, database_.settings());
}

std::string AnalysisService::start_variation_analysis_with_settings_json(
    const std::string& fen,
    const std::string& uci,
    const int depth,
    const int multi_pv,
    const int threads,
    const int hash_mb) {
  if (depth < AppSettings::min_depth || depth > AppSettings::max_depth) {
    throw std::invalid_argument("depth must be between 1 and 64");
  }
  if (multi_pv < AppSettings::min_multi_pv ||
      multi_pv > AppSettings::max_multi_pv) {
    throw std::invalid_argument("multiPv must be between 1 and 8");
  }
  if (threads < kThreadsSetting.min_int || threads > kThreadsSetting.max_int) {
    throw std::invalid_argument("threads must be between 1 and 32");
  }
  if (hash_mb < kHashMbSetting.min_int || hash_mb > kHashMbSetting.max_int) {
    throw std::invalid_argument("hashMb must be between 16 and 2048");
  }

  auto settings = database_.settings();
  settings.depth = depth;
  settings.multi_pv = multi_pv;
  settings.threads = threads;
  settings.hash_mb = hash_mb;
  return start_variation_job_json(fen, uci, std::move(settings));
}

std::string AnalysisService::start_variation_job_json(
    const std::string& fen,
    const std::string& uci,
    AppSettings settings) {
  reap_finished_variation_jobs();
  // Validate/apply the move before touching the current worker. An illegal
  // sideline attempt must not kill the analysis that is already visible.
  const auto applied = apply_legal_uci_move(fen, uci);

  // A sideline is live-only and owns the single Stockfish work slot. Stop
  // both the main-line worker and any previous sideline search before the new
  // legal board position is accepted. This prevents hidden background engines.
  stop_all_mainline_analysis_jobs();
  // Cancel/join only the previous search. Keep the sideline Stockfish instance
  // and ephemeral position cache alive so consecutive variation moves retain
  // loaded NNUE networks, Hash/TT state and the previous position evaluation.
  stop_all_variation_jobs(false);
  settings.threads = std::clamp(settings.threads, 1, maximum_worker_threads());
  auto job = std::make_shared<VariationJob>();
  job->id = "variation-" + std::to_string(next_variation_job_id_++);
  job->played_move = applied.uci;
  job->played_san = applied.san;
  job->fen_before = fen;
  job->fen = applied.fen_after;
  job->visible_multi_pv = settings.multi_pv;
  {
    std::lock_guard lock(variation_jobs_mutex_);
    if (variation_engine_ == nullptr || variation_engine_->id() != settings.engine_id) {
      if (variation_engine_ != nullptr) variation_engine_->stop();
      variation_position_results_.clear();
      variation_engine_ = create_stockfish_engine(settings.engine_id);
      variation_engine_->validate_available();
    }
    job->engine = variation_engine_;
  }

  // SF19 sideline engines are intentionally reused across consecutive moves to
  // preserve their expensive TT/NNUE state. Its public live snapshot belongs
  // to exactly one root position, though. Clear only that transient snapshot
  // before publishing the new job so the first status response can never show
  // PV/eval/bestmove data from the previous sideline position. SF18 keeps its
  // established behavior because its adapter uses the default no-op method.
  if (job->engine->id() == "stockfish19") {
    job->engine->clear_live_result();
  }

  job->worker = std::thread([this, job, settings = std::move(settings)] {
    try {
      if (job->cancel_requested) throw std::runtime_error("Variation analysis cancelled");
      job->engine->start();

      // Keep the live UX unchanged: first analyze the position AFTER the
      // sideline move so evaluation/PV immediately correspond to the board the
      // user is looking at. Classification is finalized afterwards.
      //
      // SF19 must use the same root search width for the arrow shown now and
      // for classification if the user follows that arrow on the next ply. A
      // MultiPV-1 arrow followed by a separate MultiPV-4 classifier search can
      // legitimately produce a different rank-1 move at the same depth. Keep
      // the extra SF19 lines internal; variation_analysis_status_json() still
      // exposes only the user's requested MultiPV count. SF18 intentionally
      // retains its established search behavior.
      auto after_settings = settings;
      if (job->engine->id() == "stockfish19") {
        const auto after_context = position_context(job->fen);
        if (after_context.legal_move_count > 0) {
          after_settings.multi_pv = std::max(
              settings.multi_pv,
              std::min(after_context.legal_move_count, kClassificationMultiPv));
        }
      }
      auto after_request = analysis_request(job->fen, after_settings);
      if (job->engine->id() == "stockfish19") {
        after_request.require_exact_multipv_snapshot = true;
      }
      after_request.cancel_requested = &job->cancel_requested;
      auto after = job->engine->analyze(after_request);
      {
        std::lock_guard lock(job->state_mutex);
        job->result = after;
      }
      if (job->cancel_requested || after.interrupted) {
        std::lock_guard lock(job->state_mutex);
        job->status = "paused";
        job->finished = true;
        return;
      }

      // Preserve the finished after-position result for the next sideline ply.
      // This cache is in-memory only and is cleared when variation mode ends.
      {
        std::lock_guard lock(variation_jobs_mutex_);
        variation_position_results_[variation_position_cache_key(
            job->fen, settings, job->engine->cache_identity())] = after;
      }

      // A move category compares the played move with the best alternatives in
      // the BEFORE position. Normally that position is exactly the previous
      // sideline result, so only the first sideline ply needs an extra search.
      auto before_settings = settings;
      const auto before_context = position_context(job->fen_before);
      before_settings.multi_pv = std::max(
          1,
          std::min(
              before_context.legal_move_count,
              std::max(settings.multi_pv, kClassificationMultiPv)));

      std::optional<AnalysisResult> before;
      const auto before_key = variation_position_cache_key(
          job->fen_before, before_settings, job->engine->cache_identity());
      {
        std::lock_guard lock(variation_jobs_mutex_);
        const auto cached = variation_position_results_.find(before_key);
        if (cached != variation_position_results_.end()
            && (before_context.legal_move_count == 0
                || static_cast<int>(cached->second.lines.size()) >= before_settings.multi_pv)) {
          before = cached->second;
        }
      }

      if (!before.has_value() && settings.use_global_analysis_cache) {
        before = database_.compatible_position_analysis(
            canonical_position_cache_fen(job->fen_before),
            position_cache_engine_identity(
                *job->engine, before_settings.adaptive_early_stop),
            before_settings);
      }

      if (!before.has_value()) {
        // Do not expose engine->current_result() while this second search is
        // running: it belongs to the BEFORE position. The UI keeps displaying
        // the completed after-position PV until classification is ready.
        job->expose_live_result = false;
        auto before_request = analysis_request(job->fen_before, before_settings);
        if (job->engine->id() == "stockfish19") {
          before_request.require_exact_multipv_snapshot = true;
        }
        before_request.cancel_requested = &job->cancel_requested;
        before = job->engine->analyze(before_request);
        if (job->cancel_requested || before->interrupted) {
          std::lock_guard lock(job->state_mutex);
          job->status = "paused";
          job->finished = true;
          return;
        }
        {
          std::lock_guard lock(variation_jobs_mutex_);
          variation_position_results_[before_key] = *before;
        }
      }

      TheoryMoveInfo theory;
      if (opening_theory_ != nullptr) {
        theory = opening_theory_->lookup(job->fen_before, job->played_move);
      }
      const auto category = classify_variation_move(
          job->fen_before, job->played_move, job->fen, *before, after, theory,
          job->engine->id() == "stockfish19");
      {
        std::lock_guard lock(job->state_mutex);
        job->classification = category;
        job->status = "complete";
      }
    } catch (const std::exception& error) {
      std::lock_guard lock(job->state_mutex);
      if (job->cancel_requested) {
        job->status = "paused";
        job->error.clear();
      } else {
        job->status = "error";
        job->error = error.what();
      }
    } catch (...) {
      std::lock_guard lock(job->state_mutex);
      if (job->cancel_requested) {
        job->status = "paused";
        job->error.clear();
      } else {
        job->status = "error";
        job->error = "Unknown variation analysis error";
      }
    }
    job->finished = true;
  });
  {
    std::lock_guard lock(variation_jobs_mutex_);
    variation_jobs_.emplace(job->id, job);
  }
  return variation_analysis_status_json(job->id);
}

std::string AnalysisService::variation_analysis_status_json(const std::string& job_id) {
  validate_token(job_id, "variation job id");
  std::shared_ptr<VariationJob> job;
  {
    std::lock_guard lock(variation_jobs_mutex_);
    const auto found = variation_jobs_.find(job_id);
    if (found == variation_jobs_.end()) throw std::runtime_error("Variation job not found");
    job = found->second;
  }

  std::string status;
  std::string error;
  AnalysisResult result;
  std::optional<MoveCategory> classification;
  {
    std::lock_guard lock(job->state_mutex);
    status = job->status;
    error = job->error;
    result = job->result;
    classification = job->classification;
  }
  // Side-line analysis is genuinely live: while Stockfish is thinking, expose
  // its latest complete PV iteration instead of waiting for the hard depth.
  if (status == "running" && job->expose_live_result.load()) {
    auto live = job->engine->current_result();
    if (!live.lines.empty()) result = std::move(live);
  }

  nlohmann::json json{
      {"jobId", job->id},
      {"status", status},
      {"playedMove", job->played_move},
      {"playedSan", job->played_san},
      {"fen", job->fen},
      {"position", nlohmann::json::parse(position_view_json(job->fen))},
      {"error", error.empty() ? nlohmann::json(nullptr) : nlohmann::json(error)},
      {"bestMove", result.best_move},
      {"engineVersion", job->engine->version()},
      {"liveDepth", result.reached_depth},
      {"moverEvaluationCp", nullptr},
      {"moverMateIn", nullptr},
      {"classification", classification.has_value()
          ? nlohmann::json(move_category_name(*classification)) : nlohmann::json(nullptr)},
      {"lines", nlohmann::json::array()},
  };
  if (!result.lines.empty()) {
    const auto& principal = result.lines.front();
    if (principal.evaluation_cp.has_value()) {
      json["moverEvaluationCp"] = -*principal.evaluation_cp;
    }
    if (principal.mate_in.has_value()) json["moverMateIn"] = -*principal.mate_in;
  }
  const auto visible_line_count = std::min(
      result.lines.size(),
      static_cast<std::size_t>(std::max(1, job->visible_multi_pv)));
  for (std::size_t index = 0; index < visible_line_count; ++index) {
    const auto& line = result.lines[index];
    const bool line_is_white = white_to_move(job->fen);
    const auto evaluation_bar_white_permille = job->engine->id() == "stockfish19"
        ? sf19_evaluation_bar_white_permille(line, line_is_white)
        : std::nullopt;
    nlohmann::json line_json{
        {"rank", line.rank},
        {"depth", line.depth},
        {"evaluationCp", line.evaluation_cp.has_value()
            ? nlohmann::json(line_is_white ? *line.evaluation_cp : -*line.evaluation_cp)
            : nlohmann::json(nullptr)},
        {"mateIn", line.mate_in.has_value()
            ? nlohmann::json(line_is_white ? *line.mate_in : -*line.mate_in)
            : nlohmann::json(nullptr)},
        {"evaluationBarWhitePermille", evaluation_bar_white_permille.has_value()
            ? nlohmann::json(*evaluation_bar_white_permille)
            : nlohmann::json(nullptr)},
        {"nodes", line.nodes},
        {"moves", line.moves},
        {"wdl", nullptr},
    };
    if (line.wdl.has_value()) {
      line_json["wdl"] = {
          {"wins", line_is_white ? line.wdl->wins : line.wdl->losses},
          {"draws", line.wdl->draws},
          {"losses", line_is_white ? line.wdl->losses : line.wdl->wins},
      };
    }
    json["lines"].push_back(std::move(line_json));
  }
  return json.dump();
}

void AnalysisService::cancel_variation_analysis(const std::string& job_id) {
  validate_token(job_id, "variation job id");
  std::shared_ptr<VariationJob> job;
  {
    std::lock_guard lock(variation_jobs_mutex_);
    const auto found = variation_jobs_.find(job_id);
    if (found == variation_jobs_.end()) return;
    job = found->second;
    job->cancel_requested = true;
    job->engine->cancel();
  }
  if (job->worker.joinable()) job->worker.join();
  std::shared_ptr<ChessEngine> engine_to_stop;
  {
    std::lock_guard lock(variation_jobs_mutex_);
    const auto found = variation_jobs_.find(job_id);
    if (found != variation_jobs_.end() && found->second == job) {
      variation_jobs_.erase(found);
    }
    // Explicit cancellation means the caller is leaving the active sideline.
    // Do not keep its potentially large Hash allocation next to main-line
    // Stockfish. Consecutive sideline moves use start_variation_job_json(),
    // which cancels only the search and never comes through this path.
    if (variation_jobs_.empty() && variation_engine_ == job->engine) {
      variation_position_results_.clear();
      engine_to_stop = std::move(variation_engine_);
    }
  }
  if (engine_to_stop != nullptr) engine_to_stop->stop();
}

}  // namespace kchess
