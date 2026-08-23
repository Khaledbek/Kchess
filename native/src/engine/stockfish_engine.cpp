#include "engine/chess_engine.h"

#include <algorithm>
#include <atomic>
#include <deque>
#include <cmath>
#include <filesystem>
#include <map>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string_view>

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

}  // namespace

class StockfishEngine::Impl {
 public:
  std::unique_ptr<Stockfish::Engine> engine;
  mutable std::mutex operation_mutex;
  mutable std::mutex engine_pointer_mutex;
  std::atomic_bool cancelled{false};
  std::atomic_int current_depth{0};
  mutable std::mutex live_result_mutex;
  std::map<int, EngineLine> live_lines;
  std::string live_best_move;
  bool ready{false};
  std::filesystem::path asset_directory;
};

StockfishEngine::StockfishEngine(std::filesystem::path asset_directory)
    : impl_(std::make_unique<Impl>()) {
#if defined(_WIN32)
  impl_->asset_directory = asset_directory.empty()
      ? module_directory() : std::move(asset_directory);
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
  if (request.depth < 1 || request.depth > 128 || request.multi_pv < 1
      || request.multi_pv > 16 || request.threads < 1 || request.threads > 64
      || request.hash_mb < 16 || request.hash_mb > 4096
      || request.time_limit_seconds < 0 || request.time_limit_seconds > 3600
      || request.early_stop_min_depth < 0 || request.early_stop_min_depth > 128
      || request.early_stop_stable_iterations < 1
      || request.early_stop_stable_iterations > 16
      || request.early_stop_eval_tolerance_cp < 0
      || request.early_stop_eval_tolerance_cp > 500) {
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
    impl_->live_lines.clear();
    impl_->live_best_move.clear();
  }

  // Do this before starting a Stockfish search. For checkmate/stalemate,
  // Stockfish correctly has no PV and no normal best move. Those positions
  // are valid game endpoints, not an analysis error.
  if (auto terminal = terminal_position_result(request.fen); terminal.has_value()) {
    return *terminal;
  }

  set_option(*impl_->engine, "Threads", std::to_string(request.threads));
  set_option(*impl_->engine, "Hash", std::to_string(request.hash_mb));
  set_option(*impl_->engine, "MultiPV", std::to_string(request.multi_pv));
  set_option(*impl_->engine, "Ponder", "false");
  set_option(*impl_->engine, "UCI_ShowWDL", "true");

  int last_stability_depth = 0;
  int stable_iterations = 0;
  std::vector<EngineLine> previous_stability_lines;
  bool converged_early = false;
  impl_->engine->set_on_update_no_moves([](const Stockfish::Engine::InfoShort&) {});
  impl_->engine->set_on_iter([](const Stockfish::Engine::InfoIter&) {});
  impl_->engine->set_on_update_full([&](const Stockfish::Engine::InfoFull& info) {
    impl_->current_depth.store(
        std::max(impl_->current_depth.load(), static_cast<int>(info.depth)));
    bool should_stop = false;
    {
      std::lock_guard result_lock(impl_->live_result_mutex);
      auto& line = impl_->live_lines[static_cast<int>(info.multiPV)];
      line.rank = static_cast<int>(info.multiPV);
      line.depth = info.depth;
      line.nodes = static_cast<std::uint64_t>(info.nodes);
      line.wdl = parse_wdl(info.wdl);
      line.moves = split_moves(info.pv);
      apply_score(line, info.score);

      // MultiPV callbacks arrive once per line. Evaluate convergence only when
      // every requested line has reached this depth, then compare the two most
      // important lines (best + runner-up). This keeps classification quality
      // stable while allowing quiet positions to finish well before max depth.
      const int depth = static_cast<int>(info.depth);
      if (request.dynamic_early_stop
          && depth >= request.early_stop_min_depth
          && depth > last_stability_depth) {
        std::vector<EngineLine> iteration;
        iteration.reserve(static_cast<std::size_t>(request.multi_pv));
        bool complete_iteration = true;
        for (int rank = 1; rank <= request.multi_pv; ++rank) {
          const auto found = impl_->live_lines.find(rank);
          if (found == impl_->live_lines.end() || found->second.depth != depth) {
            complete_iteration = false;
            break;
          }
          iteration.push_back(found->second);
        }
        if (complete_iteration) {
          if (stable_engine_lines(
                  iteration, previous_stability_lines,
                  request.early_stop_eval_tolerance_cp)) {
            ++stable_iterations;
          } else {
            stable_iterations = 0;
          }
          previous_stability_lines = std::move(iteration);
          last_stability_depth = depth;
          if (stable_iterations >= request.early_stop_stable_iterations) {
            converged_early = true;
            should_stop = true;
          }
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
  if (request.time_limit_seconds > 0) {
    limits.movetime = static_cast<Stockfish::TimePoint>(request.time_limit_seconds) * 1000;
  }
  impl_->engine->go(limits);
  if (request.cancel_requested != nullptr && request.cancel_requested->load()) {
    impl_->cancelled = true;
    impl_->engine->stop();
  }
  impl_->engine->wait_for_search_finished();

  impl_->engine->set_on_update_full([](const Stockfish::Engine::InfoFull&) {});
  impl_->engine->set_on_bestmove([](const std::string_view, const std::string_view) {});

  AnalysisResult result = current_result();
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
  result.best_move = impl_->live_best_move;
  for (const auto& [rank, line] : impl_->live_lines) {
    (void)rank;
    result.reached_depth = std::max(result.reached_depth, line.depth);
    result.nodes = std::max(result.nodes, line.nodes);
    result.lines.push_back(line);
  }
  if ((result.best_move.empty() || result.best_move == "(none)"
       || result.best_move == "0000")
      && !result.lines.empty() && !result.lines.front().moves.empty()) {
    result.best_move = result.lines.front().moves.front();
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
    impl_->ready = false;
  } catch (...) {
  }
}

std::string StockfishEngine::version() const {
  return "Stockfish 18 (cb3d4ee9b47d0c5aae855b12379378ea1439675c)";
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
