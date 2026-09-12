// -----------------------------------------------------------------------------
// Section: Native catalogue, progress and command routing
// -----------------------------------------------------------------------------
#include "training/practice_service.h"
#include <stdexcept>
#include <unordered_map>
#include "chess/position_view.h"
#include "training/practice_position.h"

namespace kchess {
namespace {
// Request-local index: one database read per batch, without stale progress
// caches across completions or profile/data changes. Records outlive the index.
using ProgressIndex = std::unordered_map<std::string, const TrainingProgressRecord*>;

ProgressIndex index_progress(const std::vector<TrainingProgressRecord>& records) {
  ProgressIndex index;
  index.reserve(records.size());
  for (const auto& record : records) index.emplace(record.exercise_id, &record);
  return index;
}

const TrainingProgressRecord* find_progress(const ProgressIndex& index,
                                           const std::string& key) {
  const auto found = index.find(key);
  return found == index.end() ? nullptr : found->second;
}

nlohmann::json progress_json(const std::string& key,
                             const TrainingProgressRecord* record) {
  return {{"exerciseId", key}, {"isMastered", record ? record->mastered : false},
          {"successStreak", record ? record->success_streak : 0},
          {"successCount", record ? record->success_count : 0},
          {"attemptCount", record ? record->attempt_count : 0},
          // Deepest an opening drill has ever run for this key; zero elsewhere.
          {"bestDepth", record ? record->best_depth : 0}};
}
}  // namespace

std::string PracticeService::overview(const std::string& training_overview) const {
  auto result = nlohmann::json::parse(training_overview);
  auto& category = result["categories"]["endgame"];
  const auto stored = database_.training_progress();
  const auto index = index_progress(stored);
  const auto add = [&](const std::string& key) {
    category["total"] = category.value("total", 0) + 1;
    if (const auto* p = find_progress(index, key)) {
      category["mastered"] = category.value("mastered", 0) + (p->mastered ? 1 : 0);
      category["solved"] = category.value("solved", 0) + p->success_count;
    }
  };
  for (const std::string id : {"endgame_kq_vs_k", "endgame_kr_vs_k", "endgame_q_vs_r"}) {
    for (int level = 1; level <= 3; ++level) add(id + "_l" + std::to_string(level));
  }
  const auto& studies = content_.studies();
  for (const auto& section : studies.at("sections")) {
    for (const auto& study : section.at("studies")) add(study.at("id").get<std::string>());
  }
  return result.dump();
}

nlohmann::json PracticeService::progress(const std::string& key) const {
  for (const auto& p : database_.training_progress()) {
    if (p.exercise_id == key) return progress_json(key, &p);
  }
  return progress_json(key, nullptr);
}
nlohmann::json PracticeService::catalog() {
  const auto stored = database_.training_progress();
  const auto index = index_progress(stored);
  auto drills = nlohmann::json::array();
  for (const std::string id : {"endgame_kq_vs_k", "endgame_kr_vs_k", "endgame_q_vs_r"}) {
    auto levels = nlohmann::json::array();
    bool unlocked = true;
    for (int level = 1; level <= 3; ++level) {
      const auto key = id + "_l" + std::to_string(level);
      const auto p = progress_json(key, find_progress(index, key));
      const int budget = practice_move_budget(id, level);
      levels.push_back({{"id", id}, {"key", key}, {"level", level},
          {"maxMoves", budget}, {"unlocked", unlocked}, {"progress", p}});
      unlocked = p.at("isMastered").get<bool>();
    }
    drills.push_back({{"id", id}, {"levels", levels}});
  }
  auto studies = content_.studies();
  for (auto& section : studies["sections"]) {
    int number = 0;
    for (auto& study : section["studies"]) {
      study["number"] = ++number;
      study["section"] = section["id"];
      const auto key = study.at("id").get<std::string>();
      study["progress"] = progress_json(key, find_progress(index, key));
    }
  }
  return {{"drills", drills}, {"studies", studies},
          {"masteryThreshold", TrainingService::mastery_threshold}};
}

std::string PracticeService::command(const std::string& request) {
  const auto json = nlohmann::json::parse(request);
  const auto op = json.at("op").get<std::string>();
  if (op == "catalog") return catalog().dump();
  if (op == "nodes") {
    auto nodes = content_.openings(json.value("parent", 0), json.value("query", std::string{}));
    const auto stored = database_.training_progress();
    const auto index = index_progress(stored);
    for (auto& node : nodes) {
      const auto key = node.at("progressKey").get<std::string>();
      node["progress"] = progress_json(key, find_progress(index, key));
      node["masteryThreshold"] = TrainingService::mastery_threshold;
      // What a full drill of this scenario is worth, for the depth meter.
      node["targetDepth"] = kDrillDepth;
    }
    return nodes.dump();
  }
  if (op == "match") {
    return nlohmann::json({{"id", content_.matching(
        json.value("eco", std::string{}), json.value("name", std::string{}))}}).dump();
  }
  if (op == "start") return start(json).dump();
  const auto id = json.at("session").get<std::string>();
  const auto found = sessions_.find(id);
  if (found == sessions_.end()) throw std::invalid_argument("Unknown practice session");
  auto& session = found->second;
  if (op == "cancel") {
    if (!session.job.empty()) bot_.cancel_move(session.job);
    sessions_.erase(found);
    return "{}";
  }
  if (op == "move") return move(session, json).dump();
  if (op == "poll") return poll(session).dump();
  throw std::invalid_argument("Unknown practice operation");
}

nlohmann::json PracticeService::snapshot(const Session& session) {
  nlohmann::json result = {{"session", session.id}, {"key", session.key}, {"kind", session.kind},
      {"status", session.status}, {"solverColor", session.solver},
      {"position", nlohmann::json::parse(position_view_json(session.fen))},
      {"played", session.played}, {"ply", session.ply}, {"maxMoves", session.budget},
      {"clean", session.clean}, {"evaluation", session.evaluation},
      {"progress", progress(session.key)}};

  if (session.kind == "opening") {
    // The drill's own state: how deep this run is, what the book just played,
    // and — only once it has been missed — the move it wanted.
    result["depth"] = session.played;
    result["targetDepth"] = session.budget;
    result["attempts"] = session.attempts;
    result["bookExhausted"] = session.book_exhausted;
    result["bookMoves"] = static_cast<int>(session.book.size());
    result["openingMoves"] = session.opening_moves;
    if (!session.opponent_uci.empty()) {
      result["opponentMove"] = {{"uci", session.opponent_uci},
          {"san", session.opponent_san}, {"side", session.opponent_side},
          {"moveNumber", session.opponent_number},
          {"alternatives", session.opponent_alternatives}};
    }
    if (!session.answer_uci.empty()) {
      result["answer"] = {{"uci", session.answer_uci}, {"san", session.answer_san},
          {"rank", session.answer_rank}};
    }
    if (!session.hint.empty()) {
      result["hint"] = session.hint;
      result["hintSan"] = session.hint_san;
    }
  }
  return result;
}
}  // namespace kchess
