#include "graph_store.h"

#include "../persistence/sqlite_write_priority.h"

#include <sqlite3.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <queue>
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

void step_done(sqlite3* db, sqlite3_stmt* statement, const char* operation) {
  if (sqlite3_step(statement) != SQLITE_DONE) {
    throw std::runtime_error(std::string(operation) + ": " + sqlite3_errmsg(db));
  }
}

void execute(sqlite3* db, const char* sql) {
  char* error = nullptr;
  if (sqlite3_exec(db, sql, nullptr, nullptr, &error) != SQLITE_OK) {
    const std::string message = error == nullptr ? sqlite3_errmsg(db) : error;
    sqlite3_free(error);
    throw std::runtime_error(message);
  }
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

void bind_text(sqlite3_stmt* statement, int index, const char* value) {
  sqlite3_bind_text(statement, index, value, -1, SQLITE_TRANSIENT);
}

void bind_text(sqlite3_stmt* statement, int index, const std::string& value) {
  sqlite3_bind_text(statement, index, value.c_str(), -1, SQLITE_TRANSIENT);
}

void bind_text(sqlite3_stmt* statement, int index, std::string_view value) {
  sqlite3_bind_text(statement, index, value.data(), static_cast<int>(value.size()), SQLITE_TRANSIENT);
}

std::string column_text(sqlite3_stmt* statement, int index) {
  const auto* value = sqlite3_column_text(statement, index);
  return value == nullptr ? std::string{} : reinterpret_cast<const char*>(value);
}

std::int64_t current_time_ms() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

bool same_node_state(const KnowledgeNode& lhs, const KnowledgeNode& rhs) {
  return lhs.id == rhs.id && lhs.kind == rhs.kind &&
         lhs.assertion_kind == rhs.assertion_kind &&
         lhs.schema_version == rhs.schema_version &&
         lhs.properties == rhs.properties;
}

bool same_edge_state(const KnowledgeEdge& lhs, const KnowledgeEdge& rhs) {
  return lhs.id == rhs.id && lhs.from == rhs.from && lhs.to == rhs.to &&
         lhs.kind == rhs.kind && lhs.schema_version == rhs.schema_version &&
         lhs.properties == rhs.properties;
}

void write_version_properties(sqlite3* db, const sqlite3_int64 version_id,
                              const KnowledgeProperties& properties) {
  auto insert = prepare(
      db,
      "INSERT INTO knowledge_version_properties("
      "version_id,property_key,value_type,bool_value,int_value,real_value,text_value) "
      "VALUES(?,?,?,?,?,?,?);");
  for (const auto& [key, value] : properties) {
    sqlite3_reset(insert.get());
    sqlite3_clear_bindings(insert.get());
    sqlite3_bind_int64(insert.get(), 1, version_id);
    bind_text(insert.get(), 2, key);
    if (const auto* bool_value = std::get_if<bool>(&value)) {
      bind_text(insert.get(), 3, "bool");
      sqlite3_bind_int(insert.get(), 4, *bool_value ? 1 : 0);
    } else if (const auto* int_value = std::get_if<std::int64_t>(&value)) {
      bind_text(insert.get(), 3, "int");
      sqlite3_bind_int64(insert.get(), 5, *int_value);
    } else if (const auto* real_value = std::get_if<double>(&value)) {
      bind_text(insert.get(), 3, "real");
      sqlite3_bind_double(insert.get(), 6, *real_value);
    } else if (const auto* text_value = std::get_if<std::string>(&value)) {
      bind_text(insert.get(), 3, "text");
      bind_text(insert.get(), 7, *text_value);
    }
    step_done(db, insert.get(), "insert knowledge version property");
  }
}

void copy_version_sources(sqlite3* db, const sqlite3_int64 version_id,
                          std::string_view entry_kind,
                          const std::string& entry_id) {
  auto statement = prepare(
      db,
      "INSERT INTO knowledge_version_sources("
      "version_id,source_type,source_id,source_version,first_seen_ms,last_seen_ms) "
      "SELECT ?,s.source_type,s.source_id,COALESCE(pv.source_version,s.source_version),p.first_seen_ms,p.last_seen_ms "
      "FROM knowledge_provenance p "
      "JOIN knowledge_sources s ON s.id=p.source_row_id "
      "LEFT JOIN knowledge_dependencies pv ON pv.entry_kind=p.entry_kind "
      " AND pv.entry_id=p.entry_id AND pv.source_row_id=p.source_row_id "
      "WHERE p.entry_kind=? AND p.entry_id=?;");
  sqlite3_bind_int64(statement.get(), 1, version_id);
  bind_text(statement.get(), 2, entry_kind);
  bind_text(statement.get(), 3, entry_id);
  step_done(db, statement.get(), "copy knowledge version sources");
}

sqlite3_int64 next_revision(sqlite3* db, std::string_view entry_kind,
                            const std::string& entry_id) {
  auto statement = prepare(
      db,
      "SELECT COALESCE(MAX(revision),0)+1 FROM knowledge_versions "
      "WHERE entry_kind=? AND entry_id=?;");
  bind_text(statement.get(), 1, entry_kind);
  bind_text(statement.get(), 2, entry_id);
  if (sqlite3_step(statement.get()) != SQLITE_ROW) {
    throw std::runtime_error("read next knowledge version revision failed");
  }
  return sqlite3_column_int64(statement.get(), 0);
}

sqlite3_int64 snapshot_node(sqlite3* db, const KnowledgeNode& node,
                            std::string_view change_kind) {
  auto statement = prepare(
      db,
      "INSERT INTO knowledge_versions("
      "entry_kind,entry_id,revision,change_kind,kind,assertion_kind,from_node_id,"
      "to_node_id,schema_version,recorded_at_ms) VALUES('node',?,?,?,?,?,NULL,NULL,?,?);");
  bind_text(statement.get(), 1, node.id.value);
  sqlite3_bind_int64(statement.get(), 2,
                     next_revision(db, "node", node.id.value));
  bind_text(statement.get(), 3, change_kind);
  bind_text(statement.get(), 4, to_string(node.kind));
  bind_text(statement.get(), 5, to_string(node.assertion_kind));
  sqlite3_bind_int(statement.get(), 6, static_cast<int>(node.schema_version));
  sqlite3_bind_int64(statement.get(), 7, current_time_ms());
  step_done(db, statement.get(), "snapshot knowledge node");
  const auto version_id = sqlite3_last_insert_rowid(db);
  write_version_properties(db, version_id, node.properties);
  copy_version_sources(db, version_id, "node", node.id.value);
  return version_id;
}

sqlite3_int64 snapshot_edge(sqlite3* db, const KnowledgeEdge& edge,
                            std::string_view change_kind) {
  auto statement = prepare(
      db,
      "INSERT INTO knowledge_versions("
      "entry_kind,entry_id,revision,change_kind,kind,assertion_kind,from_node_id,"
      "to_node_id,schema_version,recorded_at_ms) VALUES('edge',?,?,?,?,NULL,?,?,?,?);");
  bind_text(statement.get(), 1, edge.id.value);
  sqlite3_bind_int64(statement.get(), 2,
                     next_revision(db, "edge", edge.id.value));
  bind_text(statement.get(), 3, change_kind);
  bind_text(statement.get(), 4, to_string(edge.kind));
  bind_text(statement.get(), 5, edge.from.value);
  bind_text(statement.get(), 6, edge.to.value);
  sqlite3_bind_int(statement.get(), 7, static_cast<int>(edge.schema_version));
  sqlite3_bind_int64(statement.get(), 8, current_time_ms());
  step_done(db, statement.get(), "snapshot knowledge edge");
  const auto version_id = sqlite3_last_insert_rowid(db);
  write_version_properties(db, version_id, edge.properties);
  copy_version_sources(db, version_id, "edge", edge.id.value);
  return version_id;
}

void write_properties(sqlite3* db, const char* delete_sql, const char* insert_sql,
                      const std::string& owner_id,
                      const KnowledgeProperties& properties) {
  auto remove = prepare(db, delete_sql);
  bind_text(remove.get(), 1, owner_id);
  step_done(db, remove.get(), "clear graph properties");

  auto insert = prepare(db, insert_sql);
  for (const auto& [key, value] : properties) {
    sqlite3_reset(insert.get());
    sqlite3_clear_bindings(insert.get());
    bind_text(insert.get(), 1, owner_id);
    bind_text(insert.get(), 2, key);

    if (const auto* bool_value = std::get_if<bool>(&value)) {
      bind_text(insert.get(), 3, "bool");
      sqlite3_bind_int(insert.get(), 4, *bool_value ? 1 : 0);
    } else if (const auto* int_value = std::get_if<std::int64_t>(&value)) {
      bind_text(insert.get(), 3, "int");
      sqlite3_bind_int64(insert.get(), 5, *int_value);
    } else if (const auto* real_value = std::get_if<double>(&value)) {
      bind_text(insert.get(), 3, "real");
      sqlite3_bind_double(insert.get(), 6, *real_value);
    } else if (const auto* text_value = std::get_if<std::string>(&value)) {
      bind_text(insert.get(), 3, "text");
      bind_text(insert.get(), 7, *text_value);
    }
    step_done(db, insert.get(), "insert graph property");
  }
}

KnowledgeProperties read_properties(sqlite3* db, const char* sql,
                                    const std::string& owner_id) {
  auto statement = prepare(db, sql);
  bind_text(statement.get(), 1, owner_id);
  KnowledgeProperties properties;
  while (sqlite3_step(statement.get()) == SQLITE_ROW) {
    const auto key = column_text(statement.get(), 0);
    const auto type = column_text(statement.get(), 1);
    if (type == "bool") {
      properties.emplace(key, sqlite3_column_int(statement.get(), 2) != 0);
    } else if (type == "int") {
      properties.emplace(key,
                         static_cast<std::int64_t>(sqlite3_column_int64(statement.get(), 3)));
    } else if (type == "real") {
      properties.emplace(key, sqlite3_column_double(statement.get(), 4));
    } else if (type == "text") {
      properties.emplace(key, column_text(statement.get(), 5));
    }
  }
  return properties;
}

std::optional<KnowledgeNode> read_node(sqlite3* db, sqlite3_stmt* statement) {
  const auto kind = knowledge_node_kind_from_string(column_text(statement, 1));
  const auto assertion = knowledge_assertion_kind_from_string(column_text(statement, 2));
  if (!kind || !assertion) return std::nullopt;
  KnowledgeNode result;
  result.id = {column_text(statement, 0)};
  result.kind = *kind;
  result.assertion_kind = *assertion;
  result.schema_version = static_cast<std::uint32_t>(sqlite3_column_int(statement, 3));
  result.properties = read_properties(
      db,
      "SELECT property_key,value_type,bool_value,int_value,real_value,text_value "
      "FROM knowledge_node_properties WHERE node_id=? ORDER BY property_key;",
      result.id.value);
  return result;
}

std::optional<KnowledgeEdge> read_edge(sqlite3* db, sqlite3_stmt* statement) {
  const auto kind = knowledge_edge_kind_from_string(column_text(statement, 3));
  if (!kind) return std::nullopt;
  KnowledgeEdge result;
  result.id = {column_text(statement, 0)};
  result.from = {column_text(statement, 1)};
  result.to = {column_text(statement, 2)};
  result.kind = *kind;
  result.schema_version = static_cast<std::uint32_t>(sqlite3_column_int(statement, 4));
  result.properties = read_properties(
      db,
      "SELECT property_key,value_type,bool_value,int_value,real_value,text_value "
      "FROM knowledge_edge_properties WHERE edge_id=? ORDER BY property_key;",
      result.id.value);
  return result;
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Lifecycle
// -----------------------------------------------------------------------------

GraphStore::GraphStore(std::filesystem::path data_directory)
    : database_path_(std::move(data_directory) / "kchess.sqlite3") {}

GraphStore::~GraphStore() { close(); }

void GraphStore::open() {
  if (db_ != nullptr) return;
  if (sqlite3_open_v2(database_path_.string().c_str(), &db_,
                      SQLITE_OPEN_READWRITE | SQLITE_OPEN_FULLMUTEX, nullptr) != SQLITE_OK) {
    const std::string message = db_ == nullptr ? "open knowledge graph database failed"
                                               : sqlite3_errmsg(db_);
    close();
    throw std::runtime_error(message);
  }
  execute(db_, "PRAGMA foreign_keys = ON;");
  execute(db_, "PRAGMA busy_timeout = 5000;");
}

void GraphStore::close() noexcept {
  if (db_ != nullptr) {
    sqlite3_close_v2(db_);
    db_ = nullptr;
  }
}

// -----------------------------------------------------------------------------
// Section: Node persistence
// -----------------------------------------------------------------------------

bool GraphStore::upsert_node(const KnowledgeNode& node_value) {
  if (db_ == nullptr) throw std::runtime_error("GraphStore is not open");
  if (!is_valid_contract_node(node_value)) {
    throw std::invalid_argument("invalid knowledge graph node");
  }

  const auto previous = node(node_value.id);
  if (previous && same_node_state(*previous, node_value)) return false;

  Transaction transaction(db_);
  if (previous) snapshot_node(db_, *previous, "updated");
  auto statement = prepare(
      db_,
      "INSERT INTO knowledge_nodes(id,kind,assertion_kind,schema_version) VALUES(?,?,?,?) "
      "ON CONFLICT(id) DO UPDATE SET kind=excluded.kind,assertion_kind=excluded.assertion_kind,"
      "schema_version=excluded.schema_version;");
  bind_text(statement.get(), 1, node_value.id.value);
  bind_text(statement.get(), 2, to_string(node_value.kind));
  bind_text(statement.get(), 3, to_string(node_value.assertion_kind));
  sqlite3_bind_int(statement.get(), 4, static_cast<int>(node_value.schema_version));
  step_done(db_, statement.get(), "upsert graph node");

  write_properties(
      db_, "DELETE FROM knowledge_node_properties WHERE node_id=?;",
      "INSERT INTO knowledge_node_properties(node_id,property_key,value_type,bool_value,int_value,real_value,text_value) "
      "VALUES(?,?,?,?,?,?,?);",
      node_value.id.value, node_value.properties);
  transaction.commit();
  return true;
}

std::optional<KnowledgeNode> GraphStore::node(const KnowledgeNodeId& id) const {
  if (db_ == nullptr) throw std::runtime_error("GraphStore is not open");
  auto statement = prepare(
      db_, "SELECT id,kind,assertion_kind,schema_version FROM knowledge_nodes WHERE id=?;");
  bind_text(statement.get(), 1, id.value);
  if (sqlite3_step(statement.get()) != SQLITE_ROW) return std::nullopt;
  return read_node(db_, statement.get());
}

bool GraphStore::remove_node(const KnowledgeNodeId& id) {
  if (db_ == nullptr) throw std::runtime_error("GraphStore is not open");
  const auto previous = node(id);
  if (!previous) return false;

  Transaction transaction(db_);
  snapshot_node(db_, *previous, "deleted");

  // Deleting a node cascades its live edges. Preserve their compact graph state
  // first so temporal reasoning can still explain the former relationship.
  auto incident = prepare(
      db_,
      "SELECT id,from_node_id,to_node_id,kind,schema_version FROM knowledge_edges "
      "WHERE from_node_id=? OR to_node_id=? ORDER BY id;");
  bind_text(incident.get(), 1, id.value);
  bind_text(incident.get(), 2, id.value);
  while (sqlite3_step(incident.get()) == SQLITE_ROW) {
    if (const auto edge_value = read_edge(db_, incident.get())) {
      snapshot_edge(db_, *edge_value, "deleted");
    }
  }

  auto statement = prepare(db_, "DELETE FROM knowledge_nodes WHERE id=?;");
  bind_text(statement.get(), 1, id.value);
  step_done(db_, statement.get(), "remove graph node");
  const bool removed = sqlite3_changes(db_) > 0;
  transaction.commit();
  return removed;
}

// -----------------------------------------------------------------------------
// Section: Edge persistence
// -----------------------------------------------------------------------------

bool GraphStore::upsert_edge(const KnowledgeEdge& edge_value) {
  if (db_ == nullptr) throw std::runtime_error("GraphStore is not open");
  if (!is_valid_contract_edge(edge_value)) {
    throw std::invalid_argument("invalid knowledge graph edge");
  }

  const auto previous = edge(edge_value.id);
  if (previous && same_edge_state(*previous, edge_value)) return false;

  Transaction transaction(db_);
  if (previous) snapshot_edge(db_, *previous, "updated");
  auto statement = prepare(
      db_,
      "INSERT INTO knowledge_edges(id,from_node_id,to_node_id,kind,schema_version) VALUES(?,?,?,?,?) "
      "ON CONFLICT(id) DO UPDATE SET from_node_id=excluded.from_node_id,to_node_id=excluded.to_node_id,"
      "kind=excluded.kind,schema_version=excluded.schema_version;");
  bind_text(statement.get(), 1, edge_value.id.value);
  bind_text(statement.get(), 2, edge_value.from.value);
  bind_text(statement.get(), 3, edge_value.to.value);
  bind_text(statement.get(), 4, to_string(edge_value.kind));
  sqlite3_bind_int(statement.get(), 5, static_cast<int>(edge_value.schema_version));
  step_done(db_, statement.get(), "upsert graph edge");

  write_properties(
      db_, "DELETE FROM knowledge_edge_properties WHERE edge_id=?;",
      "INSERT INTO knowledge_edge_properties(edge_id,property_key,value_type,bool_value,int_value,real_value,text_value) "
      "VALUES(?,?,?,?,?,?,?);",
      edge_value.id.value, edge_value.properties);
  transaction.commit();
  return true;
}

std::optional<KnowledgeEdge> GraphStore::edge(const KnowledgeEdgeId& id) const {
  if (db_ == nullptr) throw std::runtime_error("GraphStore is not open");
  auto statement = prepare(
      db_,
      "SELECT id,from_node_id,to_node_id,kind,schema_version FROM knowledge_edges WHERE id=?;");
  bind_text(statement.get(), 1, id.value);
  if (sqlite3_step(statement.get()) != SQLITE_ROW) return std::nullopt;
  return read_edge(db_, statement.get());
}

bool GraphStore::remove_edge(const KnowledgeEdgeId& id) {
  if (db_ == nullptr) throw std::runtime_error("GraphStore is not open");
  const auto previous = edge(id);
  if (!previous) return false;

  Transaction transaction(db_);
  snapshot_edge(db_, *previous, "deleted");
  auto statement = prepare(db_, "DELETE FROM knowledge_edges WHERE id=?;");
  bind_text(statement.get(), 1, id.value);
  step_done(db_, statement.get(), "remove graph edge");
  const bool removed = sqlite3_changes(db_) > 0;
  transaction.commit();
  return removed;
}

// -----------------------------------------------------------------------------
// Section: Basic traversal
// -----------------------------------------------------------------------------

std::vector<KnowledgeTraversalStep> GraphStore::adjacent(
    const KnowledgeNodeId& id, GraphDirection direction,
    std::optional<KnowledgeEdgeKind> kind, std::size_t limit) const {
  if (db_ == nullptr) throw std::runtime_error("GraphStore is not open");
  if (id.empty() || limit == 0) return {};

  std::string predicate;
  switch (direction) {
    case GraphDirection::kOutgoing: predicate = "e.from_node_id=?"; break;
    case GraphDirection::kIncoming: predicate = "e.to_node_id=?"; break;
    case GraphDirection::kBoth:
      predicate = "(e.from_node_id=? OR e.to_node_id=?)";
      break;
  }
  std::string sql =
      "SELECT e.id,e.from_node_id,e.to_node_id,e.kind,e.schema_version,"
      "n.id,n.kind,n.assertion_kind,n.schema_version "
      "FROM knowledge_edges e JOIN knowledge_nodes n ON n.id=";
  if (direction == GraphDirection::kIncoming) {
    sql += "e.from_node_id WHERE ";
  } else if (direction == GraphDirection::kOutgoing) {
    sql += "e.to_node_id WHERE ";
  } else {
    sql += "CASE WHEN e.from_node_id=? THEN e.to_node_id ELSE e.from_node_id END WHERE ";
  }
  sql += predicate;
  if (kind) sql += " AND e.kind=?";
  sql += " ORDER BY e.id LIMIT ?;";

  auto statement = prepare(db_, sql.c_str());
  int bind_index = 1;
  if (direction == GraphDirection::kBoth) bind_text(statement.get(), bind_index++, id.value);
  bind_text(statement.get(), bind_index++, id.value);
  if (direction == GraphDirection::kBoth) bind_text(statement.get(), bind_index++, id.value);
  if (kind) bind_text(statement.get(), bind_index++, to_string(*kind));
  sqlite3_bind_int64(statement.get(), bind_index, static_cast<sqlite3_int64>(limit));

  std::vector<KnowledgeTraversalStep> result;
  while (sqlite3_step(statement.get()) == SQLITE_ROW) {
    auto edge_value = read_edge(db_, statement.get());
    if (!edge_value) continue;

    const auto node_kind = knowledge_node_kind_from_string(column_text(statement.get(), 6));
    const auto assertion = knowledge_assertion_kind_from_string(column_text(statement.get(), 7));
    if (!node_kind || !assertion) continue;
    KnowledgeNode adjacent_node;
    adjacent_node.id = {column_text(statement.get(), 5)};
    adjacent_node.kind = *node_kind;
    adjacent_node.assertion_kind = *assertion;
    adjacent_node.schema_version = static_cast<std::uint32_t>(sqlite3_column_int(statement.get(), 8));
    adjacent_node.properties = read_properties(
        db_,
        "SELECT property_key,value_type,bool_value,int_value,real_value,text_value "
        "FROM knowledge_node_properties WHERE node_id=? ORDER BY property_key;",
        adjacent_node.id.value);
    result.push_back({std::move(*edge_value), std::move(adjacent_node), 1});
  }
  return result;
}

std::vector<KnowledgeTraversalStep> GraphStore::traverse(
    const KnowledgeNodeId& seed, std::size_t max_depth, std::size_t max_nodes,
    GraphDirection direction) const {
  if (seed.empty() || max_depth == 0 || max_nodes == 0) return {};

  struct QueueItem {
    KnowledgeNodeId id;
    std::size_t depth;
  };
  std::queue<QueueItem> pending;
  std::unordered_set<std::string> visited;
  visited.insert(seed.value);
  pending.push({seed, 0});

  std::vector<KnowledgeTraversalStep> result;
  while (!pending.empty() && result.size() < max_nodes) {
    const auto current = std::move(pending.front());
    pending.pop();
    if (current.depth >= max_depth) continue;

    const auto remaining = max_nodes - result.size();
    for (auto step : adjacent(current.id, direction, std::nullopt, remaining)) {
      if (!visited.insert(step.node.id.value).second) continue;
      step.depth = current.depth + 1;
      pending.push({step.node.id, step.depth});
      result.push_back(std::move(step));
      if (result.size() >= max_nodes) break;
    }
  }
  return result;
}


// -----------------------------------------------------------------------------
// Section: Deterministic node lookup
// -----------------------------------------------------------------------------

std::vector<KnowledgeNode> GraphStore::find_nodes(
    const KnowledgeNodeLookup& lookup) const {
  if (db_ == nullptr) throw std::runtime_error("GraphStore is not open");
  if (lookup.limit == 0) return {};

  std::string sql =
      "SELECT n.id,n.kind,n.assertion_kind,n.schema_version "
      "FROM knowledge_nodes n WHERE 1=1";
  if (lookup.kind) sql += " AND n.kind=?";
  for (std::size_t index = 0; index < lookup.text_properties.size(); ++index) {
    sql +=
        " AND EXISTS(SELECT 1 FROM knowledge_node_properties p" +
        std::to_string(index) +
        " WHERE p" + std::to_string(index) + ".node_id=n.id AND p" +
        std::to_string(index) + ".property_key=? AND p" +
        std::to_string(index) + ".value_type='text' AND p" +
        std::to_string(index) + ".text_value=?";
    if (lookup.case_insensitive_text) sql += " COLLATE NOCASE";
    sql += ")";
  }
  sql += " ORDER BY n.id LIMIT ?;";

  auto statement = prepare(db_, sql.c_str());
  int bind_index = 1;
  if (lookup.kind) bind_text(statement.get(), bind_index++, to_string(*lookup.kind));
  for (const auto& [key, value] : lookup.text_properties) {
    bind_text(statement.get(), bind_index++, key);
    bind_text(statement.get(), bind_index++, value);
  }
  sqlite3_bind_int64(statement.get(), bind_index,
                     static_cast<sqlite3_int64>(lookup.limit));

  std::vector<KnowledgeNode> result;
  while (sqlite3_step(statement.get()) == SQLITE_ROW) {
    if (auto value = read_node(db_, statement.get())) {
      result.push_back(std::move(*value));
    }
  }
  return result;
}

}  // namespace kchess::knowledge
