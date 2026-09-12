// -----------------------------------------------------------------------------
// Section: Opening weakness detection
// -----------------------------------------------------------------------------

#include "services/opening_weakness.h"

#include <algorithm>
#include <map>
#include <set>
#include <tuple>

namespace kchess::statistics {
namespace {

constexpr int kPoorResultsMinimumDecided = 4;
constexpr int kPoorResultsMinimumLosses = 3;
constexpr double kPoorResultsLossRate = 0.6;
constexpr int kFrequentErrorsMinimumAnalysed = 3;
constexpr double kFrequentErrorsRate = 2.0 / 3.0;
constexpr double kFrequentErrorsLossRate = 0.5;
constexpr int kRecurringMinimumGames = 2;

int category_weight(const std::string& category) {
  if (category == "blunder") return 3;
  if (category == "mistake") return 2;
  if (category == "miss") return 1;
  return 0;
}

struct Slip {
  RecurringOpeningMistake mistake;
  std::set<std::size_t> games;
};

struct Group {
  std::string level, name, family, eco, color;
  Tally tally;
  int analysed{0};
  int with_errors{0};
  int errors{0};
  int blunders{0};
  std::map<std::pair<std::string, std::string>, Slip> slips;  // position, san
};

void add_game(Group& group, const OpeningGameEvidence& game, std::size_t index) {
  if (group.tally.games == 0) group.eco = game.eco;
  add_outcome(group.tally, game.outcome);
  if (!game.analysed) return;
  group.analysed += 1;
  if (!game.errors.empty()) group.with_errors += 1;
  for (const auto& error : game.errors) {
    group.errors += 1;
    if (error.category == "blunder") group.blunders += 1;
    auto& slip = group.slips[{error.position, error.san}];
    auto& mistake = slip.mistake;
    if (slip.games.empty()) {
      mistake.position = error.position;
      mistake.san = error.san;
      mistake.move_number = error.ply / 2 + 1;
      mistake.side = error.ply % 2 == 0 ? "white" : "black";
    }
    if (mistake.recommended.empty()) mistake.recommended = error.recommended;
    if (category_weight(error.category) > category_weight(mistake.category)) {
      mistake.category = error.category;
    }
    slip.games.insert(index);
  }
}

// Judges one group; nullopt when it is not weak.
std::optional<OpeningWeakness> judge(const Group& group) {
  const auto& tally = group.tally;
  const int decided = tally.decided();
  const double loss_rate =
      decided > 0 ? static_cast<double>(tally.losses) / decided : 0.0;
  const double error_rate =
      group.analysed > 0 ? static_cast<double>(group.with_errors) / group.analysed : 0.0;

  std::optional<RecurringOpeningMistake> recurring;
  for (const auto& [key, slip] : group.slips) {
    const int count = static_cast<int>(slip.games.size());
    if (count < kRecurringMinimumGames) continue;
    // The most repeated slip wins; a worse category breaks a tie.
    if (!recurring || count > recurring->count ||
        (count == recurring->count &&
         category_weight(slip.mistake.category) > category_weight(recurring->category))) {
      recurring = slip.mistake;
      recurring->count = count;
    }
  }

  const bool poor_results = decided >= kPoorResultsMinimumDecided &&
      tally.losses >= kPoorResultsMinimumLosses && loss_rate >= kPoorResultsLossRate;
  const bool frequent_errors = group.analysed >= kFrequentErrorsMinimumAnalysed &&
      error_rate >= kFrequentErrorsRate && loss_rate >= kFrequentErrorsLossRate;
  if (!poor_results && !frequent_errors && !recurring) return std::nullopt;

  // Evidence is shrunk towards zero for small samples, so a line lost four
  // times out of four does not outrank one lost fifteen times out of twenty.
  double severity =
      std::max(0.0, loss_rate - 0.5) * 2.0 * decided / (decided + 4.0) +
      error_rate * group.analysed / (group.analysed + 3.0);
  if (recurring) severity += 0.15 + 0.05 * std::min(recurring->count - 2, 3);

  OpeningWeakness weakness;
  weakness.level = group.level;
  weakness.name = group.name;
  weakness.family = group.family;
  weakness.eco = group.eco;
  weakness.color = group.color;
  weakness.tally = tally;
  weakness.analysed_games = group.analysed;
  weakness.games_with_errors = group.with_errors;
  weakness.errors = group.errors;
  weakness.blunders = group.blunders;
  weakness.poor_results = poor_results;
  weakness.frequent_errors = frequent_errors;
  weakness.recurring = recurring;
  weakness.severity = severity;
  return weakness;
}

}  // namespace

std::string opening_family(const std::string& name) {
  const auto pos = name.find_first_of(":,");
  std::string base = pos == std::string::npos ? name : name.substr(0, pos);
  const auto last = base.find_last_not_of(" \t");
  if (last == std::string::npos) return {};
  base.erase(last + 1);
  return base.substr(base.find_first_not_of(" \t"));
}

int opening_phase_end_ply(const std::optional<int> opening_ply) {
  return std::clamp(opening_ply.value_or(0) + 20, 20, kOpeningPhaseMaxPly);
}

std::vector<OpeningWeakness> find_opening_weaknesses(
    const std::vector<OpeningGameEvidence>& games, const std::size_t limit) {
  // key: name + color; variations and families kept apart.
  std::map<std::pair<std::string, std::string>, Group> variations;
  std::map<std::pair<std::string, std::string>, Group> families;
  for (std::size_t index = 0; index < games.size(); ++index) {
    const auto& game = games[index];
    if (game.name.empty() || (game.color != "white" && game.color != "black")) continue;
    const auto family = opening_family(game.name);

    auto& variation = variations[{game.name, game.color}];
    if (variation.tally.games == 0) {
      variation.level = "variation";
      variation.name = game.name;
      variation.family = family;
      variation.color = game.color;
    }
    add_game(variation, game, index);

    auto& whole = families[{family, game.color}];
    if (whole.tally.games == 0) {
      whole.level = "family";
      whole.name = family;
      whole.family = family;
      whole.color = game.color;
    }
    add_game(whole, game, index);
  }

  std::vector<OpeningWeakness> result;
  std::set<std::pair<std::string, std::string>> covered;  // family, color
  for (const auto& [key, group] : variations) {
    if (auto weakness = judge(group)) {
      covered.insert({group.family, group.color});
      result.push_back(std::move(*weakness));
    }
  }
  for (const auto& [key, group] : families) {
    if (covered.count({group.family, group.color})) continue;
    if (auto weakness = judge(group)) result.push_back(std::move(*weakness));
  }

  std::sort(result.begin(), result.end(),
            [](const OpeningWeakness& a, const OpeningWeakness& b) {
              return std::tie(b.severity, b.tally.games, a.name, a.color) <
                     std::tie(a.severity, a.tally.games, b.name, b.color);
            });
  if (result.size() > limit) result.resize(limit);
  return result;
}

}  // namespace kchess::statistics
