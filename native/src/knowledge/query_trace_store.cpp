#include "query_trace_store.h"

#include "../persistence/sqlite_write_priority.h"

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>

#include <sqlite3.h>

namespace kchess::knowledge {
namespace {

struct StatementDeleter {
  void operator()(sqlite3_stmt* statement) const noexcept {
    if (statement != nullptr) sqlite3_finalize(statement);
  }
};
using Statement = std::unique_ptr<sqlite3_stmt, StatementDeleter>;

void check(const int code, sqlite3* db, const char* operation) {
  if (code == SQLITE_OK || code == SQLITE_DONE || code == SQLITE_ROW) return;
  throw std::runtime_error(std::string(operation) + ": " + sqlite3_errmsg(db));
}

Statement prepare(sqlite3* db, const char* sql) {
  sqlite3_stmt* raw = nullptr;
  check(sqlite3_prepare_v2(db, sql, -1, &raw, nullptr), db,
        "prepare knowledge query trace statement");
  return Statement(raw);
}

std::string text_column(sqlite3_stmt* statement, const int column) {
  const auto* text = sqlite3_column_text(statement, column);
  return text == nullptr ? std::string{} :
                           std::string(reinterpret_cast<const char*>(text));
}

void bind_text(sqlite3_stmt* statement, const int index,
               const std::string& value) {
  sqlite3_bind_text(statement, index, value.c_str(), -1, SQLITE_TRANSIENT);
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Lifecycle
// -----------------------------------------------------------------------------

QueryTraceStore::QueryTraceStore(std::filesystem::path data_directory)
    : database_path_(std::move(data_directory) / "kchess.sqlite3") {}

QueryTraceStore::~QueryTraceStore() { close(); }

void QueryTraceStore::open() {
  if (db_ != nullptr) return;
  sqlite3* opened = nullptr;
  const int result = sqlite3_open_v2(
      database_path_.string().c_str(), &opened,
      SQLITE_OPEN_READWRITE | SQLITE_OPEN_FULLMUTEX, nullptr);
  if (result != SQLITE_OK) {
    const std::string error = opened == nullptr ? "sqlite open failed" : sqlite3_errmsg(opened);
    if (opened != nullptr) sqlite3_close(opened);
    throw std::runtime_error("open knowledge query trace store: " + error);
  }
  db_ = opened;
  sqlite3_busy_timeout(db_, 5000);
  check(sqlite3_exec(db_, "PRAGMA foreign_keys=ON;", nullptr, nullptr, nullptr),
        db_, "enable query trace foreign keys");
}

void QueryTraceStore::close() noexcept {
  if (db_ == nullptr) return;
  sqlite3_close(db_);
  db_ = nullptr;
}

// -----------------------------------------------------------------------------
// Section: Trace persistence
// -----------------------------------------------------------------------------

void QueryTraceStore::upsert(const KnowledgeQueryTraceRecord& trace) {
  if (db_ == nullptr) throw std::logic_error("query trace store is not open");
  if (trace.id.empty() || trace.profile_id.empty() || trace.created_at_ms < 0 ||
      trace.trace_json.empty()) {
    throw std::invalid_argument("invalid knowledge query trace");
  }

  persistence::BackgroundSqliteWriteGuard priority_guard(std::try_to_lock);
  if (!priority_guard.owns_lock()) return;
  auto statement = prepare(
      db_,
      "INSERT INTO knowledge_query_traces("
      "id,profile_id,query_text,intent,route_confidence,answerable,"
      "answerability_confidence,seed_count,candidate_node_count,"
      "candidate_chunk_count,selected_node_count,selected_chunk_count,"
      "expanded_nodes,packet_tokens,created_at_ms,trace_json) "
      "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?) "
      "ON CONFLICT(id) DO UPDATE SET "
      "profile_id=excluded.profile_id,query_text=excluded.query_text,"
      "intent=excluded.intent,route_confidence=excluded.route_confidence,"
      "answerable=excluded.answerable,"
      "answerability_confidence=excluded.answerability_confidence,"
      "seed_count=excluded.seed_count,candidate_node_count=excluded.candidate_node_count,"
      "candidate_chunk_count=excluded.candidate_chunk_count,"
      "selected_node_count=excluded.selected_node_count,"
      "selected_chunk_count=excluded.selected_chunk_count,"
      "expanded_nodes=excluded.expanded_nodes,packet_tokens=excluded.packet_tokens,"
      "created_at_ms=excluded.created_at_ms,trace_json=excluded.trace_json;");
  bind_text(statement.get(), 1, trace.id);
  bind_text(statement.get(), 2, trace.profile_id);
  bind_text(statement.get(), 3, trace.query_text);
  bind_text(statement.get(), 4, trace.intent);
  sqlite3_bind_double(statement.get(), 5, std::clamp(trace.route_confidence, 0.0, 1.0));
  sqlite3_bind_int(statement.get(), 6, trace.answerable ? 1 : 0);
  sqlite3_bind_double(statement.get(), 7,
                      std::clamp(trace.answerability_confidence, 0.0, 1.0));
  sqlite3_bind_int64(statement.get(), 8, static_cast<sqlite3_int64>(trace.seed_count));
  sqlite3_bind_int64(statement.get(), 9,
                     static_cast<sqlite3_int64>(trace.candidate_node_count));
  sqlite3_bind_int64(statement.get(), 10,
                     static_cast<sqlite3_int64>(trace.candidate_chunk_count));
  sqlite3_bind_int64(statement.get(), 11,
                     static_cast<sqlite3_int64>(trace.selected_node_count));
  sqlite3_bind_int64(statement.get(), 12,
                     static_cast<sqlite3_int64>(trace.selected_chunk_count));
  sqlite3_bind_int64(statement.get(), 13,
                     static_cast<sqlite3_int64>(trace.expanded_nodes));
  sqlite3_bind_int64(statement.get(), 14,
                     static_cast<sqlite3_int64>(trace.packet_tokens));
  sqlite3_bind_int64(statement.get(), 15, trace.created_at_ms);
  bind_text(statement.get(), 16, trace.trace_json);
  check(sqlite3_step(statement.get()), db_, "upsert knowledge query trace");
}

std::vector<KnowledgeQueryTraceRecord> QueryTraceStore::recent(
    const std::string& profile_id, const std::size_t limit) const {
  if (db_ == nullptr || profile_id.empty() || limit == 0) return {};
  auto statement = prepare(
      db_,
      "SELECT id,profile_id,query_text,intent,route_confidence,answerable,"
      "answerability_confidence,seed_count,candidate_node_count,"
      "candidate_chunk_count,selected_node_count,selected_chunk_count,"
      "expanded_nodes,packet_tokens,created_at_ms,trace_json "
      "FROM knowledge_query_traces WHERE profile_id=? "
      "ORDER BY created_at_ms DESC,id DESC LIMIT ?;");
  bind_text(statement.get(), 1, profile_id);
  sqlite3_bind_int64(statement.get(), 2,
                     static_cast<sqlite3_int64>(std::min<std::size_t>(limit, 100)));

  std::vector<KnowledgeQueryTraceRecord> result;
  while (sqlite3_step(statement.get()) == SQLITE_ROW) {
    result.push_back(KnowledgeQueryTraceRecord{
        .id = text_column(statement.get(), 0),
        .profile_id = text_column(statement.get(), 1),
        .query_text = text_column(statement.get(), 2),
        .intent = text_column(statement.get(), 3),
        .route_confidence = sqlite3_column_double(statement.get(), 4),
        .answerable = sqlite3_column_int(statement.get(), 5) != 0,
        .answerability_confidence = sqlite3_column_double(statement.get(), 6),
        .seed_count = static_cast<std::size_t>(sqlite3_column_int64(statement.get(), 7)),
        .candidate_node_count = static_cast<std::size_t>(sqlite3_column_int64(statement.get(), 8)),
        .candidate_chunk_count = static_cast<std::size_t>(sqlite3_column_int64(statement.get(), 9)),
        .selected_node_count = static_cast<std::size_t>(sqlite3_column_int64(statement.get(), 10)),
        .selected_chunk_count = static_cast<std::size_t>(sqlite3_column_int64(statement.get(), 11)),
        .expanded_nodes = static_cast<std::size_t>(sqlite3_column_int64(statement.get(), 12)),
        .packet_tokens = static_cast<std::size_t>(sqlite3_column_int64(statement.get(), 13)),
        .created_at_ms = sqlite3_column_int64(statement.get(), 14),
        .trace_json = text_column(statement.get(), 15),
    });
  }
  return result;
}

}  // namespace kchess::knowledge
