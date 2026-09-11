// -----------------------------------------------------------------------------
// Section: Native practice sessions, solutions and defender job lifecycle
// -----------------------------------------------------------------------------
#include "training/practice_service.h"
#include <algorithm>
#include <stdexcept>
#include "chess/move.h"
#include "chess/position_view.h"
#include "training/practice_position.h"

namespace kchess {
nlohmann::json PracticeService::start(const nlohmann::json& request) {
  Session session;
  session.id = "practice-" + std::to_string(next_id_++);
  session.kind = request.at("kind").get<std::string>();
  if (session.kind == "opening") {
    const int id = request.at("id").get<int>();
    const auto& game = content_.line(id);
    session.key = "opening_" + std::to_string(id);
    session.solver = request.value("color", std::string("white"));
    if (session.solver != "white" && session.solver != "black") {
      throw std::invalid_argument("Invalid solver color");
    }
    session.line = game.moves;
    session.fen = game.initial_fen;
    for (const auto& move : session.line) {
      if (move.side_to_move == session.solver) ++session.budget;
    }
    if (!session.budget) throw std::invalid_argument("No moves for selected color");
  } else if (session.kind == "drill") {
    const auto id = request.at("id").get<std::string>();
    const int level = request.at("level").get<int>();
    if (level > 1 && !progress(id + "_l" + std::to_string(level - 1)).at("isMastered").get<bool>()) {
      throw std::invalid_argument("Drill level is locked");
    }
    session.key = id + "_l" + std::to_string(level);
    session.fen = generate_practice_position(id, level);
    session.budget = practice_move_budget(id, level);
  } else if (session.kind == "study") {
    const auto study = content_.study(request.at("id").get<std::string>());
    session.key = study.at("id").get<std::string>();
    session.fen = study.at("fen").get<std::string>();
    session.solver = white_to_move(session.fen) ? "white" : "black";
  } else {
    throw std::invalid_argument("Unknown practice kind");
  }
  session.history.push_back(session.fen);
  advance(session);
  auto result = snapshot(session);
  sessions_.emplace(session.id, std::move(session));
  return result;
}

void PracticeService::finish(Session& session, bool success) {
  session.status = success ? "completed" : "failed";
  if (!session.recorded) {
    training_.record_completion(session.key, success && session.clean);
    session.recorded = true;
  }
}

void PracticeService::opponent(Session& session) {
  try {
    // Reuse Khaled's dedicated native bot pipeline; do not interrupt mainline
    // or side-line analysis, and do not set engine resources from Flutter.
    const auto job = nlohmann::json::parse(bot_.start_move_json(session.fen, kBotEloMaximum));
    session.job = job.at("jobId").get<std::string>();
    session.status = "thinking";
  } catch (...) {
    session.status = "error";
  }
}

void PracticeService::advance(Session& session) {
  if (session.kind == "opening") {
    while (session.ply < session.line.size() &&
           session.line[session.ply].side_to_move != session.solver) {
      session.fen = session.line[session.ply++].fen_after;
    }
    if (session.ply == session.line.size()) finish(session, true);
    return;
  }
  const auto outcome = position_outcome(session.fen);
  if (outcome.terminal) {
    const auto winning_result = session.solver == "white" ? "1-0" : "0-1";
    finish(session, outcome.checkmate && outcome.result == winning_result);
    return;
  }
  if (practice_dead_position(session.fen)) { finish(session, false); return; }
  int repetitions = 0;
  for (const auto& fen : session.history) if (same_chess_position(fen, session.fen)) ++repetitions;
  if (repetitions >= 3) { finish(session, false); return; }
  if (session.budget > 0 && session.played >= session.budget) {
    finish(session, false); return;
  }
  const bool solver_turn = white_to_move(session.fen) == (session.solver == "white");
  if (!solver_turn) opponent(session);
}

nlohmann::json PracticeService::move(Session& session, const nlohmann::json& request) {
  if (session.status != "active") throw std::invalid_argument("Practice is not accepting moves");
  const auto source = request.at("source").get<std::string>();
  const auto target = request.at("target").get<std::string>();
  const auto promotion = request.value("promotion", std::string{});
  if (source.size() != 2 || target.size() != 2) throw std::invalid_argument("Invalid squares");
  const auto promotions = legal_promotion_choices(session.fen, source, target);
  if (!promotions.empty() && promotion.empty()) {
    auto result = snapshot(session);
    result["promotions"] = promotions;
    return result;
  }
  if (!promotion.empty() && std::find(promotions.begin(), promotions.end(), promotion) == promotions.end()) {
    throw std::invalid_argument("Invalid promotion");
  }
  std::optional<AppliedMove> played;
  try { played = apply_legal_uci_move(session.fen, source + target + promotion); }
  catch (const std::invalid_argument&) {
    auto result = snapshot(session); result["accepted"] = false; return result;
  }
  if (session.kind == "opening" && played->uci != session.line.at(session.ply).uci) {
    session.clean = false;
    auto result = snapshot(session); result["accepted"] = false; return result;
  }
  session.fen = played->fen_after;
  session.evaluation = nullptr;
  ++session.played;
  ++session.ply;
  session.history.push_back(session.fen);
  advance(session);
  auto result = snapshot(session);
  result["accepted"] = true;
  result["lastMove"] = played->uci;
  return result;
}

nlohmann::json PracticeService::poll(Session& session) {
  if (session.status != "thinking") return snapshot(session);
  const auto job = nlohmann::json::parse(bot_.move_status_json(session.job));
  const auto status = job.at("status").get<std::string>();
  if (status == "failed" || status == "cancelled") {
    session.status = "error";
    session.job.clear();
  } else if (status == "completed") {
    const auto played = apply_legal_uci_move(session.fen, job.at("move").get<std::string>());
    session.fen = played.fen_after;
    session.history.push_back(session.fen);
    session.evaluation = {{"evaluationCp", job.value("postMoveEvaluationCp", nlohmann::json(nullptr))},
                          {"mateIn", job.value("postMoveMateIn", nlohmann::json(nullptr))}};
    session.job.clear();
    session.status = "active";
    advance(session);
  }
  return snapshot(session);
}
}  // namespace kchess
