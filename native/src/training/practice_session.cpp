// -----------------------------------------------------------------------------
// Section: Native practice sessions, solutions and defender job lifecycle
// -----------------------------------------------------------------------------
#include "training/practice_service.h"
#include <algorithm>
#include <filesystem>
#include <sstream>
#include <stdexcept>
#include "chess/move.h"
#include "chess/position_view.h"
#include "training/practice_position.h"

namespace kchess {
namespace {
// The drill book is a ~170 MB PolyGlot file that is not in version control and
// is not bundled into the app yet, so it is searched for from the working
// directory upwards: that finds the checkout's copy whether the process was
// started in the repository root, in native/build, in flutter_app or in a
// Flutter desktop runner directory (flutter_app/build/windows/x64/runner/...).
std::filesystem::path locate_polyglot_book() {
  std::error_code error;
  auto directory = std::filesystem::current_path(error);
  if (error) return {};
  for (int level = 0; level < 8 && !directory.empty(); ++level) {
    for (const char* suffix : {"native/src/training/data/Cerebellum3Merge.bin",
                               "training/data/Cerebellum3Merge.bin",
                               "data/Cerebellum3Merge.bin"}) {
      const auto candidate = directory / suffix;
      if (std::filesystem::exists(candidate, error)) return candidate;
    }
    if (!directory.has_parent_path() || directory.parent_path() == directory) break;
    directory = directory.parent_path();
  }
  return {};
}

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

PolyglotBook* PracticeService::book() {
  if (polyglot_book_) return polyglot_book_.get();
  if (polyglot_missing_) return nullptr;
  const auto path = locate_polyglot_book();
  try {
    if (path.empty()) throw std::runtime_error("No opening book on disk");
    polyglot_book_ = std::make_unique<PolyglotBook>(path);
  } catch (const std::exception&) {
    // Without the book there is no theory to drill, but every other kind of
    // practice still works, so this must not take the whole service down.
    polyglot_missing_ = true;
    return nullptr;
  }
  return polyglot_book_.get();
}

std::vector<PracticeService::DrillMove> PracticeService::book_replies(
    const std::string& fen) {
  auto* source = book();
  if (source == nullptr) return {};
  std::vector<DrillMove> replies;
  for (const auto& entry : source->ranked_moves(fen)) {
    try {
      const auto played = apply_legal_uci_move(fen, entry.uci);
      replies.push_back({played.uci, played.san, played.fen_after, entry.weight});
    } catch (const std::invalid_argument&) {
      // A book key collision decodes to a move that is not legal here. It is
      // not a reply, so it is neither asked for nor played.
    }
  }
  return replies;
}

bool PracticeService::play_book_reply(Session& session) {
  const auto* reply = pick_weighted(session.book, random_);
  if (reply == nullptr) return false;
  session.opponent_uci = reply->uci;
  session.opponent_san = reply->san;
  session.opponent_side = white_to_move(session.fen) ? "white" : "black";
  session.opponent_number = full_move_number(session.fen);
  // How wide the book was here: the same drill can come back a different way.
  session.opponent_alternatives = static_cast<int>(session.book.size());
  session.fen = reply->fen_after;
  session.history.push_back(session.fen);
  ++session.ply;
  return true;
}

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
    // The catalogue line is the scenario's setup, not its answer key: it is
    // replayed once to reach the position the opening is named after, and
    // every move past it comes from the book instead.
    session.initial_fen =
        game.moves.empty() ? game.initial_fen : game.moves.back().fen_after;
    for (const auto& move : game.moves) session.opening_moves.push_back(move.san);
    session.fen = session.initial_fen;
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

// The opening drill has no script. The book answers for the opponent, the user
// has to find the book answer in reply, and the loop stops only when the target
// depth is reached or the book runs out of theory. Every position on the way is
// whatever the last weighted draw produced, which is what makes one opening
// drill the whole tree below it instead of a single line.
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
    session.book = book_replies(session.fen);
    if (session.book.empty()) {
      session.book_exhausted = true;
      // Nothing was ever asked here, so there is no attempt worth recording.
      if (session.played == 0) session.recorded = true;
      finish(session, true);
      return;
    }
    const bool solver_turn = white_to_move(session.fen) == (session.solver == "white");
    // The user is on move: move() judges whatever comes back against the book.
    if (solver_turn) return;
    if (!play_book_reply(session)) {
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
    // The verdict is passed before the board moves: a guess outside the book
    // leaves the position exactly as it was, so the same question can be asked
    // again with the answer now showing.
    const auto found = std::find_if(
        session.book.begin(), session.book.end(),
        [&](const DrillMove& candidate) { return candidate.uci == played->uci; });
    const int rank = found == session.book.end()
        ? -1
        : static_cast<int>(found - session.book.begin());
    if (rank < 0 || rank >= kAcceptedRanks) {
      session.clean = false;
      ++session.attempts;
      // The previous answer's tick would read as approval of this miss.
      session.answer_uci.clear();
      session.answer_san.clear();
      session.hint = session.book.front().uci;
      session.hint_san = session.book.front().san;
      auto result = snapshot(session);
      result["accepted"] = false;
      return result;
    }
    session.answer_uci = played->uci;
    session.answer_san = played->san;
    session.answer_rank = rank + 1;
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
