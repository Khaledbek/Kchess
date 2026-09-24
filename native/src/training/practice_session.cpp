// -----------------------------------------------------------------------------
// Section: Native practice sessions, solutions and defender job lifecycle
// -----------------------------------------------------------------------------
#include "training/practice_service.h"
#include <algorithm>
#include <sstream>
#include <stdexcept>
#include "chess/move.h"
#include "core/weighted_choice.h"
#include "chess/position_view.h"
#include "training/practice_position.h"

namespace kchess {
namespace {
// The move number a FEN is at, for naming the reply the drill just played.
int full_move_number(const std::string& fen) {
  std::istringstream input(fen);
  std::string placement, side, castling, en_passant, halfmove, fullmove;
  input >> placement >> side >> castling >> en_passant >> halfmove >> fullmove;
  try {
    return std::max(1, std::stoi(fullmove));
  } catch (const std::exception&) {
    return 1;
  }
}
}  // namespace


void PracticeService::set_opening_sources(
    const OpeningLineGraph& lines,
    const OpeningNameIndex& names,
    const OpeningTheoryProvider& theory) {
  opening_lines_ = &lines;
  opening_names_ = &names;
  opening_theory_ = &theory;
}

std::vector<PracticeService::DrillMove> PracticeService::graph_replies(
    const std::string& fen) const {
  if (opening_lines_ == nullptr || !opening_lines_->available()) return {};

  std::vector<DrillMove> replies;
  const auto continuations = opening_lines_->continuations(fen);
  replies.reserve(continuations.size());
  for (const auto& continuation : continuations) {
    const TheoryMoveInfo theory = opening_theory_ == nullptr
        ? TheoryMoveInfo{}
        : opening_theory_->lookup(fen, continuation.uci);
    DrillMove move{
        .uci = continuation.uci,
        .san = continuation.san,
        .fen_after = continuation.fen_after,
        // A graph edge can legitimately be absent from a filtered KCB build.
        // Keep it trainable, but make any statistically observed edge dominate
        // it when the opponent reply is sampled.
        .weight = theory.games == 0 ? 1ULL : static_cast<std::uint64_t>(theory.games),
        .games = theory.games,
        .white_wins = theory.white_wins,
        .draws = theory.draws,
        .black_wins = theory.black_wins,
    };
    if (opening_names_ != nullptr) {
      if (auto name = opening_names_->lookup(continuation.destination_key); name.has_value()) {
        move.destination_eco = name->eco;
        move.destination_name = name->name;
      }
    }
    replies.push_back(std::move(move));
  }

  std::stable_sort(replies.begin(), replies.end(), [](const DrillMove& left, const DrillMove& right) {
    if (left.games != right.games) return left.games > right.games;
    return left.uci < right.uci;
  });
  return replies;
}

bool PracticeService::play_graph_reply(Session& session) {
  const auto* reply = pick_weighted(session.continuations, random_);
  if (reply == nullptr) return false;
  session.opponent_uci = reply->uci;
  session.opponent_san = reply->san;
  session.opponent_side = white_to_move(session.fen) ? "white" : "black";
  session.opponent_number = full_move_number(session.fen);
  // How wide the graph was here: the same drill can continue a different way.
  session.opponent_alternatives = static_cast<int>(session.continuations.size());
  session.fen = reply->fen_after;
  if (!reply->destination_name.empty()) {
    session.opening_eco = reply->destination_eco;
    session.opening_name = reply->destination_name;
  }
  session.history.push_back(session.fen);
  ++session.ply;
  return true;
}

nlohmann::json PracticeService::start(const nlohmann::json& request) {
  Session session;
  session.id = "practice-" + std::to_string(next_id_++);
  session.kind = request.at("kind").get<std::string>();
  if (session.kind == "opening") {
    if (opening_lines_ == nullptr) {
      throw std::runtime_error(
          "Opening training has no KCL graph provider attached to PracticeService");
    }
    if (!opening_lines_->available()) {
      const auto reason = opening_lines_->availability_error();
      throw std::runtime_error(
          reason.empty()
              ? "Opening training KCL graph is unavailable for an unspecified reason"
              : "Opening training KCL graph unavailable: " + reason);
    }
    const int id = request.at("id").get<int>();
    const auto& game = content_.line(id);
    session.key = "opening_" + std::to_string(id);
    session.solver = request.value("color", std::string("white"));
    if (session.solver != "white" && session.solver != "black") {
      throw std::invalid_argument("Invalid solver color");
    }
    // The catalogue line is the scenario's setup, not its answer key: it is
    // replayed once to reach the position the opening is named after, and
    // every move past it comes from the KCL graph instead.
    session.initial_fen =
        game.moves.empty() ? game.initial_fen : game.moves.back().fen_after;
    for (const auto& move : game.moves) session.opening_moves.push_back(move.san);
    session.fen = session.initial_fen;
    if (opening_names_ != nullptr) {
      if (auto name = opening_names_->lookup(stockfish_position_key(session.fen)); name.has_value()) {
        session.opening_eco = name->eco;
        session.opening_name = name->name;
      }
    }
    session.budget = std::clamp(request.value("depth", kDrillDepth), 1, 40);
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
    // Only an opening drill measures depth; every other kind reports zero and
    // leaves the stored best depth alone.
    const int depth = session.kind == "opening" ? session.played : 0;
    training_.record_completion(session.key, success && session.clean, depth);
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

// The opening drill has no answer script. KCL is the authoritative topology:
// the opponent draws one graph edge (weighted by KCB statistics), the user must
// answer with a permitted graph edge, and transpositions naturally converge on
// the same keyed position. The legacy catalogue line is used only to set up the
// named scenario; it never constrains continuation play.
void PracticeService::advance_drill(Session& session) {
  while (true) {
    if (session.played >= session.budget) {
      finish(session, true);
      return;
    }
    if (position_outcome(session.fen).terminal) {
      finish(session, true);
      return;
    }
    session.continuations = graph_replies(session.fen);
    if (session.continuations.empty()) {
      session.book_exhausted = true;
      // Nothing was ever asked here, so there is no attempt worth recording.
      if (session.played == 0) session.recorded = true;
      finish(session, true);
      return;
    }
    const bool solver_turn = white_to_move(session.fen) == (session.solver == "white");
    // The user is on move: move() judges the reply against KCL continuations.
    if (solver_turn) return;
    if (!play_graph_reply(session)) {
      session.book_exhausted = true;
      finish(session, true);
      return;
    }
  }
}

void PracticeService::advance(Session& session) {
  if (session.kind == "opening") {
    advance_drill(session);
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
  if (session.kind == "opening") {
    // The verdict is passed before the board moves: every KCL edge is a valid
    // training continuation. KCB statistics sort/weight those edges for hints
    // and opponent sampling, but never make a graph-valid move invalid. A move
    // outside KCL leaves the board untouched so the same question can be retried.
    const auto found = std::find_if(
        session.continuations.begin(), session.continuations.end(),
        [&](const DrillMove& candidate) { return candidate.uci == played->uci; });
    const int rank = found == session.continuations.end()
        ? -1
        : static_cast<int>(found - session.continuations.begin());
    if (rank < 0) {
      session.clean = false;
      ++session.attempts;
      // The previous answer's tick would read as approval of this miss.
      session.answer_uci.clear();
      session.answer_san.clear();
      session.hint = session.continuations.front().uci;
      session.hint_san = session.continuations.front().san;
      auto result = snapshot(session);
      result["accepted"] = false;
      return result;
    }
    session.answer_uci = played->uci;
    session.answer_san = played->san;
    session.answer_rank = rank + 1;
    if (!found->destination_name.empty()) {
      session.opening_eco = found->destination_eco;
      session.opening_name = found->destination_name;
    }
    session.attempts = 0;
    session.hint.clear();
    session.hint_san.clear();
    // Until the book replies again there is no opponent move to announce.
    session.opponent_uci.clear();
    session.opponent_san.clear();
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
