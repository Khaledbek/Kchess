#include "vector_index.h"

#include "../persistence/sqlite_write_priority.h"

#include <sqlite3.h>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <initializer_list>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
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
    throw std::runtime_error("bind vector index text failed");
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

void append_u32_le(std::vector<std::byte>& output, const std::uint32_t value) {
  output.push_back(static_cast<std::byte>(value & 0xffU));
  output.push_back(static_cast<std::byte>((value >> 8U) & 0xffU));
  output.push_back(static_cast<std::byte>((value >> 16U) & 0xffU));
  output.push_back(static_cast<std::byte>((value >> 24U) & 0xffU));
}

std::vector<std::byte> encode_vector(const std::vector<float>& values) {
  static_assert(sizeof(float) == sizeof(std::uint32_t));
  std::vector<std::byte> output;
  output.reserve(values.size() * sizeof(float));
  for (const float value : values) {
    append_u32_le(output, std::bit_cast<std::uint32_t>(value));
  }
  return output;
}

std::optional<std::vector<float>> decode_vector(const void* data,
                                                 const int bytes,
                                                 const std::size_t dimensions) {
  if (data == nullptr || bytes < 0 ||
      static_cast<std::size_t>(bytes) != dimensions * sizeof(float)) {
    return std::nullopt;
  }
  const auto* raw = static_cast<const unsigned char*>(data);
  std::vector<float> result;
  result.reserve(dimensions);
  for (std::size_t index = 0; index < dimensions; ++index) {
    const auto offset = index * 4U;
    const std::uint32_t bits = static_cast<std::uint32_t>(raw[offset]) |
                               (static_cast<std::uint32_t>(raw[offset + 1]) << 8U) |
                               (static_cast<std::uint32_t>(raw[offset + 2]) << 16U) |
                               (static_cast<std::uint32_t>(raw[offset + 3]) << 24U);
    const float value = std::bit_cast<float>(bits);
    if (!std::isfinite(value)) return std::nullopt;
    result.push_back(value);
  }
  return result;
}

float cosine_similarity(const std::vector<float>& lhs,
                        const std::vector<float>& rhs) {
  if (lhs.size() != rhs.size() || lhs.empty()) return -1.0F;
  double dot = 0.0;
  double lhs_norm = 0.0;
  double rhs_norm = 0.0;
  for (std::size_t i = 0; i < lhs.size(); ++i) {
    dot += static_cast<double>(lhs[i]) * rhs[i];
    lhs_norm += static_cast<double>(lhs[i]) * lhs[i];
    rhs_norm += static_cast<double>(rhs[i]) * rhs[i];
  }
  if (lhs_norm <= std::numeric_limits<double>::epsilon() ||
      rhs_norm <= std::numeric_limits<double>::epsilon()) {
    return -1.0F;
  }
  return static_cast<float>(dot / std::sqrt(lhs_norm * rhs_norm));
}

std::optional<KnowledgeEmbeddingMetadata> read_metadata(sqlite3_stmt* statement,
                                                         int offset = 0) {
  const auto owner_kind =
      embedding_owner_kind_from_string(column_text(statement, offset + 1));
  const auto vector_space =
      knowledge_vector_space_from_string(column_text(statement, offset + 4));
  if (!owner_kind || !vector_space) return std::nullopt;

  KnowledgeEmbeddingMetadata result;
  result.embedding_id = column_text(statement, offset);
  result.owner_kind = *owner_kind;
  result.owner_id = column_text(statement, offset + 2);
  result.player_id = column_text(statement, offset + 3);
  result.vector_space = *vector_space;
  result.model_id = column_text(statement, offset + 5);
  result.model_version = column_text(statement, offset + 6);
  result.dimensions = static_cast<std::uint32_t>(
      sqlite3_column_int64(statement, offset + 7));
  result.source_version = column_text(statement, offset + 8);
  result.created_at_ms = sqlite3_column_int64(statement, offset + 9);
  result.updated_at_ms = sqlite3_column_int64(statement, offset + 10);
  if (!is_valid_embedding_metadata(result)) return std::nullopt;
  return result;
}

std::uint64_t stable_hash(std::initializer_list<std::string_view> parts) {
  constexpr std::uint64_t kOffset = 14695981039346656037ULL;
  constexpr std::uint64_t kPrime = 1099511628211ULL;
  std::uint64_t hash = kOffset;
  for (const auto part : parts) {
    std::uint64_t size = static_cast<std::uint64_t>(part.size());
    for (int i = 0; i < 8; ++i) {
      hash ^= static_cast<unsigned char>((size >> (i * 8)) & 0xffU);
      hash *= kPrime;
    }
    for (const unsigned char ch : part) {
      hash ^= ch;
      hash *= kPrime;
    }
  }
  return hash;
}

std::string hex64(const std::uint64_t value) {
  std::ostringstream output;
  output << std::hex << std::setfill('0') << std::setw(16) << value;
  return output.str();
}

constexpr const char* kMetadataColumns =
    "m.id,m.owner_kind,m.owner_id,m.player_id,m.vector_space,m.model_id,"
    "m.model_version,m.dimensions,m.source_version,m.created_at_ms,m.updated_at_ms";

}  // namespace

std::string_view to_string(const KnowledgeVectorSpace value) noexcept {
  switch (value) {
    case KnowledgeVectorSpace::kTextSemantic:
      return "text_semantic";
    case KnowledgeVectorSpace::kChessPosition:
      return "chess_position";
  }
  return "text_semantic";
}

std::optional<KnowledgeVectorSpace> knowledge_vector_space_from_string(
    const std::string_view value) noexcept {
  if (value == "text_semantic") return KnowledgeVectorSpace::kTextSemantic;
  if (value == "chess_position") return KnowledgeVectorSpace::kChessPosition;
  return std::nullopt;
}

std::string_view to_string(const EmbeddingOwnerKind value) noexcept {
  switch (value) {
    case EmbeddingOwnerKind::kChunk:
      return "chunk";
    case EmbeddingOwnerKind::kNode:
      return "node";
  }
  return "chunk";
}

std::optional<EmbeddingOwnerKind> embedding_owner_kind_from_string(
    const std::string_view value) noexcept {
  if (value == "chunk") return EmbeddingOwnerKind::kChunk;
  if (value == "node") return EmbeddingOwnerKind::kNode;
  return std::nullopt;
}

std::string make_knowledge_embedding_id(
    const EmbeddingOwnerKind owner_kind, const std::string_view owner_id,
    const KnowledgeVectorSpace vector_space, const std::string_view model_id,
    const std::string_view model_version) {
  if (owner_id.empty() || model_id.empty() || model_version.empty()) return {};
  const auto owner = to_string(owner_kind);
  const auto space = to_string(vector_space);
  return "kg:v:" + std::string(space) + ":" +
         hex64(stable_hash({owner, owner_id, space, model_id, model_version}));
}

bool is_valid_embedding_metadata(
    const KnowledgeEmbeddingMetadata& metadata) noexcept {
  return !metadata.embedding_id.empty() && !metadata.owner_id.empty() &&
         !metadata.model_id.empty() && !metadata.model_version.empty() &&
         metadata.dimensions > 0 && !metadata.source_version.empty() &&
         metadata.created_at_ms >= 0 && metadata.updated_at_ms >= 0 &&
         metadata.updated_at_ms >= metadata.created_at_ms;
}

bool is_valid_vector_record(const KnowledgeVectorRecord& record) noexcept {
  if (!is_valid_embedding_metadata(record.metadata) ||
      record.values.size() != record.metadata.dimensions ||
      record.values.empty()) {
    return false;
  }
  for (const float value : record.values) {
    if (!std::isfinite(value)) return false;
  }
  return true;
}

SqliteVectorIndex::SqliteVectorIndex(std::filesystem::path data_directory)
    : database_path_(std::move(data_directory) / "kchess.sqlite3") {}

SqliteVectorIndex::~SqliteVectorIndex() { close(); }

void SqliteVectorIndex::open() {
  if (db_ != nullptr) return;
  if (sqlite3_open_v2(database_path_.string().c_str(), &db_,
                      SQLITE_OPEN_READWRITE | SQLITE_OPEN_FULLMUTEX, nullptr) !=
      SQLITE_OK) {
    const std::string message = db_ == nullptr ? "open vector index database failed"
                                                : sqlite3_errmsg(db_);
    close();
    throw std::runtime_error(message);
  }
  execute(db_, "PRAGMA foreign_keys = ON;");
  execute(db_, "PRAGMA busy_timeout = 5000;");
}

void SqliteVectorIndex::close() noexcept {
  if (db_ != nullptr) {
    sqlite3_close_v2(db_);
    db_ = nullptr;
  }
}

void SqliteVectorIndex::upsert(const KnowledgeVectorRecord& record) {
  if (db_ == nullptr) throw std::runtime_error("SqliteVectorIndex is not open");
  if (!is_valid_vector_record(record)) {
    throw std::invalid_argument("invalid knowledge vector record");
  }

  persistence::BackgroundSqliteWriteGuard priority_guard;
  execute(db_, "BEGIN IMMEDIATE;");
  try {
    auto owner = prepare(
        db_, record.metadata.owner_kind == EmbeddingOwnerKind::kChunk
                 ? "SELECT 1 FROM knowledge_chunks WHERE id=?;"
                 : "SELECT 1 FROM knowledge_nodes WHERE id=?;");
    bind_text(owner.get(), 1, record.metadata.owner_id);
    if (sqlite3_step(owner.get()) != SQLITE_ROW) {
      throw std::invalid_argument("knowledge vector owner does not exist");
    }

    auto metadata_statement = prepare(
        db_,
        "INSERT INTO knowledge_embeddings_metadata("
        "id,owner_kind,owner_id,player_id,vector_space,model_id,model_version,"
        "dimensions,source_version,created_at_ms,updated_at_ms) "
        "VALUES(?,?,?,?,?,?,?,?,?,?,?) ON CONFLICT(id) DO UPDATE SET "
        "owner_kind=excluded.owner_kind,owner_id=excluded.owner_id,"
        "player_id=excluded.player_id,vector_space=excluded.vector_space,"
        "model_id=excluded.model_id,model_version=excluded.model_version,"
        "dimensions=excluded.dimensions,source_version=excluded.source_version,"
        "updated_at_ms=excluded.updated_at_ms;");
    bind_text(metadata_statement.get(), 1, record.metadata.embedding_id);
    bind_text(metadata_statement.get(), 2, to_string(record.metadata.owner_kind));
    bind_text(metadata_statement.get(), 3, record.metadata.owner_id);
    bind_text(metadata_statement.get(), 4, record.metadata.player_id);
    bind_text(metadata_statement.get(), 5, to_string(record.metadata.vector_space));
    bind_text(metadata_statement.get(), 6, record.metadata.model_id);
    bind_text(metadata_statement.get(), 7, record.metadata.model_version);
    sqlite3_bind_int64(metadata_statement.get(), 8,
                       static_cast<sqlite3_int64>(record.metadata.dimensions));
    bind_text(metadata_statement.get(), 9, record.metadata.source_version);
    sqlite3_bind_int64(metadata_statement.get(), 10, record.metadata.created_at_ms);
    sqlite3_bind_int64(metadata_statement.get(), 11, record.metadata.updated_at_ms);
    step_done(db_, metadata_statement.get(), "upsert embedding metadata");

    const auto bytes = encode_vector(record.values);
    auto vector_statement = prepare(
        db_,
        "INSERT INTO knowledge_embedding_vectors(embedding_id,encoding,vector_blob) "
        "VALUES(?,'float32_le',?) ON CONFLICT(embedding_id) DO UPDATE SET "
        "encoding=excluded.encoding,vector_blob=excluded.vector_blob;");
    bind_text(vector_statement.get(), 1, record.metadata.embedding_id);
    if (sqlite3_bind_blob(vector_statement.get(), 2, bytes.data(),
                          static_cast<int>(bytes.size()), SQLITE_TRANSIENT) != SQLITE_OK) {
      throw std::runtime_error("bind embedding vector failed");
    }
    step_done(db_, vector_statement.get(), "upsert embedding vector");

    if (record.metadata.owner_kind == EmbeddingOwnerKind::kChunk) {
      auto link_statement = prepare(
          db_, "UPDATE knowledge_chunks SET embedding_id=? WHERE id=?;");
      bind_text(link_statement.get(), 1, record.metadata.embedding_id);
      bind_text(link_statement.get(), 2, record.metadata.owner_id);
      step_done(db_, link_statement.get(), "link chunk embedding");
    }
    execute(db_, "COMMIT;");
  } catch (...) {
    sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
    throw;
  }
}

std::optional<KnowledgeEmbeddingMetadata> SqliteVectorIndex::metadata(
    const std::string_view embedding_id) const {
  if (db_ == nullptr) throw std::runtime_error("SqliteVectorIndex is not open");
  if (embedding_id.empty()) return std::nullopt;
  const std::string sql = std::string("SELECT ") + kMetadataColumns +
                          " FROM knowledge_embeddings_metadata m WHERE m.id=?;";
  auto statement = prepare(db_, sql.c_str());
  bind_text(statement.get(), 1, embedding_id);
  if (sqlite3_step(statement.get()) != SQLITE_ROW) return std::nullopt;
  return read_metadata(statement.get());
}

bool SqliteVectorIndex::remove(const std::string_view embedding_id) {
  if (db_ == nullptr) throw std::runtime_error("SqliteVectorIndex is not open");
  if (embedding_id.empty()) return false;
  persistence::BackgroundSqliteWriteGuard priority_guard;
  auto statement = prepare(db_, "DELETE FROM knowledge_embeddings_metadata WHERE id=?;");
  bind_text(statement.get(), 1, embedding_id);
  step_done(db_, statement.get(), "remove embedding");
  return sqlite3_changes(db_) > 0;
}

std::vector<KnowledgeVectorMatch> SqliteVectorIndex::search(
    const KnowledgeVectorQuery& query) const {
  if (db_ == nullptr) throw std::runtime_error("SqliteVectorIndex is not open");
  if (query.limit == 0 || query.values.empty() || query.model_id.empty() ||
      query.model_version.empty()) {
    return {};
  }
  for (const float value : query.values) {
    if (!std::isfinite(value)) return {};
  }

  const std::string sql = std::string("SELECT ") + kMetadataColumns +
      ",v.vector_blob FROM knowledge_embeddings_metadata m "
      "JOIN knowledge_embedding_vectors v ON v.embedding_id=m.id "
      "WHERE m.vector_space=? AND m.model_id=? AND m.model_version=? "
      "AND m.dimensions=? AND (m.player_id=? OR m.player_id='') "
      "AND (m.owner_kind<>'chunk' OR EXISTS("
      "SELECT 1 FROM knowledge_chunks c WHERE c.id=m.owner_id "
      "AND c.embedding_id=m.id AND c.source_version=m.source_version));";
  auto statement = prepare(db_, sql.c_str());
  bind_text(statement.get(), 1, to_string(query.vector_space));
  bind_text(statement.get(), 2, query.model_id);
  bind_text(statement.get(), 3, query.model_version);
  sqlite3_bind_int64(statement.get(), 4,
                     static_cast<sqlite3_int64>(query.values.size()));
  bind_text(statement.get(), 5, query.player_id);

  std::vector<KnowledgeVectorMatch> matches;
  while (sqlite3_step(statement.get()) == SQLITE_ROW) {
    auto item_metadata = read_metadata(statement.get());
    if (!item_metadata) continue;
    const auto values = decode_vector(sqlite3_column_blob(statement.get(), 11),
                                      sqlite3_column_bytes(statement.get(), 11),
                                      item_metadata->dimensions);
    if (!values) continue;
    const float score = cosine_similarity(query.values, *values);
    if (score < query.minimum_score) continue;
    matches.push_back({std::move(*item_metadata), score});
  }

  std::stable_sort(matches.begin(), matches.end(),
                   [](const KnowledgeVectorMatch& lhs,
                      const KnowledgeVectorMatch& rhs) {
                     if (lhs.score != rhs.score) return lhs.score > rhs.score;
                     return lhs.metadata.embedding_id < rhs.metadata.embedding_id;
                   });
  if (matches.size() > query.limit) matches.resize(query.limit);
  return matches;
}

}  // namespace kchess::knowledge
