#pragma once

#include <string>

#include "persistence/database.h"

namespace kchess {

// Computes performance statistics for the active profile by aggregating stored
// games in the native layer, so the UI receives compact summaries instead of
// the whole library. The first module is the Overview; later modules (openings,
// rating, move quality) will extend this service.
class StatisticsService {
 public:
  explicit StatisticsService(Database& database);

  // Games, results, win-rate, recent form, and splits by color and time control
  // for the active profile, as a JSON object. Returns {"hasProfile":false,...}
  // when no profile is active.
  std::string overview_json() const;

  // Openings grouped into base families with nested variations, each with
  // win/draw/loss and score, split by the color the profile played. Openings
  // come from the classified games.
  std::string openings_json() const;

  // How the active profile's games ended (checkmate, resignation, timeout,
  // draw, other), aggregated from the stored PGN Termination tags.
  std::string terminations_json() const;

  // Win/draw/loss split by the game phase in which each game ended (opening,
  // middlegame, endgame), derived from each game's final move number. This is a
  // "where games conclude" heuristic, not an engine-based blunder location.
  std::string phases_json() const;

 private:
  Database& database_;
};

}  // namespace kchess
