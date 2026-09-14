#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

struct sqlite3;

namespace kchess::knowledge {

// -----------------------------------------------------------------------------
// Section: Persisted query-trace diagnostics
// -----------------------------------------------------------------------------

struct KnowledgeQueryTraceRecord {
  std::string id;
  std::string profile_id;
  std::string query_text;
  std::string intent;
  double route_confidence{0.0};
  bool answerable{false};
  double answerability_confidence{0.0};
  std::size_t seed_count{0};
  std::size_t candidate_node_count{0};
  std::size_t candidate_chunk_count{0};
  std::size_t selected_node_count{0};
  std::size_t selected_chunk_count{0};
  std::size_t expanded_nodes{0};
  std::size_t packet_tokens{0};
  std::int64_t created_at_ms{0};
  std::string trace_json;
};

// Query traces are derived diagnostics only. They contain routing/retrieval IDs,
// scores and bounded packet metadata, never provider prompts, complete PGNs or
// engine payloads.
class QueryTraceStore {
 public:
  explicit QueryTraceStore(std::filesystem::path data_directory);
  ~QueryTraceStore();

  QueryTraceStore(const QueryTraceStore&) = delete;
  QueryTraceStore& operator=(const QueryTraceStore&) = delete;

  void open();
  void close() noexcept;
  [[nodiscard]] bool is_open() const noexcept { return db_ != nullptr; }

  void upsert(const KnowledgeQueryTraceRecord& trace);
  [[nodiscard]] std::vector<KnowledgeQueryTraceRecord> recent(
      const std::string& profile_id, std::size_t limit = 20) const;

 private:
  std::filesystem::path database_path_;
  sqlite3* db_{nullptr};
};

}  // namespace kchess::knowledge
