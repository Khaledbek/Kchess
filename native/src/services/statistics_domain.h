#pragma once

#include <algorithm>
#include <cctype>
#include <cmath>
#include <nlohmann/json.hpp>
#include <string>

namespace kchess::statistics {

// -----------------------------------------------------------------------------
// Section: Shared statistics outcome and tally rules
// -----------------------------------------------------------------------------

struct Tally {
  int games{0};
  int wins{0};
  int draws{0};
  int losses{0};
  int decided() const { return wins + draws + losses; }
};

inline void add_outcome(Tally& tally, const std::string& outcome) {
  tally.games += 1;
  if (outcome == "win") {
    tally.wins += 1;
  } else if (outcome == "loss") {
    tally.losses += 1;
  } else if (outcome == "draw") {
    tally.draws += 1;
  }
}

inline nlohmann::json tally_json(const Tally& tally) {
  nlohmann::json node{
      {"games", tally.games},
      {"wins", tally.wins},
      {"draws", tally.draws},
      {"losses", tally.losses},
      {"undecided", tally.games - tally.decided()},
  };
  node["winShare"] =
      tally.games > 0
          ? nlohmann::json(static_cast<double>(tally.wins) / tally.games)
          : nlohmann::json(nullptr);
  const int decided = tally.decided();
  if (decided > 0) {
    node["winRate"] = static_cast<double>(tally.wins) / decided;
    node["scorePercent"] =
        (static_cast<double>(tally.wins) + 0.5 * tally.draws) / decided;
  } else {
    node["winRate"] = nullptr;
    node["scorePercent"] = nullptr;
  }
  return node;
}

// Lower end of the Wilson score interval for a share, e.g. "of the decided
// games in this line, how large can the loss rate confidently be said to be".
//
// A raw share of a handful of games is mostly noise: three losses out of four
// looks like 75% but is one unlucky evening. The Wilson bound answers the
// question the statistics actually want - "is this share really above the
// baseline?" - and it tightens on its own as more games arrive, so no separate
// minimum-sample rule has to guess for it.
//
// `z` is the standard-normal quantile; 1.2816 is a one-sided 90% bound, which
// is deliberately softer than the textbook 1.96 because a training hint is not
// a scientific claim and must still appear after a handful of games.
inline double wilson_lower_bound(const int successes, const int trials,
                                 const double z = 1.2816) {
  if (trials <= 0 || successes < 0) return 0.0;
  const double n = trials;
  const double share = static_cast<double>(successes) / n;
  const double z_squared = z * z;
  const double centre = (share + z_squared / (2.0 * n)) / (1.0 + z_squared / n);
  const double spread = (z / (1.0 + z_squared / n)) *
      std::sqrt(share * (1.0 - share) / n + z_squared / (4.0 * n * n));
  return std::clamp(centre - spread, 0.0, 1.0);
}

// Case-insensitive equality; provider game player names and the stored profile
// handle can differ only in case.
inline bool names_equal(const std::string& a, const std::string& b) {
  if (a.empty() || a.size() != b.size()) return false;
  for (std::size_t index = 0; index < a.size(); ++index) {
    if (std::tolower(static_cast<unsigned char>(a[index])) !=
        std::tolower(static_cast<unsigned char>(b[index]))) {
      return false;
    }
  }
  return true;
}

// Which side the profile played: "white", "black", or "unknown".
inline std::string game_color(const std::string& username,
                              const std::string& white_name,
                              const std::string& black_name) {
  if (username.empty()) return "unknown";
  if (names_equal(white_name, username)) return "white";
  if (names_equal(black_name, username)) return "black";
  return "unknown";
}

// Prefer the authoritative provider outcome; otherwise derive it from the game
// result and the profile's color (covers local imports where the profile's
// username matches a player). Returns "unknown" when it cannot be determined.
inline std::string effective_outcome(const std::string& provider_outcome,
                                     const std::string& color,
                                     const std::string& result) {
  if (provider_outcome == "win" || provider_outcome == "loss" ||
      provider_outcome == "draw") {
    return provider_outcome;
  }
  if (color != "white" && color != "black") return "unknown";
  // Providers write either spelling; missing the second one silently
  // dropped draws into "unknown" for every tally on the tab.
  if (result == "1/2-1/2" || result == "\xc2\xbd-\xc2\xbd") return "draw";
  if (result == "1-0") return color == "white" ? "win" : "loss";
  if (result == "0-1") return color == "white" ? "loss" : "win";
  return "unknown";
}

}  // namespace kchess::statistics
