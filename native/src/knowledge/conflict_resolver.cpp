#include "conflict_resolver.h"

#include "../persistence/sqlite_write_priority.h"

#include <sqlite3.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace kchess::knowledge {
namespace {

// -----------------------------------------------------------------------------
// Section: SQLite helpers
// -----------------------------------------------------------------------------

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

void bind_text(sqlite3_stmt* statement, int index, std::string_view value) {
  if (sqlite3_bind_text(statement, index, value.data(),
                        static_cast<int>(value.size()), SQLITE_TRANSIENT) !=
      SQLITE_OK) {
    throw std::runtime_error("bind conflict resolver text failed");
  }
}

void step_done(sqlite3* db, sqlite3_stmt* statement, const char* context) {
  if (sqlite3_step(statement) != SQLITE_DONE) {
    throw std::runtime_error(std::string(context) + ": " + sqlite3_errmsg(db));
  }
}

std::string column_text(sqlite3_stmt* statement, int index) {
  const auto* value = sqlite3_column_text(statement, index);
  return value == nullptr ? std::string{}
                          : std::string(reinterpret_cast<const char*>(value));
}

// -----------------------------------------------------------------------------
// Section: Stable conflict identities and properties
// -----------------------------------------------------------------------------

std::string stable_hash(std::string_view value) {
  std::uint64_t hash = 1469598103934665603ULL;
  for (const unsigned char byte : value) {
    hash ^= static_cast<std::uint64_t>(byte);
    hash *= 1099511628211ULL;
  }
  std::ostringstream out;
  out << std::hex << std::setfill('0') << std::setw(16) << hash;
  return out.str();
}

std::string conflict_id(std::string_view profile_id, std::string_view subject_key,
                        std::int64_t version_id,
                        std::string_view current_entry_id) {
  std::ostringstream canonical;
  canonical << profile_id << '\n' << subject_key << '\n' << version_id << '\n'
            << current_entry_id;
  return "kg:c:" + stable_hash(canonical.str());
}

std::optional<std::string> property_string(const KnowledgeProperties& properties,
                                           std::string_view key) {
  const auto found = properties.find(std::string(key));
  if (found == properties.end()) return std::nullopt;
  if (const auto* value = std::get_if<std::string>(&found->second)) return *value;
  return std::nullopt;
}

std::optional<double> property_double(const KnowledgeProperties& properties,
                                      std::string_view key) {
  const auto found = properties.find(std::string(key));
  if (found == properties.end()) return std::nullopt;
  if (const auto* value = std::get_if<double>(&found->second)) return *value;
  if (const auto* value = std::get_if<std::int64_t>(&found->second)) {
    return static_cast<double>(*value);
  }
  return std::nullopt;
}

bool property_bool(const KnowledgeProperties& properties, std::string_view key) {
  const auto found = properties.find(std::string(key));
  if (found == properties.end()) return false;
  if (const auto* value = std::get_if<bool>(&found->second)) return *value;
  return false;
}

double clamp01(double value) { return std::clamp(value, 0.0, 1.0); }

KnowledgeProperties version_properties(sqlite3* db, std::int64_t version_id) {
  auto statement = prepare(
      db,
      "SELECT property_key,value_type,bool_value,int_value,real_value,text_value "
      "FROM knowledge_version_properties WHERE version_id=? ORDER BY property_key;");
  sqlite3_bind_int64(statement.get(), 1, version_id);
  KnowledgeProperties properties;
  while (sqlite3_step(statement.get()) == SQLITE_ROW) {
    const auto key = column_text(statement.get(), 0);
    const auto type = column_text(statement.get(), 1);
    if (type == "bool") {
      properties.emplace(key, sqlite3_column_int(statement.get(), 2) != 0);
    } else if (type == "int") {
      properties.emplace(
          key, static_cast<std::int64_t>(sqlite3_column_int64(statement.get(), 3)));
    } else if (type == "real") {
      properties.emplace(key, sqlite3_column_double(statement.get(), 4));
    } else if (type == "text") {
      properties.emplace(key, column_text(statement.get(), 5));
    }
  }
  return properties;
}

struct HistoricalAssertion {
  std::int64_t version_id{0};
  std::string original_entry_id;
  KnowledgeNodeKind kind{KnowledgeNodeKind::kUnknown};
  KnowledgeAssertionKind assertion_kind{KnowledgeAssertionKind::kObservation};
  std::uint32_t schema_version{kKnowledgeGraphSchemaVersion};
  std::int64_t recorded_at_ms{0};
  KnowledgeProperties properties;
};

std::optional<HistoricalAssertion> latest_opposite_history(
    sqlite3* db, const std::string& profile_id, const std::string& pattern_id,
    KnowledgeNodeKind current_kind) {
  const auto opposite = current_kind == KnowledgeNodeKind::kStrength
                            ? KnowledgeNodeKind::kWeakness
                            : KnowledgeNodeKind::kStrength;
  auto statement = prepare(
      db,
      "SELECT DISTINCT v.id,v.entry_id,v.kind,v.assertion_kind,v.schema_version,"
      "v.recorded_at_ms FROM knowledge_versions v "
      "JOIN knowledge_version_properties pp ON pp.version_id=v.id "
      " AND pp.property_key='profile_id' AND pp.value_type='text' "
      "JOIN knowledge_version_properties pt ON pt.version_id=v.id "
      " AND pt.property_key='pattern_id' AND pt.value_type='text' "
      "WHERE v.entry_kind='node' AND v.kind=? AND pp.text_value=? "
      "AND pt.text_value=? ORDER BY v.recorded_at_ms DESC,v.id DESC LIMIT 1;");
  bind_text(statement.get(), 1, to_string(opposite));
  bind_text(statement.get(), 2, profile_id);
  bind_text(statement.get(), 3, pattern_id);
  if (sqlite3_step(statement.get()) != SQLITE_ROW) return std::nullopt;

  const auto kind = knowledge_node_kind_from_string(column_text(statement.get(), 2));
  const auto assertion =
      knowledge_assertion_kind_from_string(column_text(statement.get(), 3));
  if (!kind || !assertion) return std::nullopt;

  HistoricalAssertion result;
  result.version_id = sqlite3_column_int64(statement.get(), 0);
  result.original_entry_id = column_text(statement.get(), 1);
  result.kind = *kind;
  result.assertion_kind = *assertion;
  result.schema_version =
      static_cast<std::uint32_t>(sqlite3_column_int(statement.get(), 4));
  result.recorded_at_ms = sqlite3_column_int64(statement.get(), 5);
  result.properties = version_properties(db, result.version_id);
  return result;
}

std::vector<KnowledgeNodeId> current_profile_assertions(sqlite3* db,
                                                        const std::string& profile_id) {
  auto statement = prepare(
      db,
      "SELECT DISTINCT n.id FROM knowledge_nodes n "
      "JOIN knowledge_node_properties p ON p.node_id=n.id "
      "WHERE n.kind IN ('strength','weakness') AND p.property_key='profile_id' "
      "AND p.value_type='text' AND p.text_value=? ORDER BY n.id;");
  bind_text(statement.get(), 1, profile_id);
  std::vector<KnowledgeNodeId> result;
  while (sqlite3_step(statement.get()) == SQLITE_ROW) {
    result.push_back({column_text(statement.get(), 0)});
  }
  return result;
}

KnowledgeConflictResolution choose_resolution(
    const KnowledgeNode& current, double historical_confidence,
    double current_confidence) {
  const bool recent = property_bool(current.properties, "recent");
  const auto trend = property_string(current.properties, "trend").value_or("");
  if (historical_confidence < 0.35 || current_confidence < 0.35) {
    return KnowledgeConflictResolution::kInsufficientEvidence;
  }
  if ((recent || trend == "improving" || trend == "worsening") &&
      current_confidence + 0.05 >= historical_confidence) {
    return KnowledgeConflictResolution::kCurrentSupersedesHistorical;
  }
  if (current_confidence > historical_confidence + 0.10) {
    return KnowledgeConflictResolution::kCurrentSupersedesHistorical;
  }
  if (std::abs(current_confidence - historical_confidence) <= 0.10) {
    return KnowledgeConflictResolution::kBalanced;
  }
  return KnowledgeConflictResolution::kUnresolved;
}

KnowledgeTemporalChange temporal_change(KnowledgeNodeKind historical,
                                        KnowledgeNodeKind current) {
  if (historical == KnowledgeNodeKind::kWeakness &&
      current == KnowledgeNodeKind::kStrength) {
    return KnowledgeTemporalChange::kImproved;
  }
  if (historical == KnowledgeNodeKind::kStrength &&
      current == KnowledgeNodeKind::kWeakness) {
    return KnowledgeTemporalChange::kDeclined;
  }
  return KnowledgeTemporalChange::kNone;
}

KnowledgeConflictRecord read_conflict(sqlite3_stmt* statement) {
  KnowledgeConflictRecord record;
  record.id = column_text(statement, 0);
  record.profile_id = column_text(statement, 1);
  record.subject_key = column_text(statement, 2);
  record.historical_version_id = sqlite3_column_int64(statement, 3);
  record.historical_node_id = {column_text(statement, 4)};
  record.current_node_id = {column_text(statement, 5)};
  record.relation_edge_id = {column_text(statement, 6)};
  const auto resolution = column_text(statement, 7);
  if (resolution == "current_supersedes_historical") {
    record.resolution = KnowledgeConflictResolution::kCurrentSupersedesHistorical;
  } else if (resolution == "balanced") {
    record.resolution = KnowledgeConflictResolution::kBalanced;
  } else if (resolution == "insufficient_evidence") {
    record.resolution = KnowledgeConflictResolution::kInsufficientEvidence;
  }
  const auto change = column_text(statement, 8);
  if (change == "improved") {
    record.temporal_change = KnowledgeTemporalChange::kImproved;
  } else if (change == "declined") {
    record.temporal_change = KnowledgeTemporalChange::kDeclined;
  }
  record.historical_confidence = sqlite3_column_double(statement, 9);
  record.current_confidence = sqlite3_column_double(statement, 10);
  record.first_detected_ms = sqlite3_column_int64(statement, 11);
  record.last_evaluated_ms = sqlite3_column_int64(statement, 12);
  return record;
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Enum names
// -----------------------------------------------------------------------------

std::string_view to_string(KnowledgeConflictResolution value) noexcept {
  switch (value) {
    case KnowledgeConflictResolution::kCurrentSupersedesHistorical:
      return "current_supersedes_historical";
    case KnowledgeConflictResolution::kBalanced:
      return "balanced";
    case KnowledgeConflictResolution::kInsufficientEvidence:
      return "insufficient_evidence";
    case KnowledgeConflictResolution::kUnresolved:
      return "unresolved";
  }
  return "unresolved";
}

std::string_view to_string(KnowledgeTemporalChange value) noexcept {
  switch (value) {
    case KnowledgeTemporalChange::kImproved:
      return "improved";
    case KnowledgeTemporalChange::kDeclined:
      return "declined";
    case KnowledgeTemporalChange::kNone:
      return "none";
  }
  return "none";
}

// -----------------------------------------------------------------------------
// Section: Lifecycle
// -----------------------------------------------------------------------------

ConflictResolver::ConflictResolver(std::filesystem::path data_directory,
                                   GraphStore& graph,
                                   KnowledgeQualityStore& quality_store)
    : database_path_(std::move(data_directory) / "kchess.sqlite3"),
      graph_(graph),
      quality_store_(quality_store) {}

ConflictResolver::~ConflictResolver() { close(); }

void ConflictResolver::open() {
  if (db_ != nullptr) return;
  if (sqlite3_open_v2(database_path_.string().c_str(), &db_,
                      SQLITE_OPEN_READWRITE | SQLITE_OPEN_FULLMUTEX, nullptr) !=
      SQLITE_OK) {
    const std::string message =
        db_ == nullptr ? "open conflict resolver database failed" : sqlite3_errmsg(db_);
    close();
    throw std::runtime_error(message);
  }
  execute(db_, "PRAGMA foreign_keys = ON;");
  execute(db_, "PRAGMA busy_timeout = 5000;");
}

void ConflictResolver::close() noexcept {
  if (db_ != nullptr) {
    sqlite3_close_v2(db_);
    db_ = nullptr;
  }
}

// -----------------------------------------------------------------------------
// Section: Profile conflict refresh
// -----------------------------------------------------------------------------

std::vector<KnowledgeConflictRecord> ConflictResolver::refresh_profile_conflicts(
    const std::string& profile_id, const std::int64_t evaluated_at_ms) {
  if (db_ == nullptr) throw std::runtime_error("ConflictResolver is not open");
  if (profile_id.empty() || evaluated_at_ms < 0) return {};

  std::vector<KnowledgeConflictRecord> result;
  for (const auto& node_id : current_profile_assertions(db_, profile_id)) {
    const auto current = graph_.node(node_id);
    if (!current || (current->kind != KnowledgeNodeKind::kStrength &&
                     current->kind != KnowledgeNodeKind::kWeakness)) {
      continue;
    }
    const auto pattern_id = property_string(current->properties, "pattern_id");
    if (!pattern_id || pattern_id->empty()) continue;

    const auto historical =
        latest_opposite_history(db_, profile_id, *pattern_id, current->kind);
    if (!historical) continue;

    const double historical_confidence = clamp01(
        property_double(historical->properties, "confidence").value_or(0.50));
    double current_confidence =
        clamp01(property_double(current->properties, "confidence").value_or(0.50));
    if (quality_store_.is_open()) {
      if (const auto quality = quality_store_.quality(
              {KnowledgeEntryKind::kNode, current->id.value})) {
        current_confidence = quality->confidence;
      }
    }

    const auto resolution =
        choose_resolution(*current, historical_confidence, current_confidence);
    const auto change = temporal_change(historical->kind, current->kind);
    const std::string subject_key = "pattern:" + *pattern_id;
    const std::string id = conflict_id(profile_id, subject_key,
                                       historical->version_id, current->id.value);

    KnowledgeNode historical_node;
    historical_node.kind = historical->kind;
    historical_node.assertion_kind = historical->assertion_kind;
    historical_node.schema_version = historical->schema_version;
    historical_node.id = make_knowledge_node_id(
        to_string(historical->kind),
        "history:" + profile_id + ":" + *pattern_id + ":" +
            std::to_string(historical->version_id));
    historical_node.properties = historical->properties;
    historical_node.properties["historical"] = true;
    historical_node.properties["active"] = false;
    historical_node.properties["historical_version_id"] = historical->version_id;
    historical_node.properties["original_entry_id"] = historical->original_entry_id;
    historical_node.properties["recorded_at_ms"] = historical->recorded_at_ms;
    historical_node.properties["conflict_id"] = id;
    historical_node.properties["projection"] = std::string("conflict_resolver");
    graph_.upsert_node(historical_node);

    KnowledgeEdge contradiction;
    contradiction.from = historical_node.id;
    contradiction.to = current->id;
    contradiction.kind = KnowledgeEdgeKind::kContradictedBy;
    contradiction.id = make_knowledge_edge_id(
        contradiction.from, to_string(contradiction.kind), contradiction.to,
        "conflict:" + id);
    contradiction.properties = {
        {"conflict_id", id},
        {"profile_id", profile_id},
        {"subject_key", subject_key},
        {"historical_version_id", historical->version_id},
        {"resolution", std::string(to_string(resolution))},
        {"temporal_change", std::string(to_string(change))},
        {"historical_confidence", historical_confidence},
        {"current_confidence", current_confidence},
        {"projection", std::string("conflict_resolver")},
    };
    graph_.upsert_edge(contradiction);

    if (change != KnowledgeTemporalChange::kNone) {
      KnowledgeNode player;
      player.kind = KnowledgeNodeKind::kPlayer;
      player.assertion_kind = KnowledgeAssertionKind::kFact;
      player.id = make_knowledge_node_id("player", "profile:" + profile_id);
      player.properties = {{"profile_id", profile_id}};
      graph_.upsert_node(player);

      KnowledgeEdge trend_edge;
      trend_edge.from = player.id;
      trend_edge.to = current->id;
      trend_edge.kind = change == KnowledgeTemporalChange::kImproved
                            ? KnowledgeEdgeKind::kImprovingIn
                            : KnowledgeEdgeKind::kDecliningIn;
      trend_edge.id = make_knowledge_edge_id(
          trend_edge.from, to_string(trend_edge.kind), trend_edge.to,
          "temporal_conflict:" + id);
      trend_edge.properties = {
          {"conflict_id", id},
          {"historical_version_id", historical->version_id},
          {"resolution", std::string(to_string(resolution))},
          {"projection", std::string("conflict_resolver")},
      };
      graph_.upsert_edge(trend_edge);
    }

    persistence::BackgroundSqliteWriteGuard priority_guard;
    auto upsert = prepare(
        db_,
        "INSERT INTO knowledge_conflicts("
        "id,profile_id,subject_key,historical_version_id,historical_node_id,"
        "current_entry_id,relation_edge_id,resolution,temporal_change,"
        "historical_confidence,current_confidence,first_detected_ms,last_evaluated_ms) "
        "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?) ON CONFLICT(id) DO UPDATE SET "
        "historical_node_id=excluded.historical_node_id,"
        "current_entry_id=excluded.current_entry_id,"
        "relation_edge_id=excluded.relation_edge_id,resolution=excluded.resolution,"
        "temporal_change=excluded.temporal_change,"
        "historical_confidence=excluded.historical_confidence,"
        "current_confidence=excluded.current_confidence,"
        "last_evaluated_ms=excluded.last_evaluated_ms;");
    bind_text(upsert.get(), 1, id);
    bind_text(upsert.get(), 2, profile_id);
    bind_text(upsert.get(), 3, subject_key);
    sqlite3_bind_int64(upsert.get(), 4, historical->version_id);
    bind_text(upsert.get(), 5, historical_node.id.value);
    bind_text(upsert.get(), 6, current->id.value);
    bind_text(upsert.get(), 7, contradiction.id.value);
    bind_text(upsert.get(), 8, to_string(resolution));
    bind_text(upsert.get(), 9, to_string(change));
    sqlite3_bind_double(upsert.get(), 10, historical_confidence);
    sqlite3_bind_double(upsert.get(), 11, current_confidence);
    sqlite3_bind_int64(upsert.get(), 12, evaluated_at_ms);
    sqlite3_bind_int64(upsert.get(), 13, evaluated_at_ms);
    step_done(db_, upsert.get(), "upsert knowledge conflict");

    result.push_back({
        .id = id,
        .profile_id = profile_id,
        .subject_key = subject_key,
        .historical_version_id = historical->version_id,
        .historical_node_id = historical_node.id,
        .current_node_id = current->id,
        .relation_edge_id = contradiction.id,
        .resolution = resolution,
        .temporal_change = change,
        .historical_confidence = historical_confidence,
        .current_confidence = current_confidence,
        .first_detected_ms = evaluated_at_ms,
        .last_evaluated_ms = evaluated_at_ms,
    });
  }
  return result;
}

std::vector<KnowledgeConflictRecord> ConflictResolver::conflicts_for_profile(
    const std::string& profile_id, const std::size_t limit) const {
  if (db_ == nullptr) throw std::runtime_error("ConflictResolver is not open");
  if (profile_id.empty() || limit == 0) return {};
  auto statement = prepare(
      db_,
      "SELECT id,profile_id,subject_key,historical_version_id,historical_node_id,"
      "current_entry_id,relation_edge_id,resolution,temporal_change,"
      "historical_confidence,current_confidence,first_detected_ms,last_evaluated_ms "
      "FROM knowledge_conflicts WHERE profile_id=? "
      "ORDER BY last_evaluated_ms DESC,id LIMIT ?;");
  bind_text(statement.get(), 1, profile_id);
  sqlite3_bind_int64(statement.get(), 2, static_cast<sqlite3_int64>(limit));
  std::vector<KnowledgeConflictRecord> result;
  while (sqlite3_step(statement.get()) == SQLITE_ROW) {
    result.push_back(read_conflict(statement.get()));
  }
  return result;
}

}  // namespace kchess::knowledge
