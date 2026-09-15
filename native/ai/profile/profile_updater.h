#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "chess_profile.h"
#include "profile_evidence_adapter.h"

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Aggregated KChess observations (legacy/fallback bridge)
// -----------------------------------------------------------------------------

struct ProfileOpeningObservation {
  std::string eco;
  std::string name;
  int games{0};
  int wins{0};
  int draws{0};
  int losses{0};
};

struct ChessProfileObservation {
  std::string profile_id;
  std::optional<int> rating;
  int games{0};
  int analyzed_moves{0};
  std::optional<double> average_accuracy;
  int theory{0};
  int brilliant{0};
  int critical{0};
  int best{0};
  int excellent{0};
  int miss{0};
  int mistake{0};
  int blunder{0};
  std::vector<ProfileOpeningObservation> openings;
};

class ChessProfileUpdater {
 public:
  [[nodiscard]] ChessProfile update(
      const ChessProfileObservation& observation,
      const std::optional<ChessProfile>& previous = std::nullopt) const;

  [[nodiscard]] ChessProfile update_from_evidence(
      const std::string& profile_id,
      const std::optional<int>& rating,
      const std::vector<ProfileGameEvidence>& games,
      std::int64_t now_seconds,
      const ProfileBackgroundProgress& background,
      const std::optional<ChessProfile>& previous = std::nullopt,
      const std::vector<ProfileGameEvidence>& library_games = {}) const;
};

[[nodiscard]] bool repeated_personal_mistake(
    const ChessProfile& profile, const std::string& classification);

}  // namespace kchess::ai
