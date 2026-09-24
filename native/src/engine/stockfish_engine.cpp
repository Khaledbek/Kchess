#include "engine/chess_engine.h"

#include <algorithm>
#include <atomic>
#include <deque>
#include <cmath>
#include <filesystem>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string_view>

#include "engine/bot_move_selector.h"
#include "engine/stockfish_runtime.h"
#include "engine.h"
#include "misc.h"
#include "movegen.h"
#include "position.h"
#include "score.h"
#include "search.h"
#include "tune.h"
#include "uci.h"
#include "ucioption.h"

#if defined(_WIN32)
#include <windows.h>
#endif

namespace kchess {
namespace {

constexpr std::string_view kBigNetwork = "nn-c288c895ea92.nnue";
constexpr std::string_view kSmallNetwork = "nn-37f18f62d772.nnue";

std::string utf8_path(const std::filesystem::path& value) {
  const auto encoded = value.u8string();
  return {encoded.begin(), encoded.end()};
}

#if defined(_WIN32)
const int kModuleAnchor = 0;

std::filesystem::path module_directory() {
  HMODULE module = nullptr;
  if (!GetModuleHandleExW(
          GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
              | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
          reinterpret_cast<LPCWSTR>(&kModuleAnchor), &module)) {
    throw std::runtime_error("Stockfish runtime directory could not be resolved");
  }
  std::vector<wchar_t> buffer(512);
  while (true) {
    const DWORD length = GetModuleFileNameW(
        module, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0) {
      throw std::runtime_error("Stockfish runtime directory could not be resolved");
    }
    if (length < buffer.size() - 1) {
      return std::filesystem::path(std::wstring(buffer.data(), length)).parent_path();
    }
    buffer.resize(buffer.size() * 2);
  }
}
#endif

void set_option(Stockfish::Engine& engine, const std::string& name, const std::string& value) {
  std::istringstream input("name " + name + " value " + value);
  engine.get_options().setoption(input);
}

std::vector<std::string> split_moves(const std::string_view value) {
  std::istringstream input{std::string(value)};
  std::vector<std::string> moves;
  std::string move;
  while (input >> move) moves.push_back(move);
  return moves;
}

std::optional<WdlScore> parse_wdl(const std::string_view value) {
  std::istringstream input{std::string(value)};
  WdlScore result;
  if (input >> result.wins >> result.draws >> result.losses) return result;
  return std::nullopt;
}

void apply_score(EngineLine& line, const Stockfish::Score& score) {
  const std::string formatted = Stockfish::UCIEngine::format_score(score);
  std::istringstream input(formatted);
  std::string kind;
  int value = 0;
  input >> kind >> value;
  if (kind == "cp") {
    line.evaluation_cp = value;
    line.mate_in.reset();
  } else if (kind == "mate") {
    line.mate_in = value;
    line.evaluation_cp.reset();
  }
}

bool stable_engine_score(
    const EngineLine& current, const EngineLine& previous, const int tolerance_cp) {
  if (current.mate_in.has_value() && previous.mate_in.has_value()) {
    // Once both iterations see mate for the same side, the exact distance can
    // still wobble by a ply or two without changing the practical verdict.
    return (*current.mate_in >= 0) == (*previous.mate_in >= 0);
  }
  if (current.evaluation_cp.has_value() && previous.evaluation_cp.has_value()) {
    return std::abs(*current.evaluation_cp - *previous.evaluation_cp) <= tolerance_cp;
  }
  return false;
}

bool decisive_score_gap(
    const std::vector<EngineLine>& lines, const int threshold_cp) {
  if (threshold_cp <= 0 || lines.size() < 2) return false;
  const auto& best = lines[0];
  const auto& second = lines[1];
  if (best.mate_in.has_value()) {
    return *best.mate_in > 0
        && (!second.mate_in.has_value() || *second.mate_in <= 0);
  }
  if (best.evaluation_cp.has_value() && second.evaluation_cp.has_value()) {
    return *best.evaluation_cp - *second.evaluation_cp >= threshold_cp;
  }
  return false;
}

bool stable_engine_lines(
    const std::vector<EngineLine>& current,
    const std::vector<EngineLine>& previous,
    const int tolerance_cp) {
  const std::size_t compared = std::min<std::size_t>(2, current.size());
  if (compared == 0 || previous.size() < compared) return false;
  for (std::size_t index = 0; index < compared; ++index) {
    if (current[index].best_move().empty()
        || current[index].best_move() != previous[index].best_move()
        || !stable_engine_score(current[index], previous[index], tolerance_cp)) {
      return false;
    }
  }
  return true;
}

std::optional<AnalysisResult> terminal_position_result(const std::string& fen) {
  // A terminal chess position legitimately has no best move / principal
  // variation.  Treat it as a valid analysis result instead of an engine
  // failure so a game ending in mate or stalemate can finish analysis.
  std::deque<Stockfish::StateInfo> states(1);
  Stockfish::Position position;
  position.set(fen, false, &states.back());
  if (Stockfish::MoveList<Stockfish::LEGAL>(position).size() != 0) {
    return std::nullopt;
  }

  AnalysisResult result;
  EngineLine line;
  line.rank = 1;
  line.depth = 0;
  line.nodes = 0;

  if (position.checkers()) {
    // Side to move is checkmated. mate=0 maps to a zero expected score for
    // side-to-move in Kchess' existing evaluation helpers; the mover of the
    // preceding move therefore receives 1.0.
    line.mate_in = 0;
    line.wdl = WdlScore{0, 0, 1000};
  } else {
    // Stalemate / other no-legal-move draw.
    line.wdl = WdlScore{0, 1000, 0};
  }

  result.lines.push_back(std::move(line));
  return result;
}

int stockfish18_root_line_count(
    const std::string& fen, const AnalysisRequest& request) {
  if (!request.search_moves.empty()) {
    return std::max(1, std::min(
        request.multi_pv, static_cast<int>(request.search_moves.size())));
  }
  std::deque<Stockfish::StateInfo> states(1);
  Stockfish::Position position;
  position.set(fen, false, &states.back());
  const int legal_moves = static_cast<int>(
      Stockfish::MoveList<Stockfish::LEGAL>(position).size());
  return std::max(1, std::min(request.multi_pv, legal_moves));
}

}  // namespace

class StockfishEngine::Impl {
 public:
  std::unique_ptr<Stockfish::Engine> engine;
  mutable std::mutex operation_mutex;
  mutable std::mutex engine_pointer_mutex;
  std::atomic_bool cancelled{false};
  std::atomic_int current_depth{0};
  mutable std::mutex live_result_mutex;
  std::vector<EngineLine> live_complete_lines;
  int live_complete_depth{0};
  std::string live_best_move;
  bool ready{false};
  std::optional<int> configured_threads;
  std::optional<int> configured_hash_mb;
  std::optional<bool> configured_limit_strength;
  std::optional<int> configured_uci_elo;
  std::filesystem::path asset_directory;
};

StockfishEngine::StockfishEngine(std::filesystem::path asset_directory)
    : impl_(std::make_unique<Impl>()) {
#if defined(_WIN32)
  if (!asset_directory.empty()) {
    impl_->asset_directory = std::move(asset_directory);
  } else {
    const auto module_dir = module_directory();
    const bool module_has_networks =
        std::filesystem::exists(module_dir / kBigNetwork)
        && std::filesystem::exists(module_dir / kSmallNetwork);
    if (module_has_networks) {
      impl_->asset_directory = module_dir;
    }
#if defined(KCHESS_STOCKFISH_ASSET_DIR)
    else {
      impl_->asset_directory = std::filesystem::path(KCHESS_STOCKFISH_ASSET_DIR);
    }
#else
    else {
      impl_->asset_directory = module_dir;
    }
#endif
  }
#else
  impl_->asset_directory = std::move(asset_directory);
#endif
}

StockfishEngine::~StockfishEngine() { stop(); }

void StockfishEngine::start() {
  std::lock_guard lock(impl_->operation_mutex);
  if (impl_->ready) return;
  validate_available();
  initialize_stockfish_runtime();
  std::lock_guard pointer_lock(impl_->engine_pointer_mutex);
  // KChess embeds Stockfish in a DLL. Passing a pseudo executable path here
  // makes upstream CommandLine call _get_pgmptr(), whose EXE-only UCRT state is
  // not initialized in this DLL and asserts in Windows debug builds.
  impl_->engine = std::make_unique<Stockfish::Engine>(std::nullopt);
  // Discard settings cached for a previous Stockfish instance.
  impl_->configured_threads.reset();
  impl_->configured_hash_mb.reset();
  impl_->configured_limit_strength.reset();
  impl_->configured_uci_elo.reset();
#if defined(_WIN32)
  set_option(
      *impl_->engine, "EvalFile", utf8_path(impl_->asset_directory / kBigNetwork));
  set_option(
      *impl_->engine, "EvalFileSmall",
      utf8_path(impl_->asset_directory / kSmallNetwork));
#endif
  impl_->engine->set_on_verify_networks([](const std::string_view) {});
  Stockfish::Tune::init(impl_->engine->get_options());
  impl_->engine->verify_networks();
  // Construction already allocated the thread pool and TT. Record their actual
  // options so a matching first request does not rebuild them unnecessarily.
  const auto& options = impl_->engine->get_options();
  impl_->configured_threads = static_cast<int>(options["Threads"]);
  impl_->configured_hash_mb = static_cast<int>(options["Hash"]);
  impl_->ready = true;
}

bool StockfishEngine::is_ready() const noexcept { return impl_->ready; }

void StockfishEngine::new_game() {
  std::lock_guard lock(impl_->operation_mutex);
  if (!impl_->ready || !impl_->engine) throw std::runtime_error("Stockfish is not ready");
  impl_->engine->search_clear();
}

AnalysisResult StockfishEngine::analyze(const AnalysisRequest& request) {
  std::lock_guard lock(impl_->operation_mutex);
  if (!impl_->ready || !impl_->engine) throw std::runtime_error("Stockfish is not ready");
  const int maximum_multi_pv = request.allow_extended_multipv
      ? kBotMaximumCandidateLines
      : 16;
  if (request.depth < 1 || request.depth > 128 || request.multi_pv < 1
      || request.multi_pv > maximum_multi_pv
      || request.threads < 1 || request.threads > 64
      || request.hash_mb < 16 || request.hash_mb > 4096
      || request.time_limit_seconds < 0 || request.time_limit_seconds > 3600
      || request.node_limit > 1000000000ULL
      || request.time_limit_ms < 0 || request.time_limit_ms > 60000
      || (request.uci_limit_strength
          && (request.uci_elo < kStockfish18LowestUciElo
              || request.uci_elo > kStockfish18HighestUciElo))
      || request.early_stop_min_depth < 0 || request.early_stop_min_depth > 128
      || request.early_stop_stable_iterations < 1
      || request.early_stop_stable_iterations > 16
      || request.early_stop_eval_tolerance_cp < 0
      || request.early_stop_eval_tolerance_cp > 500
      || request.early_stop_score_gap_cp < 0
      || request.early_stop_score_gap_cp > 5000
      || request.search_moves.size() > 64
      || std::any_of(
          request.search_moves.begin(), request.search_moves.end(),
          [](const std::string& move) { return move.empty(); })) {
    throw std::invalid_argument("Invalid Stockfish analysis settings");
  }

  impl_->cancelled = false;
  impl_->current_depth = 0;
  if (request.cancel_requested != nullptr && request.cancel_requested->load()) {
    impl_->cancelled = true;
    throw std::runtime_error("Stockfish analysis cancelled");
  }
  {
    std::lock_guard live_lock(impl_->live_result_mutex);
    impl_->live_complete_lines.clear();
    impl_->live_complete_depth = 0;
    impl_->live_best_move.clear();
  }

  // Do this before starting a Stockfish search. For checkmate/stalemate,
  // Stockfish correctly has no PV and no normal best move. Those positions
  // are valid game endpoints, not an analysis error.
  if (auto terminal = terminal_position_result(request.fen); terminal.has_value()) {
    return *terminal;
  }

  // Stockfish's Threads callback rebuilds the thread pool and reallocates the
  // transposition table. Its Hash callback reallocates the table again. Sending
  // identical values for every position therefore destroys useful TT contents
  // and adds substantial allocation overhead. Keep these expensive options
  // stable across consecutive positions and only touch them when the requested
  // configuration actually changes.
  if (!impl_->configured_threads.has_value()
      || *impl_->configured_threads != request.threads) {
    set_option(*impl_->engine, "Threads", std::to_string(request.threads));
    impl_->configured_threads = request.threads;
  }
  if (!impl_->configured_hash_mb.has_value()
      || *impl_->configured_hash_mb != request.hash_mb) {
    set_option(*impl_->engine, "Hash", std::to_string(request.hash_mb));
    impl_->configured_hash_mb = request.hash_mb;
  }
  // Strength limiting is a cheap UCI option change and does not clear TT.
  // Always restore full strength when the persistent bot engine transitions
  // from an Elo-limited job to 3200 or to another non-limited caller.
  if (!impl_->configured_limit_strength.has_value()
      || *impl_->configured_limit_strength != request.uci_limit_strength) {
    set_option(
        *impl_->engine, "UCI_LimitStrength",
        request.uci_limit_strength ? "true" : "false");
    impl_->configured_limit_strength = request.uci_limit_strength;
  }
  if (request.uci_limit_strength
      && (!impl_->configured_uci_elo.has_value()
          || *impl_->configured_uci_elo != request.uci_elo)) {
    set_option(*impl_->engine, "UCI_Elo", std::to_string(request.uci_elo));
    impl_->configured_uci_elo = request.uci_elo;
  }

  set_option(*impl_->engine, "MultiPV", std::to_string(request.multi_pv));
  set_option(*impl_->engine, "Ponder", "false");
  set_option(*impl_->engine, "UCI_ShowWDL", "true");

  int last_stability_depth = 0;
  int stable_iterations = 0;
  std::vector<EngineLine> previous_stability_lines;
  bool converged_early = false;
  const int expected_line_count = stockfish18_root_line_count(request.fen, request);
  std::vector<EngineLine> pending_iteration;
  pending_iteration.reserve(static_cast<std::size_t>(expected_line_count));
  int pending_depth = 0;
  impl_->engine->set_on_update_no_moves([](const Stockfish::Engine::InfoShort&) {});
  impl_->engine->set_on_iter([](const Stockfish::Engine::InfoIter&) {});
  impl_->engine->set_on_update_full([&](const Stockfish::Engine::InfoFull& info) {
    impl_->current_depth.store(
        std::max(impl_->current_depth.load(), static_cast<int>(info.depth)));
    bool should_stop = false;
    {
      std::lock_guard result_lock(impl_->live_result_mutex);
      EngineLine line;
      line.rank = static_cast<int>(info.multiPV);
      line.depth = info.depth;
      line.nodes = static_cast<std::uint64_t>(info.nodes);
      line.wdl = parse_wdl(info.wdl);
      line.moves = split_moves(info.pv);
      apply_score(line, info.score);

      // SF18 can report another rank (or an aspiration retry) while older
      // slots still contain scores from a different search iteration. Publish
      // only one exact rank sequence with a shared depth and distinct moves.
      if (line.rank == 1) {
        pending_iteration.clear();
        pending_depth = line.depth;
      }
      const bool valid_move = !line.best_move().empty()
          && line.best_move() != "(none)" && line.best_move() != "0000";
      const bool duplicate_move = std::any_of(
          pending_iteration.begin(), pending_iteration.end(),
          [&](const EngineLine& previous) {
            return previous.best_move() == line.best_move();
          });
      if (!info.bound.empty() || !valid_move || duplicate_move
          || line.depth <= 0 || line.depth != pending_depth
          || line.rank != static_cast<int>(pending_iteration.size()) + 1
          || line.rank > expected_line_count) {
        pending_iteration.clear();
        pending_depth = 0;
        return;
      }
      pending_iteration.push_back(std::move(line));
      if (static_cast<int>(pending_iteration.size()) != expected_line_count) return;

      const int depth = pending_depth;
      if (depth >= impl_->live_complete_depth) {
        impl_->live_complete_lines = pending_iteration;
        impl_->live_complete_depth = depth;
      }
      pending_iteration.clear();
      pending_depth = 0;

      // Evaluate convergence only from a complete published iteration.
      if (request.dynamic_early_stop
          && depth >= request.early_stop_min_depth
          && depth > last_stability_depth) {
        std::vector<EngineLine> iteration = impl_->live_complete_lines;
        const bool ordinary_cp_iteration = std::all_of(
            iteration.begin(), iteration.end(), [](const EngineLine& line) {
              return line.evaluation_cp.has_value() && !line.mate_in.has_value();
            });
        const bool ordinary_cp_previous = std::all_of(
            previous_stability_lines.begin(), previous_stability_lines.end(),
            [](const EngineLine& line) {
              return line.evaluation_cp.has_value() && !line.mate_in.has_value();
            });
        const bool eligible_for_early_stop =
            !request.early_stop_require_cp_scores
            || (ordinary_cp_iteration && ordinary_cp_previous);
        if (eligible_for_early_stop
            && stable_engine_lines(
                iteration, previous_stability_lines,
                request.early_stop_eval_tolerance_cp)) {
          ++stable_iterations;
        } else {
          stable_iterations = 0;
        }
        previous_stability_lines = std::move(iteration);
        last_stability_depth = depth;
        const bool decisive_gap = decisive_score_gap(
            previous_stability_lines, request.early_stop_score_gap_cp);
        if (stable_iterations >= request.early_stop_stable_iterations
            || decisive_gap) {
          converged_early = true;
          should_stop = true;
        }
      }
    }
    if (should_stop) impl_->engine->stop();
  });
  impl_->engine->set_on_bestmove([&](const std::string_view move, const std::string_view) {
    std::lock_guard result_lock(impl_->live_result_mutex);
    impl_->live_best_move = move;
  });

  impl_->engine->set_position(request.fen, {});
  if (request.cancel_requested != nullptr && request.cancel_requested->load()) {
    impl_->cancelled = true;
    throw std::runtime_error("Stockfish analysis cancelled");
  }
  Stockfish::Search::LimitsType limits;
  limits.depth = request.depth;
  limits.searchmoves = request.search_moves;
  if (request.time_limit_seconds > 0) {
    limits.movetime = static_cast<Stockfish::TimePoint>(request.time_limit_seconds) * 1000;
  }
  if (request.time_limit_ms > 0) {
    limits.movetime = static_cast<Stockfish::TimePoint>(request.time_limit_ms);
  }
  limits.nodes = request.node_limit;
  impl_->engine->go(limits);
  if (request.cancel_requested != nullptr && request.cancel_requested->load()) {
    impl_->cancelled = true;
    impl_->engine->stop();
  }
  impl_->engine->wait_for_search_finished();

  impl_->engine->set_on_update_full([](const Stockfish::Engine::InfoFull&) {});
  impl_->engine->set_on_bestmove([](const std::string_view, const std::string_view) {});

  AnalysisResult result = current_result();
  {
    std::lock_guard result_lock(impl_->live_result_mutex);
    if (!impl_->live_best_move.empty()
        && impl_->live_best_move != "(none)" && impl_->live_best_move != "0000") {
      result.best_move = impl_->live_best_move;
    }
  }
  result.interrupted = impl_->cancelled.load();
  result.converged_early = converged_early;
  if (result.lines.empty()) {
    if (result.interrupted) throw std::runtime_error("Stockfish analysis cancelled");
    throw std::runtime_error("Stockfish returned no principal variation for a non-terminal position");
  }
  // An interrupted live search may be stopped between PV and bestmove
  // callbacks. The principal variation still gives us a valid checkpoint.
  if (result.best_move.empty() || result.best_move == "(none)" || result.best_move == "0000") {
    if (result.interrupted && !result.lines.front().moves.empty()) {
      result.best_move = result.lines.front().moves.front();
    } else {
      throw std::runtime_error("Stockfish returned no best move for a non-terminal position");
    }
  }
  return result;
}


AnalysisResult StockfishEngine::current_result() const {
  AnalysisResult result;
  std::lock_guard result_lock(impl_->live_result_mutex);
  if (impl_->live_complete_lines.empty()) return result;
  result.lines = impl_->live_complete_lines;
  result.reached_depth = impl_->live_complete_depth;
  // current_result() is the running-search view: publish the newest complete
  // MultiPV rank-1 recommendation. analyze() replaces it with Stockfish's
  // final bestmove once the search has actually completed.
  result.best_move = result.lines.front().best_move();
  for (const auto& line : result.lines) {
    result.nodes = std::max(result.nodes, line.nodes);
  }
  return result;
}

int StockfishEngine::current_depth() const noexcept {
  return impl_->current_depth.load();
}

void StockfishEngine::cancel() noexcept {
  impl_->cancelled = true;
  std::lock_guard pointer_lock(impl_->engine_pointer_mutex);
  if (impl_->engine) impl_->engine->stop();
}

void StockfishEngine::stop() noexcept {
  try {
    cancel();
    std::lock_guard lock(impl_->operation_mutex);
    std::lock_guard pointer_lock(impl_->engine_pointer_mutex);
    if (impl_->engine) impl_->engine->wait_for_search_finished();
    impl_->engine.reset();
    impl_->configured_threads.reset();
    impl_->configured_hash_mb.reset();
    impl_->configured_limit_strength.reset();
    impl_->configured_uci_elo.reset();
    impl_->ready = false;
  } catch (...) {
  }
}

std::string StockfishEngine::id() const { return "stockfish18"; }

std::string StockfishEngine::version() const {
  return "Stockfish 18 (cb3d4ee9b47d0c5aae855b12379378ea1439675c)";
}

std::string StockfishEngine::cache_identity() const {
  return version() + "|nnue=" + std::string(kBigNetwork) + "+" + std::string(kSmallNetwork);
}

void StockfishEngine::validate_available() const {
#if defined(_WIN32)
  for (const auto name : {kBigNetwork, kSmallNetwork}) {
    const auto path = impl_->asset_directory / name;
    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error) || error) {
      throw std::runtime_error(
          "Stockfish NNUE file is missing or invalid: " + utf8_path(path));
    }
  }
#endif
}

}  // namespace kchess
