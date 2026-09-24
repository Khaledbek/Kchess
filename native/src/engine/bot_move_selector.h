#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "engine/chess_engine.h"

namespace kchess {

// -----------------------------------------------------------------------------
// Section: Bot difficulty policy
// -----------------------------------------------------------------------------

inline constexpr int kBotEloMinimum = 100;
inline constexpr int kBotEloMaximum = 3200;
inline constexpr int kBotEloStep = 100;
inline constexpr int kBotMaximumCandidateLines = 32;
inline constexpr int kStockfish18LowestUciElo = 1320;
inline constexpr int kStockfish18HighestUciElo = 3190;
inline constexpr int kBotNativeStrengthThreshold = 1300;

// The bot no longer performs one deep MultiPV search across every candidate.
// Elo controls a cheap scout followed by a deep verification of at most three
// root moves. This keeps low-Elo variety without paying full search cost for
// moves that will never be played.
struct BotDifficultyProfile {
  int requested_elo{kBotEloMinimum};
  double strength{0.0};
  bool use_stockfish_limit_strength{false};
  int stockfish_uci_elo{0};
  int scout_depth{5};
  int verification_depth{8};
  int maximum_target_rank{kBotMaximumCandidateLines};
  int typical_loss_cp{300};
  int maximum_verified_loss_cp{1000};
  std::uint64_t scout_node_budget{2500};
  std::uint64_t verification_node_budget{9000};
  int scout_time_budget_ms{120};
  int verification_time_budget_ms{450};
  double best_move_probability{0.15};
  double rank_bias{1.0};
};

struct BotMovePlan {
  int target_rank{1};
  int target_loss_cp{0};
  bool use_stockfish_limit_strength{false};
  int stockfish_uci_elo{0};
  int scout_depth{5};
  int scout_lines{1};
  int verification_depth{8};
  int maximum_verified_loss_cp{1000};
  std::uint64_t scout_node_budget{2500};
  std::uint64_t verification_node_budget{9000};
  int scout_time_budget_ms{120};
  int verification_time_budget_ms{450};
  bool opening_phase{false};
};

struct BotMoveChoice {
  std::string move;
  int rank{0};
  double probability{0.0};
  std::vector<double> probabilities;
};

// Returns one continuous policy rather than one handcrafted bot per Elo.
// Elo 1300..3100 delegates weakening to Stockfish 18's calibrated UCI_Elo
// mechanism. Lower ratings temporarily retain the KChess scout pipeline until
// the dedicated low-Elo centipawn-loss model replaces it; 3200 stays unrestricted.
BotDifficultyProfile bot_difficulty_profile(int requested_elo);

// Draws the intended centipawn loss BEFORE Stockfish searches. Low Elo therefore
// models human-sized inaccuracies instead of asking for an arbitrary rank. Opening
// plies clamp the loss envelope so diversity never requires an opening blunder.
BotMovePlan plan_bot_move(
    int requested_elo,
    double unit_random,
    int position_ply);

// Picks the scout candidate whose evaluated loss best matches the pre-planned
// centipawn loss while respecting a hard Elo safety envelope. Forced losing
// mates are rejected when a non-losing candidate exists.
BotMoveChoice choose_scout_bot_move(
    const std::vector<EngineLine>& scout_lines,
    const BotMovePlan& plan);

// Accepts the planned move only when the targeted deep search confirms it is
// inside the Elo loss envelope. Otherwise a verified safer alternative wins.
BotMoveChoice finalize_verified_bot_move(
    const std::vector<EngineLine>& verified_lines,
    const BotMoveChoice& planned_choice,
    const BotMovePlan& plan);

// Kept as a small compatibility helper for deterministic selector tests and
// callers that already have a complete ranked set. Runtime bot play uses the
// plan/scout/verify pipeline above.
BotMoveChoice choose_bot_move(
    const std::vector<EngineLine>& lines,
    int requested_elo,
    double unit_random);

}  // namespace kchess
