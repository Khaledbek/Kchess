#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <vector>

#include "knowledge_quality.h"

struct sqlite3;

namespace kchess::knowledge {

// -----------------------------------------------------------------------------
// Section: Persisted quality metadata
// -----------------------------------------------------------------------------

class KnowledgeQualityStore {
 public:
  explicit KnowledgeQualityStore(std::filesystem::path data_directory);
  ~KnowledgeQualityStore();

  KnowledgeQualityStore(const KnowledgeQualityStore&) = delete;
  KnowledgeQualityStore& operator=(const KnowledgeQualityStore&) = delete;

  void open();
  void close() noexcept;
  [[nodiscard]] bool is_open() const noexcept { return db_ != nullptr; }

  void upsert(const KnowledgeQualityMetrics& value);
  [[nodiscard]] std::optional<KnowledgeQualityMetrics> quality(
      const KnowledgeEntryRef& entry) const;
  bool remove(const KnowledgeEntryRef& entry);

  // Enumerates graph entries from the shared DB for bounded batch refreshes.
  // This is routing metadata only and does not read payload stores.
  [[nodiscard]] std::vector<KnowledgeEntryRef> entries(
      std::size_t limit = 500, std::size_t offset = 0) const;

 private:
  std::filesystem::path database_path_;
  sqlite3* db_{nullptr};
};

}  // namespace kchess::knowledge
