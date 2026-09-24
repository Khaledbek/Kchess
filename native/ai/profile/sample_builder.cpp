#include "sample_builder.h"

#include <algorithm>
#include <map>
#include <set>
#include <tuple>

namespace kchess::ai {
namespace {

std::string opening_bucket(const ProfileGameEvidence& game) {
  if (!game.opening_eco.empty()) return game.opening_eco;
  if (!game.opening_name.empty()) return game.opening_name;
  return "unknown";
}

std::string outcome_bucket(const ProfileGameEvidence& game) {
  if (game.provider_outcome == "win" || game.provider_outcome == "draw" ||
      game.provider_outcome == "loss") {
    return game.provider_outcome;
  }
  return "unknown";
}

}  // namespace

InitialSample InitialSampleBuilder::build(
    const std::vector<ProfileGameEvidence>& games,
    const std::int64_t now_seconds,
    const std::size_t minimum,
    const std::size_t preferred,
    const std::size_t maximum) const {
  InitialSample result;
  if (games.empty()) return result;

  const std::size_t safe_maximum = std::max<std::size_t>(1, maximum);
  const std::size_t safe_minimum = std::min(minimum, safe_maximum);
  const std::size_t safe_preferred = std::clamp(preferred, safe_minimum, safe_maximum);
  const std::size_t target = games.size() <= safe_maximum
      ? std::min(games.size(), safe_preferred)
      : safe_preferred;
  result.requested_size = target;

  GameRelevanceScorer scorer;
  struct RankedGame {
    const ProfileGameEvidence* game{nullptr};
    GameRelevanceScore relevance;
  };
  std::vector<RankedGame> ranked;
  ranked.reserve(games.size());
  for (const auto& game : games) {
    ranked.push_back(RankedGame{.game = &game, .relevance = scorer.score(game, now_seconds)});
  }
  std::stable_sort(ranked.begin(), ranked.end(), [](const RankedGame& a, const RankedGame& b) {
    if (a.relevance.score != b.relevance.score) return a.relevance.score > b.relevance.score;
    return a.game->played_at > b.game->played_at;
  });

  std::set<std::string> selected;
  std::set<std::string> seen_time_controls;
  std::set<std::string> seen_outcomes;
  std::set<std::string> seen_openings;

  auto add = [&](const RankedGame& candidate) {
    if (selected.size() >= target) return;
    if (selected.insert(candidate.game->game_id).second) {
      result.game_ids.push_back(candidate.game->game_id);
      seen_time_controls.insert(profile_time_control_id(candidate.game->time_control));
      seen_outcomes.insert(outcome_bucket(*candidate.game));
      seen_openings.insert(opening_bucket(*candidate.game));
    }
  };

  // Pass 1: force breadth before score-only filling. This prevents a recent
  // streak from crowding out another time control, result type or opening.
  for (const auto& candidate : ranked) {
    const auto tc = profile_time_control_id(candidate.game->time_control);
    const auto outcome = outcome_bucket(*candidate.game);
    const auto opening = opening_bucket(*candidate.game);
    if (!seen_time_controls.contains(tc) || !seen_outcomes.contains(outcome) ||
        !seen_openings.contains(opening)) {
      add(candidate);
    }
    if (selected.size() >= target) break;
  }

  // Pass 2: fill by relevance. Already analysed games naturally rank higher,
  // so the first profile becomes useful quickly without duplicate engine work.
  for (const auto& candidate : ranked) {
    if (selected.size() >= target) break;
    add(candidate);
  }
  return result;
}

}  // namespace kchess::ai
