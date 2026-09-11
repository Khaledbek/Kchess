#pragma once
// -----------------------------------------------------------------------------
// Section: Practice orchestration over existing training and bot services
// -----------------------------------------------------------------------------
#include <map>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "services/bot_service.h"
#include "training/practice_catalog.h"
#include "training/training_service.h"

namespace kchess {
class PracticeService {
 public:
  PracticeService(Database& db, TrainingService& training, BotService& bot)
      : database_(db), training_(training), bot_(bot) {}
  std::string command(const std::string& request);
  std::string overview(const std::string& training_overview) const;
 private:
  struct Session {
    std::string id, key, kind, fen, solver{"white"}, status{"active"}, job;
    std::vector<ParsedMove> line;
    std::vector<std::string> history;
    std::size_t ply{0};
    int played{0}, budget{0};
    bool clean{true}, recorded{false};
    nlohmann::json evaluation = nullptr;
  };
  nlohmann::json catalog();
  nlohmann::json progress(const std::string& key) const;
  nlohmann::json start(const nlohmann::json& request);
  nlohmann::json move(Session& session, const nlohmann::json& request);
  nlohmann::json poll(Session& session);
  nlohmann::json snapshot(const Session& session) const;
  void advance(Session& session);
  void finish(Session& session, bool success);
  void opponent(Session& session);
  Database& database_;
  TrainingService& training_;
  BotService& bot_;
  PracticeCatalog content_;
  std::map<std::string, Session> sessions_;
  unsigned long long next_id_{1};
};
}  // namespace kchess
