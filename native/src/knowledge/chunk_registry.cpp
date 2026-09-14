#include "chunk_registry.h"

#include "../persistence/sqlite_write_priority.h"

#include <sqlite3.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
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

void bind_text(sqlite3_stmt* statement, int index, std::string_view value) {
  sqlite3_bind_text(statement, index, value.data(), static_cast<int>(value.size()),
                    SQLITE_TRANSIENT);
}

void bind_optional_text(sqlite3_stmt* statement, int index,
                        const std::optional<std::string>& value) {
  if (value) {
    bind_text(statement, index, *value);
  } else {
    sqlite3_bind_null(statement, index);
  }
}

void bind_optional_double(sqlite3_stmt* statement, int index,
                          const std::optional<double>& value) {
  if (value) {
    sqlite3_bind_double(statement, index, *value);
  } else {
    sqlite3_bind_null(statement, index);
  }
}

void bind_optional_int64(sqlite3_stmt* statement, int index,
                         const std::optional<std::int64_t>& value) {
  if (value) {
    sqlite3_bind_int64(statement, index, *value);
  } else {
    sqlite3_bind_null(statement, index);
  }
}

std::string column_text(sqlite3_stmt* statement, int index) {
  const auto* value = sqlite3_column_text(statement, index);
  return value == nullptr ? std::string{} : reinterpret_cast<const char*>(value);
}

std::optional<std::string> column_optional_text(sqlite3_stmt* statement, int index) {
  if (sqlite3_column_type(statement, index) == SQLITE_NULL) return std::nullopt;
  return column_text(statement, index);
}

std::optional<double> column_optional_double(sqlite3_stmt* statement, int index) {
  if (sqlite3_column_type(statement, index) == SQLITE_NULL) return std::nullopt;
  return sqlite3_column_double(statement, index);
}

std::optional<std::int64_t> column_optional_int64(sqlite3_stmt* statement, int index) {
  if (sqlite3_column_type(statement, index) == SQLITE_NULL) return std::nullopt;
  return sqlite3_column_int64(statement, index);
}

// FNV-1a is used only to create deterministic compact IDs, not for security.
// Each component is length-delimited so arbitrary source/topic text cannot create
// separator-based identity collisions.
std::uint64_t stable_hash(std::initializer_list<std::string_view> parts) noexcept {
  constexpr std::uint64_t kOffset = 14695981039346656037ULL;
  constexpr std::uint64_t kPrime = 1099511628211ULL;
  std::uint64_t hash = kOffset;
  for (const auto part : parts) {
    std::uint64_t size = static_cast<std::uint64_t>(part.size());
    for (int i = 0; i < 8; ++i) {
      hash ^= static_cast<unsigned char>((size >> (i * 8)) & 0xffU);
      hash *= kPrime;
    }
    for (const unsigned char byte : part) {
      hash ^= byte;
      hash *= kPrime;
    }
  }
  return hash;
}

std::string hex64(std::uint64_t value) {
  std::array<char, 16> buffer{};
  static constexpr char kHex[] = "0123456789abcdef";
  for (std::size_t index = buffer.size(); index-- > 0;) {
    buffer[index] = kHex[value & 0x0fU];
    value >>= 4U;
  }
  return std::string(buffer.data(), buffer.size());
}

std::optional<KnowledgeChunkMetadata> read_metadata(sqlite3_stmt* statement) {
  const auto granularity =
      knowledge_chunk_granularity_from_string(column_text(statement, 4));
  if (!granularity) return std::nullopt;

  KnowledgeChunkMetadata metadata;
  metadata.id = {column_text(statement, 0)};
  metadata.player_id = column_text(statement, 1);
  metadata.type = column_text(statement, 2);
  metadata.topic = column_text(statement, 3);
  metadata.granularity = *granularity;
  metadata.source_type = column_text(statement, 5);
  metadata.opening = column_optional_text(statement, 6);
  metadata.eco = column_optional_text(statement, 7);
  metadata.color = column_optional_text(statement, 8);
  metadata.result = column_optional_text(statement, 9);
  metadata.time_control = column_optional_text(statement, 10);
  metadata.date_range = column_optional_text(statement, 11);
  metadata.rating_range = column_optional_text(statement, 12);
  metadata.evidence_type = column_optional_text(statement, 13);
  metadata.confidence = column_optional_double(statement, 14);
  metadata.coverage = column_optional_double(statement, 15);
  metadata.freshness = column_optional_double(statement, 16);
  metadata.importance = column_optional_double(statement, 17);
  metadata.sample_size = column_optional_int64(statement, 18);
  metadata.source_quality = column_optional_double(statement, 19);
  metadata.source_version = column_text(statement, 20);
  metadata.embedding_id = column_optional_text(statement, 21);
  metadata.schema_version =
      static_cast<std::uint32_t>(sqlite3_column_int(statement, 22));
  return metadata;
}

constexpr const char* kSelectMetadataColumns =
    "id,player_id,type,topic,granularity,source_type,opening,eco,color,result,"
    "time_control,date_range,rating_range,evidence_type,confidence,coverage,"
    "freshness,importance,sample_size,source_quality,source_version,embedding_id,"
    "schema_version";

void require_graph_entry(sqlite3* db, const KnowledgeEntryRef& entry) {
  const char* sql = nullptr;
  switch (entry.kind) {
    case KnowledgeEntryKind::kNode:
      sql = "SELECT 1 FROM knowledge_nodes WHERE id=?;";
      break;
    case KnowledgeEntryKind::kEdge:
      sql = "SELECT 1 FROM knowledge_edges WHERE id=?;";
      break;
    case KnowledgeEntryKind::kChunk:
      throw std::invalid_argument("chunk-to-chunk graph link is not allowed");
  }
  auto statement = prepare(db, sql);
  bind_text(statement.get(), 1, entry.id);
  if (sqlite3_step(statement.get()) != SQLITE_ROW) {
    throw std::invalid_argument("chunk graph link target does not exist");
  }
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Chunk contract
// -----------------------------------------------------------------------------

KnowledgeChunkId make_knowledge_chunk_id(
    std::string_view player_id, std::string_view type, std::string_view topic,
    KnowledgeChunkGranularity granularity, std::string_view revision_key) {
  if (player_id.empty() || type.empty() || topic.empty()) return {};
  return {"kg:c:" +
          hex64(stable_hash({player_id, type, topic, to_string(granularity),
                             revision_key}))};
}

std::string_view to_string(KnowledgeChunkGranularity value) noexcept {
  switch (value) {
    case KnowledgeChunkGranularity::kSummary:
      return "summary";
    case KnowledgeChunkGranularity::kTopic:
      return "topic";
    case KnowledgeChunkGranularity::kEntity:
      return "entity";
    case KnowledgeChunkGranularity::kEvidence:
      return "evidence";
  }
  return "topic";
}

std::optional<KnowledgeChunkGranularity> knowledge_chunk_granularity_from_string(
    std::string_view value) noexcept {
  if (value == "summary") return KnowledgeChunkGranularity::kSummary;
  if (value == "topic") return KnowledgeChunkGranularity::kTopic;
  if (value == "entity") return KnowledgeChunkGranularity::kEntity;
  if (value == "evidence") return KnowledgeChunkGranularity::kEvidence;
  return std::nullopt;
}

std::string_view to_string(ChunkGraphLinkKind value) noexcept {
  switch (value) {
    case ChunkGraphLinkKind::kDescribes:
      return "describes";
    case ChunkGraphLinkKind::kEvidenceFor:
      return "evidence_for";
    case ChunkGraphLinkKind::kSummarizes:
      return "summarizes";
  }
  return "describes";
}

std::optional<ChunkGraphLinkKind> chunk_graph_link_kind_from_string(
    std::string_view value) noexcept {
  if (value == "describes") return ChunkGraphLinkKind::kDescribes;
  if (value == "evidence_for") return ChunkGraphLinkKind::kEvidenceFor;
  if (value == "summarizes") return ChunkGraphLinkKind::kSummarizes;
  return std::nullopt;
}

bool is_valid_chunk_metadata(const KnowledgeChunkMetadata& metadata) noexcept {
  if (metadata.id.empty() || metadata.player_id.empty() || metadata.type.empty() ||
      metadata.topic.empty() || metadata.source_type.empty() ||
      metadata.source_version.empty() || metadata.schema_version == 0) {
    return false;
  }
  const auto probability_like = [](const std::optional<double>& value) {
    return !value || (*value >= 0.0 && *value <= 1.0);
  };
  return probability_like(metadata.confidence) && probability_like(metadata.coverage) &&
         probability_like(metadata.freshness) && probability_like(metadata.importance) &&
         probability_like(metadata.source_quality) &&
         (!metadata.sample_size || *metadata.sample_size >= 0);
}

// -----------------------------------------------------------------------------
// Section: Lifecycle
// -----------------------------------------------------------------------------

ChunkRegistry::ChunkRegistry(std::filesystem::path data_directory)
    : database_path_(std::move(data_directory) / "kchess.sqlite3") {}

ChunkRegistry::~ChunkRegistry() { close(); }

void ChunkRegistry::open() {
  if (db_ != nullptr) return;
  if (sqlite3_open_v2(database_path_.string().c_str(), &db_,
                      SQLITE_OPEN_READWRITE | SQLITE_OPEN_FULLMUTEX, nullptr) !=
      SQLITE_OK) {
    const std::string message =
        db_ == nullptr ? "open chunk registry database failed" : sqlite3_errmsg(db_);
    close();
    throw std::runtime_error(message);
  }
  execute(db_, "PRAGMA foreign_keys = ON;");
  execute(db_, "PRAGMA busy_timeout = 5000;");
}

void ChunkRegistry::close() noexcept {
  if (db_ != nullptr) {
    sqlite3_close_v2(db_);
    db_ = nullptr;
  }
}

// -----------------------------------------------------------------------------
// Section: Chunk persistence
// -----------------------------------------------------------------------------

void ChunkRegistry::upsert_chunk(const KnowledgeChunk& value) {
  if (db_ == nullptr) throw std::runtime_error("ChunkRegistry is not open");
  if (!is_valid_chunk_metadata(value.metadata) || value.content.empty()) {
    throw std::invalid_argument("invalid knowledge chunk");
  }
  for (const auto& source : value.sources) {
    if (!source.valid()) throw std::invalid_argument("invalid knowledge chunk source");
  }

  Transaction transaction(db_);
  auto statement = prepare(
      db_,
      "INSERT INTO knowledge_chunks("
      "id,player_id,type,topic,granularity,source_type,opening,eco,color,result,"
      "time_control,date_range,rating_range,evidence_type,confidence,coverage,"
      "freshness,importance,sample_size,source_quality,source_version,embedding_id,"
      "schema_version,content) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?) "
      "ON CONFLICT(id) DO UPDATE SET player_id=excluded.player_id,type=excluded.type,"
      "topic=excluded.topic,granularity=excluded.granularity,source_type=excluded.source_type,"
      "opening=excluded.opening,eco=excluded.eco,color=excluded.color,result=excluded.result,"
      "time_control=excluded.time_control,date_range=excluded.date_range,"
      "rating_range=excluded.rating_range,evidence_type=excluded.evidence_type,"
      "confidence=excluded.confidence,coverage=excluded.coverage,freshness=excluded.freshness,"
      "importance=excluded.importance,sample_size=excluded.sample_size,"
      "source_quality=excluded.source_quality,source_version=excluded.source_version,"
      "embedding_id=excluded.embedding_id,schema_version=excluded.schema_version,"
      "content=excluded.content;");
  const auto& metadata = value.metadata;
  bind_text(statement.get(), 1, metadata.id.value);
  bind_text(statement.get(), 2, metadata.player_id);
  bind_text(statement.get(), 3, metadata.type);
  bind_text(statement.get(), 4, metadata.topic);
  bind_text(statement.get(), 5, to_string(metadata.granularity));
  bind_text(statement.get(), 6, metadata.source_type);
  bind_optional_text(statement.get(), 7, metadata.opening);
  bind_optional_text(statement.get(), 8, metadata.eco);
  bind_optional_text(statement.get(), 9, metadata.color);
  bind_optional_text(statement.get(), 10, metadata.result);
  bind_optional_text(statement.get(), 11, metadata.time_control);
  bind_optional_text(statement.get(), 12, metadata.date_range);
  bind_optional_text(statement.get(), 13, metadata.rating_range);
  bind_optional_text(statement.get(), 14, metadata.evidence_type);
  bind_optional_double(statement.get(), 15, metadata.confidence);
  bind_optional_double(statement.get(), 16, metadata.coverage);
  bind_optional_double(statement.get(), 17, metadata.freshness);
  bind_optional_double(statement.get(), 18, metadata.importance);
  bind_optional_int64(statement.get(), 19, metadata.sample_size);
  bind_optional_double(statement.get(), 20, metadata.source_quality);
  bind_text(statement.get(), 21, metadata.source_version);
  bind_optional_text(statement.get(), 22, metadata.embedding_id);
  sqlite3_bind_int(statement.get(), 23, static_cast<int>(metadata.schema_version));
  bind_text(statement.get(), 24, value.content);
  step_done(db_, statement.get(), "upsert knowledge chunk");

  auto clear_sources =
      prepare(db_, "DELETE FROM knowledge_chunk_sources WHERE chunk_id=?;");
  bind_text(clear_sources.get(), 1, metadata.id.value);
  step_done(db_, clear_sources.get(), "clear knowledge chunk sources");

  auto insert_source = prepare(
      db_,
      "INSERT INTO knowledge_chunk_sources(chunk_id,source_type,source_id,source_version) "
      "VALUES(?,?,?,?);");
  for (const auto& source : value.sources) {
    sqlite3_reset(insert_source.get());
    sqlite3_clear_bindings(insert_source.get());
    bind_text(insert_source.get(), 1, metadata.id.value);
    bind_text(insert_source.get(), 2, source.source_type);
    bind_text(insert_source.get(), 3, source.source_id);
    bind_text(insert_source.get(), 4, source.source_version);
    step_done(db_, insert_source.get(), "insert knowledge chunk source");
  }
  transaction.commit();
}

std::optional<KnowledgeChunk> ChunkRegistry::chunk(const KnowledgeChunkId& id) const {
  if (db_ == nullptr) throw std::runtime_error("ChunkRegistry is not open");
  auto statement = prepare(
      db_,
      (std::string("SELECT ") + kSelectMetadataColumns +
       ",content FROM knowledge_chunks WHERE id=?;")
          .c_str());
  bind_text(statement.get(), 1, id.value);
  if (sqlite3_step(statement.get()) != SQLITE_ROW) return std::nullopt;
  auto metadata = read_metadata(statement.get());
  if (!metadata) return std::nullopt;

  KnowledgeChunk result;
  result.metadata = std::move(*metadata);
  result.content = column_text(statement.get(), 23);

  auto sources = prepare(
      db_,
      "SELECT source_type,source_id,source_version FROM knowledge_chunk_sources "
      "WHERE chunk_id=? ORDER BY source_type,source_id;");
  bind_text(sources.get(), 1, id.value);
  while (sqlite3_step(sources.get()) == SQLITE_ROW) {
    result.sources.push_back(
        {column_text(sources.get(), 0), column_text(sources.get(), 1),
         column_text(sources.get(), 2)});
  }
  return result;
}

bool ChunkRegistry::remove_chunk(const KnowledgeChunkId& id) {
  if (db_ == nullptr) throw std::runtime_error("ChunkRegistry is not open");
  persistence::BackgroundSqliteWriteGuard priority_guard;
  auto statement = prepare(db_, "DELETE FROM knowledge_chunks WHERE id=?;");
  bind_text(statement.get(), 1, id.value);
  step_done(db_, statement.get(), "remove knowledge chunk");
  return sqlite3_changes(db_) > 0;
}

std::vector<KnowledgeChunkMetadata> ChunkRegistry::find(
    std::string_view player_id, std::optional<std::string_view> type,
    std::optional<KnowledgeChunkGranularity> granularity, std::size_t limit) const {
  if (db_ == nullptr) throw std::runtime_error("ChunkRegistry is not open");
  if (player_id.empty() || limit == 0) return {};

  std::string sql = std::string("SELECT ") + kSelectMetadataColumns +
                    " FROM knowledge_chunks WHERE player_id=?";
  if (type) sql += " AND type=?";
  if (granularity) sql += " AND granularity=?";
  sql += " ORDER BY importance DESC, freshness DESC, id LIMIT ?;";

  auto statement = prepare(db_, sql.c_str());
  int index = 1;
  bind_text(statement.get(), index++, player_id);
  if (type) bind_text(statement.get(), index++, *type);
  if (granularity) bind_text(statement.get(), index++, to_string(*granularity));
  sqlite3_bind_int64(statement.get(), index, static_cast<sqlite3_int64>(limit));

  std::vector<KnowledgeChunkMetadata> result;
  while (sqlite3_step(statement.get()) == SQLITE_ROW) {
    if (auto metadata = read_metadata(statement.get())) {
      result.push_back(std::move(*metadata));
    }
  }
  return result;
}

// -----------------------------------------------------------------------------
// Section: Retrieval lookups
// -----------------------------------------------------------------------------

std::vector<KnowledgeLexicalChunkHit> ChunkRegistry::lexical_search(
    std::string_view player_id, const std::vector<std::string>& terms,
    std::size_t limit) const {
  if (db_ == nullptr) throw std::runtime_error("ChunkRegistry is not open");
  if (player_id.empty() || terms.empty() || limit == 0) return {};

  std::vector<std::string> normalized;
  normalized.reserve(std::min<std::size_t>(terms.size(), 12));
  for (const auto& raw : terms) {
    std::string term;
    term.reserve(raw.size());
    for (const unsigned char c : raw) {
      term.push_back(c < 128 ? static_cast<char>(std::tolower(c))
                             : static_cast<char>(c));
    }
    if (term.size() < 2 || std::find(normalized.begin(), normalized.end(), term) != normalized.end()) {
      continue;
    }
    normalized.push_back(std::move(term));
    if (normalized.size() >= 12) break;
  }
  if (normalized.empty()) return {};

  std::string score_expression;
  for (std::size_t i = 0; i < normalized.size(); ++i) {
    if (!score_expression.empty()) score_expression += "+";
    score_expression +=
        "(CASE WHEN instr(lower(topic),?)>0 THEN 4.0 ELSE 0.0 END+"
        "CASE WHEN instr(lower(COALESCE(opening,'')),?)>0 THEN 4.0 ELSE 0.0 END+"
        "CASE WHEN instr(lower(COALESCE(eco,'')),?)>0 THEN 3.0 ELSE 0.0 END+"
        "CASE WHEN instr(lower(type),?)>0 THEN 2.0 ELSE 0.0 END+"
        "CASE WHEN instr(lower(content),?)>0 THEN 1.0 ELSE 0.0 END)";
  }

  const std::string sql =
      "SELECT id,lexical_score FROM (SELECT id,(" + score_expression +
      ") AS lexical_score,importance,freshness FROM knowledge_chunks WHERE player_id=?) "
      "WHERE lexical_score>0 ORDER BY lexical_score DESC,importance DESC,freshness DESC,id LIMIT ?;";
  auto statement = prepare(db_, sql.c_str());
  int bind_index = 1;
  for (const auto& term : normalized) {
    for (int field = 0; field < 5; ++field) bind_text(statement.get(), bind_index++, term);
  }
  bind_text(statement.get(), bind_index++, player_id);
  sqlite3_bind_int64(statement.get(), bind_index, static_cast<sqlite3_int64>(limit));

  struct RawHit {
    KnowledgeChunkId id;
    double score{0.0};
  };
  std::vector<RawHit> raw_hits;
  while (sqlite3_step(statement.get()) == SQLITE_ROW) {
    raw_hits.push_back({{column_text(statement.get(), 0)},
                        sqlite3_column_double(statement.get(), 1)});
  }

  std::vector<KnowledgeLexicalChunkHit> result;
  result.reserve(raw_hits.size());
  for (const auto& raw : raw_hits) {
    auto value = chunk(raw.id);
    if (!value) continue;
    std::string haystack = value->metadata.topic + " " + value->metadata.type + " " +
                           value->metadata.opening.value_or("") + " " +
                           value->metadata.eco.value_or("") + " " + value->content;
    std::transform(haystack.begin(), haystack.end(), haystack.begin(), [](unsigned char c) {
      return c < 128 ? static_cast<char>(std::tolower(c)) : static_cast<char>(c);
    });
    std::size_t matched = 0;
    for (const auto& term : normalized) {
      if (haystack.find(term) != std::string::npos) ++matched;
    }
    result.push_back({std::move(*value), raw.score, matched});
  }
  return result;
}

std::vector<KnowledgeChunk> ChunkRegistry::linked_chunks(
    std::string_view player_id, const std::vector<KnowledgeEntryRef>& entries,
    std::size_t limit) const {
  if (db_ == nullptr) throw std::runtime_error("ChunkRegistry is not open");
  if (player_id.empty() || entries.empty() || limit == 0) return {};

  std::unordered_set<std::string> seen;
  std::vector<KnowledgeChunk> result;
  result.reserve(std::min<std::size_t>(entries.size(), limit));
  auto statement = prepare(
      db_,
      "SELECT c.id FROM knowledge_chunk_graph_links l "
      "JOIN knowledge_chunks c ON c.id=l.chunk_id "
      "WHERE c.player_id=? AND l.entry_kind=? AND l.entry_id=? "
      "ORDER BY c.importance DESC,c.freshness DESC,c.id LIMIT ?;");

  for (const auto& entry : entries) {
    if (entry.empty() || result.size() >= limit) break;
    sqlite3_reset(statement.get());
    sqlite3_clear_bindings(statement.get());
    bind_text(statement.get(), 1, player_id);
    bind_text(statement.get(), 2, to_string(entry.kind));
    bind_text(statement.get(), 3, entry.id);
    sqlite3_bind_int64(statement.get(), 4,
                       static_cast<sqlite3_int64>(limit - result.size()));
    while (sqlite3_step(statement.get()) == SQLITE_ROW && result.size() < limit) {
      KnowledgeChunkId id{column_text(statement.get(), 0)};
      if (!seen.insert(id.value).second) continue;
      if (auto value = chunk(id)) result.push_back(std::move(*value));
    }
  }
  return result;
}

// -----------------------------------------------------------------------------
// Section: Graph routing references
// -----------------------------------------------------------------------------

void ChunkRegistry::replace_graph_links(const KnowledgeChunkId& chunk_id,
                                        const std::vector<ChunkGraphLink>& links) {
  if (db_ == nullptr) throw std::runtime_error("ChunkRegistry is not open");
  if (chunk_id.empty()) throw std::invalid_argument("empty knowledge chunk id");
  if (!chunk(chunk_id)) throw std::invalid_argument("knowledge chunk does not exist");

  for (const auto& link : links) {
    if (link.chunk_id != chunk_id || link.entry.empty()) {
      throw std::invalid_argument("invalid chunk graph link");
    }
    require_graph_entry(db_, link.entry);
  }

  Transaction transaction(db_);
  auto clear = prepare(db_, "DELETE FROM knowledge_chunk_graph_links WHERE chunk_id=?;");
  bind_text(clear.get(), 1, chunk_id.value);
  step_done(db_, clear.get(), "clear knowledge chunk graph links");

  auto insert = prepare(
      db_,
      "INSERT INTO knowledge_chunk_graph_links(chunk_id,entry_kind,entry_id,link_kind) "
      "VALUES(?,?,?,?);");
  for (const auto& link : links) {
    sqlite3_reset(insert.get());
    sqlite3_clear_bindings(insert.get());
    bind_text(insert.get(), 1, chunk_id.value);
    bind_text(insert.get(), 2, to_string(link.entry.kind));
    bind_text(insert.get(), 3, link.entry.id);
    bind_text(insert.get(), 4, to_string(link.kind));
    step_done(db_, insert.get(), "insert knowledge chunk graph link");
  }
  transaction.commit();
}

std::vector<ChunkGraphLink> ChunkRegistry::graph_links(
    const KnowledgeChunkId& chunk_id) const {
  if (db_ == nullptr) throw std::runtime_error("ChunkRegistry is not open");
  auto statement = prepare(
      db_,
      "SELECT entry_kind,entry_id,link_kind FROM knowledge_chunk_graph_links "
      "WHERE chunk_id=? ORDER BY entry_kind,entry_id,link_kind;");
  bind_text(statement.get(), 1, chunk_id.value);

  std::vector<ChunkGraphLink> result;
  while (sqlite3_step(statement.get()) == SQLITE_ROW) {
    const auto entry_kind = knowledge_entry_kind_from_string(column_text(statement.get(), 0));
    const auto link_kind = chunk_graph_link_kind_from_string(column_text(statement.get(), 2));
    if (!entry_kind || !link_kind) continue;
    result.push_back({chunk_id, {*entry_kind, column_text(statement.get(), 1)}, *link_kind});
  }
  return result;
}

}  // namespace kchess::knowledge
