#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace kchess {

// -----------------------------------------------------------------------------
// Section: Engine analysis contracts
// -----------------------------------------------------------------------------

struct AnalysisRequest {
  std::string fen;
  int depth{18};
  int multi_pv{3};
  int threads{2};
  int hash_mb{128};
  int time_limit_seconds{0};
  // Optional hard budgets for latency-sensitive callers such as bot play.
  // Zero keeps the established depth-only behavior.
  std::uint64_t node_limit{0};
  int time_limit_ms{0};
  // Optional native Stockfish playing-strength handicap. This is used by
  // local bot play only; normal analysis leaves it disabled. Stockfish 18
  // accepts UCI_Elo 1320..3190 when UCI_LimitStrength is enabled.
  bool uci_limit_strength{false};
  int uci_elo{0};
  // Optional UCI root restriction. Preparation analysis uses this to evaluate
  // the actually played move directly instead of expanding a costly MultiPV
  // search merely to discover that move's score. Empty means all legal moves.
  std::vector<std::string> search_moves;
  // Live refinement may stop before the hard depth target once consecutive
  // iterations are stable. Normal/minimum analysis leaves this disabled.
  bool dynamic_early_stop{false};
  int early_stop_min_depth{0};
  int early_stop_stable_iterations{3};
  int early_stop_eval_tolerance_cp{15};
  // If rank 1 is already this many centipawns ahead of rank 2 at an eligible
  // complete iteration, latency-sensitive searches may stop immediately.
  // Zero disables this shortcut.
  int early_stop_score_gap_cp{0};
  // Preparation analysis can require ordinary centipawn scores so mate/tactical
  // positions always continue to the configured hard depth.
  bool early_stop_require_cp_scores{false};
  // SF19 callers that need position-stable analysis can require a snapshot
  // made only from one complete, exact MultiPV iteration. SF18 deliberately
  // ignores this flag so its established adapter behavior remains unchanged.
  bool require_exact_multipv_snapshot{false};
  // Normal analysis keeps the established 16-line ceiling. The local bot may
  // explicitly widen Stockfish 18 to 32 candidates so low Elo can choose
  // realistic deep-ranked legal moves without changing analysis settings.
  bool allow_extended_multipv{false};
  const std::atomic_bool* cancel_requested{nullptr};
};

struct WdlScore {
  int wins{0};
  int draws{0};
  int losses{0};
};

struct EngineLine {
  int rank{1};
  int depth{0};
  std::optional<int> evaluation_cp;
  std::optional<int> mate_in;
  std::optional<WdlScore> wdl;
  std::uint64_t nodes{0};
  std::vector<std::string> moves;

  std::string best_move() const { return moves.empty() ? std::string{} : moves.front(); }
};

struct AnalysisResult {
  std::vector<EngineLine> lines;
  std::string best_move;
  int reached_depth{0};
  std::uint64_t nodes{0};
  // True when a live search was deliberately interrupted so another
  // position can take priority. Partial lines remain usable as a checkpoint.
  bool interrupted{false};
  // True when the dynamic live-analysis stability test accepted the position
  // before the hard maximum depth. It is still a completed refinement.
  bool converged_early{false};
};

class ChessEngine {
 public:
  virtual ~ChessEngine() = default;
  virtual void start() = 0;
  virtual bool is_ready() const noexcept = 0;
  virtual void new_game() = 0;
  virtual void stop() noexcept = 0;
  virtual AnalysisResult analyze(const AnalysisRequest& request) = 0;
  virtual AnalysisResult current_result() const = 0;
  virtual int current_depth() const noexcept = 0;
  // Clears only the adapter's transient published live snapshot. This must not
  // clear Hash/TT or otherwise start a new game. The default keeps established
  // adapters unchanged; SF19 overrides it so a reused sideline engine cannot
  // expose the previous position before the next search publishes a result.
  virtual void clear_live_result() noexcept {}
  virtual void cancel() noexcept = 0;
  virtual std::string id() const = 0;
  virtual std::string version() const = 0;
  // Stable identity for persisted/in-memory engine results. It deliberately
  // includes the NNUE network identity so replacing a network cannot reuse
  // results produced by a different evaluator.
  virtual std::string cache_identity() const = 0;
  virtual void validate_available() const = 0;
};

class StockfishEngine final : public ChessEngine {
 public:
  explicit StockfishEngine(std::filesystem::path asset_directory = {});
  ~StockfishEngine() override;

  StockfishEngine(const StockfishEngine&) = delete;
  StockfishEngine& operator=(const StockfishEngine&) = delete;

  void start() override;
  bool is_ready() const noexcept override;
  void new_game() override;
  void stop() noexcept override;
  AnalysisResult analyze(const AnalysisRequest& request) override;
  AnalysisResult current_result() const override;
  int current_depth() const noexcept override;
  void cancel() noexcept override;
  std::string id() const override;
  std::string version() const override;
  std::string cache_identity() const override;
  void validate_available() const override;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

class Stockfish19Engine final : public ChessEngine {
 public:
  explicit Stockfish19Engine(std::filesystem::path asset_directory = {});
  ~Stockfish19Engine() override;

  Stockfish19Engine(const Stockfish19Engine&) = delete;
  Stockfish19Engine& operator=(const Stockfish19Engine&) = delete;

  void start() override;
  bool is_ready() const noexcept override;
  void new_game() override;
  void stop() noexcept override;
  AnalysisResult analyze(const AnalysisRequest& request) override;
  AnalysisResult current_result() const override;
  int current_depth() const noexcept override;
  void clear_live_result() noexcept override;
  void cancel() noexcept override;
  std::string id() const override;
  std::string version() const override;
  std::string cache_identity() const override;
  void validate_available() const override;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace kchess
