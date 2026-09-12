#pragma once

#include <optional>
#include <string>
#include <vector>

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Learned player profile contracts
// -----------------------------------------------------------------------------

struct ProfileSignal {
  std::optional<double> value;
  double confidence{0.0};
};

struct CommonMistake {
  std::string id;
  int occurrences{0};
  double rate{0.0};
  double confidence{0.0};
};

struct OpeningPattern {
  std::string eco;
  std::string name;
  int games{0};
  int wins{0};
  int draws{0};
  int losses{0};
  double confidence{0.0};
};

struct ChessProfile {
  std::string profile_id;
  std::optional<int> rating;
  std::vector<std::string> preferences;
  ProfileSignal tactical_strength;
  ProfileSignal calculation_strength;
  ProfileSignal positional_strength;
  ProfileSignal defensive_strength;
  ProfileSignal endgame_strength;
  ProfileSignal risk_tolerance;
  std::vector<CommonMistake> common_mistakes;
  std::vector<OpeningPattern> opening_patterns;
  std::vector<std::string> strengths;
  std::vector<std::string> weaknesses;
  int source_games{0};
  int source_analyzed_moves{0};
  double confidence{0.0};
};

[[nodiscard]] std::string chess_profile_json(const ChessProfile& profile);
[[nodiscard]] std::optional<ChessProfile> chess_profile_from_json(
    const std::string& payload);

}  // namespace kchess::ai
