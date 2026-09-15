#include "knowledge_quality_store.h"

#include "../persistence/sqlite_write_priority.h"

#include <sqlite3.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
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

void bind_text(sqlite3_stmt* statement, const int index,
               const std::string_view value) {
  if (sqlite3_bind_text(statement, index, value.data(),
                        static_cast<int>(value.size()), SQLITE_TRANSIENT) !=
      SQLITE_OK) {
    throw std::runtime_error("bind knowledge quality text failed");
  }
}

void step_done(sqlite3* db, sqlite3_stmt* statement, const char* context) {
  if (sqlite3_step(statement) != SQLITE_DONE) {
    throw std::runtime_error(std::string(context) + ": " + sqlite3_errmsg(db));
  }
}

std::string column_text(sqlite3_stmt* statement, const int index) {
  const auto* value = sqlite3_column_text(statement, index);
  return value == nullptr ? std::string{} :
                            std::string(reinterpret_cast<const char*>(value));
}

}  // namespace

KnowledgeQualityStore::KnowledgeQualityStore(std::filesystem::path data_directory)
    : database_path_(std::move(data_directory) / "kchess.sqlite3") {}

KnowledgeQualityStore::~KnowledgeQualityStore() { close(); }

void KnowledgeQualityStore::open() {
  if (db_ != nullptr) return;
  if (sqlite3_open_v2(database_path_.string().c_str(), &db_,
                      SQLITE_OPEN_READWRITE | SQLITE_OPEN_FULLMUTEX, nullptr) !=
      SQLITE_OK) {
    const std::string message = db_ == nullptr
                                    ? "open knowledge quality database failed"
                                    : sqlite3_errmsg(db_);
    close();
    throw std::runtime_error(message);
  }
  execute(db_, "PRAGMA foreign_keys = ON;");
  execute(db_, "PRAGMA busy_timeout = 5000;");
}

void KnowledgeQualityStore::close() noexcept {
  if (db_ != nullptr) {
    sqlite3_close_v2(db_);
    db_ = nullptr;
  }
}

void KnowledgeQualityStore::upsert(const KnowledgeQualityMetrics& value) {
  if (db_ == nullptr) throw std::runtime_error("KnowledgeQualityStore is not open");
  if (!is_valid_quality_metrics(value)) {
    throw std::invalid_argument("invalid knowledge quality metrics");
  }

  persistence::BackgroundSqliteWriteGuard priority_guard;
  auto statement = prepare(
      db_,
      "INSERT INTO knowledge_quality("
      "entry_kind,entry_id,confidence,coverage,freshness,source_quality,"
      "evidence_diversity,importance,sample_size,evidence_count,temporal_scope,"
      "freshness_basis,evaluated_at_ms) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?) "
      "ON CONFLICT(entry_kind,entry_id) DO UPDATE SET "
      "confidence=excluded.confidence,coverage=excluded.coverage,"
      "freshness=excluded.freshness,source_quality=excluded.source_quality,"
      "evidence_diversity=excluded.evidence_diversity,"
      "importance=excluded.importance,sample_size=excluded.sample_size,"
      "evidence_count=excluded.evidence_count,temporal_scope=excluded.temporal_scope,"
      "freshness_basis=excluded.freshness_basis,"
      "evaluated_at_ms=excluded.evaluated_at_ms;");
  bind_text(statement.get(), 1, to_string(value.entry.kind));
  bind_text(statement.get(), 2, value.entry.id);
  sqlite3_bind_double(statement.get(), 3, value.confidence);
  sqlite3_bind_double(statement.get(), 4, value.coverage);
  if (value.freshness) {
    sqlite3_bind_double(statement.get(), 5, *value.freshness);
  } else {
    sqlite3_bind_null(statement.get(), 5);
  }
  sqlite3_bind_double(statement.get(), 6, value.source_quality);
  sqlite3_bind_double(statement.get(), 7, value.evidence_diversity);
  sqlite3_bind_double(statement.get(), 8, value.importance);
  sqlite3_bind_int64(statement.get(), 9,
                     static_cast<sqlite3_int64>(value.sample_size));
  sqlite3_bind_int64(statement.get(), 10,
                     static_cast<sqlite3_int64>(value.evidence_count));
  bind_text(statement.get(), 11, to_string(value.temporal_scope));
  bind_text(statement.get(), 12, to_string(value.freshness_basis));
  sqlite3_bind_int64(statement.get(), 13,
                     static_cast<sqlite3_int64>(value.evaluated_at_ms));
  step_done(db_, statement.get(), "upsert knowledge quality");
}

std::optional<KnowledgeQualityMetrics> KnowledgeQualityStore::quality(
    const KnowledgeEntryRef& entry) const {
  if (db_ == nullptr) throw std::runtime_error("KnowledgeQualityStore is not open");
  if (entry.empty()) return std::nullopt;

  auto statement = prepare(
      db_,
      "SELECT confidence,coverage,freshness,source_quality,evidence_diversity,"
      "importance,sample_size,evidence_count,temporal_scope,freshness_basis,"
      "evaluated_at_ms FROM knowledge_quality WHERE entry_kind=? AND entry_id=?;");
  bind_text(statement.get(), 1, to_string(entry.kind));
  bind_text(statement.get(), 2, entry.id);
  if (sqlite3_step(statement.get()) != SQLITE_ROW) return std::nullopt;

  KnowledgeQualityMetrics result;
  result.entry = entry;
  result.confidence = sqlite3_column_double(statement.get(), 0);
  result.coverage = sqlite3_column_double(statement.get(), 1);
  if (sqlite3_column_type(statement.get(), 2) != SQLITE_NULL) {
    result.freshness = sqlite3_column_double(statement.get(), 2);
  }
  result.source_quality = sqlite3_column_double(statement.get(), 3);
  result.evidence_diversity = sqlite3_column_double(statement.get(), 4);
  result.importance = sqlite3_column_double(statement.get(), 5);
  result.sample_size = sqlite3_column_int64(statement.get(), 6);
  result.evidence_count = sqlite3_column_int64(statement.get(), 7);
  const auto scope = knowledge_temporal_scope_from_string(column_text(statement.get(), 8));
  const auto basis = knowledge_freshness_basis_from_string(column_text(statement.get(), 9));
  if (!scope || !basis) return std::nullopt;
  result.temporal_scope = *scope;
  result.freshness_basis = *basis;
  result.evaluated_at_ms = sqlite3_column_int64(statement.get(), 10);
  return result;
}

bool KnowledgeQualityStore::remove(const KnowledgeEntryRef& entry) {
  if (db_ == nullptr) throw std::runtime_error("KnowledgeQualityStore is not open");
  if (entry.empty()) return false;
  persistence::BackgroundSqliteWriteGuard priority_guard;
  auto statement = prepare(
      db_, "DELETE FROM knowledge_quality WHERE entry_kind=? AND entry_id=?;");
  bind_text(statement.get(), 1, to_string(entry.kind));
  bind_text(statement.get(), 2, entry.id);
  step_done(db_, statement.get(), "remove knowledge quality");
  return sqlite3_changes(db_) > 0;
}

std::vector<KnowledgeEntryRef> KnowledgeQualityStore::entries(
    const std::size_t limit, const std::size_t offset) const {
  if (db_ == nullptr) throw std::runtime_error("KnowledgeQualityStore is not open");
  if (limit == 0) return {};

  auto statement = prepare(
      db_,
      "SELECT entry_kind,entry_id FROM ("
      "SELECT 'node' AS entry_kind,id AS entry_id FROM knowledge_nodes "
      "UNION ALL SELECT 'edge',id FROM knowledge_edges"
      ") ORDER BY entry_kind,entry_id LIMIT ? OFFSET ?;");
  sqlite3_bind_int64(statement.get(), 1, static_cast<sqlite3_int64>(limit));
  sqlite3_bind_int64(statement.get(), 2, static_cast<sqlite3_int64>(offset));

  std::vector<KnowledgeEntryRef> result;
  while (sqlite3_step(statement.get()) == SQLITE_ROW) {
    const auto kind = knowledge_entry_kind_from_string(column_text(statement.get(), 0));
    if (!kind) continue;
    result.push_back({*kind, column_text(statement.get(), 1)});
  }
  return result;
}

}  // namespace kchess::knowledge
