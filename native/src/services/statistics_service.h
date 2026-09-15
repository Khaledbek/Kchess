// -----------------------------------------------------------------------------
// Section: Statistics service interface
// -----------------------------------------------------------------------------

#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "persistence/database.h"

namespace kchess {

// Computes performance statistics for the active profile by aggregating stored
// games in the native layer, so the UI receives compact summaries instead of
// the whole library.
class StatisticsService {
 public:
  explicit StatisticsService(Database& database);

  // Games, results, win-rate, recent form, and splits by color and time control
  // for the active profile, as a JSON object. Returns {"hasProfile":false,...}
  // when no profile is active.
  std::string overview_json() const;

  // Read-only player-wide facts over the existing shared metadata/analysis
  // source. Empty axes mean the complete known library, explicit axes form
  // separate comparison cells. No profile-preparation or engine prerequisite.
  std::string player_knowledge_json(
      const std::string& profile_id,
      const std::vector<std::string>& time_controls = {},
      const std::vector<std::string>& player_colors = {},
      std::int64_t since = 0) const;

  [[nodiscard]] std::string performance_diagnostics_json() const;

  // Openings grouped into base families with nested variations, each with
  // win/draw/loss and score, split by the color the profile played. Openings
  // come from the classified games.
  //
  // [time_control] narrows the games to one `time_control_type` ("bullet",
  // "blitz", "rapid", ...); "all" (the default) keeps every game, so the tab's
  // filter reaches the openings card instead of it silently staying all-time.
  std::string openings_json(const std::string& time_control = "all") const;
  // Accuracy over the analysed games of the active profile: average, by colour,
  // time control and game phase, the chart series, and whether the profile is
  // improving. Only games with a finished engine analysis count.
  std::string accuracy_json(const std::string& time_control = "all") const;

  // How the active profile's games ended (checkmate, resignation, timeout,
  // draw, other), aggregated from the stored PGN Termination tags.
  std::string terminations_json() const;

  // Win/draw/loss split by the game phase in which each game ended (opening,
  // middlegame, endgame), derived from each game's final move number. This is a
  // "where games conclude" heuristic, not an engine-based blunder location.
  std::string phases_json() const;

  // Native form/rating summaries of the library query's serialized rows.
  std::string timeline_json(const std::string& games_json) const;
  // Attach local comparison, head-to-head and recommendations to a scout
  // report.
  std::string comparison_json(const std::string& report_json) const;

 private:
  Database& database_;
  mutable std::atomic_uint64_t player_knowledge_requests_{0};
  mutable std::atomic_uint64_t player_knowledge_rows_scanned_{0};
  mutable std::atomic_uint64_t player_knowledge_total_ms_{0};
  mutable std::atomic_uint64_t player_knowledge_last_ms_{0};
  mutable std::atomic_uint64_t player_knowledge_cache_hits_{0};
  mutable std::atomic_uint64_t player_knowledge_cache_misses_{0};
  mutable std::mutex player_knowledge_cache_mutex_;
  mutable std::uint64_t player_knowledge_cache_revision_{0};
  mutable std::unordered_map<std::string, std::string> player_knowledge_cache_;
};

}  // namespace kchess
