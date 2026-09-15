#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Existing KChess evidence contracts
// -----------------------------------------------------------------------------

enum class ProfileTimeControl {
  bullet,
  blitz,
  rapid,
  classical,
  other,
};

enum class ProfilePhase {
  opening,
  middlegame,
  endgame,
};

struct ProfileClassificationCounts {
  int theory{0};
  int brilliant{0};
  int critical{0};
  int best{0};
  int excellent{0};
  int miss{0};
  int mistake{0};
  int blunder{0};
  int analyzed_moves{0};
};

struct ProfilePhaseCounts {
  ProfilePhase phase{ProfilePhase::opening};
  int analyzed_moves{0};
  int miss{0};
  int mistake{0};
  int blunder{0};
};

struct ProfileMoveObservation {
  int ply{0};
  std::string classification;
  std::optional<double> expected_score_loss;
  bool theory{false};
  std::string fen_before;
  std::string uci;
};

// This is deliberately transport-like. Persistence/services populate it from
// already stored KChess data; native/ai/profile never reads SQLite directly.
struct ProfileGameObservation {
  std::string game_id;
  std::int64_t played_at{0};
  std::int64_t source_version{0};
  std::string time_control_type;
  std::string result;
  std::string provider_outcome;
  std::string player_color;
  std::optional<int> player_rating;
  std::optional<int> opponent_rating;
  std::optional<double> accuracy;
  std::string opening_eco;
  std::string opening_name;
  int move_count{0};
  bool analysis_complete{false};
  ProfileClassificationCounts classifications;
  std::array<ProfilePhaseCounts, 3> phases{};
  std::optional<double> average_expected_score_loss;
  std::optional<double> maximum_expected_score_loss;
  std::vector<ProfileMoveObservation> moves;
};

struct ProfileMoveEvidence {
  int ply{0};
  ProfilePhase phase{ProfilePhase::opening};
  std::string classification;
  std::optional<double> expected_score_loss;
  bool theory{false};
  std::string fen_before;
  std::string uci;
};

struct ProfileGameEvidence {
  std::string game_id;
  std::int64_t played_at{0};
  std::int64_t source_version{0};
  ProfileTimeControl time_control{ProfileTimeControl::other};
  std::string result;
  std::string provider_outcome;
  std::string player_color;
  std::optional<int> player_rating;
  std::optional<int> opponent_rating;
  std::optional<double> accuracy;
  std::string opening_eco;
  std::string opening_name;
  int move_count{0};
  bool analysis_complete{false};
  ProfileClassificationCounts classifications;
  std::array<ProfilePhaseCounts, 3> phases{};
  std::optional<double> average_expected_score_loss;
  std::optional<double> maximum_expected_score_loss;
  std::vector<ProfileMoveEvidence> moves;
};

class ProfileEvidenceAdapter {
 public:
  [[nodiscard]] ProfileGameEvidence adapt(
      const ProfileGameObservation& observation) const;
  [[nodiscard]] std::vector<ProfileGameEvidence> adapt(
      const std::vector<ProfileGameObservation>& observations) const;
};

[[nodiscard]] ProfileTimeControl profile_time_control(const std::string& value);
[[nodiscard]] std::string profile_time_control_id(ProfileTimeControl value);
[[nodiscard]] ProfilePhase profile_phase_for_ply(int ply);
[[nodiscard]] std::string profile_phase_id(ProfilePhase value);
[[nodiscard]] bool profile_major_error(const std::string& classification);

}  // namespace kchess::ai
