#include "pattern_matcher.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <numeric>
#include <optional>
#include <string>

namespace kchess::ai {
namespace {

double clamp01(const double value) { return std::clamp(value, 0.0, 1.0); }

double sample_confidence(const int samples, const double scale) {
  if (samples <= 0) return 0.0;
  return clamp01(1.0 - std::exp(-static_cast<double>(samples) / scale));
}

double recency_weight(const std::int64_t played_at, const std::int64_t now_seconds) {
  if (played_at <= 0 || now_seconds <= played_at) return 1.0;
  const double age_days = static_cast<double>(now_seconds - played_at) / 86400.0;
  if (age_days < 30.0) return 1.0;
  if (age_days < 90.0) return 0.85;
  if (age_days < 180.0) return 0.65;
  if (age_days < 365.0) return 0.45;
  return 0.25;
}

struct Aggregate {
  int games{0};
  int analyzed{0};
  int theory{0};
  int brilliant{0};
  int critical{0};
  int best{0};
  int miss{0};
  int mistake{0};
  int blunder{0};
  double weighted_accuracy_sum{0.0};
  double weighted_accuracy_count{0.0};
  double weighted_major_errors{0.0};
  double weighted_analyzed{0.0};
  int recent_games{0};
};

int major_errors(const Aggregate& value) {
  return value.miss + value.mistake + value.blunder;
}

double error_rate(const Aggregate& value) {
  return value.analyzed > 0
      ? static_cast<double>(major_errors(value)) / static_cast<double>(value.analyzed)
      : 0.0;
}

double weighted_error_rate(const Aggregate& value) {
  return value.weighted_analyzed > 0.0
      ? value.weighted_major_errors / value.weighted_analyzed
      : error_rate(value);
}

void add_game(Aggregate& aggregate, const ProfileGameEvidence& game,
              const std::int64_t now_seconds) {
  ++aggregate.games;
  aggregate.analyzed += game.classifications.analyzed_moves;
  aggregate.theory += game.classifications.theory;
  aggregate.brilliant += game.classifications.brilliant;
  aggregate.critical += game.classifications.critical;
  aggregate.best += game.classifications.best;
  aggregate.miss += game.classifications.miss;
  aggregate.mistake += game.classifications.mistake;
  aggregate.blunder += game.classifications.blunder;
  const double weight = recency_weight(game.played_at, now_seconds);
  const int major = game.classifications.miss + game.classifications.mistake +
                    game.classifications.blunder;
  aggregate.weighted_major_errors += weight * static_cast<double>(major);
  aggregate.weighted_analyzed += weight * static_cast<double>(game.classifications.analyzed_moves);
  if (game.accuracy) {
    aggregate.weighted_accuracy_sum += weight * *game.accuracy;
    aggregate.weighted_accuracy_count += weight;
  }
  if (weight >= 0.85) ++aggregate.recent_games;
}

std::optional<double> average_accuracy(const Aggregate& aggregate) {
  if (aggregate.weighted_accuracy_count <= 0.0) return std::nullopt;
  return aggregate.weighted_accuracy_sum / aggregate.weighted_accuracy_count;
}

std::string dominant_phase(const std::vector<ProfileGameEvidence>& games) {
  std::array<int, 3> analyzed{0, 0, 0};
  std::array<int, 3> errors{0, 0, 0};
  for (const auto& game : games) {
    for (const auto& phase : game.phases) {
      const int index = phase.phase == ProfilePhase::opening
          ? 0
          : (phase.phase == ProfilePhase::middlegame ? 1 : 2);
      analyzed[index] += phase.analyzed_moves;
      errors[index] += phase.miss + phase.mistake + phase.blunder;
    }
  }
  int best = -1;
  double best_rate = 0.0;
  for (int i = 0; i < 3; ++i) {
    if (analyzed[i] < 12) continue;
    const double rate = static_cast<double>(errors[i]) / analyzed[i];
    if (rate > best_rate) {
      best_rate = rate;
      best = i;
    }
  }
  if (best == 0) return "opening";
  if (best == 1) return "middlegame";
  if (best == 2) return "endgame";
  return "all";
}

std::string dominant_time_control(const std::map<std::string, Aggregate>& by_tc) {
  std::string best = "all";
  double best_rate = 0.0;
  int qualifying = 0;
  for (const auto& [id, aggregate] : by_tc) {
    if (id == "other" || aggregate.analyzed < 20) continue;
    ++qualifying;
    const double rate = weighted_error_rate(aggregate);
    if (rate > best_rate) {
      best_rate = rate;
      best = id;
    }
  }
  return qualifying > 0 ? best : "all";
}

std::vector<ProfileExamplePosition> collect_examples(
    const std::vector<ProfileGameEvidence>& games,
    const std::string& pattern_id,
    const std::string& phase,
    const std::string& time_control) {
  std::vector<ProfileExamplePosition> result;
  for (const auto& game : games) {
    if (time_control != "all" && profile_time_control_id(game.time_control) != time_control) {
      continue;
    }
    for (const auto& move : game.moves) {
      if (phase != "all" && profile_phase_id(move.phase) != phase) continue;
      bool match = false;
      if (pattern_id == "blunder_control") match = move.classification == "blunder";
      else if (pattern_id == "missed_opportunities") match = move.classification == "miss";
      else if (pattern_id == "calculation_consistency" ||
               pattern_id == "time_pressure_errors" ||
               pattern_id == "opening_errors" ||
               pattern_id == "endgame_errors") {
        match = profile_major_error(move.classification);
      }
      if (!match || move.fen_before.empty()) continue;
      result.push_back(ProfileExamplePosition{
          .game_id = game.game_id,
          .ply = move.ply,
          .fen = move.fen_before,
          .move = move.uci,
          .classification = move.classification,
      });
      if (result.size() >= 4) return result;
    }
  }
  return result;
}

PlayerPattern weakness_pattern(
    const std::string& id,
    const Aggregate& aggregate,
    const std::vector<ProfileGameEvidence>& games,
    const std::map<std::string, Aggregate>& by_tc,
    const double severity_multiplier = 1.0,
    const std::string& forced_phase = "") {
  const int occurrences = major_errors(aggregate);
  const double rate = weighted_error_rate(aggregate);
  const std::string phase = forced_phase.empty() ? dominant_phase(games) : forced_phase;
  const std::string time_control = dominant_time_control(by_tc);
  PlayerPattern pattern{
      .id = id,
      .type = "weakness",
      .confidence = clamp01(
          sample_confidence(aggregate.analyzed, 100.0) *
          sample_confidence(std::max(occurrences, 1), 6.0)),
      .severity = clamp01(rate * 5.5 * severity_multiplier),
      .occurrences = occurrences,
      .sample_games = aggregate.games,
      .time_control = time_control,
      .phase = phase,
      .recent = aggregate.recent_games > 0,
      .trend = "insufficient_data",
  };
  pattern.analyzed_moves = aggregate.analyzed;
  if (aggregate.analyzed > 0) {
    // Beta(1,1) posterior on major errors per analyzed move. The interval is
    // deliberately approximate and broad for sparse samples; games remain a
    // separate diversity/confidence signal.
    const double alpha = occurrences + 1.0;
    const double beta = std::max(0, aggregate.analyzed - occurrences) + 1.0;
    const double mean = alpha / (alpha + beta);
    const double deviation = std::sqrt(alpha * beta /
        ((alpha + beta) * (alpha + beta) * (alpha + beta + 1.0)));
    pattern.observed_error_rate = error_rate(aggregate);
    pattern.posterior_error_lower = aggregate.analyzed < 10 ? 0.0
        : clamp01(mean - 1.64 * deviation);
    pattern.posterior_error_upper = aggregate.analyzed < 10 ? 1.0
        : clamp01(mean + 1.64 * deviation);
  }
  pattern.example_positions = collect_examples(games, id, phase, time_control);
  return pattern;
}

PlayerPattern strength_pattern(
    const std::string& id,
    const Aggregate& aggregate,
    const double quality,
    const std::string& phase = "all",
    const std::string& time_control = "all") {
  return PlayerPattern{
      .id = id,
      .type = "strength",
      .confidence = sample_confidence(aggregate.analyzed, 120.0),
      .severity = clamp01(quality),
      .occurrences = aggregate.analyzed,
      .sample_games = aggregate.games,
      .time_control = time_control,
      .phase = phase,
      .recent = aggregate.recent_games > 0,
      .trend = "insufficient_data",
  };
}

}  // namespace

PatternMatchResult PatternMatcher::match(
    const std::vector<ProfileGameEvidence>& games,
    const std::int64_t now_seconds) const {
  PatternMatchResult result;
  Aggregate total;
  std::map<std::string, Aggregate> by_tc;
  std::array<Aggregate, 3> by_phase{};

  for (const auto& game : games) {
    add_game(total, game, now_seconds);
    add_game(by_tc[profile_time_control_id(game.time_control)], game, now_seconds);
    for (const auto& phase : game.phases) {
      const int index = phase.phase == ProfilePhase::opening
          ? 0
          : (phase.phase == ProfilePhase::middlegame ? 1 : 2);
      auto& target = by_phase[index];
      ++target.games;
      target.analyzed += phase.analyzed_moves;
      target.miss += phase.miss;
      target.mistake += phase.mistake;
      target.blunder += phase.blunder;
    }
  }

  result.analyzed_moves = total.analyzed;
  result.confidence = 0.65 * sample_confidence(total.analyzed, 240.0) +
                      0.35 * sample_confidence(total.games, 40.0);

  if (total.analyzed > 0) {
    const double major_rate = weighted_error_rate(total);
    const double accuracy = average_accuracy(total).value_or(
        std::clamp(100.0 * (1.0 - major_rate * 2.5), 0.0, 100.0));
    result.calculation_strength = ProfileSignal{
        .value = clamp01(0.72 * (accuracy / 100.0) + 0.28 * (1.0 - clamp01(major_rate * 4.0))),
        .confidence = sample_confidence(total.analyzed, 200.0),
    };
    const double positive_rate = static_cast<double>(
        total.brilliant + total.critical + total.best) / total.analyzed;
    const double miss_rate = static_cast<double>(total.miss) / total.analyzed;
    result.tactical_strength = ProfileSignal{
        .value = clamp01(0.58 * (accuracy / 100.0) + 0.22 * clamp01(positive_rate * 2.5) +
                         0.20 * (1.0 - clamp01(miss_rate * 5.0))),
        .confidence = sample_confidence(total.analyzed, 240.0) * 0.85,
    };
  }

  const auto& endgame = by_phase[2];
  if (endgame.analyzed > 0) {
    const double rate = error_rate(endgame);
    result.endgame_strength = ProfileSignal{
        .value = clamp01(1.0 - rate * 4.0),
        .confidence = sample_confidence(endgame.analyzed, 80.0),
    };
  }

  auto add_common = [&](const char* id, const int occurrences) {
    if (occurrences <= 0 || total.analyzed <= 0) return;
    result.common_mistakes.push_back(CommonMistake{
        .id = id,
        .occurrences = occurrences,
        .rate = static_cast<double>(occurrences) / total.analyzed,
        .confidence = sample_confidence(total.analyzed, 160.0),
    });
  };
  add_common("missed_opportunity", total.miss);
  add_common("mistake", total.mistake);
  add_common("blunder", total.blunder);
  std::sort(result.common_mistakes.begin(), result.common_mistakes.end(),
            [](const CommonMistake& a, const CommonMistake& b) {
              if (a.rate != b.rate) return a.rate > b.rate;
              return a.occurrences > b.occurrences;
            });

  const double total_rate = weighted_error_rate(total);
  if (total.blunder >= 3 && total.analyzed >= 40 &&
      static_cast<double>(total.blunder) / total.analyzed >= 0.025) {
    auto pattern = weakness_pattern("blunder_control", total, games, by_tc, 1.25);
    pattern.occurrences = total.blunder;
    result.patterns.push_back(std::move(pattern));
    result.weaknesses.push_back("blunder_control");
  }
  if (total.miss >= 3 && total.analyzed >= 40 &&
      static_cast<double>(total.miss) / total.analyzed >= 0.05) {
    auto pattern = weakness_pattern("missed_opportunities", total, games, by_tc, 0.9);
    pattern.occurrences = total.miss;
    result.patterns.push_back(std::move(pattern));
    result.weaknesses.push_back("missed_opportunities");
  }
  if (major_errors(total) >= 5 && total.analyzed >= 60 && total_rate >= 0.055) {
    result.patterns.push_back(
        weakness_pattern("calculation_consistency", total, games, by_tc));
    result.weaknesses.push_back("calculation_consistency");
  }

  const auto phase_rate = [](const Aggregate& value) {
    return value.analyzed > 0
        ? static_cast<double>(major_errors(value)) / value.analyzed
        : 0.0;
  };
  if (by_phase[0].analyzed >= 30 && phase_rate(by_phase[0]) >= 0.055 &&
      phase_rate(by_phase[0]) > total_rate + 0.012) {
    result.patterns.push_back(weakness_pattern(
        "opening_errors", by_phase[0], games, by_tc, 0.9, "opening"));
    result.weaknesses.push_back("opening_errors");
  }
  if (by_phase[2].analyzed >= 24 && phase_rate(by_phase[2]) >= 0.065 &&
      phase_rate(by_phase[2]) > total_rate + 0.012) {
    result.patterns.push_back(weakness_pattern(
        "endgame_errors", by_phase[2], games, by_tc, 1.05, "endgame"));
    result.weaknesses.push_back("endgame_errors");
  }

  Aggregate fast;
  Aggregate slow;
  for (const auto& [id, aggregate] : by_tc) {
    if (id == "bullet" || id == "blitz") {
      fast.games += aggregate.games;
      fast.analyzed += aggregate.analyzed;
      fast.miss += aggregate.miss;
      fast.mistake += aggregate.mistake;
      fast.blunder += aggregate.blunder;
      fast.weighted_major_errors += aggregate.weighted_major_errors;
      fast.weighted_analyzed += aggregate.weighted_analyzed;
      fast.recent_games += aggregate.recent_games;
    } else if (id == "rapid" || id == "classical") {
      slow.games += aggregate.games;
      slow.analyzed += aggregate.analyzed;
      slow.miss += aggregate.miss;
      slow.mistake += aggregate.mistake;
      slow.blunder += aggregate.blunder;
      slow.weighted_major_errors += aggregate.weighted_major_errors;
      slow.weighted_analyzed += aggregate.weighted_analyzed;
      slow.recent_games += aggregate.recent_games;
    }
  }
  if (fast.analyzed >= 40 && slow.analyzed >= 40) {
    const double fast_rate = weighted_error_rate(fast);
    const double slow_rate = weighted_error_rate(slow);
    if (fast_rate >= slow_rate + 0.025 && fast_rate >= slow_rate * 1.30) {
      auto pattern = weakness_pattern("time_pressure_errors", fast, games, by_tc, 1.1);
      const double bullet_rate = by_tc.contains("bullet") ? weighted_error_rate(by_tc.at("bullet")) : 0.0;
      const double blitz_rate = by_tc.contains("blitz") ? weighted_error_rate(by_tc.at("blitz")) : 0.0;
      pattern.time_control = bullet_rate > blitz_rate ? "bullet" : "blitz";
      pattern.example_positions = collect_examples(
          games, pattern.id, pattern.phase, pattern.time_control);
      result.patterns.push_back(std::move(pattern));
      result.weaknesses.push_back("time_pressure_errors");
    }
  }

  if (total.analyzed >= 80 && total_rate <= 0.03) {
    result.patterns.push_back(strength_pattern(
        "move_consistency", total, 1.0 - total_rate * 8.0));
    result.strengths.push_back("move_consistency");
  }
  if (by_phase[0].analyzed >= 40 && phase_rate(by_phase[0]) <= 0.03) {
    result.patterns.push_back(strength_pattern(
        "opening_stability", by_phase[0], 1.0 - phase_rate(by_phase[0]) * 8.0,
        "opening"));
    result.strengths.push_back("opening_stability");
  }
  if (by_phase[2].analyzed >= 30 && phase_rate(by_phase[2]) <= 0.035) {
    result.patterns.push_back(strength_pattern(
        "endgame_stability", by_phase[2], 1.0 - phase_rate(by_phase[2]) * 7.0,
        "endgame"));
    result.strengths.push_back("endgame_stability");
  }

  for (const auto& id : {std::string("bullet"), std::string("blitz"),
                         std::string("rapid"), std::string("classical")}) {
    const auto found = by_tc.find(id);
    if (found == by_tc.end() || found->second.games <= 0) continue;
    const auto& aggregate = found->second;
    result.time_control_profiles.push_back(TimeControlProfile{
        .id = id,
        .games = aggregate.games,
        .analyzed_moves = aggregate.analyzed,
        .average_accuracy = average_accuracy(aggregate),
        .major_error_rate = weighted_error_rate(aggregate),
        .confidence = 0.6 * sample_confidence(aggregate.analyzed, 120.0) +
                      0.4 * sample_confidence(aggregate.games, 16.0),
        .trend = "insufficient_data",
    });
  }

  std::sort(result.patterns.begin(), result.patterns.end(), [](const PlayerPattern& a,
                                                               const PlayerPattern& b) {
    if (a.type != b.type) return a.type == "weakness";
    const double a_score = a.confidence * (0.5 + 0.5 * a.severity);
    const double b_score = b.confidence * (0.5 + 0.5 * b.severity);
    return a_score > b_score;
  });
  return result;
}

}  // namespace kchess::ai
