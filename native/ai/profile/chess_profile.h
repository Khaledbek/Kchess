#pragma once

#include <cstdint>
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

struct ProfileExamplePosition {
  std::string game_id;
  int ply{0};
  std::string fen;
  std::string move;
  std::string classification;
};

struct PlayerPattern {
  std::string id;
  std::string type;  // strength | weakness
  double confidence{0.0};
  double severity{0.0};
  int occurrences{0};
  int sample_games{0};
  int analyzed_moves{0};
  std::optional<double> observed_error_rate;
  std::optional<double> posterior_error_lower;
  std::optional<double> posterior_error_upper;
  std::string time_control{"all"};
  std::string phase{"all"};
  bool recent{false};
  std::string trend{"insufficient_data"};
  std::vector<ProfileExamplePosition> example_positions;
};

struct TimeControlProfile {
  std::string id;
  int games{0};
  int analyzed_moves{0};
  std::optional<double> average_accuracy;
  double major_error_rate{0.0};
  double confidence{0.0};
  std::string trend{"insufficient_data"};
};

struct ProfileHypothesis {
  std::string id;
  std::string pattern_id;
  std::string status{"tentative"};
  double confidence{0.0};
  int evidence_for{0};
  int evidence_against{0};
  std::int64_t updated_at{0};
};

struct CoachPriority {
  std::string pattern_id;
  std::string role;  // main | secondary
  double score{0.0};
};

struct ProfileBackgroundProgress {
  std::string status{"idle"};
  int total_games{0};
  int history_accounts{0};
  int history_discovered_accounts{0};
  int history_available_months{0};
  int history_synced_months{0};
  int history_pending_months{0};
  bool history_complete{true};
  int indexed_games{0};
  int historical_sample_games{0};
  int historical_sample_budget{0};
  int sampling_eligible_games{0};
  int sampling_excluded_games{0};
  int sampling_minimum_games{0};
  int sampling_maximum_games{0};
  int sampling_required_strata{0};
  int sampling_covered_strata{0};
  double sampling_coverage{0.0};
  double sampling_diversity{0.0};
  bool sampling_capped{false};
  bool initial_preparation_complete{false};
  int interesting_games{0};
  int engine_promoted_games{0};
  int relevant_games{0};
  int resolved_relevant_games{0};
  int reused_analysis_games{0};
  int queued_games{0};
  int engine_pending_games{0};
  std::optional<std::string> current_game_id;
  std::int64_t updated_at{0};
};

struct ChessProfile {
  std::string profile_id;
  std::optional<int> rating;
  std::optional<double> average_accuracy;
  std::optional<int> estimated_strength_rating;
  double estimated_strength_confidence{0.0};
  int analyzed_games{0};
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
  std::vector<PlayerPattern> patterns;
  std::vector<TimeControlProfile> time_control_profiles;
  std::vector<ProfileHypothesis> hypotheses;
  std::vector<CoachPriority> coach_priorities;
  ProfileBackgroundProgress background;
  int source_games{0};
  int source_analyzed_moves{0};
  double confidence{0.0};
  std::int64_t updated_at{0};
};

[[nodiscard]] std::string chess_profile_json(const ChessProfile& profile);
[[nodiscard]] std::optional<ChessProfile> chess_profile_from_json(
    const std::string& payload);

}  // namespace kchess::ai
