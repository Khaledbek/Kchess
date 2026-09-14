#include "dependency_tracker.h"

#include "../persistence/sqlite_write_priority.h"

#include <sqlite3.h>

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>

namespace kchess::knowledge {
namespace {

struct StatementDeleter {
  void operator()(sqlite3_stmt* statement) const noexcept {
    if (statement != nullptr) sqlite3_finalize(statement);
  }
};
using Statement = std::unique_ptr<sqlite3_stmt, StatementDeleter>;

Statement prepare(sqlite3* db, const char* sql) {
  sqlite3_stmt* raw = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &raw, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite3_errmsg(db));
  }
  return Statement(raw);
}

void execute(sqlite3* db, const char* sql) {
  char* error = nullptr;
  if (sqlite3_exec(db, sql, nullptr, nullptr, &error) != SQLITE_OK) {
    const std::string message = error == nullptr ? sqlite3_errmsg(db) : error;
    sqlite3_free(error);
    throw std::runtime_error(message);
  }
}

void step_done(sqlite3* db, sqlite3_stmt* statement, const char* operation) {
  if (sqlite3_step(statement) != SQLITE_DONE) {
    throw std::runtime_error(std::string(operation) + ": " + sqlite3_errmsg(db));
  }
}

void bind_text(sqlite3_stmt* statement, int index, std::string_view value) {
  sqlite3_bind_text(statement, index, value.data(), static_cast<int>(value.size()), SQLITE_TRANSIENT);
}

std::string column_text(sqlite3_stmt* statement, int index) {
  const auto* value = sqlite3_column_text(statement, index);
  return value == nullptr ? std::string{} : reinterpret_cast<const char*>(value);
}

class Transaction {
 public:
  explicit Transaction(sqlite3* db) : db_(db) { execute(db_, "BEGIN IMMEDIATE;"); }
  ~Transaction() {
    if (!committed_) sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
  }
  void commit() {
    execute(db_, "COMMIT;");
    committed_ = true;
  }

 private:
  persistence::BackgroundSqliteWriteGuard priority_guard_;
  sqlite3* db_;
  bool committed_{false};
};

void validate_entry(const KnowledgeEntryRef& entry) {
  if (entry.empty()) throw std::invalid_argument("knowledge entry id must not be empty");
}

void require_entry_exists(sqlite3* db, const KnowledgeEntryRef& entry) {
  const char* sql = nullptr;
  switch (entry.kind) {
    case KnowledgeEntryKind::kNode:
      sql = "SELECT 1 FROM knowledge_nodes WHERE id=?;";
      break;
    case KnowledgeEntryKind::kEdge:
      sql = "SELECT 1 FROM knowledge_edges WHERE id=?;";
      break;
    case KnowledgeEntryKind::kChunk:
      sql = "SELECT 1 FROM knowledge_chunks WHERE id=?;";
      break;
  }
  auto statement = prepare(db, sql);
  bind_text(statement.get(), 1, entry.id);
  if (sqlite3_step(statement.get()) != SQLITE_ROW) {
    throw std::invalid_argument("knowledge provenance/dependency owner does not exist");
  }
}

void validate_sources(const std::vector<KnowledgeSourceRef>& sources) {
  std::unordered_set<std::string> identities;
  for (const auto& source : sources) {
    if (!source.valid()) throw std::invalid_argument("invalid knowledge source reference");
    const std::string identity = source.source_type + "\n" + source.source_id;
    if (!identities.insert(identity).second) {
      throw std::invalid_argument("duplicate knowledge source reference");
    }
  }
}

sqlite3_int64 upsert_source(sqlite3* db, const KnowledgeSourceRef& source,
                            std::int64_t observed_at_ms, bool update_version = true) {
  const char* sql = update_version
                        ? "INSERT INTO knowledge_sources(source_type,source_id,source_version,first_seen_ms,last_seen_ms) "
                          "VALUES(?,?,?,?,?) ON CONFLICT(source_type,source_id) DO UPDATE SET "
                          "source_version=excluded.source_version,last_seen_ms=MAX(knowledge_sources.last_seen_ms,excluded.last_seen_ms);"
                        : "INSERT INTO knowledge_sources(source_type,source_id,source_version,first_seen_ms,last_seen_ms) "
                          "VALUES(?,?,?,?,?) ON CONFLICT(source_type,source_id) DO UPDATE SET "
                          "last_seen_ms=MAX(knowledge_sources.last_seen_ms,excluded.last_seen_ms);";
  auto insert = prepare(db, sql);
  bind_text(insert.get(), 1, source.source_type);
  bind_text(insert.get(), 2, source.source_id);
  bind_text(insert.get(), 3, source.source_version);
  sqlite3_bind_int64(insert.get(), 4, observed_at_ms);
  sqlite3_bind_int64(insert.get(), 5, observed_at_ms);
  step_done(db, insert.get(), "upsert knowledge source");

  auto select = prepare(
      db, "SELECT id FROM knowledge_sources WHERE source_type=? AND source_id=?;");
  bind_text(select.get(), 1, source.source_type);
  bind_text(select.get(), 2, source.source_id);
  if (sqlite3_step(select.get()) != SQLITE_ROW) {
    throw std::runtime_error("knowledge source disappeared after upsert");
  }
  return sqlite3_column_int64(select.get(), 0);
}

KnowledgeInvalidation read_invalidation(sqlite3_stmt* statement) {
  KnowledgeInvalidation result;
  const auto entry_kind = knowledge_entry_kind_from_string(column_text(statement, 0));
  if (!entry_kind) throw std::runtime_error("invalid persisted knowledge entry kind");
  result.entry.kind = *entry_kind;
  result.entry.id = column_text(statement, 1);
  result.source.source_type = column_text(statement, 2);
  result.source.source_id = column_text(statement, 3);
  result.source.source_version = column_text(statement, 4);
  result.invalidated_at_ms = sqlite3_column_int64(statement, 5);
  return result;
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Contract conversion
// -----------------------------------------------------------------------------

std::string_view to_string(KnowledgeEntryKind kind) noexcept {
  switch (kind) {
    case KnowledgeEntryKind::kNode: return "node";
    case KnowledgeEntryKind::kEdge: return "edge";
    case KnowledgeEntryKind::kChunk: return "chunk";
  }
  return "node";
}

std::optional<KnowledgeEntryKind> knowledge_entry_kind_from_string(
    std::string_view value) noexcept {
  if (value == "node") return KnowledgeEntryKind::kNode;
  if (value == "edge") return KnowledgeEntryKind::kEdge;
  if (value == "chunk") return KnowledgeEntryKind::kChunk;
  return std::nullopt;
}

// -----------------------------------------------------------------------------
// Section: Lifecycle
// -----------------------------------------------------------------------------

DependencyTracker::DependencyTracker(std::filesystem::path data_directory)
    : database_path_(std::move(data_directory) / "kchess.sqlite3") {}

DependencyTracker::~DependencyTracker() { close(); }

void DependencyTracker::open() {
  if (db_ != nullptr) return;
  if (sqlite3_open_v2(database_path_.string().c_str(), &db_,
                      SQLITE_OPEN_READWRITE | SQLITE_OPEN_FULLMUTEX, nullptr) != SQLITE_OK) {
    const std::string message = db_ == nullptr ? "open knowledge dependency database failed"
                                               : sqlite3_errmsg(db_);
    close();
    throw std::runtime_error(message);
  }
  execute(db_, "PRAGMA foreign_keys = ON;");
  execute(db_, "PRAGMA busy_timeout = 5000;");
}

void DependencyTracker::close() noexcept {
  if (db_ != nullptr) {
    sqlite3_close_v2(db_);
    db_ = nullptr;
  }
}

// -----------------------------------------------------------------------------
// Section: Provenance
// -----------------------------------------------------------------------------

void DependencyTracker::replace_provenance(
    const KnowledgeEntryRef& entry, const std::vector<KnowledgeSourceRef>& sources,
    std::int64_t observed_at_ms) {
  if (db_ == nullptr) throw std::runtime_error("DependencyTracker is not open");
  validate_entry(entry);
  validate_sources(sources);
  require_entry_exists(db_, entry);

  Transaction transaction(db_);
  std::vector<sqlite3_int64> active_source_ids;
  active_source_ids.reserve(sources.size());
  auto insert = prepare(
      db_,
      "INSERT INTO knowledge_provenance(entry_kind,entry_id,source_row_id,first_seen_ms,last_seen_ms) "
      "VALUES(?,?,?,?,?) ON CONFLICT(entry_kind,entry_id,source_row_id) DO UPDATE SET "
      "last_seen_ms=MAX(knowledge_provenance.last_seen_ms,excluded.last_seen_ms);");
  for (const auto& source : sources) {
    const auto source_row_id = upsert_source(db_, source, observed_at_ms);
    active_source_ids.push_back(source_row_id);
    sqlite3_reset(insert.get());
    sqlite3_clear_bindings(insert.get());
    bind_text(insert.get(), 1, to_string(entry.kind));
    bind_text(insert.get(), 2, entry.id);
    sqlite3_bind_int64(insert.get(), 3, source_row_id);
    sqlite3_bind_int64(insert.get(), 4, observed_at_ms);
    sqlite3_bind_int64(insert.get(), 5, observed_at_ms);
    step_done(db_, insert.get(), "upsert knowledge provenance");
  }

  auto select_existing = prepare(
      db_, "SELECT source_row_id FROM knowledge_provenance WHERE entry_kind=? AND entry_id=?;");
  bind_text(select_existing.get(), 1, to_string(entry.kind));
  bind_text(select_existing.get(), 2, entry.id);
  std::vector<sqlite3_int64> stale_source_ids;
  while (sqlite3_step(select_existing.get()) == SQLITE_ROW) {
    const auto candidate = sqlite3_column_int64(select_existing.get(), 0);
    bool active = false;
    for (const auto source_row_id : active_source_ids) {
      if (candidate == source_row_id) {
        active = true;
        break;
      }
    }
    if (!active) stale_source_ids.push_back(candidate);
  }

  auto remove = prepare(
      db_,
      "DELETE FROM knowledge_provenance WHERE entry_kind=? AND entry_id=? AND source_row_id=?;");
  for (const auto stale_source_id : stale_source_ids) {
    sqlite3_reset(remove.get());
    sqlite3_clear_bindings(remove.get());
    bind_text(remove.get(), 1, to_string(entry.kind));
    bind_text(remove.get(), 2, entry.id);
    sqlite3_bind_int64(remove.get(), 3, stale_source_id);
    step_done(db_, remove.get(), "remove stale knowledge provenance");
  }
  transaction.commit();
}

std::vector<KnowledgeProvenanceRecord> DependencyTracker::provenance_for(
    const KnowledgeEntryRef& entry) const {
  if (db_ == nullptr) throw std::runtime_error("DependencyTracker is not open");
  validate_entry(entry);
  auto statement = prepare(
      db_,
      "SELECT s.source_type,s.source_id,s.source_version,p.first_seen_ms,p.last_seen_ms "
      "FROM knowledge_provenance p JOIN knowledge_sources s ON s.id=p.source_row_id "
      "WHERE p.entry_kind=? AND p.entry_id=? ORDER BY s.source_type,s.source_id;");
  bind_text(statement.get(), 1, to_string(entry.kind));
  bind_text(statement.get(), 2, entry.id);

  std::vector<KnowledgeProvenanceRecord> result;
  while (sqlite3_step(statement.get()) == SQLITE_ROW) {
    KnowledgeProvenanceRecord record;
    record.source.source_type = column_text(statement.get(), 0);
    record.source.source_id = column_text(statement.get(), 1);
    record.source.source_version = column_text(statement.get(), 2);
    record.first_seen_ms = sqlite3_column_int64(statement.get(), 3);
    record.last_seen_ms = sqlite3_column_int64(statement.get(), 4);
    result.push_back(std::move(record));
  }
  return result;
}

// -----------------------------------------------------------------------------
// Section: Dependency tracking and invalidation
// -----------------------------------------------------------------------------

void DependencyTracker::replace_dependencies(
    const KnowledgeEntryRef& entry, const std::vector<KnowledgeSourceRef>& sources,
    std::int64_t observed_at_ms) {
  if (db_ == nullptr) throw std::runtime_error("DependencyTracker is not open");
  validate_entry(entry);
  validate_sources(sources);
  require_entry_exists(db_, entry);

  Transaction transaction(db_);
  auto remove = prepare(
      db_, "DELETE FROM knowledge_dependencies WHERE entry_kind=? AND entry_id=?;");
  bind_text(remove.get(), 1, to_string(entry.kind));
  bind_text(remove.get(), 2, entry.id);
  step_done(db_, remove.get(), "replace knowledge dependencies");

  auto insert = prepare(
      db_,
      "INSERT INTO knowledge_dependencies(entry_kind,entry_id,source_row_id,source_version,invalidated,invalidated_at_ms) "
      "VALUES(?,?,?,?,0,NULL);");
  for (const auto& source : sources) {
    const auto source_row_id = upsert_source(db_, source, observed_at_ms, false);
    sqlite3_reset(insert.get());
    sqlite3_clear_bindings(insert.get());
    bind_text(insert.get(), 1, to_string(entry.kind));
    bind_text(insert.get(), 2, entry.id);
    sqlite3_bind_int64(insert.get(), 3, source_row_id);
    bind_text(insert.get(), 4, source.source_version);
    step_done(db_, insert.get(), "insert knowledge dependency");
  }
  transaction.commit();
}

std::vector<KnowledgeInvalidation> DependencyTracker::invalidate_source_change(
    std::string_view source_type, std::string_view source_id,
    std::string_view new_source_version, std::int64_t observed_at_ms) {
  if (db_ == nullptr) throw std::runtime_error("DependencyTracker is not open");
  if (source_type.empty() || source_id.empty() || new_source_version.empty()) {
    throw std::invalid_argument("invalid changed knowledge source");
  }

  Transaction transaction(db_);
  KnowledgeSourceRef source{std::string(source_type), std::string(source_id),
                            std::string(new_source_version)};
  const auto source_row_id = upsert_source(db_, source, observed_at_ms);

  auto invalidate = prepare(
      db_,
      "UPDATE knowledge_dependencies SET invalidated=1,invalidated_at_ms=? "
      "WHERE source_row_id=? AND source_version<>?;");
  sqlite3_bind_int64(invalidate.get(), 1, observed_at_ms);
  sqlite3_bind_int64(invalidate.get(), 2, source_row_id);
  bind_text(invalidate.get(), 3, new_source_version);
  step_done(db_, invalidate.get(), "invalidate knowledge dependencies");

  auto select = prepare(
      db_,
      "SELECT d.entry_kind,d.entry_id,s.source_type,s.source_id,s.source_version,d.invalidated_at_ms "
      "FROM knowledge_dependencies d JOIN knowledge_sources s ON s.id=d.source_row_id "
      "WHERE d.source_row_id=? AND d.invalidated=1 ORDER BY d.entry_kind,d.entry_id;");
  sqlite3_bind_int64(select.get(), 1, source_row_id);
  std::vector<KnowledgeInvalidation> result;
  while (sqlite3_step(select.get()) == SQLITE_ROW) result.push_back(read_invalidation(select.get()));
  transaction.commit();
  return result;
}

std::vector<KnowledgeInvalidation> DependencyTracker::pending_invalidations(
    std::size_t limit) const {
  if (db_ == nullptr) throw std::runtime_error("DependencyTracker is not open");
  if (limit == 0) return {};
  auto statement = prepare(
      db_,
      "SELECT d.entry_kind,d.entry_id,s.source_type,s.source_id,s.source_version,d.invalidated_at_ms "
      "FROM knowledge_dependencies d JOIN knowledge_sources s ON s.id=d.source_row_id "
      "WHERE d.invalidated=1 ORDER BY d.invalidated_at_ms,d.entry_kind,d.entry_id LIMIT ?;");
  sqlite3_bind_int64(statement.get(), 1, static_cast<sqlite3_int64>(limit));
  std::vector<KnowledgeInvalidation> result;
  while (sqlite3_step(statement.get()) == SQLITE_ROW) result.push_back(read_invalidation(statement.get()));
  return result;
}

bool DependencyTracker::dependencies_match(
    const KnowledgeEntryRef& entry,
    const std::vector<KnowledgeSourceRef>& sources) const {
  if (db_ == nullptr) throw std::runtime_error("DependencyTracker is not open");
  validate_entry(entry);
  validate_sources(sources);

  std::vector<std::string> expected;
  expected.reserve(sources.size());
  for (const auto& source : sources) {
    expected.push_back(
        source.source_type + "\n" + source.source_id + "\n" + source.source_version);
  }
  std::sort(expected.begin(), expected.end());

  auto statement = prepare(
      db_,
      "SELECT s.source_type,s.source_id,d.source_version "
      "FROM knowledge_dependencies d JOIN knowledge_sources s ON s.id=d.source_row_id "
      "WHERE d.entry_kind=? AND d.entry_id=? "
      "ORDER BY s.source_type,s.source_id,d.source_version;");
  bind_text(statement.get(), 1, to_string(entry.kind));
  bind_text(statement.get(), 2, entry.id);

  std::vector<std::string> actual;
  int status = SQLITE_ROW;
  while ((status = sqlite3_step(statement.get())) == SQLITE_ROW) {
    actual.push_back(
        column_text(statement.get(), 0) + "\n" + column_text(statement.get(), 1) +
        "\n" + column_text(statement.get(), 2));
  }
  if (status != SQLITE_DONE) throw std::runtime_error(sqlite3_errmsg(db_));
  std::sort(actual.begin(), actual.end());
  return actual == expected;
}

bool DependencyTracker::is_invalidated(const KnowledgeEntryRef& entry) const {
  if (db_ == nullptr) throw std::runtime_error("DependencyTracker is not open");
  auto statement = prepare(db_,
      "SELECT 1 FROM knowledge_dependencies WHERE entry_kind=? AND entry_id=? AND invalidated=1 LIMIT 1;");
  bind_text(statement.get(), 1, to_string(entry.kind));
  bind_text(statement.get(), 2, entry.id);
  const auto status = sqlite3_step(statement.get());
  if (status != SQLITE_ROW && status != SQLITE_DONE) throw std::runtime_error(sqlite3_errmsg(db_));
  return status == SQLITE_ROW;
}

void DependencyTracker::clear_invalidation(const KnowledgeEntryRef& entry) {
  if (db_ == nullptr) throw std::runtime_error("DependencyTracker is not open");
  validate_entry(entry);
  persistence::BackgroundSqliteWriteGuard priority_guard;
  auto statement = prepare(
      db_,
      "UPDATE knowledge_dependencies AS d SET invalidated=0,invalidated_at_ms=NULL "
      "WHERE entry_kind=? AND entry_id=? AND source_version=("
      "SELECT s.source_version FROM knowledge_sources s WHERE s.id=d.source_row_id);");
  bind_text(statement.get(), 1, to_string(entry.kind));
  bind_text(statement.get(), 2, entry.id);
  step_done(db_, statement.get(), "clear knowledge invalidation");
}

}  // namespace kchess::knowledge
