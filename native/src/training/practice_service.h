#pragma once
// -----------------------------------------------------------------------------
// Section: Practice orchestration over existing training and bot services
// -----------------------------------------------------------------------------
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "services/bot_service.h"
#include "theory/opening_line_graph.h"
#include "theory/opening_name_index.h"
#include "theory/opening_theory_provider.h"
#include "training/practice_catalog.h"
#include "training/training_service.h"

namespace kchess {
class PracticeService {
 public:
  PracticeService(Database& db, TrainingService& training, BotService& bot)
      : database_(db), training_(training), bot_(bot) {}
  void set_opening_sources(
      const OpeningLineGraph& lines,
      const OpeningNameIndex& names,
      const OpeningTheoryProvider& theory);
  std::string command(const std::string& request);
  std::string overview(const std::string& training_overview) const;

  // An opening drill runs until the user has answered this many book moves.
  static constexpr int kDrillDepth = 10;

 private:
  // One legal book reply, resolved against the position it came from.
  struct DrillMove {
    std::string uci, san, fen_after;
    std::uint64_t weight{0};
    std::uint32_t games{0};
    std::uint32_t white_wins{0};
    std::uint32_t draws{0};
    std::uint32_t black_wins{0};
    std::string destination_eco, destination_name;
  };

  struct Session {
    std::string id, key, kind, initial_fen, fen, solver{"white"}, status{"active"}, job;
    std::vector<std::string> history;
    std::size_t ply{0};
    int played{0}, budget{0};
    bool clean{true}, recorded{false};
    nlohmann::json evaluation = nullptr;

    // --- opening drill state ---------------------------------------------
    // KCL graph continuations for `fen`, weighted by KCB statistics: what the
    // user may play and what the drill may draw for the opponent.
    std::vector<DrillMove> continuations;
    // Wrong tries at the position currently on the board.
    int attempts{0};
    // The book answer, revealed only once the user has missed it.
    std::string hint, hint_san;
    // The reply the drill just played, for the banner above the board.
    std::string opponent_uci, opponent_san, opponent_side;
    int opponent_number{0};
    // Legal book replies the opponent was drawing from when it played.
    int opponent_alternatives{0};
    // The user's last accepted answer, and where the book ranked it.
    std::string answer_uci, answer_san;
    int answer_rank{0};
    // Set when the drill stopped because the book had nothing left to ask.
    bool book_exhausted{false};
    // Moves of the catalogue line that sets the scenario up, in notation.
    std::vector<std::string> opening_moves;
    std::string opening_eco, opening_name;
  };
  nlohmann::json catalog();
  nlohmann::json progress(const std::string& key) const;
  nlohmann::json start(const nlohmann::json& request);
  nlohmann::json move(Session& session, const nlohmann::json& request);
  nlohmann::json poll(Session& session);
  nlohmann::json snapshot(const Session& session);
  void advance(Session& session);
  // Follows KCL continuations until the user is on move with something to
  // find, or the graph has nowhere left to go.
  void advance_drill(Session& session);
  void finish(Session& session, bool success);
  void opponent(Session& session);

  // Opening drill -------------------------------------------------------------
  // KCL owns graph legality, KCB supplies statistical weight, and KCO names
  // the destination. This is the authoritative opening-training continuation
  // source once all three immutable assets are available.
  std::vector<DrillMove> graph_replies(const std::string& fen) const;
  // Plays one KCL continuation for the opponent, drawn by KCB weight. False
  // when the graph has no continuation, which ends the drill.
  bool play_graph_reply(Session& session);

  Database& database_;
  TrainingService& training_;
  BotService& bot_;
  PracticeCatalog content_;
  std::map<std::string, Session> sessions_;
  unsigned long long next_id_{1};
  const OpeningLineGraph* opening_lines_{nullptr};
  const OpeningNameIndex* opening_names_{nullptr};
  const OpeningTheoryProvider* opening_theory_{nullptr};
  std::mt19937 random_{std::random_device{}()};
};
}  // namespace kchess
