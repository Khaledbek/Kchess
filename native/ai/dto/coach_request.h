#pragma once

#include <optional>
#include <string>

#include "../coach_types.h"

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Coach request contract
// -----------------------------------------------------------------------------

struct CoachRequest {
  std::string user_text;
  std::string locale{"en"};
  CoachMode mode{CoachMode::answer};
  ResponseDepth depth{ResponseDepth::standard};
  std::optional<std::string> position_fen;
  std::optional<std::string> game_pgn;
  std::optional<std::string> session_id;
  std::optional<std::string> user_move_uci;
  std::optional<std::string> surface;
  std::optional<std::string> context_id;
  std::optional<int> context_ply;
  std::optional<std::string> profile_id;
  std::optional<std::string> player_color;
  std::optional<std::string> hint_move_uci;
  std::optional<std::string> variation_job_id;
  bool automatic_turn{false};
};

}  // namespace kchess::ai
