#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace kchess {

struct TheoryMoveInfo {
  bool is_theory{false};
  std::uint32_t games{0};
  std::uint32_t white_wins{0};
  std::uint32_t draws{0};
  std::uint32_t black_wins{0};
};

struct OpeningBookMetadata {
  std::uint16_t format_version{0};
  std::uint64_t entry_count{0};
  std::uint32_t max_ply{0};
  std::uint32_t min_games{0};
  std::int64_t build_timestamp{0};
  std::string source;
  std::string license;
  std::string source_dump;
  std::string builder_version;
};

class OpeningTheoryProvider {
 public:
  virtual ~OpeningTheoryProvider() = default;
  virtual TheoryMoveInfo lookup(
      const std::string& fen,
      const std::string& uci_move) const = 0;
  bool contains_move(const std::string& fen, const std::string& uci_move) const {
    return lookup(fen, uci_move).is_theory;
  }
  virtual std::string source_version() const = 0;
  virtual std::string source_license() const = 0;
  virtual OpeningBookMetadata metadata() const = 0;
};

class UnavailableOpeningTheoryProvider final : public OpeningTheoryProvider {
 public:
  TheoryMoveInfo lookup(const std::string&, const std::string&) const override { return {}; }
  std::string source_version() const override { return "not-installed"; }
  std::string source_license() const override { return "not-applicable"; }
  OpeningBookMetadata metadata() const override { return {}; }
};

class KcbOpeningTheoryProvider final : public OpeningTheoryProvider {
 public:
  explicit KcbOpeningTheoryProvider(const std::filesystem::path& path);

  TheoryMoveInfo lookup(
      const std::string& fen,
      const std::string& uci_move) const override;
  std::string source_version() const override;
  std::string source_license() const override { return metadata_.license; }
  OpeningBookMetadata metadata() const override { return metadata_; }

 private:
  struct Entry {
    std::uint64_t position_key{0};
    std::uint16_t move{0};
    std::uint32_t games{0};
    std::uint32_t white_wins{0};
    std::uint32_t draws{0};
    std::uint32_t black_wins{0};
  };

  OpeningBookMetadata metadata_;
  std::vector<Entry> entries_;
};

}  // namespace kchess
