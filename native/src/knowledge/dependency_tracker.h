#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "knowledge_graph_contract.h"

struct sqlite3;

namespace kchess::knowledge {

// -----------------------------------------------------------------------------
// Section: Provenance/dependency contract
// -----------------------------------------------------------------------------

enum class KnowledgeEntryKind { kNode, kEdge, kChunk };

struct KnowledgeEntryRef {
  KnowledgeEntryKind kind{KnowledgeEntryKind::kNode};
  std::string id;

  [[nodiscard]] bool empty() const noexcept { return id.empty(); }
  friend bool operator==(const KnowledgeEntryRef&, const KnowledgeEntryRef&) = default;
};

struct KnowledgeSourceRef {
  std::string source_type;
  std::string source_id;
  std::string source_version;

  [[nodiscard]] bool valid() const noexcept {
    return !source_type.empty() && !source_id.empty() && !source_version.empty();
  }
};

struct KnowledgeProvenanceRecord {
  KnowledgeSourceRef source;
  std::int64_t first_seen_ms{0};
  std::int64_t last_seen_ms{0};
};

struct KnowledgeInvalidation {
  KnowledgeEntryRef entry;
  KnowledgeSourceRef source;
  std::int64_t invalidated_at_ms{0};
};

[[nodiscard]] std::string_view to_string(KnowledgeEntryKind kind) noexcept;
[[nodiscard]] std::optional<KnowledgeEntryKind> knowledge_entry_kind_from_string(
    std::string_view value) noexcept;

// -----------------------------------------------------------------------------
// Section: SQLite provenance and dependency tracking
// -----------------------------------------------------------------------------

// Tracks where graph assertions came from and which source version each derived
// entry was built against. It stores locators/version metadata only; source
// payloads remain in their authoritative KChess stores.
class DependencyTracker {
 public:
  explicit DependencyTracker(std::filesystem::path data_directory);
  ~DependencyTracker();

  DependencyTracker(const DependencyTracker&) = delete;
  DependencyTracker& operator=(const DependencyTracker&) = delete;

  void open();
  void close() noexcept;
  [[nodiscard]] bool is_open() const noexcept { return db_ != nullptr; }

  // Replaces the provenance set for an entry. Existing matching source rows
  // retain first_seen while last_seen advances to observed_at_ms.
  void replace_provenance(const KnowledgeEntryRef& entry,
                          const std::vector<KnowledgeSourceRef>& sources,
                          std::int64_t observed_at_ms);

  [[nodiscard]] std::vector<KnowledgeProvenanceRecord> provenance_for(
      const KnowledgeEntryRef& entry) const;

  // Replaces dependency expectations for an entry. Rebuilding an entry through
  // this method also clears its previous invalidation state.
  void replace_dependencies(const KnowledgeEntryRef& entry,
                            const std::vector<KnowledgeSourceRef>& sources,
                            std::int64_t observed_at_ms);

  // Updates the authoritative source version and marks only dependents that were
  // built against a different version. The returned set is suitable for targeted
  // recomputation by later graph/chunk pipelines.
  [[nodiscard]] std::vector<KnowledgeInvalidation> invalidate_source_change(
      std::string_view source_type, std::string_view source_id,
      std::string_view new_source_version, std::int64_t observed_at_ms);

  [[nodiscard]] std::vector<KnowledgeInvalidation> pending_invalidations(
      std::size_t limit = 100) const;
  [[nodiscard]] bool dependencies_match(
      const KnowledgeEntryRef& entry,
      const std::vector<KnowledgeSourceRef>& sources) const;
  [[nodiscard]] bool is_invalidated(const KnowledgeEntryRef& entry) const;

  void clear_invalidation(const KnowledgeEntryRef& entry);

 private:
  std::filesystem::path database_path_;
  sqlite3* db_{nullptr};
};

}  // namespace kchess::knowledge
