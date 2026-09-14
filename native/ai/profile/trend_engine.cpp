#include "trend_engine.h"

#include <algorithm>
#include <string>

namespace kchess::ai {
namespace {

struct TrendSample {
  int recent_moves{0};
  int recent_errors{0};
  int baseline_moves{0};
  int baseline_errors{0};
};

bool matches_context(
    const ProfileGameEvidence& game,
    const std::string& time_control) {
  return time_control == "all" ||
         profile_time_control_id(game.time_control) == time_control;
}

void add_phase_counts(
    TrendSample& sample,
    const ProfilePhaseCounts& phase,
    const bool recent,
    const std::string& pattern_id) {
  int errors = phase.miss + phase.mistake + phase.blunder;
  if (pattern_id == "blunder_control") errors = phase.blunder;
  if (pattern_id == "missed_opportunities") errors = phase.miss;
  if (recent) {
    sample.recent_moves += phase.analyzed_moves;
    sample.recent_errors += errors;
  } else {
    sample.baseline_moves += phase.analyzed_moves;
    sample.baseline_errors += errors;
  }
}

TrendSample sample_for_pattern(
    const PlayerPattern& pattern,
    const std::vector<ProfileGameEvidence>& games,
    const std::int64_t now_seconds) {
  TrendSample sample;
  for (const auto& game : games) {
    if (!matches_context(game, pattern.time_control) || game.played_at <= 0) continue;
    const double age_days = now_seconds > game.played_at
        ? static_cast<double>(now_seconds - game.played_at) / 86400.0
        : 0.0;
    const bool recent = age_days <= 60.0;
    const bool baseline = age_days > 60.0 && age_days <= 240.0;
    if (!recent && !baseline) continue;

    if (pattern.phase != "all") {
      for (const auto& phase : game.phases) {
        if (profile_phase_id(phase.phase) == pattern.phase) {
          add_phase_counts(sample, phase, recent, pattern.id);
          break;
        }
      }
      continue;
    }

    const int analyzed = game.classifications.analyzed_moves;
    int errors = game.classifications.miss + game.classifications.mistake +
                 game.classifications.blunder;
    if (pattern.id == "blunder_control") errors = game.classifications.blunder;
    if (pattern.id == "missed_opportunities") errors = game.classifications.miss;
    if (recent) {
      sample.recent_moves += analyzed;
      sample.recent_errors += errors;
    } else {
      sample.baseline_moves += analyzed;
      sample.baseline_errors += errors;
    }
  }
  return sample;
}

std::string trend_name(const TrendSample& sample) {
  if (sample.recent_moves < 24 || sample.baseline_moves < 24) {
    return "insufficient_data";
  }
  const double recent_rate = static_cast<double>(sample.recent_errors) / sample.recent_moves;
  const double baseline_rate = static_cast<double>(sample.baseline_errors) / sample.baseline_moves;
  const double delta = recent_rate - baseline_rate;
  if (delta <= -0.018 && recent_rate <= baseline_rate * 0.82) return "improving";
  if (delta >= 0.018 && recent_rate >= baseline_rate * 1.18) return "worsening";
  return "stable";
}

}  // namespace

void ProfileTrendEngine::apply(
    std::vector<PlayerPattern>& patterns,
    std::vector<TimeControlProfile>& time_control_profiles,
    const std::vector<ProfileGameEvidence>& games,
    const std::int64_t now_seconds) const {
  for (auto& pattern : patterns) {
    pattern.trend = trend_name(sample_for_pattern(pattern, games, now_seconds));
  }
  for (auto& profile : time_control_profiles) {
    PlayerPattern proxy;
    proxy.id = "time_control";
    proxy.time_control = profile.id;
    proxy.phase = "all";
    profile.trend = trend_name(sample_for_pattern(proxy, games, now_seconds));
  }
}

}  // namespace kchess::ai
