#include "chess_profile.h"

#include <nlohmann/json.hpp>

namespace kchess::ai {
namespace {

using json = nlohmann::json;

json signal_json(const ProfileSignal& signal) {
  json value = {{"confidence", signal.confidence}};
  if (signal.value) value["value"] = *signal.value;
  return value;
}

ProfileSignal read_signal(const json& value) {
  ProfileSignal signal;
  if (value.contains("value") && value["value"].is_number()) {
    signal.value = value["value"].get<double>();
  }
  signal.confidence = value.value("confidence", 0.0);
  return signal;
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Stable JSON persistence
// -----------------------------------------------------------------------------

std::string chess_profile_json(const ChessProfile& profile) {
  json value = {
      {"version", 1},
      {"profileId", profile.profile_id},
      {"preferences", profile.preferences},
      {"tacticalStrength", signal_json(profile.tactical_strength)},
      {"calculationStrength", signal_json(profile.calculation_strength)},
      {"positionalStrength", signal_json(profile.positional_strength)},
      {"defensiveStrength", signal_json(profile.defensive_strength)},
      {"endgameStrength", signal_json(profile.endgame_strength)},
      {"riskTolerance", signal_json(profile.risk_tolerance)},
      {"strengths", profile.strengths},
      {"weaknesses", profile.weaknesses},
      {"sourceGames", profile.source_games},
      {"sourceAnalyzedMoves", profile.source_analyzed_moves},
      {"confidence", profile.confidence},
  };
  if (profile.rating) value["rating"] = *profile.rating;

  value["commonMistakes"] = json::array();
  for (const auto& mistake : profile.common_mistakes) {
    value["commonMistakes"].push_back({
        {"id", mistake.id},
        {"occurrences", mistake.occurrences},
        {"rate", mistake.rate},
        {"confidence", mistake.confidence},
    });
  }
  value["openingPatterns"] = json::array();
  for (const auto& opening : profile.opening_patterns) {
    value["openingPatterns"].push_back({
        {"eco", opening.eco},
        {"name", opening.name},
        {"games", opening.games},
        {"wins", opening.wins},
        {"draws", opening.draws},
        {"losses", opening.losses},
        {"confidence", opening.confidence},
    });
  }
  return value.dump();
}

std::optional<ChessProfile> chess_profile_from_json(const std::string& payload) {
  try {
    const auto value = json::parse(payload);
    if (value.value("version", 0) != 1) return std::nullopt;
    ChessProfile profile;
    profile.profile_id = value.value("profileId", "");
    if (profile.profile_id.empty()) return std::nullopt;
    if (value.contains("rating") && value["rating"].is_number_integer()) {
      profile.rating = value["rating"].get<int>();
    }
    profile.preferences = value.value("preferences", std::vector<std::string>{});
    profile.tactical_strength = read_signal(value.value("tacticalStrength", json::object()));
    profile.calculation_strength = read_signal(value.value("calculationStrength", json::object()));
    profile.positional_strength = read_signal(value.value("positionalStrength", json::object()));
    profile.defensive_strength = read_signal(value.value("defensiveStrength", json::object()));
    profile.endgame_strength = read_signal(value.value("endgameStrength", json::object()));
    profile.risk_tolerance = read_signal(value.value("riskTolerance", json::object()));
    profile.strengths = value.value("strengths", std::vector<std::string>{});
    profile.weaknesses = value.value("weaknesses", std::vector<std::string>{});
    profile.source_games = value.value("sourceGames", 0);
    profile.source_analyzed_moves = value.value("sourceAnalyzedMoves", 0);
    profile.confidence = value.value("confidence", 0.0);

    for (const auto& item : value.value("commonMistakes", json::array())) {
      profile.common_mistakes.push_back(CommonMistake{
          .id = item.value("id", ""),
          .occurrences = item.value("occurrences", 0),
          .rate = item.value("rate", 0.0),
          .confidence = item.value("confidence", 0.0),
      });
    }
    for (const auto& item : value.value("openingPatterns", json::array())) {
      profile.opening_patterns.push_back(OpeningPattern{
          .eco = item.value("eco", ""),
          .name = item.value("name", ""),
          .games = item.value("games", 0),
          .wins = item.value("wins", 0),
          .draws = item.value("draws", 0),
          .losses = item.value("losses", 0),
          .confidence = item.value("confidence", 0.0),
      });
    }
    return profile;
  } catch (...) {
    return std::nullopt;
  }
}

}  // namespace kchess::ai
