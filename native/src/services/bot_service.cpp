#include "services/bot_service.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <deque>
#include <ctime>
#include <iomanip>
#include <optional>
#include <random>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "chess/fen.h"
#include "chess/move.h"
#include "chess/position_view.h"
#include "engine/stockfish_factory.h"
#include "engine/stockfish_runtime.h"
#include "movegen.h"
#include "position.h"
#include "uci.h"

namespace kchess {
namespace {

// -----------------------------------------------------------------------------
// Section: Bot job serialization helpers
// -----------------------------------------------------------------------------

std::uint64_t seed_material() {
  std::random_device device;
  const auto now = static_cast<std::uint64_t>(
      std::chrono::steady_clock::now().time_since_epoch().count());
  return (static_cast<std::uint64_t>(device()) << 32U)
      ^ static_cast<std::uint64_t>(device()) ^ now;
}

std::string bot_outcome(
    const std::string& player_color,
    const std::string& status,
    const std::string& result) {
  if (status == "active" || result == "*") return "unfinished";
  if (result == "1/2-1/2" || result == "½-½") return "draw";
  const bool player_won =
      (player_color == "white" && result == "1-0")
      || (player_color == "black" && result == "0-1");
  return player_won ? "win" : "loss";
}

std::string pgn_date(const std::int64_t unix_seconds) {
  const std::time_t time = static_cast<std::time_t>(unix_seconds);
  std::tm utc{};
#if defined(_WIN32)
  gmtime_s(&utc, &time);
#else
  gmtime_r(&time, &utc);
#endif
  std::ostringstream output;
  output << std::put_time(&utc, "%Y.%m.%d");
  return output.str();
}

int fen_position_ply(const std::string& fen) {
  std::istringstream input(fen);
  std::string board;
  std::string side;
  std::string castling;
  std::string en_passant;
  int halfmove = 0;
  int fullmove = 1;
  if (!(input >> board >> side >> castling >> en_passant >> halfmove >> fullmove)) {
    return 0;
  }
  return std::max(0, (fullmove - 1) * 2 + (side == "b" ? 1 : 0));
}

const EngineLine* line_for_move(
    const std::vector<EngineLine>& lines, const std::string& move) {
  const auto found = std::find_if(lines.begin(), lines.end(), [&](const EngineLine& line) {
    return line.best_move() == move;
  });
  return found == lines.end() ? nullptr : &*found;
}


struct BotPositionBudget {
  double multiplier{1.0};
  int legal_moves{0};
  int captures{0};
  bool in_check{false};
  std::string forced_move;
};

BotPositionBudget position_budget(const std::string& fen) {
  initialize_stockfish_runtime();
  std::deque<Stockfish::StateInfo> states(1);
  Stockfish::Position position;
  position.set(fen, false, &states.back());

  BotPositionBudget budget;
  budget.in_check = static_cast<bool>(position.checkers());
  for (const auto move : Stockfish::MoveList<Stockfish::LEGAL>(position)) {
    ++budget.legal_moves;
    if (position.capture(move)) ++budget.captures;
    if (budget.legal_moves == 1) {
      budget.forced_move = Stockfish::UCIEngine::move(move, false);
    } else {
      budget.forced_move.clear();
    }
  }

  // Quiet positions get less work; checks, many captures, and broad move trees
  // receive a bounded extension. This heuristic is deliberately cheap and
  // runs before Stockfish search, so it cannot become another latency source.
  if (budget.in_check) budget.multiplier *= 1.55;
  if (budget.captures >= 4) budget.multiplier *= 1.25;
  if (budget.legal_moves >= 34) budget.multiplier *= 1.15;
  if (!budget.in_check && budget.captures == 0 && budget.legal_moves <= 28) {
    budget.multiplier *= 0.78;
  }
  budget.multiplier = std::clamp(budget.multiplier, 0.70, 1.80);
  return budget;
}

std::uint64_t scaled_nodes(const std::uint64_t base, const double multiplier) {
  return static_cast<std::uint64_t>(std::max(1.0, std::round(base * multiplier)));
}

int scaled_milliseconds(const int base, const double multiplier) {
  return std::max(1, static_cast<int>(std::lround(base * multiplier)));
}

std::vector<std::string> verification_moves(
    const AnalysisResult& scout, const BotMoveChoice& choice) {
  std::vector<EngineLine> lines = scout.lines;
  std::sort(lines.begin(), lines.end(), [](const EngineLine& left, const EngineLine& right) {
    return left.rank < right.rank;
  });
  std::vector<std::string> result;
  auto add = [&](const std::string& move) {
    if (move.empty() || move == "(none)" || move == "0000") return;
    if (std::find(result.begin(), result.end(), move) == result.end()) result.push_back(move);
  };
  add(choice.move);
  if (!lines.empty()) add(lines.front().best_move());
  if (choice.rank <= 1 && lines.size() > 1) {
    // A shallow rank-1 guess still gets checked against the runner-up. This
    // catches tactical scout mistakes without widening the deep search.
    add(lines[1].best_move());
  }
  if (choice.rank > 2 && lines.size() > 2) {
    const int safer_rank = std::max(2, (choice.rank + 1) / 2);
    const auto safer = std::find_if(lines.begin(), lines.end(), [&](const EngineLine& line) {
      return line.rank == safer_rank;
    });
    if (safer != lines.end()) add(safer->best_move());
  }
  return result;
}

}  // namespace

BotService::BotService(Database& database)
    : database_(database), random_(seed_material()) {}

// -----------------------------------------------------------------------------
// Section: Persisted bot game lifecycle
// -----------------------------------------------------------------------------

std::string BotService::session_json(const BotGameRecord& game) const {
  const auto terminal_outcome = game.status == "complete"
      ? position_outcome(game.current_fen)
      : PositionOutcome{};
  nlohmann::json moves = nlohmann::json::array();
  nlohmann::json positions = nlohmann::json::array();
  positions.push_back(nlohmann::json::parse(position_view_json(game.starting_fen)));
  for (const auto& move : game.moves) {
    moves.push_back({
        {"ply", move.ply},
        {"uci", move.uci},
        {"san", move.san},
        {"fenAfter", move.fen_after},
    });
    positions.push_back(nlohmann::json::parse(position_view_json(move.fen_after)));
  }
  nlohmann::json json{
      {"gameId", game.id},
      {"botElo", game.bot_elo},
      {"playerColor", game.player_color},
      {"botColor", game.bot_color},
      {"status", game.status},
      {"result", game.result},
      {"checkmate", terminal_outcome.checkmate},
      {"position", nlohmann::json::parse(position_view_json(game.current_fen))},
      {"positions", std::move(positions)},
      {"moves", std::move(moves)},
      {"createdAt", game.created_at},
      {"updatedAt", game.updated_at},
      {"showEvaluationBar", game.show_eval_bar},
      {"analysisGameId", game.analysis_game_id.has_value()
          ? nlohmann::json(*game.analysis_game_id) : nlohmann::json(nullptr)},
  };
  return json.dump();
}

std::string BotService::create_game_json(const int requested_elo) {
  const auto normalized_elo = bot_difficulty_profile(requested_elo).requested_elo;
  return session_json(database_.create_bot_game(normalized_elo, kStartFen));
}

std::string BotService::active_game_json() const {
  const auto game = database_.active_bot_game();
  return game.has_value() ? session_json(*game) : "null";
}

std::string BotService::game_json(const std::string& game_id) const {
  const auto game = database_.bot_game(game_id);
  if (!game.has_value()) throw std::invalid_argument("Bot game not found");
  return session_json(*game);
}

std::string BotService::games_json() const {
  nlohmann::json result = nlohmann::json::array();
  for (const auto& game : database_.bot_games()) {
    result.push_back({
        {"gameId", game.id},
        {"botElo", game.bot_elo},
        {"playerColor", game.player_color},
        {"botColor", game.bot_color},
        {"status", game.status},
        {"result", game.result},
        {"outcome", bot_outcome(game.player_color, game.status, game.result)},
        {"moveCount", game.move_count},
        {"createdAt", game.created_at},
        {"updatedAt", game.updated_at},
        {"analysisGameId", game.analysis_game_id.has_value()
            ? nlohmann::json(*game.analysis_game_id) : nlohmann::json(nullptr)},
    });
  }
  return result.dump();
}

std::string BotService::analysis_pgn(const std::string& game_id) const {
  const auto game = database_.bot_game(game_id);
  if (!game.has_value()) throw std::invalid_argument("Bot game not found");
  if (game->status == "active") {
    throw std::runtime_error("Finish the bot game before opening analysis");
  }
  if (game->moves.empty()) {
    throw std::runtime_error("Bot game has no moves to analyse");
  }

  const bool player_is_white = game->player_color == "white";
  const std::string white_name = player_is_white ? "Player" : "Stockfish 18";
  const std::string black_name = player_is_white ? "Stockfish 18" : "Player";

  std::ostringstream pgn;
  pgn << "[Event \"KChess Bot Game\"]\n"
      << "[Site \"KChess\"]\n"
      << "[Date \"" << pgn_date(game->created_at) << "\"]\n"
      << "[Round \"-\"]\n"
      << "[White \"" << white_name << "\"]\n"
      << "[Black \"" << black_name << "\"]\n";
  if (player_is_white) {
    pgn << "[BlackElo \"" << game->bot_elo << "\"]\n";
  } else {
    pgn << "[WhiteElo \"" << game->bot_elo << "\"]\n";
  }
  pgn << "[Result \"" << game->result << "\"]\n";
  if (game->starting_fen != kStartFen) {
    pgn << "[SetUp \"1\"]\n"
        << "[FEN \"" << game->starting_fen << "\"]\n";
  }
  pgn << '\n';

  for (std::size_t index = 0; index < game->moves.size(); ++index) {
    if (index % 2 == 0) pgn << (index / 2 + 1) << ". ";
    pgn << game->moves[index].san << ' ';
  }
  pgn << game->result;
  return pgn.str();
}

std::string BotService::record_game_move_json(
    const std::string& game_id,
    const std::string& expected_fen_before,
    const std::string& uci) {
  const auto game = database_.bot_game(game_id);
  if (!game.has_value()) throw std::invalid_argument("Bot game not found");
  return record_game_move_from_ply_json(
      game_id, static_cast<int>(game->moves.size()), expected_fen_before, uci);
}

std::string BotService::record_game_move_from_ply_json(
    const std::string& game_id,
    const int base_ply,
    const std::string& expected_fen_before,
    const std::string& uci) {
  const auto game = database_.bot_game(game_id);
  if (!game.has_value()) throw std::invalid_argument("Bot game not found");
  if (game->status != "active") throw std::runtime_error("Bot game is no longer active");
  if (base_ply < 0 || base_ply > static_cast<int>(game->moves.size())) {
    throw std::invalid_argument("Bot game branch ply is out of range");
  }

  const std::string& branch_fen = base_ply == 0
      ? game->starting_fen
      : game->moves[static_cast<std::size_t>(base_ply - 1)].fen_after;
  if (branch_fen != expected_fen_before) {
    throw std::runtime_error("Bot game branch position changed before move was saved");
  }

  const auto applied = apply_legal_uci_move(branch_fen, uci);
  database_.append_bot_game_move(
      game_id,
      base_ply,
      branch_fen,
      BotGameMoveRecord{
          .uci = applied.uci,
          .san = applied.san,
          .fen_after = applied.fen_after,
      });
  const auto outcome = position_outcome(applied.fen_after);
  if (outcome.terminal) {
    database_.finish_bot_game(game_id, "complete", outcome.result);
  }
  nlohmann::json json{
      {"uci", applied.uci},
      {"san", applied.san},
      {"fenAfter", applied.fen_after},
      {"positionAfter", nlohmann::json::parse(position_view_json(applied.fen_after))},
      {"mainLinePly", base_ply + 1},
  };
  return json.dump();
}

void BotService::resign_game(const std::string& game_id) {
  const auto game = database_.bot_game(game_id);
  if (!game.has_value()) throw std::invalid_argument("Bot game not found");
  const std::string result = game->player_color == "white" ? "0-1" : "1-0";
  database_.finish_bot_game(game_id, "resigned", result);
}

void BotService::abort_game(const std::string& game_id) {
  database_.delete_bot_game(game_id);
}

void BotService::delete_game(const std::string& game_id) {
  const auto game = database_.bot_game(game_id);
  if (!game.has_value()) throw std::invalid_argument("Bot game not found");
  if (game->status == "active") {
    throw std::runtime_error("Active bot games must be aborted, not deleted from history");
  }
  database_.delete_bot_game(game_id);
}

void BotService::set_game_show_eval_bar(
    const std::string& game_id, const bool enabled) {
  database_.set_bot_game_show_eval_bar(game_id, enabled);
}

BotService::~BotService() {
  stop_all_jobs();
  std::shared_ptr<ChessEngine> engine;
  {
    std::lock_guard engine_lock(engine_mutex_);
    engine = engine_;
  }
  if (engine) engine->stop();
}

const char* BotService::state_name(const JobState state) noexcept {
  switch (state) {
    case JobState::queued: return "queued";
    case JobState::running: return "running";
    case JobState::cancelling: return "cancelling";
    case JobState::cancelled: return "cancelled";
    case JobState::completed: return "complete";
    case JobState::failed: return "failed";
  }
  return "failed";
}

std::string BotService::job_json(const BotJob& job) const {
  nlohmann::json json{
      {"jobId", job.id},
      {"status", state_name(job.state.load())},
      {"engineId", std::string(kStockfish18Id)},
      {"requestedElo", job.requested_elo},
      {"searchDepth", job.plan.verification_depth},
      {"scoutDepth", job.plan.scout_depth},
      {"targetRank", job.plan.target_rank},
      {"targetLossCp", job.plan.target_loss_cp},
      {"requestedCandidateLines", job.plan.scout_lines},
      {"scoutNodeBudget", job.plan.scout_node_budget},
      {"verificationNodeBudget", job.plan.verification_node_budget},
      {"scoutTimeBudgetMs", job.plan.scout_time_budget_ms},
      {"verificationTimeBudgetMs", job.plan.verification_time_budget_ms},
  };

  std::lock_guard result_lock(job.result_mutex);
  json["engineVersion"] = job.engine_version;
  json["engineSessionId"] = job.engine_session_id;
  json["reusedWarmEngine"] = job.reused_warm_engine;
  json["error"] = job.error.empty() ? nlohmann::json(nullptr) : nlohmann::json(job.error);
  json["bestMove"] = job.analysis.best_move;
  json["searchedCandidateLines"] = job.analysis.lines.size();
  json["evaluationFen"] = job.fen;
  json["evaluationCp"] = nullptr;
  json["mateIn"] = nullptr;
  json["wdl"] = nullptr;
  if (!job.analysis.lines.empty()) {
    const auto principal = std::min_element(
        job.analysis.lines.begin(), job.analysis.lines.end(),
        [](const EngineLine& left, const EngineLine& right) {
          return left.rank < right.rank;
        });
    const bool white = white_to_move(job.fen);
    if (principal->evaluation_cp.has_value()) {
      json["evaluationCp"] = white
          ? *principal->evaluation_cp : -*principal->evaluation_cp;
    }
    if (principal->mate_in.has_value()) {
      json["mateIn"] = white ? *principal->mate_in : -*principal->mate_in;
    }
    if (principal->wdl.has_value()) {
      json["wdl"] = {
          {"wins", white ? principal->wdl->wins : principal->wdl->losses},
          {"draws", principal->wdl->draws},
          {"losses", white ? principal->wdl->losses : principal->wdl->wins},
      };
    }
  }
  json["postMoveEvaluationFen"] = job.selected_fen_after.empty()
      ? nlohmann::json(nullptr) : nlohmann::json(job.selected_fen_after);
  json["postMoveEvaluationCp"] = job.selected_evaluation_cp.has_value()
      ? nlohmann::json(*job.selected_evaluation_cp) : nlohmann::json(nullptr);
  json["postMoveMateIn"] = job.selected_mate_in.has_value()
      ? nlohmann::json(*job.selected_mate_in) : nlohmann::json(nullptr);
  json["postMoveWdl"] = nullptr;
  if (job.selected_wdl.has_value()) {
    json["postMoveWdl"] = {
        {"wins", job.selected_wdl->wins},
        {"draws", job.selected_wdl->draws},
        {"losses", job.selected_wdl->losses},
    };
  }
  json["move"] = job.choice.move;
  json["selectedRank"] = job.choice.rank > 0
      ? nlohmann::json(job.choice.rank)
      : nlohmann::json(nullptr);
  json["selectedProbability"] = job.choice.rank > 0
      ? nlohmann::json(job.choice.probability)
      : nlohmann::json(nullptr);
  json["san"] = job.selected_san.empty()
      ? nlohmann::json(nullptr) : nlohmann::json(job.selected_san);
  json["fenAfter"] = job.selected_fen_after.empty()
      ? nlohmann::json(nullptr) : nlohmann::json(job.selected_fen_after);
  json["positionAfter"] = job.selected_position_after_json.empty()
      ? nlohmann::json(nullptr)
      : nlohmann::json::parse(job.selected_position_after_json);
  json["terminal"] = job.terminal;
  json["checkmate"] = job.checkmate;
  json["result"] = job.result;
  return json.dump();
}

std::string BotService::start_move_json(
    const std::string& fen, const int requested_elo) {
  const auto validation = validate_fen(fen);
  if (!validation.valid) throw std::invalid_argument(validation.error);
  const auto profile = bot_difficulty_profile(requested_elo);

  reap_finished_jobs();

  auto job = std::make_shared<BotJob>();
  job->id = "bot-" + std::to_string(next_job_id_.fetch_add(1));
  job->fen = validation.normalized;
  job->requested_elo = requested_elo;
  job->profile = profile;
  // Native UCI_Elo owns its own randomized candidate choice, so consuming a
  // KChess RNG value there would be both redundant and misleading. Low Elo
  // keeps the probability-first scout until the dedicated low-Elo loss model replaces it.
  job->plan = plan_bot_move(
      requested_elo,
      (requested_elo == kBotEloMaximum || profile.use_stockfish_limit_strength)
          ? 0.0
          : next_random_unit(),
      fen_position_ply(job->fen));

  {
    std::lock_guard lock(jobs_mutex_);
    const bool active = std::any_of(
        jobs_.begin(), jobs_.end(), [](const auto& entry) {
          const auto state = entry.second->state.load();
          return !entry.second->finished.load()
              && state != JobState::cancelled
              && state != JobState::failed
              && state != JobState::completed;
        });
    if (active) {
      throw std::runtime_error("A bot move is already being calculated");
    }
    jobs_[job->id] = job;
  }

  job->worker = std::thread([this, job] { run_job(job); });
  return job_json(*job);
}

std::string BotService::move_status_json(const std::string& job_id) const {
  std::shared_ptr<BotJob> job;
  {
    std::lock_guard lock(jobs_mutex_);
    const auto found = jobs_.find(job_id);
    if (found == jobs_.end()) throw std::invalid_argument("Bot move job not found");
    job = found->second;
  }
  return job_json(*job);
}

void BotService::cancel_move(const std::string& job_id) {
  std::shared_ptr<BotJob> job;
  {
    std::lock_guard lock(jobs_mutex_);
    const auto found = jobs_.find(job_id);
    if (found == jobs_.end()) throw std::invalid_argument("Bot move job not found");
    job = found->second;
  }

  if (job->finished.load()) return;
  job->state = JobState::cancelling;
  job->cancel_requested = true;
  std::shared_ptr<ChessEngine> engine;
  {
    std::lock_guard engine_lock(engine_mutex_);
    engine = engine_;
  }
  if (engine) engine->cancel();
}

BotService::EngineLease BotService::acquire_engine() {
  std::lock_guard engine_lock(engine_mutex_);
  if (!engine_) {
    engine_ = create_stockfish_engine(kStockfish18Id);
    engine_->start();
    engine_session_id_ = next_engine_session_id_++;
    engine_jobs_started_ = 0;
  } else if (!engine_->is_ready()) {
    engine_->start();
  }

  EngineLease lease{
      .engine = engine_,
      .session_id = engine_session_id_,
      .warm = engine_jobs_started_ > 0,
  };
  ++engine_jobs_started_;
  return lease;
}

void BotService::run_job(const std::shared_ptr<BotJob>& job) noexcept {
  try {
    job->state = JobState::running;
    const auto budget = position_budget(job->fen);

    AnalysisResult scout;
    AnalysisResult verification;
    BotMoveChoice planned_choice;
    BotMoveChoice choice;
    std::shared_ptr<ChessEngine> engine;

    if (!budget.forced_move.empty()) {
      // There is no chess decision to make. Avoid spending any search budget
      // on a position where exactly one legal move exists.
      choice.move = budget.forced_move;
      choice.rank = 1;
      choice.probability = 1.0;
    } else {
      const auto lease = acquire_engine();
      engine = lease.engine;
      {
        std::lock_guard result_lock(job->result_mutex);
        job->engine_session_id = lease.session_id;
        job->reused_warm_engine = lease.warm;
      }
    }

    if (budget.forced_move.empty() && job->requested_elo == kBotEloMaximum) {
      AnalysisRequest request;
      request.fen = job->fen;
      request.depth = job->plan.verification_depth;
      request.multi_pv = 1;
      request.threads = 1;
      request.hash_mb = 64;
      request.node_limit = scaled_nodes(
          job->plan.verification_node_budget, budget.multiplier);
      request.time_limit_ms = scaled_milliseconds(
          job->plan.verification_time_budget_ms, budget.multiplier);
      request.dynamic_early_stop = true;
      request.early_stop_min_depth = std::max(7, job->plan.verification_depth - 5);
      request.early_stop_stable_iterations = 2;
      request.early_stop_eval_tolerance_cp = 12;
      request.cancel_requested = &job->cancel_requested;
      scout = engine->analyze(request);
      verification = scout;
      if (!scout.best_move.empty() && scout.best_move != "(none)" && scout.best_move != "0000") {
        choice.move = scout.best_move;
        choice.rank = 1;
        choice.probability = 1.0;
      }
    } else if (budget.forced_move.empty()
               && job->plan.use_stockfish_limit_strength) {
      // Stockfish 18 already implements Elo-limited play by internally
      // searching a small candidate set and statistically selecting a weaker
      // move. Exposing our own MultiPV scout on top would repeat that work.
      // One public PV + one search is therefore the entire 1300..3100 path.
      AnalysisRequest request;
      request.fen = job->fen;
      request.depth = job->plan.verification_depth;
      request.multi_pv = 1;
      request.threads = 1;
      request.hash_mb = 64;
      request.node_limit = scaled_nodes(
          job->plan.verification_node_budget, budget.multiplier);
      request.time_limit_ms = scaled_milliseconds(
          job->plan.verification_time_budget_ms, budget.multiplier);
      request.uci_limit_strength = true;
      request.uci_elo = job->plan.stockfish_uci_elo;
      request.dynamic_early_stop = true;
      request.early_stop_min_depth = std::max(
          5, job->plan.verification_depth - 4);
      request.early_stop_stable_iterations = 2;
      request.early_stop_eval_tolerance_cp =
          job->requested_elo >= 2200 ? 18 : 30;
      request.cancel_requested = &job->cancel_requested;
      scout = engine->analyze(request);
      verification = scout;
      if (!scout.best_move.empty() && scout.best_move != "(none)"
          && scout.best_move != "0000") {
        choice.move = scout.best_move;
        choice.rank = 1;
        choice.probability = 1.0;
      }
    } else if (budget.forced_move.empty()) {
      // Phase 1: cheap root scout. Even a planned rank-20 move is only searched
      // shallowly here, so low Elo no longer means a deep 20/32-PV analysis.
      AnalysisRequest scout_request;
      scout_request.fen = job->fen;
      scout_request.depth = job->plan.scout_depth;
      scout_request.multi_pv = job->plan.scout_lines;
      scout_request.threads = 1;
      scout_request.hash_mb = 64;
      scout_request.node_limit = scaled_nodes(
          job->plan.scout_node_budget, budget.multiplier);
      scout_request.time_limit_ms = scaled_milliseconds(
          job->plan.scout_time_budget_ms, budget.multiplier);
      scout_request.allow_extended_multipv = job->plan.scout_lines > 16;
      scout_request.dynamic_early_stop = true;
      scout_request.early_stop_min_depth = std::max(3, job->plan.scout_depth - 2);
      scout_request.early_stop_stable_iterations = 1;
      scout_request.early_stop_eval_tolerance_cp = 45;
      scout_request.cancel_requested = &job->cancel_requested;
      scout = engine->analyze(scout_request);
      if (job->cancel_requested.load()) {
        job->state = JobState::cancelled;
        job->finished = true;
        return;
      }

      planned_choice = choose_scout_bot_move(scout.lines, job->plan);
      if (planned_choice.move.empty() && !scout.best_move.empty()) {
        planned_choice.move = scout.best_move;
        planned_choice.rank = 1;
        planned_choice.probability = 1.0;
      }

      const auto search_moves = verification_moves(scout, planned_choice);
      if (!search_moves.empty()) {
        // Phase 2: only the selected candidate, the scout best move and at most
        // one safer neighbour receive a deeper search. Stable quiet positions
        // can stop several plies before the hard verification depth.
        AnalysisRequest verify_request;
        verify_request.fen = job->fen;
        verify_request.depth = job->plan.verification_depth;
        verify_request.multi_pv = static_cast<int>(search_moves.size());
        verify_request.threads = 1;
        verify_request.hash_mb = 64;
        verify_request.node_limit = scaled_nodes(
            job->plan.verification_node_budget, budget.multiplier);
        verify_request.time_limit_ms = scaled_milliseconds(
            job->plan.verification_time_budget_ms, budget.multiplier);
        verify_request.search_moves = search_moves;
        verify_request.dynamic_early_stop = true;
        verify_request.early_stop_min_depth = std::max(4, job->plan.verification_depth - 5);
        verify_request.early_stop_stable_iterations = 2;
        verify_request.early_stop_eval_tolerance_cp =
            job->requested_elo >= 2200 ? 18 : 32;
        verify_request.early_stop_score_gap_cp =
            job->requested_elo >= 2200 ? 120 : 170;
        verify_request.cancel_requested = &job->cancel_requested;
        verification = engine->analyze(verify_request);
        choice = finalize_verified_bot_move(
            verification.lines, planned_choice, job->plan);
      } else {
        verification = scout;
        choice = planned_choice;
      }
    }

    if (job->cancel_requested.load()) {
      job->state = JobState::cancelled;
      job->finished = true;
      return;
    }

    if (choice.move.empty() && !scout.best_move.empty()) {
      choice.move = scout.best_move;
      choice.rank = 1;
      choice.probability = 1.0;
    }

    std::string selected_san;
    std::string selected_fen_after;
    std::string selected_position_after_json;
    if (!choice.move.empty() && choice.move != "(none)" && choice.move != "0000") {
      const auto applied = apply_legal_uci_move(job->fen, choice.move);
      selected_san = applied.san;
      selected_fen_after = applied.fen_after;
      selected_position_after_json = position_view_json(applied.fen_after);
    }

    // The verified root score for the actually selected move is also a very
    // good post-move eval-bar estimate. Reuse it instead of launching a second
    // full-strength Stockfish job after every bot move.
    std::optional<int> selected_cp;
    std::optional<int> selected_mate;
    std::optional<WdlScore> selected_wdl;
    const EngineLine* selected_line = line_for_move(verification.lines, choice.move);
    if (selected_line == nullptr) selected_line = line_for_move(scout.lines, choice.move);
    if (selected_line != nullptr) {
      const bool white = white_to_move(job->fen);
      if (selected_line->evaluation_cp.has_value()) {
        selected_cp = white ? *selected_line->evaluation_cp : -*selected_line->evaluation_cp;
      }
      if (selected_line->mate_in.has_value()) {
        selected_mate = white ? *selected_line->mate_in : -*selected_line->mate_in;
      }
      if (selected_line->wdl.has_value()) {
        selected_wdl = white
            ? *selected_line->wdl
            : WdlScore{
                  .wins = selected_line->wdl->losses,
                  .draws = selected_line->wdl->draws,
                  .losses = selected_line->wdl->wins,
              };
      }
    }

    const auto outcome = position_outcome(
        selected_fen_after.empty() ? job->fen : selected_fen_after);
    {
      std::lock_guard result_lock(job->result_mutex);
      job->analysis = std::move(scout);
      job->choice = std::move(choice);
      job->selected_san = std::move(selected_san);
      job->selected_fen_after = std::move(selected_fen_after);
      job->selected_position_after_json = std::move(selected_position_after_json);
      job->selected_evaluation_cp = selected_cp;
      job->selected_mate_in = selected_mate;
      job->selected_wdl = selected_wdl;
      job->terminal = outcome.terminal;
      job->checkmate = outcome.checkmate;
      job->result = outcome.result;
      job->engine_version = engine ? engine->version() : "Stockfish 18 (forced move)";
    }
    job->state = JobState::completed;
  } catch (const std::exception& error) {
    if (job->cancel_requested.load()) {
      job->state = JobState::cancelled;
    } else {
      std::lock_guard result_lock(job->result_mutex);
      job->error = error.what();
      job->state = JobState::failed;
    }
  } catch (...) {
    std::lock_guard result_lock(job->result_mutex);
    job->error = "Unknown bot move error";
    job->state = JobState::failed;
  }
  job->finished = true;
}

void BotService::reap_finished_jobs() {
  std::vector<std::shared_ptr<BotJob>> finished;
  {
    std::lock_guard lock(jobs_mutex_);
    for (const auto& [id, job] : jobs_) {
      (void)id;
      if (job->finished.load() && job->worker.joinable()) finished.push_back(job);
    }
  }
  for (const auto& job : finished) {
    if (job->worker.joinable()) job->worker.join();
  }
  if (!finished.empty()) {
    std::lock_guard lock(jobs_mutex_);
    for (const auto& job : finished) jobs_.erase(job->id);
  }
}

void BotService::stop_all_jobs() noexcept {
  std::vector<std::shared_ptr<BotJob>> jobs;
  {
    std::lock_guard lock(jobs_mutex_);
    for (const auto& [id, job] : jobs_) {
      (void)id;
      jobs.push_back(job);
      if (!job->finished.load()) {
        job->cancel_requested = true;
        job->state = JobState::cancelling;
      }
    }
  }
  std::shared_ptr<ChessEngine> engine;
  {
    std::lock_guard engine_lock(engine_mutex_);
    engine = engine_;
  }
  if (engine) engine->cancel();
  for (const auto& job : jobs) {
    if (job->worker.joinable()) job->worker.join();
    if (job->state.load() == JobState::cancelling) job->state = JobState::cancelled;
    job->finished = true;
  }
}

double BotService::next_random_unit() {
  std::lock_guard lock(random_mutex_);
  return std::generate_canonical<double, 53>(random_);
}

}  // namespace kchess
