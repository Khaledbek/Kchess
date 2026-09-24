#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "theory/position_key.h"

namespace kchess {

struct OpeningLineContinuation {
  std::string uci;
  std::string san;
  std::string fen_after;
  std::uint64_t destination_key{0};
  // Present when the destination is part of the runtime graph reconstructed
  // from the stored KCL lines. Multiple move orders that transpose to the same
  // canonical position resolve to the same runtime node index.
  std::optional<std::uint32_t> destination_node;
};

struct OpeningLineGraphMetadata {
  std::uint16_t format_version{0};
  // KCL1 persists one sorted record per named terminal opening position.
  std::uint64_t node_count{0};
  // Number of persisted 16-bit move entries across all root-to-node lines.
  // This is not the number of unique runtime graph edges.
  std::uint64_t edge_count{0};
  std::uint32_t max_ply{0};
  std::int64_t build_timestamp{0};
  std::string source;
  std::string license;
  std::string builder_version;
};

// Immutable navigation surface reconstructed from compact KCL1 opening lines.
// KCL1 does not persist outgoing adjacency directly: each sorted record stores
// the complete legal move sequence from the standard start position to that
// record's terminal position. The reader replays every line once, validates its
// terminal Stockfish key, merges shared prefixes/transpositions by canonical
// position key, and exposes the resulting position -> legal continuations graph.
class OpeningLineGraph {
 public:
  virtual ~OpeningLineGraph() = default;
  virtual OpeningLineGraphMetadata metadata() const = 0;
  virtual std::string source_version() const = 0;
  virtual std::uint64_t position_key_fingerprint() const = 0;
  virtual bool available() const = 0;
  virtual std::string availability_error() const = 0;
  virtual std::optional<std::uint32_t> node_index(std::uint64_t position_key) const = 0;
  virtual std::vector<OpeningLineContinuation> continuations(const std::string& fen) const = 0;
};

class UnavailableOpeningLineGraph final : public OpeningLineGraph {
 public:
  explicit UnavailableOpeningLineGraph(std::string reason = "Opening-line graph is not installed")
      : reason_(std::move(reason)) {}

  OpeningLineGraphMetadata metadata() const override { return {}; }
  std::string source_version() const override { return "not-installed"; }
  std::uint64_t position_key_fingerprint() const override { return 0; }
  bool available() const override { return false; }
  std::string availability_error() const override { return reason_; }
  std::optional<std::uint32_t> node_index(std::uint64_t) const override {
    return std::nullopt;
  }
  std::vector<OpeningLineContinuation> continuations(const std::string&) const override {
    return {};
  }

 private:
  std::string reason_;
};

class KclOpeningLineGraph final : public OpeningLineGraph {
 public:
  explicit KclOpeningLineGraph(const std::filesystem::path& path);

  OpeningLineGraphMetadata metadata() const override { return metadata_; }
  std::string source_version() const override;
  std::uint64_t position_key_fingerprint() const override { return position_key_fingerprint_; }
  bool available() const override { return true; }
  std::string availability_error() const override { return {}; }
  std::optional<std::uint32_t> node_index(std::uint64_t position_key) const override;
  std::vector<OpeningLineContinuation> continuations(const std::string& fen) const override;

 private:
  struct LineRecord {
    std::uint64_t terminal_position_key{0};
    // Runtime move index after normalizing the persisted byte offset / 2.
    std::uint32_t move_offset{0};
    std::uint32_t move_count{0};
  };

  struct RuntimeNode {
    std::uint64_t position_key{0};
    std::vector<std::uint16_t> outgoing_moves;
  };

  std::uint32_t ensure_runtime_node(std::uint64_t position_key);
  void add_runtime_edge(std::uint32_t source_node, std::uint16_t encoded_move);

  OpeningLineGraphMetadata metadata_;
  std::uint64_t position_key_fingerprint_{kPositionKeyFingerprintSeed};
  std::vector<LineRecord> lines_;
  std::vector<std::uint16_t> moves_;
  std::vector<RuntimeNode> runtime_nodes_;
  std::unordered_map<std::uint64_t, std::uint32_t> runtime_lookup_;
};

}  // namespace kchess
