#include "personal_training_selector.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace kchess::ai {
namespace {

std::string schedule_skill_for_pattern(const PlayerPattern& pattern) {
  if (pattern.id == "opening_errors" || pattern.phase == "opening") {
    return "opening.decision";
  }
  if (pattern.id == "endgame_errors" || pattern.phase == "endgame") {
    return "endgame.decision";
  }
  return "calculation.candidate_selection";
}

double coach_priority_for(const ChessProfile& profile,
                          std::string_view pattern_id) {
  double score = 0.0;
  for (const auto& priority : profile.coach_priorities) {
    if (priority.pattern_id == pattern_id) score = std::max(score, priority.score);
  }
  return std::clamp(score, 0.0, 1.0);
}

double due_priority_for(
    const std::unordered_map<std::string, PracticeProgress>& progress,
    std::string_view skill_id, std::int64_t now_seconds) {
  const auto found = progress.find(std::string(skill_id));
  if (found == progress.end()) return 1.0;
  return std::clamp(practice_due_priority(found->second, now_seconds) / 2.0,
                    0.0, 1.0);
}

double verified_attempt_count(const PracticeProgress& progress) {
  return static_cast<double>(std::max(0, progress.independent_successes) +
                             std::max(0, progress.verified_weak_attempts));
}

// Conservative contextual UCB policy over the existing verified practice
// outcomes. It explores under-sampled eligible skills only after more than
// one skill has real graded history; the score is exercise priority, never a
// claim of learning gain or measured chess strength.
double practice_bandit_adjustment(
    const std::unordered_map<std::string, PracticeProgress>& progress,
    std::string_view skill_id, double total_attempts) {
  const auto found = progress.find(std::string(skill_id));
  const double attempts = found == progress.end() ? 0.0
                                                  : verified_attempt_count(found->second);
  const double weak = found == progress.end() ? 0.0
      : static_cast<double>(std::max(0, found->second.verified_weak_attempts));
  const double posterior_error = (weak + 1.0) / (attempts + 2.0);
  const double exploration = std::min(
      1.0, std::sqrt(2.0 * std::log(total_attempts + 2.0) / (attempts + 2.0)));
  return 0.07 * posterior_error + 0.05 * exploration;
}

std::size_t rotating_example_index(
    const PlayerPattern& pattern,
    const std::unordered_map<std::string, PracticeProgress>& progress,
    std::string_view skill_id, std::int64_t now_seconds) {
  if (pattern.example_positions.empty()) return 0;
  std::int64_t last_practiced = 0;
  if (const auto found = progress.find(std::string(skill_id)); found != progress.end()) {
    last_practiced = found->second.last_practiced_at;
  }
  const std::string seed = pattern.id + ":" +
      std::to_string(now_seconds / 86400) + ":" +
      std::to_string(last_practiced / 86400);
  return std::hash<std::string>{}(seed) % pattern.example_positions.size();
}

}  // namespace

std::optional<PersonalTrainingSelection> select_personal_training_position(
    const ChessProfile& profile,
    const std::unordered_map<std::string, PracticeProgress>& practice_progress,
    const std::int64_t now_seconds) {
  struct Candidate {
    const PlayerPattern* pattern{nullptr};
    std::string skill_id;
    double score{0.0};
  };

  std::vector<Candidate> candidates;
  for (const auto& pattern : profile.patterns) {
    if (pattern.type != "weakness" || pattern.example_positions.empty()) continue;
    const bool has_usable_example = std::any_of(
        pattern.example_positions.begin(), pattern.example_positions.end(),
        [](const ProfileExamplePosition& example) {
          return !example.game_id.empty() && !example.fen.empty() && example.ply >= 0;
        });
    if (!has_usable_example) continue;

    const auto skill_id = schedule_skill_for_pattern(pattern);
    const double score =
        0.30 * std::clamp(pattern.severity, 0.0, 1.0) +
        0.25 * std::clamp(pattern.confidence, 0.0, 1.0) +
        0.25 * coach_priority_for(profile, pattern.id) +
        0.20 * due_priority_for(practice_progress, skill_id, now_seconds);
    candidates.push_back(Candidate{.pattern = &pattern,
                                   .skill_id = skill_id,
                                   .score = score});
  }

  if (candidates.empty()) return std::nullopt;
  double total_graded_attempts = 0.0;
  std::unordered_set<std::string> candidate_skills;
  for (const auto& candidate : candidates)
    candidate_skills.insert(candidate.skill_id);
  std::size_t trained_skill_count = 0;
  for (const auto& skill : candidate_skills) {
    const auto found = practice_progress.find(skill);
    if (found == practice_progress.end()) continue;
    const double count = verified_attempt_count(found->second);
    if (count < 3.0) continue;
    ++trained_skill_count;
    total_graded_attempts += count;
  }
  if (trained_skill_count >= 2 && total_graded_attempts >= 24.0) {
    for (auto& candidate : candidates)
      candidate.score += practice_bandit_adjustment(
          practice_progress, candidate.skill_id, total_graded_attempts);
  }
  std::stable_sort(candidates.begin(), candidates.end(),
                   [](const Candidate& a, const Candidate& b) {
                     if (std::abs(a.score - b.score) > 1e-9) return a.score > b.score;
                     return a.pattern->id < b.pattern->id;
                   });

  for (const auto& candidate : candidates) {
    const auto& examples = candidate.pattern->example_positions;
    const std::size_t start = rotating_example_index(
        *candidate.pattern, practice_progress, candidate.skill_id, now_seconds);
    for (std::size_t offset = 0; offset < examples.size(); ++offset) {
      const auto& example = examples[(start + offset) % examples.size()];
      if (example.game_id.empty() || example.fen.empty() || example.ply < 0) continue;
      return PersonalTrainingSelection{.example = example,
                                       .pattern_id = candidate.pattern->id,
                                       .schedule_skill_id = candidate.skill_id,
                                       .priority = candidate.score};
    }
  }
  return std::nullopt;
}

}  // namespace kchess::ai
