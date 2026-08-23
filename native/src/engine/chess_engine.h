#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace kchess {

struct AnalysisRequest {
  std::string fen;
  int depth{18};
  int multi_pv{3};
  int threads{2};
  int hash_mb{128};
  int time_limit_seconds{0};
  // Live refinement may stop before the hard depth target once consecutive
  // iterations are stable. Normal/minimum analysis leaves this disabled.
  bool dynamic_early_stop{false};
  int early_stop_min_depth{0};
  int early_stop_stable_iterations{3};
  int early_stop_eval_tolerance_cp{15};
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
  virtual void cancel() noexcept = 0;
  virtual std::string version() const = 0;
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
  AnalysisResult current_result() const;
  int current_depth() const noexcept;
  void cancel() noexcept override;
  std::string version() const override;
  void validate_available() const;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace kchess
