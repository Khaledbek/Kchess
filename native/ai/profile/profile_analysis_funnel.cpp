#include "ai/profile/profile_analysis_funnel.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <functional>
#include <limits>
#include <map>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace kchess::ai {
namespace {

constexpr std::int64_t kDay = 24 * 60 * 60;

std::string opening_key(const ProfileGameMetadata& game) {
  if (!game.opening_eco.empty()) return game.opening_eco;
  if (!game.opening_name.empty()) return game.opening_name;
  return "unknown";
}

double recency_score(const int bucket) {
  switch (bucket) {
    case 0: return 1.00;  // <= 30 days
    case 1: return 0.82;  // <= 3 months
    case 2: return 0.58;  // <= 6 months
    case 3: return 0.34;  // <= 12 months
    default: return 0.14;
  }
}

double information_score(const ProfileGameMetadata& game) {
  double score = game.has_complete_analysis ? 0.30 : 0.0;
  if (game.accuracy.has_value()) score += 0.12;
  if (game.mistake_count > 0) score += std::min(0.12, game.mistake_count * 0.035);
  if (game.blunder_count > 0) score += std::min(0.18, game.blunder_count * 0.07);
  if (game.miss_count > 0) score += std::min(0.08, game.miss_count * 0.025);
  if (!game.opening_eco.empty() || !game.opening_name.empty()) score += 0.05;
  if (game.plies >= 50) score += 0.04;
  return score;
}

double diversity_bonus(
    const ProfileGameMetadata& game,
    const int age_bucket,
    const std::unordered_map<std::string, int>& time_controls,
    const std::unordered_map<std::string, int>& outcomes,
    const std::unordered_map<std::string, int>& openings,
    const std::unordered_map<int, int>& ages,
    const int analyzed_count,
    const int metadata_only_count) {
  double bonus = 0.0;
  const auto count_for = [](const auto& values, const auto& key) {
    const auto it = values.find(key);
    return it == values.end() ? 0 : it->second;
  };

  const int tc_count = count_for(time_controls, game.time_control);
  const int outcome_count = count_for(outcomes, game.outcome);
  const int opening_count = count_for(openings, opening_key(game));
  const int age_count = count_for(ages, age_bucket);

  if (tc_count == 0) bonus += 0.34;
  else if (tc_count == 1) bonus += 0.12;

  if (outcome_count == 0) bonus += 0.24;
  else if (outcome_count == 1) bonus += 0.08;

  if (opening_count == 0) bonus += 0.19;
  else if (opening_count == 1) bonus += 0.05;

  if (age_count == 0) bonus += 0.12;
  else if (age_count == 1) bonus += 0.04;

  // Prefer analyzed games for a useful first profile, but retain some
  // metadata-only games so the sample can represent portions of the library
  // that have not been analyzed yet.
  if (game.has_complete_analysis && analyzed_count == 0) bonus += 0.15;
  if (!game.has_complete_analysis && metadata_only_count == 0) bonus += 0.08;

  return bonus;
}

bool contains_token(
    const std::vector<std::string>& values, const std::string& candidate) {
  if (candidate.empty()) return false;
  return std::find(values.begin(), values.end(), candidate) != values.end();
}

double metadata_error_score(const ProfileGameMetadata& game) {
  const int weighted_errors =
      game.miss_count + game.mistake_count * 2 + game.blunder_count * 4;
  if (weighted_errors <= 0) return 0.0;
  // Saturate so one chaotic game cannot monopolize the queue.
  return std::min(1.0, static_cast<double>(weighted_errors) / 12.0);
}

double accuracy_interest_score(const ProfileGameMetadata& game) {
  if (!game.accuracy.has_value()) return 0.0;
  // Low accuracy is useful weakness evidence; very high accuracy is useful
  // strength evidence. Middle-of-the-road games are less informative.
  const double accuracy = std::clamp(*game.accuracy, 0.0, 100.0);
  if (accuracy <= 65.0) return std::min(1.0, (65.0 - accuracy) / 25.0 + 0.35);
  if (accuracy >= 92.0) return std::min(0.55, (accuracy - 92.0) / 8.0 + 0.20);
  return 0.0;
}

double relevance_recency_weight(const int bucket) {
  switch (bucket) {
    case 0: return 1.00;
    case 1: return 0.82;
    case 2: return 0.58;
    case 3: return 0.34;
    default: return 0.12;
  }
}

std::string normalized_sampling_token(std::string value, const char* fallback) {
  value.erase(
      std::remove_if(value.begin(), value.end(), [](const unsigned char ch) {
        return std::isspace(ch) != 0;
      }),
      value.end());
  std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return value.empty() ? std::string(fallback) : value;
}

std::string sampling_opening_family(const ProfileGameMetadata& game) {
  // Prefer the already persisted opening name family so statistically distinct
  // openings such as Italian Game and Ruy Lopez are not collapsed into the
  // same ECO letter. Variations after ':', ',' or ';' remain grouped under the
  // family; ECO is only the deterministic fallback when no name is available.
  if (!game.opening_name.empty()) {
    std::string name = game.opening_name;
    const auto separator = name.find_first_of(":,;");
    if (separator != std::string::npos) name.resize(separator);
    return "name:" + normalized_sampling_token(name, "unknown");
  }
  if (!game.opening_eco.empty()) {
    return "eco:" + normalized_sampling_token(game.opening_eco, "unknown");
  }
  return "unknown";
}

std::string sampling_ending_phase(const ProfileGameMetadata& game) {
  // This is a metadata proxy only; exact chess phase evidence remains owned by
  // later move/position stages. The coarse bands are sufficient for sampling
  // representation without opening PGNs or board states.
  if (game.plies <= 0) return "unknown";
  if (game.plies <= 24) return "early";
  if (game.plies <= 70) return "middle";
  return "late";
}

std::string sampling_opponent_strength(const ProfileGameMetadata& game) {
  if (!game.player_rating.has_value() || !game.opponent_rating.has_value()) {
    return "unknown";
  }
  const int delta = *game.opponent_rating - *game.player_rating;
  if (delta >= 150) return "much_stronger";
  if (delta >= 50) return "stronger";
  if (delta <= -150) return "much_weaker";
  if (delta <= -50) return "weaker";
  return "similar";
}


template <typename Key>
double effective_category_count(
    const std::map<Key, std::size_t>& counts, const std::size_t total) {
  if (total == 0 || counts.empty()) return 0.0;
  double squared_share_sum = 0.0;
  for (const auto& [key, count] : counts) {
    (void)key;
    if (count == 0) continue;
    const double share = static_cast<double>(count) / static_cast<double>(total);
    squared_share_sum += share * share;
  }
  return squared_share_sum <= 0.0 ? 0.0 : 1.0 / squared_share_sum;
}

template <typename Key>
double simpson_diversity(
    const std::map<Key, std::size_t>& counts, const std::size_t total) {
  if (total == 0 || counts.empty()) return 0.0;
  double squared_share_sum = 0.0;
  for (const auto& [key, count] : counts) {
    (void)key;
    if (count == 0) continue;
    const double share = static_cast<double>(count) / static_cast<double>(total);
    squared_share_sum += share * share;
  }
  return std::clamp(1.0 - squared_share_sum, 0.0, 1.0);
}


bool is_accuracy_outlier(const ProfileGameMetadata& game) {
  if (!game.accuracy.has_value()) return false;
  const double accuracy = std::clamp(*game.accuracy, 0.0, 100.0);
  return accuracy <= 68.0 || accuracy >= 94.0;
}



double position_classification_score(const std::string& classification) {
  if (classification == "blunder") return 1.00;
  if (classification == "mistake") return 0.82;
  if (classification == "miss") return 0.68;
  if (classification == "inaccuracy") return 0.56;
  if (classification == "critical") return 0.50;
  if (classification == "brilliant") return 0.38;
  return 0.0;
}

bool decisive_existing_classification(const std::string& classification) {
  return classification == "blunder" || classification == "mistake" ||
      classification == "miss" || classification == "inaccuracy" ||
      classification == "critical";
}

bool profile_interest_match(
    const ProfileGameMetadata& game,
    const ProfileRelevanceContext& context) {
  if (contains_token(context.priority_game_ids, game.game_id)) return true;
  if (context.prioritize_losses && game.outcome == "loss") return true;
  if (contains_token(context.priority_time_controls, game.time_control)) return true;
  if (contains_token(context.priority_openings, opening_key(game))) return true;
  if (context.prioritize_endgame_length && game.plies >= 70) return true;
  return false;
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Shared recency classification
// -----------------------------------------------------------------------------

int profile_age_bucket(
    const std::int64_t played_at,
    const std::int64_t now_epoch_seconds) {
  if (played_at <= 0) return 4;
  if (now_epoch_seconds <= played_at) return 0;
  const auto age_days = (now_epoch_seconds - played_at) / kDay;
  if (age_days <= 30) return 0;
  if (age_days <= 90) return 1;
  if (age_days <= 180) return 2;
  if (age_days <= 365) return 3;
  return 4;
}



// -----------------------------------------------------------------------------
// Section: Stage 2 all-game metadata classification
// -----------------------------------------------------------------------------

ProfileMetadataSummary summarize_profile_metadata(
    const std::vector<ProfileGameMetadata>& games,
    const std::int64_t now_epoch_seconds) {
  ProfileMetadataSummary summary;
  for (const auto& game : games) {
    if (game.game_id.empty()) continue;
    ++summary.total_games;
    if (game.has_complete_analysis) ++summary.analyzed_games;
    else ++summary.metadata_only_games;
    if (game.accuracy.has_value()) ++summary.games_with_accuracy;
    if (metadata_error_score(game) > 0.0) ++summary.games_with_error_signal;

    const int bucket = profile_age_bucket(game.played_at, now_epoch_seconds);
    if (bucket == 0) ++summary.recent_30d;
    if (bucket <= 1) ++summary.recent_90d;
    if (bucket >= 4) ++summary.older_than_year;
  }
  return summary;
}

// -----------------------------------------------------------------------------
// Section: Initial twenty-game representative sample
// -----------------------------------------------------------------------------

std::vector<InitialSampleEntry> build_initial_profile_sample(
    const std::vector<ProfileGameMetadata>& games,
    const std::int64_t now_epoch_seconds,
    const std::size_t limit) {
  if (games.empty() || limit == 0) return {};

  const std::size_t eligible_count = static_cast<std::size_t>(std::count_if(
      games.begin(), games.end(),
      [](const ProfileGameMetadata& game) { return game.sample_eligible; }));
  const std::size_t target = std::min(limit, eligible_count);
  std::vector<InitialSampleEntry> result;
  result.reserve(target);
  std::unordered_set<std::string> selected;
  std::unordered_map<std::string, int> time_controls;
  std::unordered_map<std::string, int> outcomes;
  std::unordered_map<std::string, int> openings;
  std::unordered_map<int, int> ages;
  int analyzed_count = 0;
  int metadata_only_count = 0;

  while (result.size() < target) {
    const ProfileGameMetadata* best = nullptr;
    double best_score = -std::numeric_limits<double>::infinity();

    for (const auto& game : games) {
      if (!game.sample_eligible || game.game_id.empty() ||
          selected.contains(game.game_id)) {
        continue;
      }
      const int age_bucket = profile_age_bucket(game.played_at, now_epoch_seconds);
      const double score =
          recency_score(age_bucket) * 0.72 + information_score(game) +
          diversity_bonus(
              game, age_bucket, time_controls, outcomes, openings, ages,
              analyzed_count, metadata_only_count);

      if (best == nullptr || score > best_score ||
          (std::abs(score - best_score) < 1e-9 && game.played_at > best->played_at) ||
          (std::abs(score - best_score) < 1e-9 && game.played_at == best->played_at &&
           game.game_id < best->game_id)) {
        best = &game;
        best_score = score;
      }
    }

    if (best == nullptr) break;
    result.push_back(InitialSampleEntry{.game_id = best->game_id, .score = best_score});
    selected.insert(best->game_id);
    ++time_controls[best->time_control];
    ++outcomes[best->outcome];
    ++openings[opening_key(*best)];
    ++ages[profile_age_bucket(best->played_at, now_epoch_seconds)];
    if (best->has_complete_analysis) ++analyzed_count;
    else ++metadata_only_count;
  }

  return result;
}

// -----------------------------------------------------------------------------
// Section: Stage 2 recency-aware relevance ranking
// -----------------------------------------------------------------------------

std::vector<ProfileGameRelevance> rank_profile_games_by_relevance(
    const std::vector<ProfileGameMetadata>& games,
    const std::int64_t now_epoch_seconds,
    const ProfileRelevanceContext& context) {
  std::vector<ProfileGameRelevance> ranked;
  ranked.reserve(games.size());

  for (const auto& game : games) {
    if (game.game_id.empty()) continue;

    const int age_bucket = profile_age_bucket(game.played_at, now_epoch_seconds);
    const double error_signal = metadata_error_score(game);
    const double accuracy_signal = accuracy_interest_score(game);

    // Date owns the largest single share of the score. Everything else may
    // promote an old game, but an ordinary old game naturally sinks toward
    // metadata-only processing instead of consuming engine time.
    double score = relevance_recency_weight(age_bucket) * 0.48;
    score += error_signal * 0.23;
    score += accuracy_signal * 0.10;

    if (game.has_complete_analysis) score += 0.07;
    else if (game.evidence_level == ProfileEvidenceLevel::metadata_only) score += 0.025;

    if (contains_token(context.priority_game_ids, game.game_id)) score += 0.18;
    if (context.prioritize_losses && game.outcome == "loss") score += 0.06;
    if (contains_token(context.priority_time_controls, game.time_control)) score += 0.08;
    if (contains_token(context.priority_openings, opening_key(game))) score += 0.08;

    // Long games are useful only as a weak proxy for possible late-game
    // evidence. A later candidate-position stage must verify the actual phase.
    if (context.prioritize_endgame_length && game.plies >= 70) score += 0.04;

    // Missing timestamps must never accidentally become "today" and jump to
    // the front of the queue. They remain processable but low-priority.
    if (game.played_at <= 0) score *= 0.25;

    ranked.push_back(ProfileGameRelevance{
        .game_id = game.game_id,
        .score = std::clamp(score, 0.0, 1.0),
        .age_bucket = age_bucket,
        .recent = age_bucket <= 1,
        .has_error_signal = error_signal > 0.0,
        .has_existing_analysis = game.has_complete_analysis,
    });
  }

  std::stable_sort(
      ranked.begin(), ranked.end(),
      [](const ProfileGameRelevance& left, const ProfileGameRelevance& right) {
        if (std::abs(left.score - right.score) > 1e-9) return left.score > right.score;
        if (left.age_bucket != right.age_bucket) return left.age_bucket < right.age_bucket;
        return left.game_id < right.game_id;
      });
  return ranked;
}


// -----------------------------------------------------------------------------
// Section: Stage 3 representative historical sampling strata
// -----------------------------------------------------------------------------

namespace {

constexpr std::size_t kSamplingHierarchyDepth = 8;

std::string sampling_hierarchy_value(
    const ProfileSamplingCandidate& candidate, const std::size_t depth) {
  switch (depth) {
    case 0: return candidate.player_color;
    case 1: return candidate.opening_family;
    case 2: return candidate.outcome;
    case 3: return candidate.time_control;
    case 4: return candidate.termination_type;
    case 5: return std::to_string(candidate.age_bucket);
    case 6: return candidate.opponent_strength;
    case 7: return candidate.ending_phase;
    default: return "unknown";
  }
}

std::string sampling_primary_hierarchy_key(
    const ProfileSamplingCandidate& candidate) {
  std::ostringstream out;
  out << "color=" << candidate.player_color
      << "|opening=" << candidate.opening_family
      << "|result=" << candidate.outcome
      << "|tc=" << candidate.time_control
      << "|termination=" << candidate.termination_type;
  return out.str();
}

template <typename Key>
std::vector<std::size_t> proportional_quotas(
    const std::vector<std::pair<Key, std::size_t>>& groups,
    const std::size_t seats,
    const std::size_t population) {
  std::vector<std::size_t> quotas(groups.size(), 0);
  if (groups.empty() || seats == 0 || population == 0) return quotas;

  struct Remainder {
    std::size_t index{0};
    double value{0.0};
  };
  std::vector<Remainder> remainders;
  remainders.reserve(groups.size());

  std::size_t assigned = 0;
  for (std::size_t i = 0; i < groups.size(); ++i) {
    const double exact = static_cast<double>(seats) *
        static_cast<double>(groups[i].second) /
        static_cast<double>(population);
    const auto base = std::min(
        groups[i].second, static_cast<std::size_t>(std::floor(exact)));
    quotas[i] = base;
    assigned += base;
    remainders.push_back(Remainder{.index = i, .value = exact - base});
  }

  std::stable_sort(
      remainders.begin(), remainders.end(),
      [&](const Remainder& left, const Remainder& right) {
        if (std::abs(left.value - right.value) > 1e-12) {
          return left.value > right.value;
        }
        const auto left_population = groups[left.index].second;
        const auto right_population = groups[right.index].second;
        if (left_population != right_population) {
          return left_population > right_population;
        }
        return groups[left.index].first < groups[right.index].first;
      });

  while (assigned < seats) {
    bool progressed = false;
    for (const auto& remainder : remainders) {
      if (assigned >= seats) break;
      const auto index = remainder.index;
      if (quotas[index] >= groups[index].second) continue;
      ++quotas[index];
      ++assigned;
      progressed = true;
    }
    if (!progressed) break;
  }
  return quotas;
}

}  // namespace

std::string ProfileSamplingCandidate::hierarchy_key() const {
  return sampling_primary_hierarchy_key(*this);
}

ProfileSamplingPopulation build_profile_sampling_population(
    const std::vector<ProfileGameMetadata>& games,
    const std::int64_t now_epoch_seconds) {
  ProfileSamplingPopulation population;
  population.total_games = games.size();
  population.candidates.reserve(games.size());

  for (const auto& game : games) {
    if (game.game_id.empty()) continue;
    if (!game.sample_eligible) {
      ++population.excluded_games;
      continue;
    }

    population.candidates.push_back(ProfileSamplingCandidate{
        .game_id = game.game_id,
        .opening_family = sampling_opening_family(game),
        .player_color = normalized_sampling_token(game.player_color, "unknown"),
        .outcome = normalized_sampling_token(game.outcome, "unknown"),
        .time_control = normalized_sampling_token(game.time_control, "unknown"),
        .termination_type = normalized_sampling_token(
            game.termination_type, "unknown"),
        .age_bucket = profile_age_bucket(game.played_at, now_epoch_seconds),
        .ending_phase = sampling_ending_phase(game),
        .opponent_strength = sampling_opponent_strength(game),
    });
    ++population.eligible_games;
  }

  std::stable_sort(
      population.candidates.begin(), population.candidates.end(),
      [](const ProfileSamplingCandidate& left,
         const ProfileSamplingCandidate& right) {
        return left.game_id < right.game_id;
      });
  return population;
}

ProfileSamplingRequirement estimate_profile_sampling_requirement(
    const ProfileSamplingPopulation& population,
    const ProfileSamplingPolicy& policy) {
  ProfileSamplingRequirement requirement;
  requirement.total_games = population.total_games;
  requirement.eligible_games = population.eligible_games;
  requirement.excluded_games = population.excluded_games;

  if (population.eligible_games == 0 || policy.maximum_games == 0) {
    return requirement;
  }

  requirement.maximum_games = std::min(
      policy.maximum_games, population.eligible_games);
  requirement.minimum_games = std::min(
      policy.minimum_games, requirement.maximum_games);

  std::map<std::string, std::size_t> openings;
  std::map<std::string, std::size_t> colors;
  std::map<std::string, std::size_t> outcomes;
  std::map<std::string, std::size_t> time_controls;
  std::map<std::string, std::size_t> terminations;
  std::map<int, std::size_t> age_buckets;
  std::map<std::string, std::size_t> ending_phases;
  std::map<std::string, std::size_t> opponent_strengths;
  std::map<std::string, std::size_t> primary_paths;

  for (const auto& candidate : population.candidates) {
    ++openings[candidate.opening_family];
    ++colors[candidate.player_color];
    ++outcomes[candidate.outcome];
    ++time_controls[candidate.time_control];
    ++terminations[candidate.termination_type];
    ++age_buckets[candidate.age_bucket];
    ++ending_phases[candidate.ending_phase];
    ++opponent_strengths[candidate.opponent_strength];
    ++primary_paths[candidate.hierarchy_key()];
  }

  std::vector<std::size_t> path_populations;
  path_populations.reserve(primary_paths.size());
  double path_squared_share_sum = 0.0;
  for (const auto& [key, count] : primary_paths) {
    (void)key;
    path_populations.push_back(count);
    const double share = static_cast<double>(count) /
        static_cast<double>(population.eligible_games);
    path_squared_share_sum += share * share;
  }

  std::sort(path_populations.begin(), path_populations.end(), std::greater<>());
  const double coverage_target = std::clamp(
      policy.diversity_coverage_target, 0.0, 1.0);
  const std::size_t target_population = static_cast<std::size_t>(std::ceil(
      static_cast<double>(population.eligible_games) * coverage_target));
  std::size_t covered_population = 0;
  for (const auto count : path_populations) {
    if (covered_population >= target_population) break;
    covered_population += count;
    ++requirement.required_strata;
  }
  requirement.covered_population_share = population.eligible_games == 0
      ? 0.0
      : std::clamp(
          static_cast<double>(covered_population) /
              static_cast<double>(population.eligible_games),
          0.0, 1.0);

  struct DimensionDiversity {
    double weight;
    double effective_categories;
    double diversity;
  };
  const std::array dimensions{
      DimensionDiversity{2.40, effective_category_count(openings, population.eligible_games),
                         simpson_diversity(openings, population.eligible_games)},
      DimensionDiversity{1.00, effective_category_count(colors, population.eligible_games),
                         simpson_diversity(colors, population.eligible_games)},
      DimensionDiversity{1.70, effective_category_count(outcomes, population.eligible_games),
                         simpson_diversity(outcomes, population.eligible_games)},
      DimensionDiversity{2.00, effective_category_count(time_controls, population.eligible_games),
                         simpson_diversity(time_controls, population.eligible_games)},
      DimensionDiversity{1.40, effective_category_count(terminations, population.eligible_games),
                         simpson_diversity(terminations, population.eligible_games)},
      DimensionDiversity{1.00, effective_category_count(age_buckets, population.eligible_games),
                         simpson_diversity(age_buckets, population.eligible_games)},
      DimensionDiversity{0.80, effective_category_count(ending_phases, population.eligible_games),
                         simpson_diversity(ending_phases, population.eligible_games)},
      DimensionDiversity{0.80, effective_category_count(opponent_strengths, population.eligible_games),
                         simpson_diversity(opponent_strengths, population.eligible_games)},
  };

  double total_weight = 0.0;
  double weighted_diversity = 0.0;
  double weighted_effective_excess = 0.0;
  for (const auto& dimension : dimensions) {
    total_weight += dimension.weight;
    weighted_diversity += dimension.weight * dimension.diversity;
    weighted_effective_excess += dimension.weight *
        std::max(0.0, dimension.effective_categories - 1.0);
  }
  requirement.diversity_score = total_weight <= 0.0
      ? 0.0
      : std::clamp(weighted_diversity / total_weight, 0.0, 1.0);

  // Only observed primary hierarchy paths contribute here. No theoretical
  // cross-product is materialized, so missing combinations cannot inflate the
  // requested sample size.
  const double effective_primary_paths = path_squared_share_sum <= 0.0
      ? 1.0
      : 1.0 / path_squared_share_sum;

  const double adaptive_extra =
      0.65 * weighted_effective_excess +
      std::log2(1.0 + effective_primary_paths) +
      0.40 * std::sqrt(static_cast<double>(requirement.required_strata));
  const std::size_t uncapped_recommendation = requirement.minimum_games +
      static_cast<std::size_t>(std::ceil(std::max(0.0, adaptive_extra)));
  requirement.capped = uncapped_recommendation > requirement.maximum_games;
  requirement.recommended_games = std::min(
      requirement.maximum_games,
      std::max(requirement.minimum_games, uncapped_recommendation));
  return requirement;
}

ProfileHistoricalSample select_profile_historical_sample(
    const ProfileSamplingPopulation& population,
    const std::vector<ProfileGameRelevance>& ranked,
    const ProfileSamplingPolicy& policy) {
  ProfileHistoricalSample result;
  result.requirement = estimate_profile_sampling_requirement(population, policy);
  if (population.eligible_games == 0 || population.candidates.empty() ||
      result.requirement.recommended_games == 0) {
    return result;
  }

  const std::size_t target = result.requirement.recommended_games;
  const double representation_fraction =
      std::clamp(policy.representation_fraction, 0.0, 1.0);
  const std::size_t representation_target = std::min(
      target,
      static_cast<std::size_t>(std::ceil(
          static_cast<double>(target) * representation_fraction)));

  std::unordered_map<std::string, double> relevance_by_id;
  relevance_by_id.reserve(ranked.size());
  for (const auto& item : ranked) {
    relevance_by_id[item.game_id] = std::clamp(item.score, 0.0, 1.0);
  }

  std::unordered_map<std::string, const ProfileSamplingCandidate*> candidate_by_id;
  candidate_by_id.reserve(population.candidates.size());
  for (const auto& candidate : population.candidates) {
    candidate_by_id.emplace(candidate.game_id, &candidate);
  }

  std::vector<const ProfileSamplingCandidate*> root;
  root.reserve(population.candidates.size());
  for (const auto& candidate : population.candidates) root.push_back(&candidate);

  std::unordered_set<std::string> selected_ids;
  selected_ids.reserve(target * 2);

  auto append_candidate = [&](const ProfileSamplingCandidate& candidate,
                              const bool priority_seat) {
    if (result.games.size() >= target || selected_ids.contains(candidate.game_id)) {
      return false;
    }
    result.games.push_back(ProfileHistoricalSampleEntry{
        .game_id = candidate.game_id,
        .stratum_key = candidate.hierarchy_key(),
        .relevance = relevance_by_id.contains(candidate.game_id)
            ? relevance_by_id[candidate.game_id] : 0.0,
        .priority_seat = priority_seat,
    });
    selected_ids.insert(candidate.game_id);
    return true;
  };

  // Recursively allocate the representative majority. Every level preserves
  // the *conditional* distribution inside its parent. This is what allows an
  // observed combination such as White -> Italian -> win -> rapid -> mate to
  // receive seats when its actual population share justifies them, without
  // allocating one seat to every tiny theoretical combination.
  std::function<void(
      const std::vector<const ProfileSamplingCandidate*>&,
      std::size_t,
      std::size_t)> select_representative;
  select_representative = [&](const auto& members,
                              const std::size_t seats,
                              const std::size_t depth) {
    if (members.empty() || seats == 0 || result.games.size() >= target) return;
    const std::size_t bounded_seats = std::min(seats, members.size());

    if (depth >= kSamplingHierarchyDepth || bounded_seats >= members.size()) {
      std::vector<const ProfileSamplingCandidate*> ordered = members;
      std::stable_sort(
          ordered.begin(), ordered.end(),
          [&](const auto* left, const auto* right) {
            const double left_score = relevance_by_id.contains(left->game_id)
                ? relevance_by_id[left->game_id] : 0.0;
            const double right_score = relevance_by_id.contains(right->game_id)
                ? relevance_by_id[right->game_id] : 0.0;
            if (std::abs(left_score - right_score) > 1e-9) {
              return left_score > right_score;
            }
            return left->game_id < right->game_id;
          });
      for (std::size_t i = 0; i < bounded_seats; ++i) {
        append_candidate(*ordered[i], false);
      }
      return;
    }

    std::map<std::string, std::vector<const ProfileSamplingCandidate*>> by_value;
    for (const auto* candidate : members) {
      by_value[sampling_hierarchy_value(*candidate, depth)].push_back(candidate);
    }
    if (by_value.size() <= 1) {
      select_representative(members, bounded_seats, depth + 1);
      return;
    }

    std::vector<std::pair<std::string, std::size_t>> group_sizes;
    group_sizes.reserve(by_value.size());
    for (const auto& [key, grouped] : by_value) {
      group_sizes.emplace_back(key, grouped.size());
    }
    const auto quotas = proportional_quotas(
        group_sizes, bounded_seats, members.size());
    for (std::size_t i = 0; i < group_sizes.size(); ++i) {
      const auto quota = quotas[i];
      if (quota == 0) continue;
      const auto group_it = by_value.find(group_sizes[i].first);
      if (group_it == by_value.end()) continue;
      select_representative(group_it->second, quota, depth + 1);
    }
  };

  select_representative(root, representation_target, 0);

  // The bounded tail may follow relevance / active-learning priorities, but it
  // cannot let one primary hierarchy path monopolize the sample. Its allowance
  // is proportional to the path's real population share times the configured
  // bounded multiplier.
  std::unordered_map<std::string, std::size_t> path_population;
  std::unordered_map<std::string, std::size_t> selected_per_path;
  for (const auto& candidate : population.candidates) {
    ++path_population[candidate.hierarchy_key()];
  }
  for (const auto& entry : result.games) {
    ++selected_per_path[entry.stratum_key];
  }

  const double multiplier_cap = std::max(1.0, policy.maximum_priority_multiplier);
  const std::size_t priority_target = target > result.games.size()
      ? target - result.games.size() : 0;
  std::size_t priority_seats = 0;

  auto path_priority_cap = [&](const std::string& path) {
    const auto population_it = path_population.find(path);
    if (population_it == path_population.end() || population.eligible_games == 0) {
      return std::size_t{0};
    }
    const double expected = static_cast<double>(target) *
        static_cast<double>(population_it->second) /
        static_cast<double>(population.eligible_games);
    return std::min(
        population_it->second,
        std::max<std::size_t>(
            1, static_cast<std::size_t>(std::ceil(expected * multiplier_cap))));
  };

  for (const auto& relevance : ranked) {
    if (priority_seats >= priority_target) break;
    const auto candidate_it = candidate_by_id.find(relevance.game_id);
    if (candidate_it == candidate_by_id.end() || candidate_it->second == nullptr) {
      continue;
    }
    const auto& candidate = *candidate_it->second;
    if (selected_ids.contains(candidate.game_id)) continue;
    const auto path = candidate.hierarchy_key();
    if (selected_per_path[path] >= path_priority_cap(path)) continue;
    if (append_candidate(candidate, true)) {
      ++selected_per_path[path];
      ++priority_seats;
    }
  }

  // Correlated dimensions can make all path caps collectively too tight for a
  // very small/discrete library. Fill any remaining seats deterministically by
  // relevance rather than returning fewer games than the native requirement.
  if (result.games.size() < target) {
    std::vector<const ProfileSamplingCandidate*> remaining;
    remaining.reserve(population.candidates.size() - result.games.size());
    for (const auto& candidate : population.candidates) {
      if (!selected_ids.contains(candidate.game_id)) remaining.push_back(&candidate);
    }
    std::stable_sort(
        remaining.begin(), remaining.end(),
        [&](const auto* left, const auto* right) {
          const double left_score = relevance_by_id.contains(left->game_id)
              ? relevance_by_id[left->game_id] : 0.0;
          const double right_score = relevance_by_id.contains(right->game_id)
              ? relevance_by_id[right->game_id] : 0.0;
          if (std::abs(left_score - right_score) > 1e-9) {
            return left_score > right_score;
          }
          return left->game_id < right->game_id;
        });
    for (const auto* candidate : remaining) {
      if (result.games.size() >= target) break;
      if (append_candidate(*candidate, true)) ++priority_seats;
    }
  }

  if (result.games.size() > target) result.games.resize(target);

  // Stage-3 coverage is the share of the *eligible historical population*
  // represented by at least one selected observed hierarchy path. This stays
  // separate from profile confidence: one representative game can cover a
  // metadata stratum without providing enough analytical evidence to make a
  // high-confidence chess claim about it.
  std::unordered_set<std::string> covered_paths;
  covered_paths.reserve(result.games.size());
  for (const auto& entry : result.games) covered_paths.insert(entry.stratum_key);
  std::size_t covered_population = 0;
  for (const auto& [path, count] : path_population) {
    if (!covered_paths.contains(path)) continue;
    covered_population += count;
    ++result.covered_strata;
  }
  if (population.eligible_games > 0) {
    result.covered_population_share = std::clamp(
        static_cast<double>(covered_population) /
            static_cast<double>(population.eligible_games),
        0.0, 1.0);
  }
  return result;
}

// -----------------------------------------------------------------------------
// Section: Stage 4 interesting-game filter
// -----------------------------------------------------------------------------

std::vector<InterestingProfileGame> select_interesting_profile_games(
    const std::vector<ProfileGameMetadata>& games,
    const std::vector<ProfileGameRelevance>& ranked,
    const ProfileRelevanceContext& context,
    const InterestingGamePolicy& policy) {
  if (games.empty() || ranked.empty() || policy.maximum_candidates == 0) return {};

  std::unordered_map<std::string, const ProfileGameMetadata*> by_id;
  by_id.reserve(games.size());
  for (const auto& game : games) {
    if (!game.game_id.empty()) by_id.emplace(game.game_id, &game);
  }

  const double fraction = std::clamp(policy.target_fraction, 0.0, 1.0);
  const std::size_t proportional = static_cast<std::size_t>(
      std::ceil(static_cast<double>(ranked.size()) * fraction));
  const std::size_t bounded_min = std::min(policy.minimum_candidates, ranked.size());
  const std::size_t target = std::min(
      policy.maximum_candidates,
      std::min(ranked.size(), std::max(bounded_min, proportional)));

  // Build an age-balanced quota before filtering.  A cold profile often has no
  // accuracy/error signal for older unanalysed games; requiring such a signal
  // would make those games impossible to discover.  The quota keeps recency as
  // a ranking factor while ensuring every populated age band contributes a
  // proportional share of the bounded candidate budget.
  std::array<std::size_t, 5> populations{};
  for (const auto& relevance : ranked) {
    const int bucket = std::clamp(relevance.age_bucket, 0, 4);
    ++populations[static_cast<std::size_t>(bucket)];
  }

  std::array<std::size_t, 5> quotas{};
  std::array<double, 5> remainders{};
  std::size_t non_empty_buckets = 0;
  for (const auto count : populations) {
    if (count > 0) ++non_empty_buckets;
  }

  std::size_t assigned = 0;
  if (target >= non_empty_buckets) {
    for (std::size_t bucket = 0; bucket < populations.size(); ++bucket) {
      if (populations[bucket] == 0) continue;
      quotas[bucket] = 1;
      ++assigned;
    }
  }

  const std::size_t remaining = target > assigned ? target - assigned : 0;
  const std::size_t population_total = ranked.size();
  for (std::size_t bucket = 0; bucket < populations.size(); ++bucket) {
    if (populations[bucket] == 0 || remaining == 0) continue;
    const double exact = static_cast<double>(remaining) *
        static_cast<double>(populations[bucket]) /
        static_cast<double>(population_total);
    const auto extra = static_cast<std::size_t>(std::floor(exact));
    const auto capacity = populations[bucket] - quotas[bucket];
    const auto accepted = std::min(extra, capacity);
    quotas[bucket] += accepted;
    assigned += accepted;
    remainders[bucket] = exact - std::floor(exact);
  }

  while (assigned < target) {
    std::size_t best_bucket = populations.size();
    double best_remainder = -1.0;
    for (std::size_t bucket = 0; bucket < populations.size(); ++bucket) {
      if (quotas[bucket] >= populations[bucket]) continue;
      if (remainders[bucket] > best_remainder) {
        best_remainder = remainders[bucket];
        best_bucket = bucket;
      }
    }
    if (best_bucket >= populations.size()) break;
    ++quotas[best_bucket];
    ++assigned;
    // After receiving the largest-remainder seat, keep subsequent fallback
    // distribution deterministic instead of repeatedly favouring one bucket.
    remainders[best_bucket] = -1.0;
  }

  std::vector<InterestingProfileGame> selected;
  selected.reserve(target);
  std::unordered_set<std::string> selected_ids;
  std::array<std::size_t, 5> selected_per_bucket{};

  auto append = [&](const ProfileGameRelevance& relevance) {
    if (selected.size() >= target || selected_ids.contains(relevance.game_id)) return false;
    const auto it = by_id.find(relevance.game_id);
    if (it == by_id.end() || it->second == nullptr) return false;
    const auto& game = *it->second;
    const int bucket = std::clamp(relevance.age_bucket, 0, 4);
    if (selected_per_bucket[static_cast<std::size_t>(bucket)] >=
        quotas[static_cast<std::size_t>(bucket)]) {
      return false;
    }
    const bool accuracy_outlier = is_accuracy_outlier(game);
    const bool profile_promoted = profile_interest_match(game, context);
    selected.push_back(InterestingProfileGame{
        .game_id = relevance.game_id,
        .relevance = relevance.score,
        .age_bucket = relevance.age_bucket,
        .recent = relevance.recent,
        .error_signal = relevance.has_error_signal,
        .accuracy_outlier = accuracy_outlier,
        .existing_analysis = relevance.has_existing_analysis,
        .profile_promoted = profile_promoted,
    });
    selected_ids.insert(relevance.game_id);
    ++selected_per_bucket[static_cast<std::size_t>(bucket)];
    return true;
  };

  // Pass 1: high-confidence metadata/evidence signals win inside each age band.
  for (const auto& relevance : ranked) {
    const auto it = by_id.find(relevance.game_id);
    if (it == by_id.end() || it->second == nullptr) continue;
    const auto& game = *it->second;
    const bool accuracy_outlier = is_accuracy_outlier(game);
    const bool concrete_signal = relevance.has_error_signal || accuracy_outlier ||
        profile_interest_match(game, context);
    const bool passes_threshold = relevance.score >= policy.minimum_relevance;
    if (concrete_signal || relevance.has_existing_analysis || passes_threshold) {
      append(relevance);
    }
  }

  // Pass 2: bootstrap missing age-band coverage from metadata alone.  These are
  // still bounded by the same target and retain ranking order, but an older
  // unanalysed game no longer needs analysis-derived evidence in order to be
  // selected for the very analysis that could discover that evidence.
  for (const auto& relevance : ranked) {
    append(relevance);
  }

  return selected;
}


// -----------------------------------------------------------------------------
// Section: Stage 5 move/position candidate filter
// -----------------------------------------------------------------------------

std::vector<ProfilePositionCandidate> select_profile_position_candidates(
    const InterestingProfileGame& game,
    const std::vector<ProfilePositionSignal>& positions,
    const ProfileCandidatePolicy& policy) {
  if (game.game_id.empty() || positions.empty() ||
      policy.maximum_candidates_per_game == 0) {
    return {};
  }

  std::vector<ProfilePositionCandidate> candidates;
  candidates.reserve(std::min(policy.maximum_candidates_per_game, positions.size()));
  bool seen_theory = false;
  bool emitted_theory_exit = false;

  for (const auto& position : positions) {
    if (position.theory) {
      seen_theory = true;
      continue;
    }

    double score = position_classification_score(position.classification) * 0.62;
    std::string reason;
    if (position_classification_score(position.classification) > 0.0) {
      reason = "existing_classification";
    }

    if (position.expected_score_loss.has_value()) {
      const double loss = std::max(0.0, *position.expected_score_loss);
      if (loss >= policy.minimum_expected_score_loss) {
        score += std::min(1.0, loss / 0.35) * 0.38;
        reason = reason.empty() ? "score_loss" : "classification_and_score_loss";
      }
    }

    // The first move after known opening theory is useful as a cheap plan/
    // transition candidate, but it receives only a modest boost and cannot
    // outrank a concrete major error by itself.
    if (seen_theory && !emitted_theory_exit) {
      score += 0.16;
      if (reason.empty()) reason = "theory_exit";
      emitted_theory_exit = true;
    }

    // Profile-promoted or accuracy-outlier games may contribute one contextual
    // position even when the persisted move classification is not severe.
    if ((game.profile_promoted || game.accuracy_outlier) && score > 0.0) {
      score += 0.07;
    }

    if (score < policy.minimum_candidate_score) continue;
    candidates.push_back(ProfilePositionCandidate{
        .game_id = game.game_id,
        .ply = position.ply,
        .score = std::clamp(score, 0.0, 1.0),
        .reason = reason.empty() ? "context" : reason,
        .classification = position.classification,
        .expected_score_loss = position.expected_score_loss,
        .fen_before = position.fen_before,
        .uci = position.uci,
    });
  }

  // An interesting game can legitimately have no persisted move analysis yet.
  // Do not fall back to whole-game Stockfish. Instead create at most three
  // sparse scout positions from the already loaded game moves. Those scouts
  // are still subject to the existing-evidence gate and bounded fast probe.
  if (candidates.empty() &&
      (game.recent || game.error_signal || game.accuracy_outlier || game.profile_promoted)) {
    const std::size_t scout_count = std::min<std::size_t>(
        3, std::min(policy.maximum_candidates_per_game, positions.size()));
    for (std::size_t index = 0; index < scout_count; ++index) {
      const std::size_t slot = std::min(
          positions.size() - 1,
          ((index + 1) * positions.size()) / (scout_count + 1));
      const auto& position = positions[slot];
      if (position.fen_before.empty() || position.uci.empty()) continue;
      candidates.push_back(ProfilePositionCandidate{
          .game_id = game.game_id,
          .ply = position.ply,
          .score = std::clamp(0.30 + game.relevance * 0.22, 0.0, 1.0),
          .reason = "sparse_game_scout",
          .classification = position.classification,
          .expected_score_loss = position.expected_score_loss,
          .fen_before = position.fen_before,
          .uci = position.uci,
      });
    }
  }

  std::stable_sort(
      candidates.begin(), candidates.end(),
      [](const ProfilePositionCandidate& left, const ProfilePositionCandidate& right) {
        if (std::abs(left.score - right.score) > 1e-9) return left.score > right.score;
        return left.ply < right.ply;
      });
  if (candidates.size() > policy.maximum_candidates_per_game) {
    candidates.resize(policy.maximum_candidates_per_game);
  }
  return candidates;
}

// -----------------------------------------------------------------------------
// Section: Stage 6 existing-evidence filter
// -----------------------------------------------------------------------------

std::vector<ProfileEvidenceDecision> filter_existing_profile_evidence(
    const std::vector<ProfilePositionCandidate>& candidates,
    const ProfileExistingEvidencePolicy& policy) {
  std::vector<ProfileEvidenceDecision> decisions;
  decisions.reserve(candidates.size());

  for (const auto& candidate : candidates) {
    const double loss = candidate.expected_score_loss.value_or(-1.0);
    const bool decisive_classification =
        decisive_existing_classification(candidate.classification);
    const bool decisive_loss = candidate.expected_score_loss.has_value() &&
        loss >= policy.decisive_expected_score_loss;

    if (decisive_classification && decisive_loss) {
      decisions.push_back(ProfileEvidenceDecision{
          .candidate = candidate,
          .decision = ProfileEvidenceDecisionKind::reuse_existing,
          .reason = "classification_and_loss_already_decisive",
      });
      continue;
    }

    if (candidate.expected_score_loss.has_value() &&
        loss <= policy.quiet_expected_score_loss && !decisive_classification) {
      decisions.push_back(ProfileEvidenceDecision{
          .candidate = candidate,
          .decision = ProfileEvidenceDecisionKind::discard,
          .reason = "existing_evidence_low_signal",
      });
      continue;
    }

    if (candidate.fen_before.empty()) {
      // Without a board position a new targeted engine probe cannot be made.
      // Keep strong existing evidence, otherwise end the candidate here rather
      // than falling back to an expensive whole-game analysis automatically.
      decisions.push_back(ProfileEvidenceDecision{
          .candidate = candidate,
          .decision = decisive_classification
              ? ProfileEvidenceDecisionKind::reuse_existing
              : ProfileEvidenceDecisionKind::discard,
          .reason = decisive_classification
              ? "classification_reused_without_fen"
              : "no_target_position_for_probe",
      });
      continue;
    }

    decisions.push_back(ProfileEvidenceDecision{
        .candidate = candidate,
        .decision = ProfileEvidenceDecisionKind::fast_probe,
        .reason = "targeted_evidence_missing_or_ambiguous",
    });
  }

  return decisions;
}


std::vector<ProfileProbeRequest> plan_fast_profile_probes(
    const std::vector<ProfileEvidenceDecision>& decisions,
    const double game_relevance,
    const ProfileEscalationPolicy& escalation) {
  std::vector<ProfileEvidenceDecision> ranked = decisions;
  std::stable_sort(
      ranked.begin(), ranked.end(),
      [](const ProfileEvidenceDecision& left, const ProfileEvidenceDecision& right) {
        if (std::abs(left.candidate.score - right.candidate.score) > 1e-9) {
          return left.candidate.score > right.candidate.score;
        }
        return left.candidate.ply < right.candidate.ply;
      });

  std::vector<ProfileProbeRequest> result;
  result.reserve(std::min(escalation.maximum_fast_probes_per_game, ranked.size()));
  for (const auto& decision : ranked) {
    if (result.size() >= escalation.maximum_fast_probes_per_game) break;
    if (decision.decision != ProfileEvidenceDecisionKind::fast_probe) continue;
    if (decision.candidate.fen_before.empty()) continue;

    ProfileProbeBudget budget;
    budget.tier = ProfileProbeTier::fast;
    // Profile preparation needs a player impression, not a publication-grade
    // game analysis. Stage 7 is therefore a deliberately tiny fixed-quality
    // search. Relevance decides whether a position escalates later; it never
    // raises this tier above depth 4.
    const double relevance = std::clamp(game_relevance, 0.0, 1.0);
    budget.node_limit = static_cast<std::uint64_t>(
        std::llround(8000.0 + relevance * 8000.0));
    budget.hard_depth = 4;
    budget.multi_pv = 1;
    budget.threads = 2;
    budget.hash_mb = 1024;
    budget.time_limit_ms = 300;
    budget.dynamic_early_stop = true;
    budget.early_stop_min_depth = 3;
    budget.stable_iterations = 2;
    budget.eval_tolerance_cp = 18;
    result.push_back(ProfileProbeRequest{
        .candidate = decision.candidate,
        .budget = budget,
        .reason = "missing_targeted_evidence",
    });
  }
  return result;
}

ProfileFastProbeDecision classify_fast_profile_probe(
    const ProfileFastProbeObservation& observation,
    const ProfileFastProbePolicy& policy) {
  const auto loss = observation.expected_score_loss;
  if (!loss.has_value()) {
    return ProfileFastProbeDecision{
        .observation = observation,
        .decision = ProfileFastProbeDecisionKind::verification_probe,
        .reason = "fast_probe_inconclusive",
    };
  }

  if (*loss < policy.discard_below_loss && !observation.tactical_signal &&
      !observation.mate_signal) {
    return ProfileFastProbeDecision{
        .observation = observation,
        .decision = ProfileFastProbeDecisionKind::discard,
        .reason = "fast_probe_quiet",
    };
  }

  if (observation.best_move_stable &&
      (*loss >= policy.direct_accept_from_loss || observation.mate_signal)) {
    return ProfileFastProbeDecision{
        .observation = observation,
        .decision = ProfileFastProbeDecisionKind::accept_evidence,
        .reason = "fast_probe_decisive",
    };
  }

  if (*loss >= policy.verify_from_loss || observation.tactical_signal ||
      observation.mate_signal || !observation.best_move_stable) {
    return ProfileFastProbeDecision{
        .observation = observation,
        .decision = ProfileFastProbeDecisionKind::verification_probe,
        .reason = "fast_probe_needs_verification",
    };
  }

  return ProfileFastProbeDecision{
      .observation = observation,
      .decision = ProfileFastProbeDecisionKind::accept_evidence,
      .reason = "fast_probe_sufficient",
  };
}

std::optional<ProfileProbeRequest> plan_profile_verification_probe(
    const ProfileFastProbeDecision& decision,
    const double game_relevance,
    const ProfileEscalationPolicy& escalation) {
  const double relevance = std::clamp(game_relevance, 0.0, 1.0);
  if (decision.decision != ProfileFastProbeDecisionKind::verification_probe ||
      escalation.maximum_verification_probes_per_game == 0 ||
      relevance < escalation.verification_minimum_relevance) {
    return std::nullopt;
  }

  ProfileProbeBudget budget;
  budget.tier = ProfileProbeTier::verification;
  budget.node_limit = static_cast<std::uint64_t>(
      std::llround(30000.0 + relevance * 30000.0));
  budget.hard_depth = 8;
  budget.multi_pv = 2;
  budget.threads = 2;
  budget.hash_mb = 1024;
  budget.time_limit_ms = 650;
  budget.dynamic_early_stop = true;
  budget.early_stop_min_depth = 6;
  budget.stable_iterations = 2;
  budget.eval_tolerance_cp = 12;
  return ProfileProbeRequest{
      .candidate = decision.observation.candidate,
      .budget = budget,
      .reason = decision.reason,
  };
}


ProfileVerificationDecision classify_profile_verification(
    const ProfileVerificationObservation& observation,
    const ProfileVerificationPolicy& policy) {
  if (!observation.expected_score_loss.has_value()) {
    return ProfileVerificationDecision{
        .observation = observation,
        .decision = ProfileVerificationDecisionKind::deep_probe,
        .reason = "verification_inconclusive",
    };
  }

  const double loss = *observation.expected_score_loss;
  if (loss < policy.discard_below_loss && !observation.tactical_signal &&
      !observation.mate_signal && observation.best_move_stable) {
    return ProfileVerificationDecision{
        .observation = observation,
        .decision = ProfileVerificationDecisionKind::discard,
        .reason = "verification_quiet",
    };
  }

  const bool still_complex = !observation.best_move_stable ||
      observation.tactical_signal || observation.mate_signal ||
      observation.alternative_moves_close;
  if (still_complex && (loss >= policy.deep_from_loss || observation.mate_signal ||
                        observation.alternative_moves_close)) {
    return ProfileVerificationDecision{
        .observation = observation,
        .decision = ProfileVerificationDecisionKind::deep_probe,
        .reason = "verification_still_complex",
    };
  }

  return ProfileVerificationDecision{
      .observation = observation,
      .decision = ProfileVerificationDecisionKind::accept_evidence,
      .reason = "verification_sufficient",
  };
}

std::optional<ProfileProbeRequest> plan_deep_profile_probe(
    const ProfileVerificationDecision& decision,
    const double game_relevance,
    const ProfileEscalationPolicy& escalation) {
  const double relevance = std::clamp(game_relevance, 0.0, 1.0);
  if (decision.decision != ProfileVerificationDecisionKind::deep_probe ||
      escalation.maximum_deep_probes_per_game == 0 ||
      relevance < escalation.deep_minimum_relevance) {
    return std::nullopt;
  }

  ProfileProbeBudget budget;
  budget.tier = ProfileProbeTier::deep;
  budget.node_limit = static_cast<std::uint64_t>(
      std::llround(90000.0 + relevance * 90000.0));
  budget.hard_depth = 12;
  budget.multi_pv = 2;
  budget.threads = 4;
  budget.hash_mb = 1024;
  budget.time_limit_ms = 1400;
  budget.dynamic_early_stop = true;
  budget.early_stop_min_depth = 9;
  budget.stable_iterations = 3;
  budget.eval_tolerance_cp = 8;
  return ProfileProbeRequest{
      .candidate = decision.observation.candidate,
      .budget = budget,
      .reason = decision.reason,
  };
}

}  // namespace kchess::ai
