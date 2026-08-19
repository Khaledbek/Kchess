#pragma once

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
  void cancel() noexcept override;
  std::string version() const override;
  void validate_available() const;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace kchess
