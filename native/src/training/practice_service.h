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
#include "theory/polyglot.h"
#include "training/practice_catalog.h"
#include "training/training_service.h"

namespace kchess {
class PracticeService {
 public:
  PracticeService(Database& db, TrainingService& training, BotService& bot)
      : database_(db), training_(training), bot_(bot) {}
  std::string command(const std::string& request);
  std::string overview(const std::string& training_overview) const;

  // An opening drill runs until the user has answered this many book moves.
  static constexpr int kDrillDepth = 10;
  // Book replies the drill accepts: the best move and its closest rivals.
  static constexpr int kAcceptedRanks = 3;

 private:
  // One legal book reply, resolved against the position it came from.
  struct DrillMove {
    std::string uci, san, fen_after;
    std::uint16_t weight{0};
  };

  struct Session {
    std::string id, key, kind, initial_fen, fen, solver{"white"}, status{"active"}, job;
    std::vector<std::string> history;
    std::size_t ply{0};
    int played{0}, budget{0};
    bool clean{true}, recorded{false};
    nlohmann::json evaluation = nullptr;

    // --- opening drill state ---------------------------------------------
    // Book replies for `fen`, heaviest first: what the user has to find on
    // their turn, and what the drill picks from on the opponent's.
    std::vector<DrillMove> book;
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
  };
  nlohmann::json catalog();
  nlohmann::json progress(const std::string& key) const;
  nlohmann::json start(const nlohmann::json& request);
  nlohmann::json move(Session& session, const nlohmann::json& request);
  nlohmann::json poll(Session& session);
  nlohmann::json snapshot(const Session& session);
  void advance(Session& session);
  // Plays book replies until the user is on move with something to find, or
  // the drill has nowhere left to go.
  void advance_drill(Session& session);
  void finish(Session& session, bool success);
  void opponent(Session& session);

  // Opening drill -------------------------------------------------------------
  // The book's legal replies for a position, heaviest first. Illegal decodes
  // (a book key collision) are dropped rather than offered to the board.
  std::vector<DrillMove> book_replies(const std::string& fen);
  // Plays one book reply for the opponent, drawn by weight. False when the
  // book has no reply, which ends the drill.
  bool play_book_reply(Session& session);
  PolyglotBook* book();

  Database& database_;
  TrainingService& training_;
  BotService& bot_;
  PracticeCatalog content_;
  std::map<std::string, Session> sessions_;
  unsigned long long next_id_{1};
  std::unique_ptr<PolyglotBook> polyglot_book_;
  // Set once the book failed to open, so every session does not retry it.
  bool polyglot_missing_{false};
  std::mt19937 random_{std::random_device{}()};
};
}  // namespace kchess
