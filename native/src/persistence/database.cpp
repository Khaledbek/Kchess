// -----------------------------------------------------------------------------
// Section: Persistent application data
// -----------------------------------------------------------------------------

#include "persistence/database.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <iomanip>
#include <memory>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string_view>

#include "sqlite3.h"

#include "engine/stockfish_factory.h"
#include "services/termination.h"

namespace kchess {
namespace {

using Statement = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>;

// A single SQLite connection is shared by UI, analysis, cache and provider
// worker threads. SQLITE_OPEN_FULLMUTEX serializes individual SQLite API
// calls, but it does not make a multi-call BEGIN...COMMIT sequence atomic with
// respect to another thread using the same connection. Hold SQLite's own
// recursive connection mutex for the complete explicit transaction so another
// worker cannot start a nested BEGIN or accidentally join the transaction.
class SqliteConnectionTransactionLock {
 public:
  explicit SqliteConnectionTransactionLock(sqlite3* db) noexcept
      : mutex_(db == nullptr ? nullptr : sqlite3_db_mutex(db)) {
    if (mutex_ != nullptr) sqlite3_mutex_enter(mutex_);
  }

  ~SqliteConnectionTransactionLock() {
    if (mutex_ != nullptr) sqlite3_mutex_leave(mutex_);
  }

  SqliteConnectionTransactionLock(const SqliteConnectionTransactionLock&) = delete;
  SqliteConnectionTransactionLock& operator=(const SqliteConnectionTransactionLock&) = delete;

 private:
  sqlite3_mutex* mutex_{nullptr};
};

void check(const int result, sqlite3* db, const char* operation) {
  if (result != SQLITE_OK && result != SQLITE_DONE && result != SQLITE_ROW) {
    throw std::runtime_error(
        std::string(operation) + ": " + (db == nullptr ? "unknown" : sqlite3_errmsg(db)));
  }
}

void step_nonessential_cache_touch(sqlite3_stmt* statement, sqlite3* db,
                                   const char* operation) {
  const int result = sqlite3_step(statement);
  if (result == SQLITE_DONE || result == SQLITE_ROW) return;
  // last_used_at is only LRU metadata. A transient writer conflict must never
  // turn a valid cache hit into a failed foreground analysis.
  if (result == SQLITE_BUSY || result == SQLITE_LOCKED) return;
  check(result, db, operation);
}

Statement prepare(sqlite3* db, const char* sql) {
  sqlite3_stmt* raw = nullptr;
  check(sqlite3_prepare_v2(db, sql, -1, &raw, nullptr), db, "prepare statement");
  return Statement(raw, sqlite3_finalize);
}

std::string text_column(sqlite3_stmt* statement, const int column) {
  const auto* text = sqlite3_column_text(statement, column);
  return text == nullptr ? std::string{} : reinterpret_cast<const char*>(text);
}

bool table_has_column(sqlite3* db, const char* table, const char* column) {
  const std::string sql = std::string("PRAGMA table_info(") + table + ");";
  auto statement = prepare(db, sql.c_str());
  while (sqlite3_step(statement.get()) == SQLITE_ROW) {
    if (text_column(statement.get(), 1) == column) return true;
  }
  return false;
}

void ensure_critical_count_column(sqlite3* db) {
  if (table_has_column(db, "analysis_runs", "critical_count")) return;
  char* error = nullptr;
  const int rc = sqlite3_exec(
      db,
      "ALTER TABLE analysis_runs ADD COLUMN critical_count INTEGER NOT NULL DEFAULT 0;",
      nullptr,
      nullptr,
      &error);
  if (rc != SQLITE_OK) {
    const std::string message = error == nullptr ? sqlite3_errmsg(db) : error;
    sqlite3_free(error);
    throw std::runtime_error("repair critical_count column: " + message);
  }
}

std::optional<int> optional_int_column(sqlite3_stmt* statement, const int column) {
  if (sqlite3_column_type(statement, column) == SQLITE_NULL) return std::nullopt;
  return sqlite3_column_int(statement, column);
}

std::optional<std::int64_t> optional_int64_column(
    sqlite3_stmt* statement, const int column) {
  if (sqlite3_column_type(statement, column) == SQLITE_NULL) return std::nullopt;
  return sqlite3_column_int64(statement, column);
}

std::optional<std::string> optional_text_column(
    sqlite3_stmt* statement, const int column) {
  if (sqlite3_column_type(statement, column) == SQLITE_NULL) return std::nullopt;
  return text_column(statement, column);
}

std::optional<double> optional_double_column(sqlite3_stmt* statement, const int column) {
  if (sqlite3_column_type(statement, column) == SQLITE_NULL) return std::nullopt;
  return sqlite3_column_double(statement, column);
}

void bind_optional_double(
    sqlite3_stmt* statement, const int index, const std::optional<double>& value) {
  if (value.has_value()) sqlite3_bind_double(statement, index, *value);
  else sqlite3_bind_null(statement, index);
}

MoveCategory parse_category(const std::string& value) {
  if (value == "theory") return MoveCategory::theory;
  if (value == "forced") return MoveCategory::forced;
  if (value == "brilliant") return MoveCategory::brilliant;
  if (value == "critical") return MoveCategory::critical;
  if (value == "best") return MoveCategory::best;
  if (value == "excellent") return MoveCategory::excellent;
  if (value == "good") return MoveCategory::good;
  if (value == "okay") return MoveCategory::okay;
  if (value == "miss") return MoveCategory::miss;
  if (value == "mistake") return MoveCategory::mistake;
  if (value == "blunder") return MoveCategory::blunder;
  return MoveCategory::unknown;
}

std::string category_name(const MoveCategory value) {
  switch (value) {
    case MoveCategory::theory: return "theory";
    case MoveCategory::forced: return "forced";
    case MoveCategory::brilliant: return "brilliant";
    case MoveCategory::critical: return "critical";
    case MoveCategory::best: return "best";
    case MoveCategory::excellent: return "excellent";
    case MoveCategory::good: return "good";
    case MoveCategory::okay: return "okay";
    case MoveCategory::miss: return "miss";
    case MoveCategory::mistake: return "mistake";
    case MoveCategory::blunder: return "blunder";
    case MoveCategory::unknown: return "unknown";
  }
  return "unknown";
}

void increment_category(PlayerAnalysisSummary& summary, const MoveCategory category, const int count) {
  switch (category) {
    case MoveCategory::theory: summary.theory += count; break;
    case MoveCategory::forced: summary.forced += count; break;
    case MoveCategory::brilliant: summary.brilliant += count; break;
    case MoveCategory::critical: summary.critical += count; break;
    case MoveCategory::best: summary.best += count; break;
    case MoveCategory::excellent: summary.excellent += count; break;
    case MoveCategory::good: summary.good += count; break;
    case MoveCategory::okay: summary.okay += count; break;
    case MoveCategory::miss: summary.miss += count; break;
    case MoveCategory::mistake: summary.mistake += count; break;
    case MoveCategory::blunder: summary.blunder += count; break;
    case MoveCategory::unknown: break;
  }
}

void bind_optional_int(
    sqlite3_stmt* statement, const int index, const std::optional<int>& value) {
  if (value.has_value()) {
    sqlite3_bind_int(statement, index, *value);
  } else {
    sqlite3_bind_null(statement, index);
  }
}

void bind_optional_int64(
    sqlite3_stmt* statement, const int index, const std::optional<std::int64_t>& value) {
  if (value.has_value()) sqlite3_bind_int64(statement, index, *value);
  else sqlite3_bind_null(statement, index);
}

void bind_optional_text(
    sqlite3_stmt* statement, const int index, const std::optional<std::string>& value) {
  if (value.has_value()) {
    sqlite3_bind_text(statement, index, value->c_str(), -1, SQLITE_TRANSIENT);
  } else {
    sqlite3_bind_null(statement, index);
  }
}

std::optional<int> parse_optional_rating(
    const std::map<std::string, std::string>& tags, const std::string& key) {
  const auto value = tags.find(key);
  if (value == tags.end() || value->second.empty() || value->second == "?") {
    return std::nullopt;
  }
  try {
    const int rating = std::stoi(value->second);
    return rating > 0 && rating < 10000 ? std::optional<int>(rating) : std::nullopt;
  } catch (...) {
    return std::nullopt;
  }
}

std::string tag_or(
    const std::map<std::string, std::string>& tags,
    const std::string& key,
    const std::string& fallback) {
  const auto value = tags.find(key);
  return value == tags.end() || value->second.empty() ? fallback : value->second;
}

std::string join_moves(const std::vector<std::string>& moves) {
  std::ostringstream result;
  for (std::size_t index = 0; index < moves.size(); ++index) {
    if (index != 0) result << ' ';
    result << moves[index];
  }
  return result.str();
}

std::int64_t unix_time_seconds() {
  return std::chrono::duration_cast<std::chrono::seconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

std::string make_uuid() {
  std::random_device random;
  std::mt19937_64 generator(random());
  std::uniform_int_distribution<std::uint64_t> distribution;
  const auto high = distribution(generator);
  const auto low = distribution(generator);
  std::ostringstream value;
  value << std::hex << std::setfill('0') << std::setw(8)
        << static_cast<std::uint32_t>(high >> 32) << '-' << std::setw(4)
        << static_cast<std::uint16_t>(high >> 16) << '-' << std::setw(4)
        << static_cast<std::uint16_t>(high) << '-' << std::setw(4)
        << static_cast<std::uint16_t>(low >> 48) << '-' << std::setw(12)
        << (low & 0x0000FFFFFFFFFFFFULL);
  return value.str();
}

Profile read_profile(sqlite3_stmt* statement) {
  Profile profile;
  profile.id = text_column(statement, 0);
  profile.type = static_cast<ProfileType>(sqlite3_column_int(statement, 1));
  profile.display_name = text_column(statement, 2);
  if (sqlite3_column_type(statement, 3) != SQLITE_NULL) {
    profile.provider_username = text_column(statement, 3);
  }
  profile.avatar_asset = text_column(statement, 4);
  profile.title = optional_text_column(statement, 5);
  profile.avatar_url = optional_text_column(statement, 6);
  profile.avatar_file = optional_text_column(statement, 7);
  profile.flair = optional_text_column(statement, 8);
  profile.joined_at = optional_int64_column(statement, 9);
  profile.last_online_at = optional_int64_column(statement, 10);
  profile.country = optional_text_column(statement, 11);
  profile.location = optional_text_column(statement, 12);
  profile.public_url = optional_text_column(statement, 13);
  profile.provider_specific_id = optional_text_column(statement, 14);
  profile.followers = optional_int_column(statement, 15);
  profile.fide = optional_int_column(statement, 16);
  profile.provider_games = optional_int_column(statement, 17);
  profile.provider_wins = optional_int_column(statement, 18);
  profile.provider_losses = optional_int_column(statement, 19);
  profile.provider_draws = optional_int_column(statement, 20);
  profile.play_time_seconds = optional_int64_column(statement, 21);
  profile.provider_status = optional_text_column(statement, 22);
  profile.provider_disabled = sqlite3_column_int(statement, 23) != 0;
  profile.provider_tos_violation = sqlite3_column_int(statement, 24) != 0;
  profile.profile_fetched_at = sqlite3_column_int64(statement, 25);
  profile.created_at = sqlite3_column_int64(statement, 26);
  profile.last_opened_at = sqlite3_column_int64(statement, 27);
  return profile;
}

constexpr const char* kProfileColumns =
    "id,type,display_name,provider_username,avatar_asset,title,avatar_url,avatar_file,"
    "flair,joined_at,last_online_at,country,location,public_url,provider_specific_id,"
    "followers,fide,provider_games,provider_wins,provider_losses,provider_draws,"
    "play_time_seconds,provider_status,provider_disabled,provider_tos_violation,"
    "profile_fetched_at,created_at,last_opened_at";

// Player personalization treats all online provider accounts added to this
// KChess library as one player identity. A local PGN/FEN profile remains its
// own scope until the existing merge flow moves those games into an online
// profile. Keep this predicate transport-only: SQL decides which rows belong
// to the player library, not how profile evidence is interpreted.
constexpr const char* kPlayerProfileGameScopeSql =
    "(g.profile_id=? OR (p.type<>2 AND EXISTS(SELECT 1 FROM profiles requested_profile "
    "WHERE requested_profile.id=? AND requested_profile.type<>2)))";

constexpr const char* kGameColumns =
    "g.id,g.profile_id,g.kind,g.white_name,g.black_name,g.white_rating,g.black_rating,"
    "g.result,g.event,g.site,g.game_date,g.time_control,g.pgn,g.starting_fen,g.created_at,"
    "g.provider_game_id,gs.provider_url,g.provider_accuracy_white,g.provider_accuracy_black,"
    "ar.white_local_accuracy,ar.black_local_accuracy,g.provider_outcome,g.time_control_type,"
    "g.provider_ended_at,EXISTS(SELECT 1 FROM favorites f WHERE f.profile_id=g.profile_id "
    "AND f.game_id=g.id),(SELECT f.collection_id FROM favorites f WHERE "
    "f.profile_id=g.profile_id AND f.game_id=g.id),"
    "EXISTS(SELECT 1 FROM favorites f JOIN favorite_collections c ON c.id=f.collection_id "
    "WHERE f.game_id=g.id AND c.name='Downloads' COLLATE NOCASE),(ar.id IS NOT NULL),"
    "g.opening_eco,g.opening_name,g.opening_ply";

constexpr const char* kGameJoins =
    " FROM games g LEFT JOIN game_sources gs ON gs.game_id=g.id "
    "LEFT JOIN analysis_runs ar ON ar.id=(SELECT a.id FROM analysis_runs a "
    "WHERE a.game_id=g.id AND a.status='complete' ORDER BY a.completed_at DESC LIMIT 1) ";

GameRecord read_game_record(sqlite3_stmt* statement) {
  GameRecord game;
  game.id = text_column(statement, 0);
  game.profile_id = text_column(statement, 1);
  game.kind = text_column(statement, 2);
  game.white_name = text_column(statement, 3);
  game.black_name = text_column(statement, 4);
  game.white_rating = optional_int_column(statement, 5);
  game.black_rating = optional_int_column(statement, 6);
  game.result = text_column(statement, 7);
  game.event = text_column(statement, 8);
  game.site = text_column(statement, 9);
  game.date = text_column(statement, 10);
  game.time_control = text_column(statement, 11);
  game.pgn = text_column(statement, 12);
  game.starting_fen = text_column(statement, 13);
  game.created_at = sqlite3_column_int64(statement, 14);
  game.provider_game_id = optional_text_column(statement, 15);
  game.provider_url = optional_text_column(statement, 16);
  game.provider_accuracy_white = optional_double_column(statement, 17);
  game.provider_accuracy_black = optional_double_column(statement, 18);
  game.local_accuracy_white = optional_double_column(statement, 19);
  game.local_accuracy_black = optional_double_column(statement, 20);
  game.provider_outcome = text_column(statement, 21);
  game.time_control_type = text_column(statement, 22);
  game.ended_at = sqlite3_column_int64(statement, 23);
  game.favorite = sqlite3_column_int(statement, 24) != 0;
  game.favorite_collection_id = optional_text_column(statement, 25);
  game.downloaded = sqlite3_column_int(statement, 26) != 0;
  game.analyzed = sqlite3_column_int(statement, 27) != 0;
  game.opening_eco = optional_text_column(statement, 28);
  game.opening_name = optional_text_column(statement, 29);
  game.opening_ply = optional_int_column(statement, 30);
  return game;
}

constexpr const char* kSchema = R"sql(
CREATE TABLE IF NOT EXISTS profiles (
  id TEXT PRIMARY KEY,
  type INTEGER NOT NULL CHECK(type BETWEEN 0 AND 2),
  display_name TEXT NOT NULL CHECK(length(trim(display_name)) > 0),
  provider_username TEXT,
  avatar_asset TEXT NOT NULL,
  created_at INTEGER NOT NULL,
  last_opened_at INTEGER NOT NULL
);
CREATE TABLE IF NOT EXISTS games (
  id TEXT PRIMARY KEY,
  profile_id TEXT NOT NULL REFERENCES profiles(id) ON DELETE CASCADE,
  provider_game_id TEXT,
  white_name TEXT NOT NULL,
  black_name TEXT NOT NULL,
  result TEXT NOT NULL,
  pgn TEXT NOT NULL,
  provider_accuracy REAL,
  local_accuracy REAL,
  created_at INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS games_profile_idx ON games(profile_id, created_at DESC);
CREATE TABLE IF NOT EXISTS game_sources (
  game_id TEXT PRIMARY KEY REFERENCES games(id) ON DELETE CASCADE,
  provider INTEGER NOT NULL,
  provider_url TEXT,
  etag TEXT,
  last_modified TEXT
);
CREATE TABLE IF NOT EXISTS downloads (
  profile_id TEXT NOT NULL REFERENCES profiles(id) ON DELETE CASCADE,
  game_id TEXT NOT NULL REFERENCES games(id) ON DELETE CASCADE,
  downloaded_at INTEGER NOT NULL,
  PRIMARY KEY(profile_id, game_id)
);
CREATE TABLE IF NOT EXISTS favorites (
  profile_id TEXT NOT NULL REFERENCES profiles(id) ON DELETE CASCADE,
  game_id TEXT NOT NULL REFERENCES games(id) ON DELETE CASCADE,
  created_at INTEGER NOT NULL,
  PRIMARY KEY(profile_id, game_id)
);
CREATE TABLE IF NOT EXISTS analysis_runs (
  id TEXT PRIMARY KEY,
  game_id TEXT NOT NULL REFERENCES games(id) ON DELETE CASCADE,
  analysis_version TEXT NOT NULL,
  engine_name TEXT NOT NULL,
  engine_version TEXT NOT NULL,
  depth INTEGER NOT NULL,
  multi_pv INTEGER NOT NULL,
  status TEXT NOT NULL,
  total_plies INTEGER NOT NULL,
  completed_plies INTEGER NOT NULL DEFAULT 0,
  local_accuracy REAL,
  theory_count INTEGER NOT NULL DEFAULT 0,
  brilliant_count INTEGER NOT NULL DEFAULT 0,
  critical_count INTEGER NOT NULL DEFAULT 0,
  best_count INTEGER NOT NULL DEFAULT 0,
  excellent_count INTEGER NOT NULL DEFAULT 0,
  okay_count INTEGER NOT NULL DEFAULT 0,
  miss_count INTEGER NOT NULL DEFAULT 0,
  mistake_count INTEGER NOT NULL DEFAULT 0,
  blunder_count INTEGER NOT NULL DEFAULT 0,
  started_at INTEGER NOT NULL,
  completed_at INTEGER,
  UNIQUE(game_id, analysis_version)
);
CREATE TABLE IF NOT EXISTS move_analysis (
  analysis_run_id TEXT NOT NULL REFERENCES analysis_runs(id) ON DELETE CASCADE,
  ply INTEGER NOT NULL,
  category TEXT NOT NULL,
  best_move TEXT,
  engine_score TEXT,
  is_theory INTEGER NOT NULL DEFAULT 0,
  PRIMARY KEY(analysis_run_id, ply)
);
CREATE TABLE IF NOT EXISTS provider_stats_cache (
  profile_id TEXT NOT NULL REFERENCES profiles(id) ON DELETE CASCADE,
  cache_key TEXT NOT NULL,
  payload_json TEXT NOT NULL,
  expires_at INTEGER NOT NULL,
  PRIMARY KEY(profile_id, cache_key)
);
CREATE TABLE IF NOT EXISTS provider_sync_state (
  profile_id TEXT PRIMARY KEY REFERENCES profiles(id) ON DELETE CASCADE,
  cursor TEXT,
  etag TEXT,
  last_modified TEXT,
  last_sync_at INTEGER,
  last_error TEXT
);
CREATE TABLE IF NOT EXISTS engine_settings (
  profile_id TEXT PRIMARY KEY REFERENCES profiles(id) ON DELETE CASCADE,
  preset TEXT NOT NULL DEFAULT 'medium',
  depth INTEGER NOT NULL DEFAULT 18,
  multi_pv INTEGER NOT NULL DEFAULT 3,
  threads INTEGER NOT NULL DEFAULT 2,
  hash_mb INTEGER NOT NULL DEFAULT 128,
  ponder INTEGER NOT NULL DEFAULT 0
);
CREATE TABLE IF NOT EXISTS app_settings (
  key TEXT PRIMARY KEY,
  value TEXT NOT NULL
);
INSERT OR IGNORE INTO app_settings(key, value) VALUES
  ('showBoardArrows', 'true'),
  ('themeMode', 'system'),
  ('locale', 'de'),
  ('engineId', 'stockfish18');
)sql";

constexpr const char* kMigration2 = R"sql(
ALTER TABLE games ADD COLUMN kind TEXT NOT NULL DEFAULT 'pgn';
ALTER TABLE games ADD COLUMN starting_fen TEXT;
ALTER TABLE games ADD COLUMN white_rating INTEGER;
ALTER TABLE games ADD COLUMN black_rating INTEGER;
ALTER TABLE games ADD COLUMN event TEXT;
ALTER TABLE games ADD COLUMN site TEXT;
ALTER TABLE games ADD COLUMN game_date TEXT;
ALTER TABLE games ADD COLUMN time_control TEXT;

CREATE TABLE game_moves (
  game_id TEXT NOT NULL REFERENCES games(id) ON DELETE CASCADE,
  ply_index INTEGER NOT NULL,
  move_number INTEGER NOT NULL,
  side_to_move TEXT NOT NULL,
  san TEXT NOT NULL,
  uci TEXT NOT NULL,
  fen_before TEXT NOT NULL,
  fen_after TEXT NOT NULL,
  PRIMARY KEY(game_id, ply_index)
);

ALTER TABLE analysis_runs ADD COLUMN config_hash TEXT;
ALTER TABLE analysis_runs ADD COLUMN error TEXT;
ALTER TABLE move_analysis ADD COLUMN engine_depth INTEGER;
ALTER TABLE move_analysis ADD COLUMN evaluation_cp INTEGER;
ALTER TABLE move_analysis ADD COLUMN mate_in INTEGER;
ALTER TABLE move_analysis ADD COLUMN wdl_wins INTEGER;
ALTER TABLE move_analysis ADD COLUMN wdl_draws INTEGER;
ALTER TABLE move_analysis ADD COLUMN wdl_losses INTEGER;
ALTER TABLE move_analysis ADD COLUMN nodes INTEGER;
ALTER TABLE move_analysis ADD COLUMN analysis_timestamp INTEGER;
ALTER TABLE move_analysis ADD COLUMN stockfish_version TEXT;
ALTER TABLE move_analysis ADD COLUMN config_hash TEXT;

CREATE TABLE engine_lines (
  analysis_run_id TEXT NOT NULL REFERENCES analysis_runs(id) ON DELETE CASCADE,
  ply INTEGER NOT NULL,
  rank INTEGER NOT NULL,
  engine_depth INTEGER NOT NULL,
  evaluation_cp INTEGER,
  mate_in INTEGER,
  wdl_wins INTEGER,
  wdl_draws INTEGER,
  wdl_losses INTEGER,
  nodes INTEGER NOT NULL,
  best_move TEXT,
  principal_variation TEXT NOT NULL,
  PRIMARY KEY(analysis_run_id, ply, rank)
);

UPDATE analysis_runs SET config_hash=analysis_version WHERE config_hash IS NULL;
DELETE FROM games WHERE id LIKE 'fixture-%';
PRAGMA user_version = 2;
)sql";

constexpr const char* kMigration3 = R"sql(
ALTER TABLE analysis_runs ADD COLUMN engine_analysis_version TEXT;
ALTER TABLE analysis_runs ADD COLUMN engine_config_hash TEXT;
ALTER TABLE analysis_runs ADD COLUMN classifier_version INTEGER;
ALTER TABLE analysis_runs ADD COLUMN accuracy_algorithm_version INTEGER;
ALTER TABLE analysis_runs ADD COLUMN opening_book_version TEXT;
ALTER TABLE analysis_runs ADD COLUMN white_local_accuracy REAL;
ALTER TABLE analysis_runs ADD COLUMN black_local_accuracy REAL;
ALTER TABLE move_analysis ADD COLUMN classifier_version INTEGER;
ALTER TABLE move_analysis ADD COLUMN expected_score_before REAL;
ALTER TABLE move_analysis ADD COLUMN expected_score_best REAL;
ALTER TABLE move_analysis ADD COLUMN expected_score_played REAL;
ALTER TABLE move_analysis ADD COLUMN expected_score_loss REAL;
ALTER TABLE move_analysis ADD COLUMN recommended_move TEXT;
ALTER TABLE move_analysis ADD COLUMN theory_games INTEGER;
ALTER TABLE move_analysis ADD COLUMN theory_white_wins INTEGER;
ALTER TABLE move_analysis ADD COLUMN theory_draws INTEGER;
ALTER TABLE move_analysis ADD COLUMN theory_black_wins INTEGER;
UPDATE analysis_runs SET engine_analysis_version='1',engine_config_hash=config_hash
WHERE engine_analysis_version IS NULL;
PRAGMA user_version = 3;
)sql";

constexpr const char* kMigration4 = R"sql(
ALTER TABLE profiles ADD COLUMN title TEXT;
ALTER TABLE profiles ADD COLUMN avatar_url TEXT;
ALTER TABLE profiles ADD COLUMN avatar_file TEXT;
ALTER TABLE profiles ADD COLUMN flair TEXT;
ALTER TABLE profiles ADD COLUMN joined_at INTEGER;
ALTER TABLE profiles ADD COLUMN last_online_at INTEGER;
ALTER TABLE profiles ADD COLUMN country TEXT;
ALTER TABLE profiles ADD COLUMN location TEXT;
ALTER TABLE profiles ADD COLUMN public_url TEXT;
ALTER TABLE profiles ADD COLUMN provider_specific_id TEXT;
ALTER TABLE profiles ADD COLUMN followers INTEGER;
ALTER TABLE profiles ADD COLUMN fide INTEGER;
ALTER TABLE profiles ADD COLUMN provider_games INTEGER;
ALTER TABLE profiles ADD COLUMN provider_wins INTEGER;
ALTER TABLE profiles ADD COLUMN provider_losses INTEGER;
ALTER TABLE profiles ADD COLUMN provider_draws INTEGER;
ALTER TABLE profiles ADD COLUMN play_time_seconds INTEGER;
ALTER TABLE profiles ADD COLUMN provider_status TEXT;
ALTER TABLE profiles ADD COLUMN provider_disabled INTEGER NOT NULL DEFAULT 0;
ALTER TABLE profiles ADD COLUMN provider_tos_violation INTEGER NOT NULL DEFAULT 0;
ALTER TABLE profiles ADD COLUMN profile_fetched_at INTEGER NOT NULL DEFAULT 0;

ALTER TABLE games ADD COLUMN provider INTEGER;
ALTER TABLE games ADD COLUMN provider_accuracy_white REAL;
ALTER TABLE games ADD COLUMN provider_accuracy_black REAL;
ALTER TABLE games ADD COLUMN provider_outcome TEXT NOT NULL DEFAULT 'unknown';
ALTER TABLE games ADD COLUMN time_control_type TEXT NOT NULL DEFAULT 'unknown';
ALTER TABLE games ADD COLUMN provider_ended_at INTEGER NOT NULL DEFAULT 0;
ALTER TABLE games ADD COLUMN provider_rules TEXT;
CREATE UNIQUE INDEX games_provider_external_id_unique
  ON games(profile_id,provider,provider_game_id)
  WHERE provider_game_id IS NOT NULL;

CREATE TABLE provider_profiles_cache (
  profile_id TEXT PRIMARY KEY REFERENCES profiles(id) ON DELETE CASCADE,
  provider INTEGER NOT NULL,
  username TEXT NOT NULL,
  payload_json TEXT NOT NULL,
  fetched_at INTEGER NOT NULL,
  expires_at INTEGER NOT NULL,
  etag TEXT,
  last_modified TEXT,
  normalization_version INTEGER NOT NULL
);
ALTER TABLE provider_stats_cache ADD COLUMN fetched_at INTEGER NOT NULL DEFAULT 0;
ALTER TABLE provider_stats_cache ADD COLUMN etag TEXT;
ALTER TABLE provider_stats_cache ADD COLUMN last_modified TEXT;
ALTER TABLE provider_stats_cache ADD COLUMN normalization_version INTEGER NOT NULL DEFAULT 1;
CREATE TABLE provider_month_cache (
  profile_id TEXT NOT NULL REFERENCES profiles(id) ON DELETE CASCADE,
  month TEXT NOT NULL,
  payload_json TEXT NOT NULL DEFAULT '{}',
  fetched_at INTEGER NOT NULL,
  expires_at INTEGER NOT NULL,
  etag TEXT,
  last_modified TEXT,
  normalization_version INTEGER NOT NULL,
  PRIMARY KEY(profile_id,month)
);
ALTER TABLE provider_sync_state ADD COLUMN provider INTEGER;
ALTER TABLE provider_sync_state ADD COLUMN status TEXT NOT NULL DEFAULT 'idle';
ALTER TABLE provider_sync_state ADD COLUMN retry_after INTEGER;
PRAGMA user_version = 4;
)sql";

constexpr const char* kMigration5 = R"sql(
ALTER TABLE engine_settings
  ADD COLUMN time_limit_seconds INTEGER NOT NULL DEFAULT 0;
ALTER TABLE analysis_runs
  ADD COLUMN time_limit_seconds INTEGER NOT NULL DEFAULT 0;
PRAGMA user_version = 5;
)sql";

constexpr const char* kMigration6 = R"sql(
PRAGMA user_version = 6;
)sql";

constexpr const char* kMigration7 = R"sql(
CREATE TABLE engine_position_cache (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  position_fen TEXT NOT NULL,
  stockfish_version TEXT NOT NULL,
  depth INTEGER NOT NULL,
  multi_pv INTEGER NOT NULL,
  time_limit_seconds INTEGER NOT NULL DEFAULT 0,
  reached_depth INTEGER NOT NULL DEFAULT 0,
  nodes INTEGER NOT NULL DEFAULT 0,
  best_move TEXT,
  analyzed_at INTEGER NOT NULL,
  UNIQUE(position_fen,stockfish_version,depth,multi_pv,time_limit_seconds)
);
CREATE INDEX engine_position_cache_lookup
  ON engine_position_cache(position_fen,stockfish_version,depth,multi_pv,time_limit_seconds);
CREATE TABLE engine_position_cache_lines (
  cache_id INTEGER NOT NULL REFERENCES engine_position_cache(id) ON DELETE CASCADE,
  rank INTEGER NOT NULL,
  engine_depth INTEGER NOT NULL,
  evaluation_cp INTEGER,
  mate_in INTEGER,
  wdl_wins INTEGER,
  wdl_draws INTEGER,
  wdl_losses INTEGER,
  nodes INTEGER NOT NULL,
  principal_variation TEXT NOT NULL,
  PRIMARY KEY(cache_id,rank)
);
PRAGMA user_version = 7;
)sql";

// Query-shape indexes used by the library, analysis resume/cache lookup and profile list.
// Keep these additive so existing databases can be upgraded without rewriting data.
constexpr const char* kMigration8 = R"sql(
CREATE INDEX IF NOT EXISTS profiles_last_opened_idx
  ON profiles(last_opened_at DESC, created_at DESC);

CREATE INDEX IF NOT EXISTS games_profile_recent_idx
  ON games(profile_id, provider_ended_at DESC, created_at DESC);

CREATE INDEX IF NOT EXISTS analysis_runs_game_config_idx
  ON analysis_runs(game_id, config_hash);

CREATE INDEX IF NOT EXISTS analysis_runs_complete_compat_idx
  ON analysis_runs(
    game_id, status, engine_analysis_version, engine_version,
    depth, multi_pv, time_limit_seconds, completed_at DESC
  );

CREATE INDEX IF NOT EXISTS analysis_runs_game_status_idx
  ON analysis_runs(game_id, status);

CREATE INDEX IF NOT EXISTS move_analysis_classifier_idx
  ON move_analysis(analysis_run_id, classifier_version, ply);

CREATE INDEX IF NOT EXISTS provider_month_cache_recent_idx
  ON provider_month_cache(profile_id, month DESC);

PRAGMA user_version = 8;
)sql";

constexpr const char* kMigration9 = R"sql(
ALTER TABLE engine_position_cache
  ADD COLUMN last_used_at INTEGER NOT NULL DEFAULT 0;
UPDATE engine_position_cache
  SET last_used_at=analyzed_at
  WHERE last_used_at=0;
CREATE INDEX IF NOT EXISTS engine_position_cache_lru_idx
  ON engine_position_cache(last_used_at DESC, analyzed_at DESC);
CREATE INDEX IF NOT EXISTS analysis_runs_cleanup_idx
  ON analysis_runs(status, started_at);
PRAGMA user_version = 9;
)sql";

// Favorites can be grouped into exactly one top-level collection. There is no
// parent collection column by design, so nested collections cannot exist.
constexpr const char* kMigration10 = R"sql(
CREATE TABLE favorite_collections (
  id TEXT PRIMARY KEY,
  profile_id TEXT NOT NULL REFERENCES profiles(id) ON DELETE CASCADE,
  name TEXT NOT NULL CHECK(length(trim(name)) > 0),
  created_at INTEGER NOT NULL
);
CREATE UNIQUE INDEX favorite_collections_profile_name_unique
  ON favorite_collections(profile_id, name COLLATE NOCASE);
CREATE INDEX favorite_collections_profile_created_idx
  ON favorite_collections(profile_id, created_at ASC);
ALTER TABLE favorites
  ADD COLUMN collection_id TEXT REFERENCES favorite_collections(id) ON DELETE SET NULL;
CREATE INDEX favorites_collection_idx
  ON favorites(profile_id, collection_id, created_at DESC);
PRAGMA user_version = 10;
)sql";

// Favorite collections are application-wide. Games themselves still belong to
// exactly one profile, but their favorite membership can be surfaced together
// across every profile. Rebuilding both tables preserves existing memberships
// while removing collection ownership/cascade from a profile.
constexpr const char* kMigration11 = R"sql(
CREATE TABLE favorite_collections_global (
  id TEXT PRIMARY KEY,
  name TEXT NOT NULL CHECK(length(trim(name)) > 0),
  created_at INTEGER NOT NULL
);
INSERT INTO favorite_collections_global(id,name,created_at)
  SELECT id,name,created_at FROM favorite_collections;
CREATE TABLE favorites_global (
  profile_id TEXT NOT NULL REFERENCES profiles(id) ON DELETE CASCADE,
  game_id TEXT NOT NULL REFERENCES games(id) ON DELETE CASCADE,
  created_at INTEGER NOT NULL,
  collection_id TEXT REFERENCES favorite_collections_global(id) ON DELETE SET NULL,
  PRIMARY KEY(profile_id, game_id)
);
INSERT INTO favorites_global(profile_id,game_id,created_at,collection_id)
  SELECT profile_id,game_id,created_at,collection_id FROM favorites;
DROP TABLE favorites;
DROP TABLE favorite_collections;
ALTER TABLE favorite_collections_global RENAME TO favorite_collections;
ALTER TABLE favorites_global RENAME TO favorites;
CREATE INDEX favorite_collections_created_idx
  ON favorite_collections(created_at ASC);
CREATE INDEX favorite_collections_name_idx
  ON favorite_collections(name COLLATE NOCASE);
CREATE INDEX favorites_collection_idx
  ON favorites(collection_id, created_at DESC);
CREATE INDEX favorites_game_idx
  ON favorites(game_id);
PRAGMA user_version = 11;
)sql";

// The old download flag is no longer an independent state. Existing saved
// provider games become normal global favorites inside a top-level Downloads
// collection. The legacy downloads table stays empty only for schema/backward
// compatibility with older binaries.
constexpr const char* kMigration12 = R"sql(
INSERT INTO favorite_collections(id,name,created_at)
SELECT
  'system-downloads',
  'Downloads',
  COALESCE((SELECT MIN(downloaded_at) FROM downloads), CAST(strftime('%s','now') AS INTEGER))
WHERE EXISTS(SELECT 1 FROM downloads)
  AND NOT EXISTS(
    SELECT 1 FROM favorite_collections WHERE name='Downloads' COLLATE NOCASE
  );

INSERT OR IGNORE INTO favorites(profile_id,game_id,created_at,collection_id)
SELECT
  d.profile_id,
  d.game_id,
  d.downloaded_at,
  (SELECT id FROM favorite_collections
   WHERE name='Downloads' COLLATE NOCASE
   ORDER BY created_at ASC LIMIT 1)
FROM downloads d;

UPDATE favorites
SET collection_id=(
  SELECT id FROM favorite_collections
  WHERE name='Downloads' COLLATE NOCASE
  ORDER BY created_at ASC LIMIT 1
)
WHERE game_id IN (SELECT game_id FROM downloads);

DELETE FROM downloads;
PRAGMA user_version = 12;
)sql";

// Opening classification: per-game ECO code and named opening/variation from the
// offline KCO1 name index. opening_ply is NULL until a game has been classified,
// 0 once classified with no named opening, and the named line's ply otherwise.
constexpr const char* kMigration13 = R"sql(
ALTER TABLE games ADD COLUMN opening_eco TEXT;
ALTER TABLE games ADD COLUMN opening_name TEXT;
ALTER TABLE games ADD COLUMN opening_ply INTEGER;
PRAGMA user_version = 13;
)sql";

// Accuracy algorithm v2 changes the meaning of local_accuracy. Do not surface
// stale v1 values after an upgrade; completed engine analysis remains reusable
// and the inexpensive classification/accuracy pass repopulates these fields
// the next time that game analysis is opened.
constexpr const char* kMigration14 = R"sql(
UPDATE games SET local_accuracy=NULL;
UPDATE analysis_runs
SET local_accuracy=NULL,white_local_accuracy=NULL,black_local_accuracy=NULL,
    accuracy_algorithm_version=0;
PRAGMA user_version = 14;
)sql";

// Saved-analysis compatibility needs to know whether a completed run was
// allowed to stop adaptively.  Strict (adaptive=0) runs can satisfy both
// strict and adaptive requests; adaptive runs must never masquerade as strict.
constexpr const char* kMigration15 = R"sql(
ALTER TABLE analysis_runs ADD COLUMN adaptive_early_stop INTEGER NOT NULL DEFAULT 1;
PRAGMA user_version = 15;
)sql";

// Accuracy algorithm v3 no longer consumes Stockfish WDL and therefore cannot
// reuse the persisted v2 percentages. Keep all engine position analysis intact;
// only invalidate the cheap derived accuracy fields so they are recomputed from
// the already saved CP/mate lines when the analysis is next opened.
constexpr const char* kMigration16 = R"sql(
UPDATE games SET local_accuracy=NULL;
UPDATE analysis_runs
SET local_accuracy=NULL,white_local_accuracy=NULL,black_local_accuracy=NULL,
    accuracy_algorithm_version=0;
PRAGMA user_version = 16;
)sql";

// Local bot games are first-class persisted sessions. An active game survives
// application/screen restarts; resigned games are retained for the later bot
// history screen, while aborted games are deleted explicitly.
constexpr const char* kMigration17 = R"sql(
CREATE TABLE bot_games (
  id TEXT PRIMARY KEY,
  bot_elo INTEGER NOT NULL CHECK(bot_elo BETWEEN 100 AND 3200),
  player_color TEXT NOT NULL CHECK(player_color IN ('white','black')),
  bot_color TEXT NOT NULL CHECK(bot_color IN ('white','black')),
  status TEXT NOT NULL CHECK(status IN ('active','resigned','complete')),
  result TEXT NOT NULL DEFAULT '*',
  starting_fen TEXT NOT NULL,
  current_fen TEXT NOT NULL,
  created_at INTEGER NOT NULL,
  updated_at INTEGER NOT NULL
);
CREATE TABLE bot_game_moves (
  game_id TEXT NOT NULL REFERENCES bot_games(id) ON DELETE CASCADE,
  ply INTEGER NOT NULL CHECK(ply > 0),
  uci TEXT NOT NULL,
  san TEXT NOT NULL,
  fen_after TEXT NOT NULL,
  PRIMARY KEY(game_id, ply)
);
CREATE UNIQUE INDEX bot_games_one_active_idx
  ON bot_games((1)) WHERE status='active';
CREATE INDEX bot_games_updated_idx ON bot_games(updated_at DESC);
PRAGMA user_version = 17;
)sql";


// Bot-play display preferences belong to the persisted bot session rather than
// the global application/analysis settings.
constexpr const char* kMigration18 = R"sql(
ALTER TABLE bot_games ADD COLUMN show_eval_bar INTEGER NOT NULL DEFAULT 0;
PRAGMA user_version = 18;
)sql";

// A bot-history row may be materialized once into the normal local game
// library so it can use the existing analysis pipeline without duplicate
// imports on every visit. Deleting that local analysis game only clears the
// link; the original bot history remains authoritative.
constexpr const char* kMigration19 = R"sql(
ALTER TABLE bot_games ADD COLUMN analysis_game_id TEXT REFERENCES games(id) ON DELETE SET NULL;
CREATE INDEX bot_games_analysis_game_idx ON bot_games(analysis_game_id);
PRAGMA user_version = 19;
)sql";

// Training progress is application-local and independent from the active
// provider profile. One authoritative row is stored per stable exercise id.
constexpr const char* kMigration20 = R"sql(
CREATE TABLE training_progress (
  exercise_id TEXT PRIMARY KEY,
  mastered INTEGER NOT NULL DEFAULT 0,
  success_streak INTEGER NOT NULL DEFAULT 0,
  success_count INTEGER NOT NULL DEFAULT 0,
  attempt_count INTEGER NOT NULL DEFAULT 0,
  last_attempt_at INTEGER
);
PRAGMA user_version = 20;
)sql";

// AI chess profiles are compact learned overlays keyed by the existing KChess
// profile. The JSON payload is versioned by native/ai/profile so persistence
// does not duplicate the AI schema.
constexpr const char* kMigration21 = R"sql(
CREATE TABLE ai_chess_profiles (
  profile_id TEXT PRIMARY KEY REFERENCES profiles(id) ON DELETE CASCADE,
  payload_json TEXT NOT NULL,
  updated_at INTEGER NOT NULL
);
PRAGMA user_version = 21;
)sql";


// Background player-profile work is persisted independently from the compact
// profile JSON so large libraries do not inflate every coach context read.
constexpr const char* kMigration22 = R"sql(
CREATE TABLE ai_profile_queue (
  profile_id TEXT NOT NULL REFERENCES profiles(id) ON DELETE CASCADE,
  game_id TEXT NOT NULL REFERENCES games(id) ON DELETE CASCADE,
  state TEXT NOT NULL DEFAULT 'queued',
  priority REAL NOT NULL DEFAULT 0,
  reason TEXT NOT NULL DEFAULT '',
  source_version INTEGER NOT NULL DEFAULT 0,
  processed_version INTEGER NOT NULL DEFAULT -1,
  attempts INTEGER NOT NULL DEFAULT 0,
  updated_at INTEGER NOT NULL,
  PRIMARY KEY(profile_id, game_id)
);
CREATE INDEX ai_profile_queue_next_idx
  ON ai_profile_queue(profile_id, state, priority DESC, updated_at ASC);
PRAGMA user_version = 22;
)sql";


// Targeted profile probes are persisted separately from normal game analysis.
// This keeps sparse background evidence from becoming the authoritative UI
// analysis run while still allowing resume/profile learning across restarts.
constexpr const char* kMigration23 = R"sql(
CREATE TABLE ai_profile_probe_evidence (
  profile_id TEXT NOT NULL REFERENCES profiles(id) ON DELETE CASCADE,
  game_id TEXT NOT NULL REFERENCES games(id) ON DELETE CASCADE,
  ply INTEGER NOT NULL,
  classification TEXT NOT NULL DEFAULT '',
  expected_score_loss REAL,
  fen_before TEXT NOT NULL DEFAULT '',
  uci TEXT NOT NULL DEFAULT '',
  tier TEXT NOT NULL DEFAULT 'verification',
  updated_at INTEGER NOT NULL,
  PRIMARY KEY(profile_id, game_id, ply)
);
CREATE INDEX ai_profile_probe_game_idx
  ON ai_profile_probe_evidence(profile_id, game_id, updated_at DESC);
PRAGMA user_version = 23;
)sql";

// Stable, payload-free index of every authoritative source the learned profile
// may reference. The future profile graph points at these IDs instead of
// copying analysis/statistics data into a second store.
constexpr const char* kMigration24 = R"sql(
CREATE TABLE ai_profile_evidence_registry (
  profile_id TEXT NOT NULL REFERENCES profiles(id) ON DELETE CASCADE,
  evidence_id TEXT NOT NULL,
  source_kind TEXT NOT NULL,
  source_table TEXT NOT NULL,
  source_key TEXT NOT NULL,
  game_id TEXT REFERENCES games(id) ON DELETE CASCADE,
  ply INTEGER,
  evidence_tier TEXT NOT NULL DEFAULT 'metadata',
  evidence_level INTEGER NOT NULL DEFAULT 0,
  source_version INTEGER NOT NULL DEFAULT 0,
  updated_at INTEGER NOT NULL,
  PRIMARY KEY(profile_id, evidence_id)
);
CREATE INDEX ai_profile_evidence_registry_kind_idx
  ON ai_profile_evidence_registry(profile_id, source_kind, evidence_level DESC);
CREATE INDEX ai_profile_evidence_registry_game_idx
  ON ai_profile_evidence_registry(profile_id, game_id, ply);
PRAGMA user_version = 24;
)sql";

// Payload-free semantic graph over the evidence registry/profile model. Nodes
// store only stable locators/selectors and routing metadata; authoritative
// profile, game and engine data remain in their existing tables/caches.
constexpr const char* kMigration25 = R"sql(
CREATE TABLE ai_profile_graph_nodes (
  profile_id TEXT NOT NULL REFERENCES profiles(id) ON DELETE CASCADE,
  node_id TEXT NOT NULL,
  node_kind TEXT NOT NULL,
  source_evidence_id TEXT NOT NULL DEFAULT '',
  source_selector TEXT NOT NULL DEFAULT '',
  evidence_level INTEGER NOT NULL DEFAULT 0,
  search_key TEXT NOT NULL DEFAULT '',
  graph_revision INTEGER NOT NULL DEFAULT 0,
  PRIMARY KEY(profile_id, node_id)
);
CREATE INDEX ai_profile_graph_nodes_kind_idx
  ON ai_profile_graph_nodes(profile_id, node_kind, evidence_level DESC);
CREATE TABLE ai_profile_graph_edges (
  profile_id TEXT NOT NULL REFERENCES profiles(id) ON DELETE CASCADE,
  from_node_id TEXT NOT NULL,
  to_node_id TEXT NOT NULL,
  relation TEXT NOT NULL,
  weight REAL NOT NULL DEFAULT 1.0,
  graph_revision INTEGER NOT NULL DEFAULT 0,
  PRIMARY KEY(profile_id, from_node_id, to_node_id, relation)
);
CREATE INDEX ai_profile_graph_edges_from_idx
  ON ai_profile_graph_edges(profile_id, from_node_id);
CREATE INDEX ai_profile_graph_edges_to_idx
  ON ai_profile_graph_edges(profile_id, to_node_id);
PRAGMA user_version = 25;
)sql";

// Update 94: persist the highest profile-pipeline stage reached by each game.
// Existing rows were created by the pre-sampling pipeline; derive a conservative
// stage from their terminal/relevant state without claiming verification/deep work.
constexpr const char* kMigration26 = R"sql(
ALTER TABLE ai_profile_queue ADD COLUMN pipeline_stage INTEGER NOT NULL DEFAULT 2;
UPDATE ai_profile_queue
SET pipeline_stage = CASE
  WHEN reason='initial_sample' THEN 1
  WHEN reason='metadata_only' OR state='indexed' THEN 2
  WHEN state='engine_pending' THEN 7
  WHEN reason<>'' THEN 4
  ELSE 2
END;
PRAGMA user_version = 26;
)sql";

// Update 99: persist current Stage-3/Stage-4 membership for truthful pipeline
// diagnostics. These flags are recomputed on every queue sync, unlike
// pipeline_stage which intentionally records the highest stage ever reached.
constexpr const char* kMigration27 = R"sql(
ALTER TABLE ai_profile_queue ADD COLUMN historical_sample INTEGER NOT NULL DEFAULT 0;
ALTER TABLE ai_profile_queue ADD COLUMN interesting_selected INTEGER NOT NULL DEFAULT 0;
PRAGMA user_version = 27;
)sql";

// Update 105: general Knowledge Graph storage. These tables contain only
// graph identities, relations and compact typed metadata; source payloads stay
// in their authoritative KChess stores.
constexpr const char* kMigration28 = R"sql(
CREATE TABLE knowledge_nodes (
  id TEXT PRIMARY KEY,
  kind TEXT NOT NULL,
  assertion_kind TEXT NOT NULL,
  schema_version INTEGER NOT NULL
);
CREATE INDEX knowledge_nodes_kind_idx
  ON knowledge_nodes(kind, assertion_kind);
CREATE TABLE knowledge_node_properties (
  node_id TEXT NOT NULL REFERENCES knowledge_nodes(id) ON DELETE CASCADE,
  property_key TEXT NOT NULL,
  value_type TEXT NOT NULL CHECK(value_type IN ('bool','int','real','text')),
  bool_value INTEGER,
  int_value INTEGER,
  real_value REAL,
  text_value TEXT,
  PRIMARY KEY(node_id, property_key)
);
CREATE TABLE knowledge_edges (
  id TEXT PRIMARY KEY,
  from_node_id TEXT NOT NULL REFERENCES knowledge_nodes(id) ON DELETE CASCADE,
  to_node_id TEXT NOT NULL REFERENCES knowledge_nodes(id) ON DELETE CASCADE,
  kind TEXT NOT NULL,
  schema_version INTEGER NOT NULL
);
CREATE INDEX knowledge_edges_from_idx
  ON knowledge_edges(from_node_id, kind, to_node_id);
CREATE INDEX knowledge_edges_to_idx
  ON knowledge_edges(to_node_id, kind, from_node_id);
CREATE TABLE knowledge_edge_properties (
  edge_id TEXT NOT NULL REFERENCES knowledge_edges(id) ON DELETE CASCADE,
  property_key TEXT NOT NULL,
  value_type TEXT NOT NULL CHECK(value_type IN ('bool','int','real','text')),
  bool_value INTEGER,
  int_value INTEGER,
  real_value REAL,
  text_value TEXT,
  PRIMARY KEY(edge_id, property_key)
);
PRAGMA user_version = 28;
)sql";

// Update 106: source provenance and dependency tracking for targeted Knowledge
// Graph invalidation. Payloads remain in their authoritative stores; this schema
// persists only stable locators, versions and invalidation state.
constexpr const char* kMigration29 = R"sql(
CREATE TABLE knowledge_sources (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  source_type TEXT NOT NULL,
  source_id TEXT NOT NULL,
  source_version TEXT NOT NULL,
  first_seen_ms INTEGER NOT NULL,
  last_seen_ms INTEGER NOT NULL,
  UNIQUE(source_type, source_id)
);
CREATE INDEX knowledge_sources_type_idx
  ON knowledge_sources(source_type, source_id);
CREATE TABLE knowledge_provenance (
  entry_kind TEXT NOT NULL CHECK(entry_kind IN ('node','edge')),
  entry_id TEXT NOT NULL,
  source_row_id INTEGER NOT NULL REFERENCES knowledge_sources(id) ON DELETE CASCADE,
  first_seen_ms INTEGER NOT NULL,
  last_seen_ms INTEGER NOT NULL,
  PRIMARY KEY(entry_kind, entry_id, source_row_id)
);
CREATE INDEX knowledge_provenance_source_idx
  ON knowledge_provenance(source_row_id, entry_kind, entry_id);
CREATE TABLE knowledge_dependencies (
  entry_kind TEXT NOT NULL CHECK(entry_kind IN ('node','edge')),
  entry_id TEXT NOT NULL,
  source_row_id INTEGER NOT NULL REFERENCES knowledge_sources(id) ON DELETE CASCADE,
  source_version TEXT NOT NULL,
  invalidated INTEGER NOT NULL DEFAULT 0 CHECK(invalidated IN (0,1)),
  invalidated_at_ms INTEGER,
  PRIMARY KEY(entry_kind, entry_id, source_row_id)
);
CREATE INDEX knowledge_dependencies_source_idx
  ON knowledge_dependencies(source_row_id, invalidated, entry_kind, entry_id);
CREATE INDEX knowledge_dependencies_invalidated_idx
  ON knowledge_dependencies(invalidated, invalidated_at_ms);
CREATE TRIGGER knowledge_node_tracking_cleanup
AFTER DELETE ON knowledge_nodes
BEGIN
  DELETE FROM knowledge_provenance WHERE entry_kind='node' AND entry_id=OLD.id;
  DELETE FROM knowledge_dependencies WHERE entry_kind='node' AND entry_id=OLD.id;
END;
CREATE TRIGGER knowledge_edge_tracking_cleanup
AFTER DELETE ON knowledge_edges
BEGIN
  DELETE FROM knowledge_provenance WHERE entry_kind='edge' AND entry_id=OLD.id;
  DELETE FROM knowledge_dependencies WHERE entry_kind='edge' AND entry_id=OLD.id;
END;
PRAGMA user_version = 29;
)sql";


// Update 107: semantic Chunk Registry. Chunks store compact retrieval text and
// metadata, while source locators and graph links remain normalized. Dependency
// tracking is widened to chunks so later incremental refresh can invalidate only
// affected semantic units.
constexpr const char* kMigration30 = R"sql(
CREATE TABLE knowledge_chunks (
  id TEXT PRIMARY KEY,
  player_id TEXT NOT NULL,
  type TEXT NOT NULL,
  topic TEXT NOT NULL,
  granularity TEXT NOT NULL CHECK(granularity IN ('summary','topic','entity','evidence')),
  source_type TEXT NOT NULL,
  opening TEXT,
  eco TEXT,
  color TEXT,
  result TEXT,
  time_control TEXT,
  date_range TEXT,
  rating_range TEXT,
  evidence_type TEXT,
  confidence REAL CHECK(confidence IS NULL OR (confidence >= 0.0 AND confidence <= 1.0)),
  coverage REAL CHECK(coverage IS NULL OR (coverage >= 0.0 AND coverage <= 1.0)),
  freshness REAL CHECK(freshness IS NULL OR (freshness >= 0.0 AND freshness <= 1.0)),
  importance REAL CHECK(importance IS NULL OR (importance >= 0.0 AND importance <= 1.0)),
  sample_size INTEGER CHECK(sample_size IS NULL OR sample_size >= 0),
  source_quality REAL CHECK(source_quality IS NULL OR (source_quality >= 0.0 AND source_quality <= 1.0)),
  source_version TEXT NOT NULL,
  embedding_id TEXT,
  schema_version INTEGER NOT NULL,
  content TEXT NOT NULL
);
CREATE INDEX knowledge_chunks_player_type_idx
  ON knowledge_chunks(player_id, type, granularity);
CREATE INDEX knowledge_chunks_topic_idx
  ON knowledge_chunks(player_id, topic);
CREATE INDEX knowledge_chunks_embedding_idx
  ON knowledge_chunks(embedding_id) WHERE embedding_id IS NOT NULL;
CREATE TABLE knowledge_chunk_sources (
  chunk_id TEXT NOT NULL REFERENCES knowledge_chunks(id) ON DELETE CASCADE,
  source_type TEXT NOT NULL,
  source_id TEXT NOT NULL,
  source_version TEXT NOT NULL,
  PRIMARY KEY(chunk_id, source_type, source_id)
);
CREATE INDEX knowledge_chunk_sources_lookup_idx
  ON knowledge_chunk_sources(source_type, source_id, chunk_id);
CREATE TABLE knowledge_chunk_graph_links (
  chunk_id TEXT NOT NULL REFERENCES knowledge_chunks(id) ON DELETE CASCADE,
  entry_kind TEXT NOT NULL CHECK(entry_kind IN ('node','edge')),
  entry_id TEXT NOT NULL,
  link_kind TEXT NOT NULL CHECK(link_kind IN ('describes','evidence_for','summarizes')),
  PRIMARY KEY(chunk_id, entry_kind, entry_id, link_kind)
);
CREATE INDEX knowledge_chunk_graph_links_entry_idx
  ON knowledge_chunk_graph_links(entry_kind, entry_id, chunk_id);

-- SQLite cannot widen a CHECK constraint in place. Rebuild only the two small
-- routing tables so dependency/provenance ownership can also refer to chunks.
-- Drop dependent indexes/triggers first because SQLite keeps global object names
-- and may rewrite trigger bodies when referenced tables are renamed.
DROP TRIGGER knowledge_node_tracking_cleanup;
DROP TRIGGER knowledge_edge_tracking_cleanup;
DROP INDEX knowledge_provenance_source_idx;
DROP INDEX knowledge_dependencies_source_idx;
DROP INDEX knowledge_dependencies_invalidated_idx;
ALTER TABLE knowledge_provenance RENAME TO knowledge_provenance_v29;
CREATE TABLE knowledge_provenance (
  entry_kind TEXT NOT NULL CHECK(entry_kind IN ('node','edge','chunk')),
  entry_id TEXT NOT NULL,
  source_row_id INTEGER NOT NULL REFERENCES knowledge_sources(id) ON DELETE CASCADE,
  first_seen_ms INTEGER NOT NULL,
  last_seen_ms INTEGER NOT NULL,
  PRIMARY KEY(entry_kind, entry_id, source_row_id)
);
INSERT INTO knowledge_provenance
  SELECT entry_kind,entry_id,source_row_id,first_seen_ms,last_seen_ms
  FROM knowledge_provenance_v29;
DROP TABLE knowledge_provenance_v29;
CREATE INDEX knowledge_provenance_source_idx
  ON knowledge_provenance(source_row_id, entry_kind, entry_id);

ALTER TABLE knowledge_dependencies RENAME TO knowledge_dependencies_v29;
CREATE TABLE knowledge_dependencies (
  entry_kind TEXT NOT NULL CHECK(entry_kind IN ('node','edge','chunk')),
  entry_id TEXT NOT NULL,
  source_row_id INTEGER NOT NULL REFERENCES knowledge_sources(id) ON DELETE CASCADE,
  source_version TEXT NOT NULL,
  invalidated INTEGER NOT NULL DEFAULT 0 CHECK(invalidated IN (0,1)),
  invalidated_at_ms INTEGER,
  PRIMARY KEY(entry_kind, entry_id, source_row_id)
);
INSERT INTO knowledge_dependencies
  SELECT entry_kind,entry_id,source_row_id,source_version,invalidated,invalidated_at_ms
  FROM knowledge_dependencies_v29;
DROP TABLE knowledge_dependencies_v29;
CREATE INDEX knowledge_dependencies_source_idx
  ON knowledge_dependencies(source_row_id, invalidated, entry_kind, entry_id);
CREATE INDEX knowledge_dependencies_invalidated_idx
  ON knowledge_dependencies(invalidated, invalidated_at_ms);
CREATE TRIGGER knowledge_node_tracking_cleanup
AFTER DELETE ON knowledge_nodes
BEGIN
  DELETE FROM knowledge_provenance WHERE entry_kind='node' AND entry_id=OLD.id;
  DELETE FROM knowledge_dependencies WHERE entry_kind='node' AND entry_id=OLD.id;
  DELETE FROM knowledge_chunk_graph_links WHERE entry_kind='node' AND entry_id=OLD.id;
END;
CREATE TRIGGER knowledge_edge_tracking_cleanup
AFTER DELETE ON knowledge_edges
BEGIN
  DELETE FROM knowledge_provenance WHERE entry_kind='edge' AND entry_id=OLD.id;
  DELETE FROM knowledge_dependencies WHERE entry_kind='edge' AND entry_id=OLD.id;
  DELETE FROM knowledge_chunk_graph_links WHERE entry_kind='edge' AND entry_id=OLD.id;
END;
CREATE TRIGGER knowledge_chunk_tracking_cleanup
AFTER DELETE ON knowledge_chunks
BEGIN
  DELETE FROM knowledge_provenance WHERE entry_kind='chunk' AND entry_id=OLD.id;
  DELETE FROM knowledge_dependencies WHERE entry_kind='chunk' AND entry_id=OLD.id;
END;
PRAGMA user_version = 30;
)sql";

// Update 113: persisted Knowledge Graph quality metadata. This table contains
// only derived routing-quality scores for existing graph entries; upstream
// profile/statistics/analysis confidence remains authoritative in its own store.
constexpr const char* kMigration31 = R"sql(
CREATE TABLE knowledge_quality (
  entry_kind TEXT NOT NULL CHECK(entry_kind IN ('node','edge','chunk')),
  entry_id TEXT NOT NULL,
  confidence REAL NOT NULL CHECK(confidence >= 0.0 AND confidence <= 1.0),
  coverage REAL NOT NULL CHECK(coverage >= 0.0 AND coverage <= 1.0),
  freshness REAL CHECK(freshness IS NULL OR (freshness >= 0.0 AND freshness <= 1.0)),
  source_quality REAL NOT NULL CHECK(source_quality >= 0.0 AND source_quality <= 1.0),
  evidence_diversity REAL NOT NULL CHECK(evidence_diversity >= 0.0 AND evidence_diversity <= 1.0),
  importance REAL NOT NULL CHECK(importance >= 0.0 AND importance <= 1.0),
  sample_size INTEGER NOT NULL DEFAULT 0 CHECK(sample_size >= 0),
  evidence_count INTEGER NOT NULL DEFAULT 0 CHECK(evidence_count >= 0),
  temporal_scope TEXT NOT NULL CHECK(temporal_scope IN ('unknown','lifetime','recent','mixed')),
  freshness_basis TEXT NOT NULL CHECK(freshness_basis IN ('unknown','evidence_timestamp','explicit_recent_scope','source_observation')),
  evaluated_at_ms INTEGER NOT NULL CHECK(evaluated_at_ms >= 0),
  PRIMARY KEY(entry_kind, entry_id)
);
CREATE INDEX knowledge_quality_ranking_idx
  ON knowledge_quality(importance DESC, confidence DESC, coverage DESC);
CREATE INDEX knowledge_quality_freshness_idx
  ON knowledge_quality(temporal_scope, freshness DESC);
CREATE TRIGGER knowledge_node_quality_cleanup
AFTER DELETE ON knowledge_nodes
BEGIN
  DELETE FROM knowledge_quality WHERE entry_kind='node' AND entry_id=OLD.id;
END;
CREATE TRIGGER knowledge_edge_quality_cleanup
AFTER DELETE ON knowledge_edges
BEGIN
  DELETE FROM knowledge_quality WHERE entry_kind='edge' AND entry_id=OLD.id;
END;
CREATE TRIGGER knowledge_chunk_quality_cleanup
AFTER DELETE ON knowledge_chunks
BEGIN
  DELETE FROM knowledge_quality WHERE entry_kind='chunk' AND entry_id=OLD.id;
END;
PRAGMA user_version = 31;
)sql";


// Update 114: immutable Knowledge Graph history plus explicit conflict records.
// Versions contain only compact graph metadata/properties and source locators;
// authoritative PGN/statistics/analysis payloads remain in their existing stores.
constexpr const char* kMigration32 = R"sql(
CREATE TABLE knowledge_versions (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  entry_kind TEXT NOT NULL CHECK(entry_kind IN ('node','edge')),
  entry_id TEXT NOT NULL,
  revision INTEGER NOT NULL CHECK(revision > 0),
  change_kind TEXT NOT NULL CHECK(change_kind IN ('updated','deleted')),
  kind TEXT NOT NULL,
  assertion_kind TEXT,
  from_node_id TEXT,
  to_node_id TEXT,
  schema_version INTEGER NOT NULL,
  recorded_at_ms INTEGER NOT NULL CHECK(recorded_at_ms >= 0),
  UNIQUE(entry_kind, entry_id, revision)
);
CREATE INDEX knowledge_versions_entry_idx
  ON knowledge_versions(entry_kind, entry_id, revision DESC);
CREATE INDEX knowledge_versions_time_idx
  ON knowledge_versions(recorded_at_ms DESC);

CREATE TABLE knowledge_version_properties (
  version_id INTEGER NOT NULL REFERENCES knowledge_versions(id) ON DELETE CASCADE,
  property_key TEXT NOT NULL,
  value_type TEXT NOT NULL CHECK(value_type IN ('bool','int','real','text')),
  bool_value INTEGER,
  int_value INTEGER,
  real_value REAL,
  text_value TEXT,
  PRIMARY KEY(version_id, property_key)
);

CREATE TABLE knowledge_version_sources (
  version_id INTEGER NOT NULL REFERENCES knowledge_versions(id) ON DELETE CASCADE,
  source_type TEXT NOT NULL,
  source_id TEXT NOT NULL,
  source_version TEXT NOT NULL,
  first_seen_ms INTEGER NOT NULL,
  last_seen_ms INTEGER NOT NULL,
  PRIMARY KEY(version_id, source_type, source_id)
);
CREATE INDEX knowledge_version_sources_lookup_idx
  ON knowledge_version_sources(source_type, source_id, version_id);

CREATE TABLE knowledge_conflicts (
  id TEXT PRIMARY KEY,
  profile_id TEXT NOT NULL,
  subject_key TEXT NOT NULL,
  historical_version_id INTEGER NOT NULL REFERENCES knowledge_versions(id) ON DELETE CASCADE,
  historical_node_id TEXT NOT NULL,
  current_entry_id TEXT NOT NULL,
  relation_edge_id TEXT NOT NULL,
  resolution TEXT NOT NULL CHECK(resolution IN (
    'unresolved','current_supersedes_historical','balanced','insufficient_evidence'
  )),
  temporal_change TEXT NOT NULL CHECK(temporal_change IN ('none','improved','declined')),
  historical_confidence REAL NOT NULL CHECK(historical_confidence >= 0.0 AND historical_confidence <= 1.0),
  current_confidence REAL NOT NULL CHECK(current_confidence >= 0.0 AND current_confidence <= 1.0),
  first_detected_ms INTEGER NOT NULL CHECK(first_detected_ms >= 0),
  last_evaluated_ms INTEGER NOT NULL CHECK(last_evaluated_ms >= 0),
  UNIQUE(profile_id, subject_key, historical_version_id, current_entry_id)
);
CREATE INDEX knowledge_conflicts_profile_idx
  ON knowledge_conflicts(profile_id, last_evaluated_ms DESC);
CREATE INDEX knowledge_conflicts_current_idx
  ON knowledge_conflicts(current_entry_id, resolution);
PRAGMA user_version = 32;
)sql";

// Update 115: persistent embedding metadata plus exact vector payload storage.
// The metadata table is the stable retrieval contract; vector bytes remain a
// replaceable implementation detail behind knowledge::VectorIndex.
constexpr const char* kMigration33 = R"sql(
CREATE TABLE knowledge_embeddings_metadata (
  id TEXT PRIMARY KEY,
  owner_kind TEXT NOT NULL CHECK(owner_kind IN ('chunk','node')),
  owner_id TEXT NOT NULL,
  player_id TEXT NOT NULL DEFAULT '',
  vector_space TEXT NOT NULL CHECK(vector_space IN ('text_semantic','chess_position')),
  model_id TEXT NOT NULL,
  model_version TEXT NOT NULL,
  dimensions INTEGER NOT NULL CHECK(dimensions > 0),
  source_version TEXT NOT NULL,
  created_at_ms INTEGER NOT NULL CHECK(created_at_ms >= 0),
  updated_at_ms INTEGER NOT NULL CHECK(updated_at_ms >= created_at_ms),
  UNIQUE(owner_kind, owner_id, vector_space, model_id, model_version)
);
CREATE INDEX knowledge_embeddings_scope_idx
  ON knowledge_embeddings_metadata(player_id, vector_space, model_id, model_version, dimensions);
CREATE INDEX knowledge_embeddings_owner_idx
  ON knowledge_embeddings_metadata(owner_kind, owner_id);

CREATE TABLE knowledge_embedding_vectors (
  embedding_id TEXT PRIMARY KEY REFERENCES knowledge_embeddings_metadata(id) ON DELETE CASCADE,
  encoding TEXT NOT NULL CHECK(encoding='float32_le'),
  vector_blob BLOB NOT NULL
);

CREATE TRIGGER knowledge_chunk_embedding_cleanup
AFTER DELETE ON knowledge_chunks
BEGIN
  DELETE FROM knowledge_embeddings_metadata
  WHERE owner_kind='chunk' AND owner_id=OLD.id;
END;
CREATE TRIGGER knowledge_node_embedding_cleanup
AFTER DELETE ON knowledge_nodes
BEGIN
  DELETE FROM knowledge_embeddings_metadata
  WHERE owner_kind='node' AND owner_id=OLD.id;
END;
CREATE TRIGGER knowledge_embedding_chunk_reference_cleanup
AFTER DELETE ON knowledge_embeddings_metadata
BEGIN
  UPDATE knowledge_chunks SET embedding_id=NULL WHERE embedding_id=OLD.id;
END;
PRAGMA user_version = 33;
)sql";

// Update 123: bounded Knowledge Graph retrieval traces for the developer
// inspector/live diagnostics. Traces contain routing/retrieval metadata only;
// provider prompts, PGNs and engine payloads are deliberately excluded.
constexpr const char* kMigration34 = R"sql(
CREATE TABLE knowledge_query_traces (
  id TEXT PRIMARY KEY,
  profile_id TEXT NOT NULL,
  query_text TEXT NOT NULL,
  intent TEXT NOT NULL,
  route_confidence REAL NOT NULL CHECK(route_confidence >= 0.0 AND route_confidence <= 1.0),
  answerable INTEGER NOT NULL CHECK(answerable IN (0,1)),
  answerability_confidence REAL NOT NULL CHECK(answerability_confidence >= 0.0 AND answerability_confidence <= 1.0),
  seed_count INTEGER NOT NULL DEFAULT 0 CHECK(seed_count >= 0),
  candidate_node_count INTEGER NOT NULL DEFAULT 0 CHECK(candidate_node_count >= 0),
  candidate_chunk_count INTEGER NOT NULL DEFAULT 0 CHECK(candidate_chunk_count >= 0),
  selected_node_count INTEGER NOT NULL DEFAULT 0 CHECK(selected_node_count >= 0),
  selected_chunk_count INTEGER NOT NULL DEFAULT 0 CHECK(selected_chunk_count >= 0),
  expanded_nodes INTEGER NOT NULL DEFAULT 0 CHECK(expanded_nodes >= 0),
  packet_tokens INTEGER NOT NULL DEFAULT 0 CHECK(packet_tokens >= 0),
  created_at_ms INTEGER NOT NULL CHECK(created_at_ms >= 0),
  trace_json TEXT NOT NULL
);
CREATE INDEX knowledge_query_traces_profile_time_idx
  ON knowledge_query_traces(profile_id, created_at_ms DESC);
PRAGMA user_version = 34;
)sql";

// Cleanup 124: remove stores that belonged to the retired profile-only graph
// and sparse profile-owned probe cache. The general Knowledge Graph and the
// shared analysis cache are now authoritative, so keeping these tables would
// preserve duplicate dead state indefinitely. Profile-probe registry locators
// are removed before the legacy table disappears.
constexpr const char* kMigration35 = R"sql(
DELETE FROM ai_profile_evidence_registry
WHERE source_kind='profile_probe' OR source_table='ai_profile_probe_evidence';
DROP TABLE IF EXISTS ai_profile_graph_edges;
DROP TABLE IF EXISTS ai_profile_graph_nodes;
DROP TABLE IF EXISTS ai_profile_probe_evidence;
PRAGMA user_version = 35;
)sql";

// Update 127: persist the coarse termination bucket once so the profile
// sampler can consume the same metadata Statistics uses without loading PGN
// payloads during every Stage-3 sweep. Existing rows are backfilled once in
// open_and_migrate() with the authoritative native classifier.
constexpr const char* kMigration36 = R"sql(
ALTER TABLE games ADD COLUMN termination_type TEXT NOT NULL DEFAULT 'unknown';
PRAGMA user_version = 36;
)sql";

// Update 130: freeze the historical/bootstrap boundary once per learned
// profile. The cutoff is a played-at watermark, not wall-clock time, so older
// provider archive months discovered later remain historical while games
// actually played after bootstrap may learn incrementally outside Stage 3.
constexpr const char* kMigration37 = R"sql(
CREATE TABLE ai_profile_sampling_state (
  profile_id TEXT PRIMARY KEY REFERENCES profiles(id) ON DELETE CASCADE,
  bootstrap_cutoff_played_at INTEGER NOT NULL CHECK(bootstrap_cutoff_played_at >= 0),
  updated_at INTEGER NOT NULL
);
PRAGMA user_version = 37;
)sql";

// Update 131: persist the adaptive Stage-3 requirement/coverage telemetry so
// UI/diagnostics can read it cheaply without rebuilding sampling decisions.
// These fields describe sampling/work readiness only and never replace learned
// profile confidence.
constexpr const char* kMigration38 = R"sql(
ALTER TABLE ai_profile_sampling_state ADD COLUMN total_games INTEGER NOT NULL DEFAULT 0;
ALTER TABLE ai_profile_sampling_state ADD COLUMN eligible_games INTEGER NOT NULL DEFAULT 0;
ALTER TABLE ai_profile_sampling_state ADD COLUMN excluded_games INTEGER NOT NULL DEFAULT 0;
ALTER TABLE ai_profile_sampling_state ADD COLUMN minimum_games INTEGER NOT NULL DEFAULT 0;
ALTER TABLE ai_profile_sampling_state ADD COLUMN recommended_games INTEGER NOT NULL DEFAULT 0;
ALTER TABLE ai_profile_sampling_state ADD COLUMN maximum_games INTEGER NOT NULL DEFAULT 0;
ALTER TABLE ai_profile_sampling_state ADD COLUMN selected_games INTEGER NOT NULL DEFAULT 0;
ALTER TABLE ai_profile_sampling_state ADD COLUMN required_strata INTEGER NOT NULL DEFAULT 0;
ALTER TABLE ai_profile_sampling_state ADD COLUMN covered_strata INTEGER NOT NULL DEFAULT 0;
ALTER TABLE ai_profile_sampling_state ADD COLUMN diversity_score REAL NOT NULL DEFAULT 0.0;
ALTER TABLE ai_profile_sampling_state ADD COLUMN covered_population_share REAL NOT NULL DEFAULT 0.0;
ALTER TABLE ai_profile_sampling_state ADD COLUMN capped INTEGER NOT NULL DEFAULT 0;
PRAGMA user_version = 38;
)sql";

// Update 136: independent learner attempts live beside the learned profile.
// These counters are not engine evidence and never alter the analysis cache.
constexpr const char* kMigration39 = R"sql(
CREATE TABLE IF NOT EXISTS ai_coach_skill_progress (
  profile_id TEXT NOT NULL REFERENCES profiles(id) ON DELETE CASCADE,
  motif_id TEXT NOT NULL,
  independent_successes INTEGER NOT NULL DEFAULT 0,
  other_attempts INTEGER NOT NULL DEFAULT 0,
  last_practiced_at INTEGER NOT NULL DEFAULT 0,
  PRIMARY KEY(profile_id,motif_id)
);
PRAGMA user_version = 39;
)sql";

// Update 156: extend the same learner-practice row with scheduling metadata.
// This is spaced-repetition orchestration state, not a skill rating. Historical
// `other_attempts` remains for compatibility while verified weak attempts gain
// an explicit counter for the richer native teaching policy.
constexpr const char* kMigration40 = R"sql(
ALTER TABLE ai_coach_skill_progress ADD COLUMN verified_weak_attempts INTEGER NOT NULL DEFAULT 0;
ALTER TABLE ai_coach_skill_progress ADD COLUMN guided_successes INTEGER NOT NULL DEFAULT 0;
ALTER TABLE ai_coach_skill_progress ADD COLUMN success_streak INTEGER NOT NULL DEFAULT 0;
ALTER TABLE ai_coach_skill_progress ADD COLUMN schedule_level INTEGER NOT NULL DEFAULT 0;
ALTER TABLE ai_coach_skill_progress ADD COLUMN interval_seconds INTEGER NOT NULL DEFAULT 0;
ALTER TABLE ai_coach_skill_progress ADD COLUMN next_practice_at INTEGER NOT NULL DEFAULT 0;
UPDATE ai_coach_skill_progress SET verified_weak_attempts=other_attempts
WHERE verified_weak_attempts=0 AND other_attempts>0;
PRAGMA user_version = 40;
)sql";

// Ali's opening-drill and per-move accuracy columns follow the existing
// Khaled schema; migration numbers 21/22 already belong to the profile queue.
constexpr const char* kMigration41 = R"sql(
ALTER TABLE training_progress ADD COLUMN best_depth INTEGER NOT NULL DEFAULT 0;
PRAGMA user_version = 41;
)sql";

constexpr const char* kMigration42 = R"sql(
ALTER TABLE move_analysis ADD COLUMN move_accuracy REAL;
ALTER TABLE move_analysis ADD COLUMN accuracy_weight REAL;
-- Rebuild the derived classification from saved engine slots on normal reuse.
-- No new engine work or second analysis store is needed for old games.
UPDATE analysis_runs SET accuracy_algorithm_version=0
WHERE status='complete' AND classifier_version>0;
PRAGMA user_version = 42;
)sql";


}  // namespace

Database::Database(std::filesystem::path data_directory)
    : data_directory_(std::move(data_directory)) {}

Database::~Database() { close(); }

void Database::open_and_migrate() {
  std::filesystem::create_directories(data_directory_);
  const auto database_path = data_directory_ / "kchess.sqlite3";
  check(
      sqlite3_open_v2(
          database_path.string().c_str(),
          &db_,
          SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
          nullptr),
      db_,
      "open database");
  execute("PRAGMA foreign_keys = ON;");
  execute("PRAGMA journal_mode = WAL;");
  // WAL + NORMAL keeps local persistence durable enough for an application cache/database
  // while avoiding an fsync on every small write. busy_timeout makes short writer
  // contention fail gracefully instead of surfacing SQLITE_BUSY immediately.
  execute("PRAGMA synchronous = NORMAL;");
  execute("PRAGMA busy_timeout = 5000;");
  // Keep statistics-cache invalidation attached to the authoritative SQLite
  // source instead of requiring every service mutation path to remember an
  // explicit callback. Only tables read by player_knowledge_json advance this
  // generation; unrelated queue/cache writes therefore do not evict it.
  sqlite3_update_hook(
      db_,
      [](void* context, int, const char*, const char* table, sqlite3_int64) {
        if (context == nullptr || table == nullptr) return;
        const std::string_view name(table);
        if (name != "profiles" && name != "games") return;
        static_cast<std::atomic_uint64_t*>(context)->fetch_add(
            1, std::memory_order_relaxed);
      },
      &statistics_source_revision_);
  const int version = schema_version();
  SqliteConnectionTransactionLock transaction_lock(db_);
  execute("BEGIN IMMEDIATE;");
  try {
    execute(kSchema);
    // Repair databases that were marked as migrated while the v6 column was
    // missing. This check is idempotent and also protects fresh installs.
    ensure_critical_count_column(db_);
    if (version < 1) execute("PRAGMA user_version = 1;");
    if (version < 2) execute(kMigration2);
    if (version < 3) execute(kMigration3);
    if (version < 4) execute(kMigration4);
    if (version < 5) execute(kMigration5);
    if (version < 6) execute(kMigration6);
    if (version < 7) execute(kMigration7);
    if (version < 8) execute(kMigration8);
    if (version < 9) execute(kMigration9);
    if (version < 10) execute(kMigration10);
    if (version < 11) execute(kMigration11);
    if (version < 12) execute(kMigration12);
    if (version < 13) execute(kMigration13);
    if (version < 14) execute(kMigration14);
    if (version < 15) execute(kMigration15);
    if (version < 16) execute(kMigration16);
    if (version < 17) execute(kMigration17);
    if (version < 18) execute(kMigration18);
    if (version < 19) execute(kMigration19);
    if (version < 20) execute(kMigration20);
    if (version < 21) execute(kMigration21);
    if (version < 22) execute(kMigration22);
    if (version < 23) execute(kMigration23);
    if (version < 24) execute(kMigration24);
    if (version < 25) execute(kMigration25);
    if (version < 26) execute(kMigration26);
    if (version < 27) execute(kMigration27);
    if (version < 28) execute(kMigration28);
    if (version < 29) execute(kMigration29);
    if (version < 30) execute(kMigration30);
    if (version < 31) execute(kMigration31);
    if (version < 32) execute(kMigration32);
    if (version < 33) execute(kMigration33);
    if (version < 34) execute(kMigration34);
    if (version < 35) execute(kMigration35);
    if (version < 36) {
      execute(kMigration36);
      auto read = prepare(db_, "SELECT id,pgn,result FROM games;");
      auto write = prepare(db_, "UPDATE games SET termination_type=? WHERE id=?;");
      while (sqlite3_step(read.get()) == SQLITE_ROW) {
        const std::string id = text_column(read.get(), 0);
        const std::string pgn = text_column(read.get(), 1);
        const std::string result = text_column(read.get(), 2);
        const std::string termination = termination_bucket(pgn, result);
        sqlite3_reset(write.get());
        sqlite3_clear_bindings(write.get());
        sqlite3_bind_text(write.get(), 1, termination.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(write.get(), 2, id.c_str(), -1, SQLITE_TRANSIENT);
        check(sqlite3_step(write.get()), db_, "backfill game termination metadata");
      }
    }
    if (version < 37) execute(kMigration37);
    if (version < 38) execute(kMigration38);
    if (version < 39) execute(kMigration39);
    if (version < 40) execute(kMigration40);
    if (version < 41) execute(kMigration41);
    if (version < 42) execute(kMigration42);

    // Older builds could leave several completed/cancelled analysis_runs for
    // the same game. At process startup there are no live workers, so collapse
    // them to one authoritative saved run. Prefer a complete run, then the
    // highest analysis quality. The independent position cache is untouched.
    execute(
        "DELETE FROM analysis_runs WHERE id NOT IN ("
        "SELECT (SELECT candidate.id FROM analysis_runs candidate "
        "WHERE candidate.game_id=grouped.game_id ORDER BY "
        "CASE WHEN candidate.status='complete' THEN 0 ELSE 1 END ASC,"
        "candidate.depth DESC,candidate.multi_pv DESC,"
        "CASE WHEN candidate.time_limit_seconds=0 THEN 2147483647 "
        "ELSE candidate.time_limit_seconds END DESC,"
        "candidate.adaptive_early_stop ASC,"
        "COALESCE(candidate.completed_at,0) DESC,candidate.started_at DESC LIMIT 1) "
        "FROM analysis_runs grouped GROUP BY grouped.game_id);");

    // Privacy hardening for online profiles. Earlier builds could persist public
    // real-world/account metadata returned by provider profile endpoints. Kchess
    // intentionally retains only the provider handle, chess title/avatar/flair,
    // chess ratings and game-derived statistics. Existing values are scrubbed
    // in-place and old normalized profile-cache payloads are discarded once.
    execute(
        "UPDATE profiles SET "
        "display_name=CASE WHEN provider_username IS NOT NULL AND length(trim(provider_username))>0 "
        "THEN provider_username ELSE display_name END,"
        "joined_at=NULL,last_online_at=NULL,country=NULL,location=NULL,public_url=NULL,"
        "provider_specific_id=NULL,followers=NULL,provider_status=NULL "
        "WHERE type IN (0,1);");
    execute(
        "DELETE FROM provider_profiles_cache WHERE "
        "payload_json LIKE '%\"joined\"%' OR payload_json LIKE '%\"lastOnline\"%' OR "
        "payload_json LIKE '%\"country\"%' OR payload_json LIKE '%\"location\"%' OR "
        "payload_json LIKE '%\"publicUrl\"%' OR payload_json LIKE '%\"providerSpecificId\"%' OR "
        "payload_json LIKE '%\"followers\"%' OR payload_json LIKE '%\"status\"%';");
    execute("COMMIT;");
  } catch (...) {
    sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
    throw;
  }
  // Lightweight SQLite-recommended planner maintenance; it does not rewrite user data.
  execute("PRAGMA optimize;");
  run_maintenance();
}

int Database::schema_version() const {
  auto statement = prepare(db_, "PRAGMA user_version;");
  return sqlite3_step(statement.get()) == SQLITE_ROW ? sqlite3_column_int(statement.get(), 0)
                                                     : 0;
}

void Database::close() noexcept {
  if (db_ != nullptr) {
    sqlite3_update_hook(db_, nullptr, nullptr);
    sqlite3_close_v2(db_);
    db_ = nullptr;
  }
}

void Database::execute(const std::string& sql) const {
  char* error = nullptr;
  const int result = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &error);
  if (result != SQLITE_OK) {
    const std::string message = error == nullptr ? sqlite3_errmsg(db_) : error;
    sqlite3_free(error);
    throw std::runtime_error(message);
  }
}

std::vector<Profile> Database::profiles() const {
  const std::string sql = std::string("SELECT ") + kProfileColumns
      + " FROM profiles ORDER BY last_opened_at DESC;";
  auto statement = prepare(db_, sql.c_str());
  std::vector<Profile> result;
  while (sqlite3_step(statement.get()) == SQLITE_ROW) {
    result.push_back(read_profile(statement.get()));
  }
  return result;
}

Profile Database::create_profile(
    const ProfileType type,
    const std::string& display_name,
    const std::optional<std::string>& provider_username,
    const std::string& avatar_asset) {
  const Profile profile{
      .id = make_uuid(),
      .type = type,
      .display_name = display_name,
      .provider_username = provider_username,
      .avatar_asset = avatar_asset,
      .created_at = unix_time_seconds(),
      .last_opened_at = unix_time_seconds(),
  };
  auto statement = prepare(
      db_,
      "INSERT INTO profiles(id,type,display_name,provider_username,avatar_asset,"
      "created_at,last_opened_at) VALUES(?,?,?,?,?,?,?);");
  sqlite3_bind_text(statement.get(), 1, profile.id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(statement.get(), 2, static_cast<int>(profile.type));
  sqlite3_bind_text(statement.get(), 3, profile.display_name.c_str(), -1, SQLITE_TRANSIENT);
  if (profile.provider_username.has_value()) {
    sqlite3_bind_text(
        statement.get(), 4, profile.provider_username->c_str(), -1, SQLITE_TRANSIENT);
  } else {
    sqlite3_bind_null(statement.get(), 4);
  }
  sqlite3_bind_text(statement.get(), 5, profile.avatar_asset.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(statement.get(), 6, profile.created_at);
  sqlite3_bind_int64(statement.get(), 7, profile.last_opened_at);
  check(sqlite3_step(statement.get()), db_, "insert profile");
  auto engine_settings = prepare(
      db_,
      "INSERT INTO engine_settings(profile_id,preset,depth,multi_pv,threads,hash_mb,"
      "ponder,time_limit_seconds) VALUES(?,'medium',18,3,2,128,0,0);");
  sqlite3_bind_text(engine_settings.get(), 1, profile.id.c_str(), -1, SQLITE_TRANSIENT);
  check(sqlite3_step(engine_settings.get()), db_, "insert profile engine settings");
  set_active_profile(profile.id);
  return profile;
}

Profile Database::create_provider_profile(
    const ProviderProfile& provider_profile,
    const ResponseCacheInfo& cache,
    const std::string& normalized_json) {
  const auto type = static_cast<ProfileType>(provider_profile.provider);
  auto profile = create_profile(
      type, provider_profile.display_name, provider_profile.username,
      provider_profile.fallback_asset);
  return update_provider_profile(profile.id, provider_profile, cache, normalized_json);
}

Profile Database::update_provider_profile(
    const std::string& profile_id,
    const ProviderProfile& value,
    const ResponseCacheInfo& cache,
    const std::string& normalized_json) {
  auto statement = prepare(
      db_,
      "UPDATE profiles SET display_name=?,provider_username=?,avatar_asset=?,title=?,"
      "avatar_url=?,flair=?,joined_at=NULL,last_online_at=NULL,country=NULL,location=NULL,"
      "public_url=NULL,provider_specific_id=NULL,followers=NULL,fide=?,provider_games=?,"
      "provider_wins=?,provider_losses=?,provider_draws=?,play_time_seconds=?,provider_status=NULL,"
      "provider_disabled=?,provider_tos_violation=?,profile_fetched_at=? WHERE id=?;");
  sqlite3_bind_text(statement.get(), 1, value.username.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(statement.get(), 2, value.username.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(statement.get(), 3, value.fallback_asset.c_str(), -1, SQLITE_TRANSIENT);
  bind_optional_text(statement.get(), 4, value.title);
  bind_optional_text(statement.get(), 5, value.avatar_url);
  bind_optional_text(statement.get(), 6, value.flair);
  bind_optional_int(statement.get(), 7, value.fide);
  bind_optional_int(statement.get(), 8, value.games);
  bind_optional_int(statement.get(), 9, value.wins);
  bind_optional_int(statement.get(), 10, value.losses);
  bind_optional_int(statement.get(), 11, value.draws);
  bind_optional_int64(statement.get(), 12, value.play_time_seconds);
  sqlite3_bind_int(statement.get(), 13, value.disabled ? 1 : 0);
  sqlite3_bind_int(statement.get(), 14, value.tos_violation ? 1 : 0);
  sqlite3_bind_int64(statement.get(), 15, cache.fetched_at);
  sqlite3_bind_text(statement.get(), 16, profile_id.c_str(), -1, SQLITE_TRANSIENT);
  check(sqlite3_step(statement.get()), db_, "update provider profile");
  if (sqlite3_changes(db_) == 0) throw std::runtime_error("Profile not found");
  put_provider_cache(profile_id, "profile", normalized_json, cache);
  return profile(profile_id).value();
}

void Database::set_active_profile(const std::string& profile_id) {
  auto update = prepare(db_, "UPDATE profiles SET last_opened_at=? WHERE id=?;");
  sqlite3_bind_int64(update.get(), 1, unix_time_seconds());
  sqlite3_bind_text(update.get(), 2, profile_id.c_str(), -1, SQLITE_TRANSIENT);
  check(sqlite3_step(update.get()), db_, "activate profile");
  if (sqlite3_changes(db_) == 0) {
    throw std::runtime_error("Profile not found");
  }
  set_setting("activeProfileId", profile_id);
}

std::optional<Profile> Database::delete_profile(const std::string& profile_id) {
  if (!profile(profile_id).has_value()) throw std::runtime_error("Profile not found");
  const auto active_id = setting("activeProfileId");
  SqliteConnectionTransactionLock transaction_lock(db_);
  execute("BEGIN IMMEDIATE;");
  try {
    auto remove = prepare(db_, "DELETE FROM profiles WHERE id=?;");
    sqlite3_bind_text(remove.get(), 1, profile_id.c_str(), -1, SQLITE_TRANSIENT);
    check(sqlite3_step(remove.get()), db_, "delete profile");
    if (active_id == profile_id) {
      auto replacement = prepare(
          db_, "SELECT id FROM profiles ORDER BY last_opened_at DESC,created_at DESC LIMIT 1;");
      if (sqlite3_step(replacement.get()) == SQLITE_ROW) {
        auto save = prepare(
            db_,
            "INSERT INTO app_settings(key,value) VALUES('activeProfileId',?) "
            "ON CONFLICT(key) DO UPDATE SET value=excluded.value;");
        const auto replacement_id = text_column(replacement.get(), 0);
        sqlite3_bind_text(save.get(), 1, replacement_id.c_str(), -1, SQLITE_TRANSIENT);
        check(sqlite3_step(save.get()), db_, "select replacement profile");
      } else {
        auto clear = prepare(db_, "DELETE FROM app_settings WHERE key='activeProfileId';");
        check(sqlite3_step(clear.get()), db_, "clear active profile");
      }
    }
    execute("COMMIT;");
  } catch (...) {
    sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
    throw;
  }
  return active_profile();
}

std::optional<Profile> Database::active_profile() const {
  const auto active_id = setting("activeProfileId");
  if (!active_id.has_value()) {
    return std::nullopt;
  }
  const std::string sql = std::string("SELECT ") + kProfileColumns
      + " FROM profiles WHERE id=?;";
  auto statement = prepare(db_, sql.c_str());
  sqlite3_bind_text(statement.get(), 1, active_id->c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(statement.get()) != SQLITE_ROW) {
    return std::nullopt;
  }
  return read_profile(statement.get());
}

std::optional<Profile> Database::profile(const std::string& profile_id) const {
  const std::string sql = std::string("SELECT ") + kProfileColumns
      + " FROM profiles WHERE id=?;";
  auto statement = prepare(db_, sql.c_str());
  sqlite3_bind_text(statement.get(), 1, profile_id.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(statement.get()) != SQLITE_ROW) return std::nullopt;
  return read_profile(statement.get());
}

std::string Database::player_profile_owner_id(
    const std::string& profile_id) const {
  const auto requested = profile(profile_id);
  if (!requested.has_value()) throw std::runtime_error("Profile not found");
  if (requested->type == ProfileType::local_pgn_fen) return requested->id;

  // The oldest surviving online profile is the stable persistence owner for
  // the shared learned profile/queue/graph. Provider account switching must
  // not fork personalization into one background model per provider.
  auto statement = prepare(
      db_,
      "SELECT id FROM profiles WHERE type<>2 "
      "ORDER BY created_at ASC,id ASC LIMIT 1;");
  if (sqlite3_step(statement.get()) != SQLITE_ROW) return requested->id;
  return text_column(statement.get(), 0);
}

std::vector<std::string> Database::profile_game_ids(
    const std::string& profile_id) const {
  auto statement = prepare(db_, "SELECT id FROM games WHERE profile_id=?;");
  sqlite3_bind_text(statement.get(), 1, profile_id.c_str(), -1, SQLITE_TRANSIENT);
  std::vector<std::string> result;
  while (sqlite3_step(statement.get()) == SQLITE_ROW) {
    result.push_back(text_column(statement.get(), 0));
  }
  return result;
}


void Database::merge_local_profile(
    const std::string& source_profile_id,
    const std::string& target_profile_id) {
  if (source_profile_id == target_profile_id) {
    throw std::invalid_argument("Source and target profile must be different");
  }

  const auto source_profile = profile(source_profile_id);
  const auto target_profile = profile(target_profile_id);
  if (!source_profile.has_value() || !target_profile.has_value()) {
    throw std::runtime_error("Profile not found");
  }
  if (source_profile->type != ProfileType::local_pgn_fen) {
    throw std::invalid_argument("Only a local PGN/FEN profile can be merged");
  }
  if (target_profile->type == ProfileType::local_pgn_fen) {
    throw std::invalid_argument("Merge target must be a Chess.com or Lichess profile");
  }

  struct AnalysisRunCandidate {
    std::string id;
    std::string analysis_version;
    std::string status;
    int completed_plies{0};
  };

  const auto source_game_ids = profile_game_ids(source_profile_id);

  SqliteConnectionTransactionLock transaction_lock(db_);
  execute("BEGIN IMMEDIATE;");
  try {
    for (const auto& source_game_id : source_game_ids) {
      auto source_game = prepare(
          db_,
          "SELECT kind,COALESCE(starting_fen,''),pgn,white_name,black_name,result,game_date "
          "FROM games WHERE id=? AND profile_id=?;");
      sqlite3_bind_text(
          source_game.get(), 1, source_game_id.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(
          source_game.get(), 2, source_profile_id.c_str(), -1, SQLITE_TRANSIENT);
      if (sqlite3_step(source_game.get()) != SQLITE_ROW) continue;

      const auto kind = text_column(source_game.get(), 0);
      const auto starting_fen = text_column(source_game.get(), 1);
      const auto pgn = text_column(source_game.get(), 2);
      const auto white_name = text_column(source_game.get(), 3);
      const auto black_name = text_column(source_game.get(), 4);
      const auto result = text_column(source_game.get(), 5);
      const auto game_date = text_column(source_game.get(), 6);
      std::optional<std::string> duplicate_id;

      if (kind == "fen") {
        auto duplicate = prepare(
            db_,
            "SELECT id FROM games "
            "WHERE profile_id=? AND kind='fen' "
            "AND TRIM(COALESCE(starting_fen,''))=TRIM(?) "
            "ORDER BY created_at ASC LIMIT 1;");
        sqlite3_bind_text(
            duplicate.get(), 1, target_profile_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(
            duplicate.get(), 2, starting_fen.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(duplicate.get()) == SQLITE_ROW) {
          duplicate_id = text_column(duplicate.get(), 0);
        }
      } else {
        int source_move_count = 0;
        auto move_count = prepare(
            db_, "SELECT COUNT(*) FROM game_moves WHERE game_id=?;");
        sqlite3_bind_text(
            move_count.get(), 1, source_game_id.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(move_count.get()) == SQLITE_ROW) {
          source_move_count = sqlite3_column_int(move_count.get(), 0);
        }

        if (source_move_count > 0) {
          auto duplicate = prepare(
              db_,
              "SELECT g.id FROM games g "
              "WHERE g.profile_id=? AND g.kind='pgn' "
              "AND COALESCE(g.starting_fen,'')=? "
              "AND LOWER(TRIM(g.white_name))=LOWER(TRIM(?)) "
              "AND LOWER(TRIM(g.black_name))=LOWER(TRIM(?)) "
              "AND g.result=? "
              "AND ((?<>'' AND TRIM(g.game_date)=TRIM(?)) "
              "     OR (?='' AND TRIM(g.pgn)=TRIM(?))) "
              "AND (SELECT COUNT(*) FROM game_moves tm WHERE tm.game_id=g.id)=? "
              "AND NOT EXISTS("
              "  SELECT 1 FROM game_moves sm "
              "  LEFT JOIN game_moves tm "
              "    ON tm.game_id=g.id AND tm.ply_index=sm.ply_index "
              "  WHERE sm.game_id=? AND (tm.uci IS NULL OR tm.uci<>sm.uci)"
              ") "
              "ORDER BY g.created_at ASC LIMIT 1;");
          sqlite3_bind_text(
              duplicate.get(), 1, target_profile_id.c_str(), -1, SQLITE_TRANSIENT);
          sqlite3_bind_text(
              duplicate.get(), 2, starting_fen.c_str(), -1, SQLITE_TRANSIENT);
          sqlite3_bind_text(
              duplicate.get(), 3, white_name.c_str(), -1, SQLITE_TRANSIENT);
          sqlite3_bind_text(
              duplicate.get(), 4, black_name.c_str(), -1, SQLITE_TRANSIENT);
          sqlite3_bind_text(
              duplicate.get(), 5, result.c_str(), -1, SQLITE_TRANSIENT);
          sqlite3_bind_text(
              duplicate.get(), 6, game_date.c_str(), -1, SQLITE_TRANSIENT);
          sqlite3_bind_text(
              duplicate.get(), 7, game_date.c_str(), -1, SQLITE_TRANSIENT);
          sqlite3_bind_text(
              duplicate.get(), 8, game_date.c_str(), -1, SQLITE_TRANSIENT);
          sqlite3_bind_text(
              duplicate.get(), 9, pgn.c_str(), -1, SQLITE_TRANSIENT);
          sqlite3_bind_int(duplicate.get(), 10, source_move_count);
          sqlite3_bind_text(
              duplicate.get(), 11, source_game_id.c_str(), -1, SQLITE_TRANSIENT);
          if (sqlite3_step(duplicate.get()) == SQLITE_ROW) {
            duplicate_id = text_column(duplicate.get(), 0);
          }
        } else if (!pgn.empty()) {
          auto duplicate = prepare(
              db_,
              "SELECT id FROM games "
              "WHERE profile_id=? AND kind='pgn' AND TRIM(pgn)=TRIM(?) "
              "ORDER BY created_at ASC LIMIT 1;");
          sqlite3_bind_text(
              duplicate.get(), 1, target_profile_id.c_str(), -1, SQLITE_TRANSIENT);
          sqlite3_bind_text(duplicate.get(), 2, pgn.c_str(), -1, SQLITE_TRANSIENT);
          if (sqlite3_step(duplicate.get()) == SQLITE_ROW) {
            duplicate_id = text_column(duplicate.get(), 0);
          }
        }
      }

      if (!duplicate_id.has_value()) {
        auto move_game = prepare(
            db_, "UPDATE games SET profile_id=? WHERE id=? AND profile_id=?;");
        sqlite3_bind_text(
            move_game.get(), 1, target_profile_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(
            move_game.get(), 2, source_game_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(
            move_game.get(), 3, source_profile_id.c_str(), -1, SQLITE_TRANSIENT);
        check(sqlite3_step(move_game.get()), db_, "move local game to target profile");

        auto move_favorite = prepare(
            db_, "UPDATE favorites SET profile_id=? WHERE game_id=? AND profile_id=?;");
        sqlite3_bind_text(
            move_favorite.get(), 1, target_profile_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(
            move_favorite.get(), 2, source_game_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(
            move_favorite.get(), 3, source_profile_id.c_str(), -1, SQLITE_TRANSIENT);
        check(sqlite3_step(move_favorite.get()), db_, "move favorite ownership");

        auto move_download = prepare(
            db_, "UPDATE downloads SET profile_id=? WHERE game_id=? AND profile_id=?;");
        sqlite3_bind_text(
            move_download.get(), 1, target_profile_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(
            move_download.get(), 2, source_game_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(
            move_download.get(), 3, source_profile_id.c_str(), -1, SQLITE_TRANSIENT);
        check(sqlite3_step(move_download.get()), db_, "move legacy download ownership");
        continue;
      }

      const auto& target_game_id = *duplicate_id;

      std::optional<std::int64_t> source_favorite_created;
      std::optional<std::string> source_collection;
      {
        auto favorite = prepare(
            db_, "SELECT created_at,collection_id FROM favorites WHERE game_id=?;");
        sqlite3_bind_text(
            favorite.get(), 1, source_game_id.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(favorite.get()) == SQLITE_ROW) {
          source_favorite_created = sqlite3_column_int64(favorite.get(), 0);
          source_collection = optional_text_column(favorite.get(), 1);
        }
      }

      if (source_favorite_created.has_value()) {
        std::optional<std::int64_t> target_favorite_created;
        std::optional<std::string> target_collection;
        auto target_favorite = prepare(
            db_, "SELECT created_at,collection_id FROM favorites WHERE game_id=?;");
        sqlite3_bind_text(
            target_favorite.get(), 1, target_game_id.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(target_favorite.get()) == SQLITE_ROW) {
          target_favorite_created = sqlite3_column_int64(target_favorite.get(), 0);
          target_collection = optional_text_column(target_favorite.get(), 1);
        }

        if (!target_favorite_created.has_value()) {
          auto insert = prepare(
              db_,
              "INSERT INTO favorites(profile_id,game_id,created_at,collection_id) "
              "VALUES(?,?,?,?);");
          sqlite3_bind_text(
              insert.get(), 1, target_profile_id.c_str(), -1, SQLITE_TRANSIENT);
          sqlite3_bind_text(
              insert.get(), 2, target_game_id.c_str(), -1, SQLITE_TRANSIENT);
          sqlite3_bind_int64(insert.get(), 3, *source_favorite_created);
          bind_optional_text(insert.get(), 4, source_collection);
          check(sqlite3_step(insert.get()), db_, "preserve favorite during profile merge");
        } else {
          const auto created_at =
              std::min(*source_favorite_created, *target_favorite_created);
          const auto collection =
              source_collection.has_value() ? source_collection : target_collection;
          auto update = prepare(
              db_,
              "UPDATE favorites SET profile_id=?,created_at=?,collection_id=? "
              "WHERE game_id=?;");
          sqlite3_bind_text(
              update.get(), 1, target_profile_id.c_str(), -1, SQLITE_TRANSIENT);
          sqlite3_bind_int64(update.get(), 2, created_at);
          bind_optional_text(update.get(), 3, collection);
          sqlite3_bind_text(
              update.get(), 4, target_game_id.c_str(), -1, SQLITE_TRANSIENT);
          check(sqlite3_step(update.get()), db_, "merge duplicate favorite");
        }

        auto remove_source_favorite =
            prepare(db_, "DELETE FROM favorites WHERE game_id=?;");
        sqlite3_bind_text(
            remove_source_favorite.get(),
            1,
            source_game_id.c_str(),
            -1,
            SQLITE_TRANSIENT);
        check(
            sqlite3_step(remove_source_favorite.get()),
            db_,
            "remove duplicate source favorite");
      }

      std::vector<AnalysisRunCandidate> source_runs;
      {
        auto runs = prepare(
            db_,
            "SELECT id,analysis_version,status,completed_plies "
            "FROM analysis_runs WHERE game_id=?;");
        sqlite3_bind_text(runs.get(), 1, source_game_id.c_str(), -1, SQLITE_TRANSIENT);
        while (sqlite3_step(runs.get()) == SQLITE_ROW) {
          source_runs.push_back({
              .id = text_column(runs.get(), 0),
              .analysis_version = text_column(runs.get(), 1),
              .status = text_column(runs.get(), 2),
              .completed_plies = sqlite3_column_int(runs.get(), 3),
          });
        }
      }

      for (const auto& source_run : source_runs) {
        std::optional<AnalysisRunCandidate> target_run;
        auto candidate = prepare(
            db_,
            "SELECT id,analysis_version,status,completed_plies "
            "FROM analysis_runs WHERE game_id=? AND analysis_version=? LIMIT 1;");
        sqlite3_bind_text(
            candidate.get(), 1, target_game_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(
            candidate.get(),
            2,
            source_run.analysis_version.c_str(),
            -1,
            SQLITE_TRANSIENT);
        if (sqlite3_step(candidate.get()) == SQLITE_ROW) {
          target_run = AnalysisRunCandidate{
              .id = text_column(candidate.get(), 0),
              .analysis_version = text_column(candidate.get(), 1),
              .status = text_column(candidate.get(), 2),
              .completed_plies = sqlite3_column_int(candidate.get(), 3),
          };
        }

        if (!target_run.has_value()) {
          auto move_run =
              prepare(db_, "UPDATE analysis_runs SET game_id=? WHERE id=?;");
          sqlite3_bind_text(
              move_run.get(), 1, target_game_id.c_str(), -1, SQLITE_TRANSIENT);
          sqlite3_bind_text(
              move_run.get(), 2, source_run.id.c_str(), -1, SQLITE_TRANSIENT);
          check(sqlite3_step(move_run.get()), db_, "move analysis to duplicate game");
          continue;
        }

        const bool source_complete = source_run.status == "complete";
        const bool target_complete = target_run->status == "complete";
        const bool source_is_better =
            (source_complete && !target_complete) ||
            (source_complete == target_complete &&
             source_run.completed_plies > target_run->completed_plies);

        if (source_is_better) {
          auto remove_target =
              prepare(db_, "DELETE FROM analysis_runs WHERE id=?;");
          sqlite3_bind_text(
              remove_target.get(), 1, target_run->id.c_str(), -1, SQLITE_TRANSIENT);
          check(sqlite3_step(remove_target.get()), db_, "replace duplicate analysis");
          auto move_run =
              prepare(db_, "UPDATE analysis_runs SET game_id=? WHERE id=?;");
          sqlite3_bind_text(
              move_run.get(), 1, target_game_id.c_str(), -1, SQLITE_TRANSIENT);
          sqlite3_bind_text(
              move_run.get(), 2, source_run.id.c_str(), -1, SQLITE_TRANSIENT);
          check(sqlite3_step(move_run.get()), db_, "preserve better analysis");
        } else {
          auto remove_source =
              prepare(db_, "DELETE FROM analysis_runs WHERE id=?;");
          sqlite3_bind_text(
              remove_source.get(), 1, source_run.id.c_str(), -1, SQLITE_TRANSIENT);
          check(sqlite3_step(remove_source.get()), db_, "remove duplicate analysis");
        }
      }

      // The target record is the canonical provider/local copy. Once favorites
      // and the best available analyses have been retained, the duplicate local
      // record can be removed safely together with its duplicate move rows.
      auto remove_source_game =
          prepare(db_, "DELETE FROM games WHERE id=? AND profile_id=?;");
      sqlite3_bind_text(
          remove_source_game.get(), 1, source_game_id.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(
          remove_source_game.get(), 2, source_profile_id.c_str(), -1, SQLITE_TRANSIENT);
      check(sqlite3_step(remove_source_game.get()), db_, "remove duplicate local game");
    }

    auto activate_target =
        prepare(db_, "UPDATE profiles SET last_opened_at=? WHERE id=?;");
    sqlite3_bind_int64(activate_target.get(), 1, unix_time_seconds());
    sqlite3_bind_text(
        activate_target.get(), 2, target_profile_id.c_str(), -1, SQLITE_TRANSIENT);
    check(sqlite3_step(activate_target.get()), db_, "activate merge target");

    auto save_active = prepare(
        db_,
        "INSERT INTO app_settings(key,value) VALUES('activeProfileId',?) "
        "ON CONFLICT(key) DO UPDATE SET value=excluded.value;");
    sqlite3_bind_text(
        save_active.get(), 1, target_profile_id.c_str(), -1, SQLITE_TRANSIENT);
    check(sqlite3_step(save_active.get()), db_, "save merge target");

    auto remove_profile = prepare(db_, "DELETE FROM profiles WHERE id=?;");
    sqlite3_bind_text(
        remove_profile.get(), 1, source_profile_id.c_str(), -1, SQLITE_TRANSIENT);
    check(sqlite3_step(remove_profile.get()), db_, "delete merged local profile");

    execute("COMMIT;");
  } catch (...) {
    sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
    throw;
  }
}

std::optional<std::string> Database::setting(const std::string& key) const {
  auto statement = prepare(db_, "SELECT value FROM app_settings WHERE key=?;");
  sqlite3_bind_text(statement.get(), 1, key.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(statement.get()) != SQLITE_ROW) {
    return std::nullopt;
  }
  return text_column(statement.get(), 0);
}

AppSettings Database::settings() const {
  AppSettings result;
  const auto legacy_arrows = setting("showBoardArrows").value_or("true");
  result.show_board_arrows = setting("showBestMoveArrow").value_or(legacy_arrows) == "true";
  result.show_threat_arrow = setting("showThreatArrow").value_or("true") == "true";
  result.show_evaluation_bar = setting("showEvaluationBar").value_or("true") == "true";
  result.show_engine_lines = setting("showEngineLines").value_or("true") == "true";
  result.show_classifications = setting("showClassifications").value_or("true") == "true";
  result.show_accuracy = setting("showAccuracy").value_or("true") == "true";
  result.show_theory = setting("showTheory").value_or("true") == "true";
  result.show_result_symbols = setting("showResultSymbols").value_or("true") == "true";
  result.adaptive_early_stop = setting("adaptiveEarlyStop").value_or("true") == "true";
  result.show_board_coordinates = setting("showBoardCoordinates").value_or("true") == "true";
  result.highlight_last_move = setting("highlightLastMove").value_or("true") == "true";
  result.highlight_selected_square = setting("highlightSelectedSquare").value_or("true") == "true";
  result.auto_sync_online = setting("autoSyncOnline").value_or("true") == "true";
  result.confirm_before_delete = setting("confirmBeforeDelete").value_or("true") == "true";
  result.use_global_analysis_cache = setting("useGlobalAnalysisCache").value_or("true") == "true";
  result.diagnostic_logging = setting("diagnosticLogging").value_or("true") == "true";
  result.theme_mode = setting("themeMode").value_or("system");
  result.locale = setting("locale").value_or("de");
  result.engine_id = std::string(normalize_stockfish_engine_id(
      setting("engineId").value_or(std::string(kStockfish18Id))));
  try {
    result.min_analysis_depth = std::clamp(
        std::stoi(setting("minAnalysisDepth").value_or("12")),
        AppSettings::min_depth, AppSettings::max_depth);
  } catch (...) {
    result.min_analysis_depth = 12;
  }
  const auto active_id = setting("activeProfileId");
  if (active_id.has_value()) {
    auto statement = prepare(
        db_,
        "SELECT depth,multi_pv,time_limit_seconds,threads,hash_mb FROM engine_settings WHERE profile_id=?;");
    sqlite3_bind_text(statement.get(), 1, active_id->c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(statement.get()) == SQLITE_ROW) {
      result.depth = std::clamp(
          sqlite3_column_int(statement.get(), 0),
          AppSettings::min_depth, AppSettings::max_depth);
      result.multi_pv = std::clamp(
          sqlite3_column_int(statement.get(), 1),
          AppSettings::min_multi_pv, AppSettings::max_multi_pv);
      result.time_limit_seconds = std::clamp(
          sqlite3_column_int(statement.get(), 2),
          AppSettings::min_time_limit_seconds, AppSettings::max_time_limit_seconds);
      result.threads = std::clamp(sqlite3_column_int(statement.get(), 3), kThreadsSetting.min_int, kThreadsSetting.max_int);
      result.hash_mb = std::clamp(sqlite3_column_int(statement.get(), 4), kHashMbSetting.min_int, kHashMbSetting.max_int);
      result.min_analysis_depth = std::min(result.min_analysis_depth, result.depth);
    }
  }
  // Side-line settings are app-level "last used" values.  On older
  // databases where these keys do not exist yet, start from the current main
  // engine settings instead of a hard-coded side-line preset.
  auto read_sideline_int = [this](const char* key, const int fallback,
                                  const int minimum, const int maximum) {
    try {
      return std::clamp(std::stoi(setting(key).value_or(std::to_string(fallback))),
                        minimum, maximum);
    } catch (...) {
      return fallback;
    }
  };
  result.sideline_depth = read_sideline_int(
      "sidelineDepth", result.depth, kDepthSetting.min_int, kDepthSetting.max_int);
  result.sideline_multi_pv = read_sideline_int(
      "sidelineMultiPv", result.multi_pv, kMultiPvSetting.min_int, kMultiPvSetting.max_int);
  result.sideline_threads = read_sideline_int(
      "sidelineThreads", result.threads, kThreadsSetting.min_int, kThreadsSetting.max_int);
  result.sideline_hash_mb = read_sideline_int(
      "sidelineHashMb", result.hash_mb, kHashMbSetting.min_int, kHashMbSetting.max_int);
  return result;
}

void Database::set_engine_settings(
    const int depth, const int multi_pv, const int time_limit_seconds) {
  const auto active = active_profile();
  if (!active.has_value()) throw std::runtime_error("No active profile");
  auto statement = prepare(
      db_,
      "INSERT INTO engine_settings(profile_id,preset,depth,multi_pv,threads,hash_mb,"
      "ponder,time_limit_seconds) VALUES(?,'custom',?,?,2,128,0,?) "
      "ON CONFLICT(profile_id) DO UPDATE SET preset='custom',depth=excluded.depth,"
      "multi_pv=excluded.multi_pv,time_limit_seconds=excluded.time_limit_seconds;");
  sqlite3_bind_text(statement.get(), 1, active->id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(statement.get(), 2, depth);
  sqlite3_bind_int(statement.get(), 3, multi_pv);
  sqlite3_bind_int(statement.get(), 4, time_limit_seconds);
  check(sqlite3_step(statement.get()), db_, "save engine settings");
}

void Database::set_engine_resources(const int threads, const int hash_mb) {
  const auto active = active_profile();
  if (!active.has_value()) throw std::runtime_error("No active profile");
  auto statement = prepare(db_,
      "UPDATE engine_settings SET threads=?,hash_mb=?,preset='custom' WHERE profile_id=?;");
  sqlite3_bind_int(statement.get(), 1, threads);
  sqlite3_bind_int(statement.get(), 2, hash_mb);
  sqlite3_bind_text(statement.get(), 3, active->id.c_str(), -1, SQLITE_TRANSIENT);
  check(sqlite3_step(statement.get()), db_, "save engine resources");
  if (sqlite3_changes(db_) == 0) {
    const auto current = settings();
    set_engine_settings(current.depth, current.multi_pv, current.time_limit_seconds);
    set_engine_resources(threads, hash_mb);
  }
}

void Database::set_setting(const std::string& key, const std::string& value) {
  auto statement = prepare(
      db_,
      "INSERT INTO app_settings(key,value) VALUES(?,?) "
      "ON CONFLICT(key) DO UPDATE SET value=excluded.value;");
  sqlite3_bind_text(statement.get(), 1, key.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(statement.get(), 2, value.c_str(), -1, SQLITE_TRANSIENT);
  check(sqlite3_step(statement.get()), db_, "save setting");
}

std::optional<ProviderCacheRecord> Database::provider_cache(
    const std::string& profile_id, const std::string& cache_key) const {
  std::string sql;
  if (cache_key == "profile") {
    sql = "SELECT payload_json,etag,last_modified,fetched_at,expires_at,"
          "normalization_version FROM provider_profiles_cache WHERE profile_id=?;";
  } else if (cache_key.starts_with("month:")) {
    sql = "SELECT payload_json,etag,last_modified,fetched_at,expires_at,"
          "normalization_version FROM provider_month_cache WHERE profile_id=? AND month=?;";
  } else {
    sql = "SELECT payload_json,etag,last_modified,fetched_at,expires_at,"
          "normalization_version FROM provider_stats_cache WHERE profile_id=? AND cache_key=?;";
  }
  auto statement = prepare(db_, sql.c_str());
  sqlite3_bind_text(statement.get(), 1, profile_id.c_str(), -1, SQLITE_TRANSIENT);
  if (cache_key != "profile") {
    const auto key = cache_key.starts_with("month:") ? cache_key.substr(6) : cache_key;
    sqlite3_bind_text(statement.get(), 2, key.c_str(), -1, SQLITE_TRANSIENT);
  }
  if (sqlite3_step(statement.get()) != SQLITE_ROW) return std::nullopt;
  return ProviderCacheRecord{
      .payload_json = text_column(statement.get(), 0),
      .validators = {
          .etag = optional_text_column(statement.get(), 1).value_or(""),
          .last_modified = optional_text_column(statement.get(), 2).value_or("")},
      .fetched_at = sqlite3_column_int64(statement.get(), 3),
      .expires_at = sqlite3_column_int64(statement.get(), 4),
      .normalization_version = sqlite3_column_int(statement.get(), 5),
  };
}

void Database::put_provider_cache(
    const std::string& profile_id,
    const std::string& cache_key,
    const std::string& payload_json,
    const ResponseCacheInfo& cache) {
  std::string sql;
  if (cache_key == "profile") {
    sql = "INSERT INTO provider_profiles_cache(profile_id,provider,username,payload_json,"
          "fetched_at,expires_at,etag,last_modified,normalization_version) "
          "SELECT id,type,provider_username,?,?,?,?,?,1 FROM profiles WHERE id=? "
          "ON CONFLICT(profile_id) DO UPDATE SET username=excluded.username,"
          "payload_json=excluded.payload_json,fetched_at=excluded.fetched_at,"
          "expires_at=excluded.expires_at,etag=excluded.etag,"
          "last_modified=excluded.last_modified,normalization_version=excluded.normalization_version;";
    auto statement = prepare(db_, sql.c_str());
    sqlite3_bind_text(statement.get(), 1, payload_json.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(statement.get(), 2, cache.fetched_at);
    sqlite3_bind_int64(statement.get(), 3, cache.expires_at);
    sqlite3_bind_text(statement.get(), 4, cache.etag.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement.get(), 5, cache.last_modified.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement.get(), 6, profile_id.c_str(), -1, SQLITE_TRANSIENT);
    check(sqlite3_step(statement.get()), db_, "cache provider profile");
    return;
  }
  if (cache_key.starts_with("month:")) {
    sql = "INSERT INTO provider_month_cache(profile_id,month,payload_json,fetched_at,"
          "expires_at,etag,last_modified,normalization_version) VALUES(?,?,?,?,?,?,?,1) "
          "ON CONFLICT(profile_id,month) DO UPDATE SET payload_json=excluded.payload_json,"
          "fetched_at=excluded.fetched_at,expires_at=excluded.expires_at,etag=excluded.etag,"
          "last_modified=excluded.last_modified,normalization_version=excluded.normalization_version;";
  } else {
    sql = "INSERT INTO provider_stats_cache(profile_id,cache_key,payload_json,expires_at,"
          "fetched_at,etag,last_modified,normalization_version) VALUES(?,?,?,?,?,?,?,1) "
          "ON CONFLICT(profile_id,cache_key) DO UPDATE SET payload_json=excluded.payload_json,"
          "expires_at=excluded.expires_at,fetched_at=excluded.fetched_at,etag=excluded.etag,"
          "last_modified=excluded.last_modified,normalization_version=excluded.normalization_version;";
  }
  auto statement = prepare(db_, sql.c_str());
  sqlite3_bind_text(statement.get(), 1, profile_id.c_str(), -1, SQLITE_TRANSIENT);
  const auto key = cache_key.starts_with("month:") ? cache_key.substr(6) : cache_key;
  sqlite3_bind_text(statement.get(), 2, key.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(statement.get(), 3, payload_json.c_str(), -1, SQLITE_TRANSIENT);
  if (cache_key.starts_with("month:")) {
    sqlite3_bind_int64(statement.get(), 4, cache.fetched_at);
    sqlite3_bind_int64(statement.get(), 5, cache.expires_at);
    sqlite3_bind_text(statement.get(), 6, cache.etag.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement.get(), 7, cache.last_modified.c_str(), -1, SQLITE_TRANSIENT);
  } else {
    sqlite3_bind_int64(statement.get(), 4, cache.expires_at);
    sqlite3_bind_int64(statement.get(), 5, cache.fetched_at);
    sqlite3_bind_text(statement.get(), 6, cache.etag.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement.get(), 7, cache.last_modified.c_str(), -1, SQLITE_TRANSIENT);
  }
  check(sqlite3_step(statement.get()), db_, "cache provider resource");
}

std::vector<std::string> Database::cached_months(const std::string& profile_id) const {
  auto statement = prepare(
      db_, "SELECT month FROM provider_month_cache WHERE profile_id=? ORDER BY month DESC;");
  sqlite3_bind_text(statement.get(), 1, profile_id.c_str(), -1, SQLITE_TRANSIENT);
  std::vector<std::string> result;
  while (sqlite3_step(statement.get()) == SQLITE_ROW) {
    result.push_back(text_column(statement.get(), 0));
  }
  return result;
}

std::string Database::import_pgn(const std::string& profile_id, const ParsedGame& game) {
  const std::string id = make_uuid();
  SqliteConnectionTransactionLock transaction_lock(db_);
  execute("BEGIN IMMEDIATE;");
  try {
    auto statement = prepare(
        db_,
        "INSERT INTO games(id,profile_id,white_name,black_name,result,pgn,created_at,"
        "kind,starting_fen,white_rating,black_rating,event,site,game_date,time_control,termination_type) "
        "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);");
    const auto white = tag_or(game.tags, "White", "White");
    const auto black = tag_or(game.tags, "Black", "Black");
    const auto result = tag_or(game.tags, "Result", "*");
    const auto white_rating = parse_optional_rating(game.tags, "WhiteElo");
    const auto black_rating = parse_optional_rating(game.tags, "BlackElo");
    const auto event = tag_or(game.tags, "Event", "");
    const auto site = tag_or(game.tags, "Site", "");
    const auto date = tag_or(game.tags, "Date", "");
    const auto time_control = tag_or(game.tags, "TimeControl", "");
    sqlite3_bind_text(statement.get(), 1, id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement.get(), 2, profile_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement.get(), 3, white.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement.get(), 4, black.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement.get(), 5, result.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement.get(), 6, game.raw_pgn.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(statement.get(), 7, unix_time_seconds());
    sqlite3_bind_text(statement.get(), 8, "pgn", -1, SQLITE_STATIC);
    sqlite3_bind_text(statement.get(), 9, game.initial_fen.c_str(), -1, SQLITE_TRANSIENT);
    bind_optional_int(statement.get(), 10, white_rating);
    bind_optional_int(statement.get(), 11, black_rating);
    sqlite3_bind_text(statement.get(), 12, event.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement.get(), 13, site.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement.get(), 14, date.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement.get(), 15, time_control.c_str(), -1, SQLITE_TRANSIENT);
    const auto termination = termination_bucket(game.raw_pgn, result);
    sqlite3_bind_text(statement.get(), 16, termination.c_str(), -1, SQLITE_TRANSIENT);
    check(sqlite3_step(statement.get()), db_, "insert PGN game");

    auto move_statement = prepare(
        db_,
        "INSERT INTO game_moves(game_id,ply_index,move_number,side_to_move,san,uci,"
        "fen_before,fen_after) VALUES(?,?,?,?,?,?,?,?);");
    for (const auto& move : game.moves) {
      sqlite3_reset(move_statement.get());
      sqlite3_clear_bindings(move_statement.get());
      sqlite3_bind_text(move_statement.get(), 1, id.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_int(move_statement.get(), 2, move.ply_index);
      sqlite3_bind_int(move_statement.get(), 3, move.move_number);
      sqlite3_bind_text(
          move_statement.get(), 4, move.side_to_move.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(move_statement.get(), 5, move.san.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(move_statement.get(), 6, move.uci.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(
          move_statement.get(), 7, move.fen_before.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(
          move_statement.get(), 8, move.fen_after.c_str(), -1, SQLITE_TRANSIENT);
      check(sqlite3_step(move_statement.get()), db_, "insert parsed move");
    }
    execute("COMMIT;");
  } catch (...) {
    sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
    throw;
  }
  return id;
}

std::string Database::import_fen(
    const std::string& profile_id,
    const std::string& fen,
    const std::string& display_name) {
  const std::string id = make_uuid();
  auto statement = prepare(
      db_,
      "INSERT INTO games(id,profile_id,white_name,black_name,result,pgn,created_at,"
      "kind,starting_fen,event) VALUES(?,?,?,?,?,'',?,'fen',?,?);");
  sqlite3_bind_text(statement.get(), 1, id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(statement.get(), 2, profile_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(statement.get(), 3, display_name.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(statement.get(), 4, "FEN", -1, SQLITE_STATIC);
  sqlite3_bind_text(statement.get(), 5, "*", -1, SQLITE_STATIC);
  sqlite3_bind_int64(statement.get(), 6, unix_time_seconds());
  sqlite3_bind_text(statement.get(), 7, fen.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(statement.get(), 8, display_name.c_str(), -1, SQLITE_TRANSIENT);
  check(sqlite3_step(statement.get()), db_, "insert FEN position");
  return id;
}

int Database::upsert_provider_games(
    const std::string& profile_id,
    const std::string& month,
    const std::vector<ProviderStoredGame>& games_to_store,
    const ResponseCacheInfo& cache) {
  int inserted = 0;
  SqliteConnectionTransactionLock transaction_lock(db_);
  execute("BEGIN IMMEDIATE;");
  try {
    for (const auto& stored : games_to_store) {
      const auto& remote = stored.provider_game;
      const auto& parsed = stored.parsed_game;
      std::string id;
      auto existing = prepare(
          db_, "SELECT id FROM games WHERE profile_id=? AND provider=? AND provider_game_id=?;");
      sqlite3_bind_text(existing.get(), 1, profile_id.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_int(existing.get(), 2, static_cast<int>(remote.provider));
      sqlite3_bind_text(
          existing.get(), 3, remote.provider_game_id.c_str(), -1, SQLITE_TRANSIENT);
      if (sqlite3_step(existing.get()) == SQLITE_ROW) {
        id = text_column(existing.get(), 0);
      } else {
        id = make_uuid();
        ++inserted;
      }
      auto statement = prepare(
          db_,
          "INSERT INTO games(id,profile_id,provider_game_id,white_name,black_name,result,pgn,"
          "created_at,kind,starting_fen,white_rating,black_rating,event,site,game_date,"
          "time_control,provider,provider_accuracy_white,provider_accuracy_black,"
          "provider_outcome,time_control_type,provider_ended_at,provider_rules,termination_type) "
          "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?) "
          "ON CONFLICT(profile_id,provider,provider_game_id) WHERE provider_game_id IS NOT NULL "
          "DO UPDATE SET white_name=excluded.white_name,black_name=excluded.black_name,"
          "result=excluded.result,pgn=excluded.pgn,white_rating=excluded.white_rating,"
          "black_rating=excluded.black_rating,event=excluded.event,site=excluded.site,"
          "game_date=excluded.game_date,time_control=excluded.time_control,"
          "provider_accuracy_white=excluded.provider_accuracy_white,"
          "provider_accuracy_black=excluded.provider_accuracy_black,"
          "provider_outcome=excluded.provider_outcome,time_control_type=excluded.time_control_type,"
          "provider_ended_at=excluded.provider_ended_at,provider_rules=excluded.provider_rules,"
          "termination_type=excluded.termination_type;");
      sqlite3_bind_text(statement.get(), 1, id.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(statement.get(), 2, profile_id.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(
          statement.get(), 3, remote.provider_game_id.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(statement.get(), 4, remote.white_username.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(statement.get(), 5, remote.black_username.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(statement.get(), 6, remote.result.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(statement.get(), 7, remote.pgn.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_int64(
          statement.get(), 8, remote.ended_at > 0 ? remote.ended_at : unix_time_seconds());
      sqlite3_bind_text(statement.get(), 9, "pgn", -1, SQLITE_STATIC);
      sqlite3_bind_text(
          statement.get(), 10, parsed.initial_fen.c_str(), -1, SQLITE_TRANSIENT);
      bind_optional_int(statement.get(), 11, remote.white_rating);
      bind_optional_int(statement.get(), 12, remote.black_rating);
      const auto event = remote.tournament.value_or(remote.match.value_or(
          tag_or(parsed.tags, "Event", "")));
      sqlite3_bind_text(statement.get(), 13, event.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(statement.get(), 14, remote.url.c_str(), -1, SQLITE_TRANSIENT);
      const auto date = tag_or(parsed.tags, "Date", "");
      sqlite3_bind_text(statement.get(), 15, date.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(
          statement.get(), 16, remote.time_control.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_int(statement.get(), 17, static_cast<int>(remote.provider));
      bind_optional_double(statement.get(), 18, remote.provider_accuracy_white);
      bind_optional_double(statement.get(), 19, remote.provider_accuracy_black);
      const auto outcome = provider_outcome_name(remote.profile_outcome);
      sqlite3_bind_text(statement.get(), 20, outcome.c_str(), -1, SQLITE_TRANSIENT);
      const auto time_type = time_control_name(remote.time_control_type);
      sqlite3_bind_text(statement.get(), 21, time_type.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_int64(statement.get(), 22, remote.ended_at);
      sqlite3_bind_text(statement.get(), 23, remote.rules.c_str(), -1, SQLITE_TRANSIENT);
      const auto termination = termination_bucket(remote.pgn, remote.result);
      sqlite3_bind_text(statement.get(), 24, termination.c_str(), -1, SQLITE_TRANSIENT);
      check(sqlite3_step(statement.get()), db_, "upsert provider game");

      auto source = prepare(
          db_, "INSERT INTO game_sources(game_id,provider,provider_url,etag,last_modified) "
               "VALUES(?,?,?,?,?) ON CONFLICT(game_id) DO UPDATE SET provider_url=excluded.provider_url,"
               "etag=excluded.etag,last_modified=excluded.last_modified;");
      sqlite3_bind_text(source.get(), 1, id.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_int(source.get(), 2, static_cast<int>(remote.provider));
      sqlite3_bind_text(source.get(), 3, remote.url.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(source.get(), 4, cache.etag.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(source.get(), 5, cache.last_modified.c_str(), -1, SQLITE_TRANSIENT);
      check(sqlite3_step(source.get()), db_, "upsert provider source");

      auto remove_moves = prepare(db_, "DELETE FROM game_moves WHERE game_id=?;");
      sqlite3_bind_text(remove_moves.get(), 1, id.c_str(), -1, SQLITE_TRANSIENT);
      check(sqlite3_step(remove_moves.get()), db_, "replace provider game moves");
      auto move_statement = prepare(
          db_, "INSERT INTO game_moves(game_id,ply_index,move_number,side_to_move,san,uci,"
               "fen_before,fen_after) VALUES(?,?,?,?,?,?,?,?);");
      for (const auto& move : parsed.moves) {
        sqlite3_reset(move_statement.get());
        sqlite3_clear_bindings(move_statement.get());
        sqlite3_bind_text(move_statement.get(), 1, id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(move_statement.get(), 2, move.ply_index);
        sqlite3_bind_int(move_statement.get(), 3, move.move_number);
        sqlite3_bind_text(
            move_statement.get(), 4, move.side_to_move.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(move_statement.get(), 5, move.san.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(move_statement.get(), 6, move.uci.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(
            move_statement.get(), 7, move.fen_before.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(
            move_statement.get(), 8, move.fen_after.c_str(), -1, SQLITE_TRANSIENT);
        check(sqlite3_step(move_statement.get()), db_, "insert provider parsed move");
      }
    }
    put_provider_cache(profile_id, "month:" + month, "{}", cache);
    execute("COMMIT;");
  } catch (...) {
    sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
    throw;
  }
  return inserted;
}

void Database::set_favorite(
    const std::string& /*profile_id*/, const std::string& game_id, const bool value) {
  auto game = prepare(db_, "SELECT profile_id FROM games WHERE id=?;");
  sqlite3_bind_text(game.get(), 1, game_id.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(game.get()) != SQLITE_ROW) throw std::runtime_error("Game not found");
  const auto owner_profile_id = text_column(game.get(), 0);

  if (value) {
    auto statement = prepare(
        db_,
        "INSERT OR IGNORE INTO favorites(profile_id,game_id,created_at,collection_id) "
        "VALUES(?,?,?,NULL);");
    sqlite3_bind_text(statement.get(), 1, owner_profile_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement.get(), 2, game_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(statement.get(), 3, unix_time_seconds());
    check(sqlite3_step(statement.get()), db_, "favorite game");
  } else {
    auto statement = prepare(db_, "DELETE FROM favorites WHERE game_id=?;");
    sqlite3_bind_text(statement.get(), 1, game_id.c_str(), -1, SQLITE_TRANSIENT);
    check(sqlite3_step(statement.get()), db_, "unfavorite game");
  }
}

std::vector<FavoriteCollectionRecord> Database::favorite_collections(
    const std::string& /*profile_id*/) const {
  auto statement = prepare(
      db_,
      "SELECT c.id,c.name,COUNT(f.game_id),c.created_at "
      "FROM favorite_collections c "
      "LEFT JOIN favorites f ON f.collection_id=c.id "
      "GROUP BY c.id,c.name,c.created_at "
      "ORDER BY c.created_at ASC,c.name COLLATE NOCASE ASC;");
  std::vector<FavoriteCollectionRecord> collections;
  while (sqlite3_step(statement.get()) == SQLITE_ROW) {
    FavoriteCollectionRecord collection;
    collection.id = text_column(statement.get(), 0);
    collection.profile_id.clear();
    collection.name = text_column(statement.get(), 1);
    collection.game_count = sqlite3_column_int(statement.get(), 2);
    collection.created_at = sqlite3_column_int64(statement.get(), 3);
    collections.push_back(std::move(collection));
  }
  return collections;
}

FavoriteCollectionRecord Database::create_favorite_collection(
    const std::string& /*profile_id*/, const std::string& name) {
  auto duplicate = prepare(
      db_, "SELECT 1 FROM favorite_collections WHERE name=? COLLATE NOCASE;");
  sqlite3_bind_text(duplicate.get(), 1, name.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(duplicate.get()) == SQLITE_ROW) {
    throw std::invalid_argument("A favorite collection with this name already exists");
  }

  FavoriteCollectionRecord collection;
  collection.id = make_uuid();
  collection.profile_id.clear();
  collection.name = name;
  collection.created_at = unix_time_seconds();
  auto statement = prepare(
      db_, "INSERT INTO favorite_collections(id,name,created_at) VALUES(?,?,?);");
  sqlite3_bind_text(statement.get(), 1, collection.id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(statement.get(), 2, collection.name.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(statement.get(), 3, collection.created_at);
  check(sqlite3_step(statement.get()), db_, "create favorite collection");
  return collection;
}

void Database::rename_favorite_collection(
    const std::string& /*profile_id*/,
    const std::string& collection_id,
    const std::string& name) {
  auto duplicate = prepare(
      db_,
      "SELECT 1 FROM favorite_collections WHERE id<>? AND name=? COLLATE NOCASE;");
  sqlite3_bind_text(duplicate.get(), 1, collection_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(duplicate.get(), 2, name.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(duplicate.get()) == SQLITE_ROW) {
    throw std::invalid_argument("A favorite collection with this name already exists");
  }

  auto statement = prepare(
      db_, "UPDATE favorite_collections SET name=? WHERE id=?;");
  sqlite3_bind_text(statement.get(), 1, name.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(statement.get(), 2, collection_id.c_str(), -1, SQLITE_TRANSIENT);
  check(sqlite3_step(statement.get()), db_, "rename favorite collection");
  if (sqlite3_changes(db_) == 0) throw std::runtime_error("Favorite collection not found");
}

void Database::delete_favorite_collection(
    const std::string& /*profile_id*/, const std::string& collection_id) {
  auto statement = prepare(db_, "DELETE FROM favorite_collections WHERE id=?;");
  sqlite3_bind_text(statement.get(), 1, collection_id.c_str(), -1, SQLITE_TRANSIENT);
  check(sqlite3_step(statement.get()), db_, "delete favorite collection");
  if (sqlite3_changes(db_) == 0) throw std::runtime_error("Favorite collection not found");
}

void Database::set_favorite_collection(
    const std::string& /*profile_id*/,
    const std::string& game_id,
    const std::optional<std::string>& collection_id) {
  auto game = prepare(db_, "SELECT profile_id FROM games WHERE id=?;");
  sqlite3_bind_text(game.get(), 1, game_id.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(game.get()) != SQLITE_ROW) throw std::runtime_error("Game not found");
  const auto owner_profile_id = text_column(game.get(), 0);

  if (collection_id.has_value()) {
    auto collection = prepare(db_, "SELECT 1 FROM favorite_collections WHERE id=?;");
    sqlite3_bind_text(collection.get(), 1, collection_id->c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(collection.get()) != SQLITE_ROW) {
      throw std::runtime_error("Favorite collection not found");
    }
    auto insert = prepare(
        db_,
        "INSERT OR IGNORE INTO favorites(profile_id,game_id,created_at,collection_id) "
        "VALUES(?,?,?,?);");
    sqlite3_bind_text(insert.get(), 1, owner_profile_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(insert.get(), 2, game_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(insert.get(), 3, unix_time_seconds());
    sqlite3_bind_text(insert.get(), 4, collection_id->c_str(), -1, SQLITE_TRANSIENT);
    check(sqlite3_step(insert.get()), db_, "favorite game for collection");

    auto update = prepare(db_, "UPDATE favorites SET collection_id=? WHERE game_id=?;");
    sqlite3_bind_text(update.get(), 1, collection_id->c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(update.get(), 2, game_id.c_str(), -1, SQLITE_TRANSIENT);
    check(sqlite3_step(update.get()), db_, "assign favorite collection");
    return;
  }

  auto update = prepare(db_, "UPDATE favorites SET collection_id=NULL WHERE game_id=?;");
  sqlite3_bind_text(update.get(), 1, game_id.c_str(), -1, SQLITE_TRANSIENT);
  check(sqlite3_step(update.get()), db_, "remove favorite from collection");
}

void Database::set_downloaded(
    const std::string& profile_id, const std::string& game_id, const bool value) {
  auto owned = prepare(db_, "SELECT profile_id FROM games WHERE id=? AND profile_id=?;");
  sqlite3_bind_text(owned.get(), 1, game_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(owned.get(), 2, profile_id.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(owned.get()) != SQLITE_ROW) throw std::runtime_error("Game not found");

  SqliteConnectionTransactionLock transaction_lock(db_);
  execute("BEGIN IMMEDIATE;");
  try {
    auto collection = prepare(
        db_,
        "SELECT id FROM favorite_collections WHERE name='Downloads' COLLATE NOCASE "
        "ORDER BY created_at ASC LIMIT 1;");
    std::optional<std::string> downloads_id;
    if (sqlite3_step(collection.get()) == SQLITE_ROW) {
      downloads_id = text_column(collection.get(), 0);
    }

    if (value) {
      if (!downloads_id.has_value()) {
        downloads_id = make_uuid();
        auto create = prepare(
            db_,
            "INSERT INTO favorite_collections(id,name,created_at) VALUES(?,?,?);");
        sqlite3_bind_text(create.get(), 1, downloads_id->c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(create.get(), 2, "Downloads", -1, SQLITE_STATIC);
        sqlite3_bind_int64(create.get(), 3, unix_time_seconds());
        check(sqlite3_step(create.get()), db_, "create Downloads collection");
      }

      auto favorite = prepare(
          db_,
          "INSERT OR IGNORE INTO favorites(profile_id,game_id,created_at,collection_id) "
          "VALUES(?,?,?,?);");
      sqlite3_bind_text(favorite.get(), 1, profile_id.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(favorite.get(), 2, game_id.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_int64(favorite.get(), 3, unix_time_seconds());
      sqlite3_bind_text(
          favorite.get(), 4, downloads_id->c_str(), -1, SQLITE_TRANSIENT);
      check(sqlite3_step(favorite.get()), db_, "favorite downloaded game");

      auto move = prepare(
          db_, "UPDATE favorites SET collection_id=? WHERE game_id=?;");
      sqlite3_bind_text(move.get(), 1, downloads_id->c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(move.get(), 2, game_id.c_str(), -1, SQLITE_TRANSIENT);
      check(sqlite3_step(move.get()), db_, "move game to Downloads collection");
    } else if (downloads_id.has_value()) {
      // Compatibility for older callers: removing the old download flag simply
      // moves the game out of Downloads into loose favorites. It never deletes
      // the favorite itself.
      auto move = prepare(
          db_,
          "UPDATE favorites SET collection_id=NULL WHERE game_id=? AND collection_id=?;");
      sqlite3_bind_text(move.get(), 1, game_id.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(move.get(), 2, downloads_id->c_str(), -1, SQLITE_TRANSIENT);
      check(sqlite3_step(move.get()), db_, "move download to loose favorites");
    }

    // Keep the obsolete table empty. New builds derive the compatibility
    // `downloaded` field from membership in the Downloads collection.
    auto legacy = prepare(db_, "DELETE FROM downloads WHERE game_id=?;");
    sqlite3_bind_text(legacy.get(), 1, game_id.c_str(), -1, SQLITE_TRANSIENT);
    check(sqlite3_step(legacy.get()), db_, "clear legacy download flag");
    execute("COMMIT;");
  } catch (...) {
    sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
    throw;
  }
}

std::vector<std::pair<std::string, std::string>> Database::games_needing_opening(
    const int limit) const {
  auto statement = limit > 0
      ? prepare(db_, "SELECT id,pgn FROM games WHERE opening_ply IS NULL LIMIT ?;")
      : prepare(db_, "SELECT id,pgn FROM games WHERE opening_ply IS NULL;");
  if (limit > 0) sqlite3_bind_int(statement.get(), 1, limit);
  std::vector<std::pair<std::string, std::string>> result;
  while (sqlite3_step(statement.get()) == SQLITE_ROW) {
    result.emplace_back(text_column(statement.get(), 0), text_column(statement.get(), 1));
  }
  return result;
}

void Database::set_game_opening(
    const std::string& game_id,
    const std::optional<std::string>& eco,
    const std::optional<std::string>& name,
    const int ply) {
  auto statement = prepare(
      db_, "UPDATE games SET opening_eco=?,opening_name=?,opening_ply=? WHERE id=?;");
  if (eco.has_value()) {
    sqlite3_bind_text(statement.get(), 1, eco->c_str(), -1, SQLITE_TRANSIENT);
  } else {
    sqlite3_bind_null(statement.get(), 1);
  }
  if (name.has_value()) {
    sqlite3_bind_text(statement.get(), 2, name->c_str(), -1, SQLITE_TRANSIENT);
  } else {
    sqlite3_bind_null(statement.get(), 2);
  }
  sqlite3_bind_int(statement.get(), 3, ply);
  sqlite3_bind_text(statement.get(), 4, game_id.c_str(), -1, SQLITE_TRANSIENT);
  check(sqlite3_step(statement.get()), db_, "update game opening");
}

std::vector<GameStatRow> Database::games_for_statistics(
    const std::string& profile_id) const {
  auto statement = prepare(
      db_,
      "SELECT provider_outcome,result,white_name,black_name,time_control_type,"
      "opening_eco,opening_name,id,opening_ply,"
      "EXISTS(SELECT 1 FROM analysis_runs r WHERE r.game_id=games.id "
      "AND r.status='complete' AND r.classifier_version>0) "
      "FROM games WHERE profile_id=? "
      "ORDER BY provider_ended_at DESC, created_at DESC;");
  sqlite3_bind_text(statement.get(), 1, profile_id.c_str(), -1, SQLITE_TRANSIENT);
  std::vector<GameStatRow> result;
  while (sqlite3_step(statement.get()) == SQLITE_ROW) {
    result.push_back(GameStatRow{
        .game_id = text_column(statement.get(), 7),
        .provider_outcome = text_column(statement.get(), 0),
        .result = text_column(statement.get(), 1),
        .white_name = text_column(statement.get(), 2),
        .black_name = text_column(statement.get(), 3),
        .time_control_type = text_column(statement.get(), 4),
        .opening_eco = text_column(statement.get(), 5),
        .opening_name = text_column(statement.get(), 6),
        .opening_ply = optional_int_column(statement.get(), 8),
        .analysed = sqlite3_column_int(statement.get(), 9) != 0,
    });
  }
  return result;
}

namespace {
// Read only the completed, classified run from the shared analysis cache.
constexpr const char* kLatestClassifiedRun =
    "(SELECT r.id FROM analysis_runs r WHERE r.game_id=g.id AND r.status='complete' "
    "AND r.classifier_version>0 ORDER BY r.completed_at DESC LIMIT 1)";
constexpr const char* kLatestCompletedRun =
    "(SELECT r.id FROM analysis_runs r WHERE r.game_id=g.id AND r.status='complete' "
    "ORDER BY r.completed_at DESC LIMIT 1)";
}  // namespace

std::vector<AccuracyGameRow> Database::accuracy_games_for_statistics(
    const std::string& profile_id) const {
  const std::string sql = std::string(
      "SELECT g.id,g.provider_outcome,g.result,g.white_name,g.black_name,"
      "g.time_control_type,"
      "CASE WHEN g.provider_ended_at>0 THEN g.provider_ended_at ELSE g.created_at END,"
      "r.white_local_accuracy,r.black_local_accuracy "
      "FROM games g JOIN analysis_runs r ON r.id=") + kLatestClassifiedRun
      + " WHERE g.profile_id=? "
      + "ORDER BY CASE WHEN g.provider_ended_at>0 THEN g.provider_ended_at "
      + "ELSE g.created_at END ASC, g.id ASC;";
  auto statement = prepare(db_, sql.c_str());
  sqlite3_bind_text(statement.get(), 1, profile_id.c_str(), -1, SQLITE_TRANSIENT);
  std::vector<AccuracyGameRow> rows;
  while (sqlite3_step(statement.get()) == SQLITE_ROW) {
    rows.push_back(AccuracyGameRow{
        .game_id = text_column(statement.get(), 0),
        .provider_outcome = text_column(statement.get(), 1),
        .result = text_column(statement.get(), 2),
        .white_name = text_column(statement.get(), 3),
        .black_name = text_column(statement.get(), 4),
        .time_control_type = text_column(statement.get(), 5),
        .ended_at = sqlite3_column_int64(statement.get(), 6),
        .white_accuracy = optional_double_column(statement.get(), 7),
        .black_accuracy = optional_double_column(statement.get(), 8),
    });
  }
  return rows;
}

std::vector<GameClockRow> Database::accuracy_game_clocks_for_statistics(
    const std::string& profile_id) const {
  const std::string sql = std::string(
      "SELECT g.id,g.pgn,COALESCE(g.time_control,''),"
      "(SELECT COUNT(*) FROM game_moves gm WHERE gm.game_id=g.id) "
      "FROM games g WHERE g.profile_id=? AND ") + kLatestClassifiedRun
      + " IS NOT NULL;";
  auto statement = prepare(db_, sql.c_str());
  sqlite3_bind_text(statement.get(), 1, profile_id.c_str(), -1, SQLITE_TRANSIENT);
  std::vector<GameClockRow> rows;
  while (sqlite3_step(statement.get()) == SQLITE_ROW) {
    rows.push_back(GameClockRow{
        .game_id = text_column(statement.get(), 0),
        .pgn = text_column(statement.get(), 1),
        .time_control = text_column(statement.get(), 2),
        .ply_count = sqlite3_column_int(statement.get(), 3),
    });
  }
  return rows;
}

std::vector<AccuracyMoveRow> Database::accuracy_moves_for_statistics(
    const std::string& profile_id) const {
  const std::string sql = std::string(
      "SELECT g.id,m.ply,m.category,m.is_theory,m.move_accuracy,m.accuracy_weight "
      "FROM games g JOIN move_analysis m ON m.analysis_run_id=") + kLatestClassifiedRun
      + " WHERE g.profile_id=? AND m.category<>'unknown' ORDER BY g.id,m.ply;";
  auto statement = prepare(db_, sql.c_str());
  sqlite3_bind_text(statement.get(), 1, profile_id.c_str(), -1, SQLITE_TRANSIENT);
  std::vector<AccuracyMoveRow> rows;
  while (sqlite3_step(statement.get()) == SQLITE_ROW) {
    rows.push_back(AccuracyMoveRow{
        .game_id = text_column(statement.get(), 0),
        .ply = sqlite3_column_int(statement.get(), 1),
        .category = text_column(statement.get(), 2),
        .theory = sqlite3_column_int(statement.get(), 3) != 0,
        .accuracy = optional_double_column(statement.get(), 4),
        .weight = optional_double_column(statement.get(), 5),
    });
  }
  return rows;
}

std::vector<CachedAccuracyBackfillRow> Database::statistics_games_missing_move_accuracy(
    const std::string& profile_id, const int limit) const {
  const std::string sql = std::string(
      "SELECT g.id,r.config_hash FROM games g JOIN analysis_runs r ON r.id=")
      + kLatestCompletedRun
      + " WHERE g.profile_id=? AND (COALESCE(r.classifier_version,0)<=0 OR "
        "EXISTS(SELECT 1 FROM move_analysis m "
        "WHERE m.analysis_run_id=r.id AND m.category<>'unknown' "
        "AND m.accuracy_weight IS NULL)) "
        "ORDER BY g.provider_ended_at DESC,g.created_at DESC LIMIT ?;";
  auto statement = prepare(db_, sql.c_str());
  sqlite3_bind_text(statement.get(), 1, profile_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(statement.get(), 2, limit);
  std::vector<CachedAccuracyBackfillRow> rows;
  while (sqlite3_step(statement.get()) == SQLITE_ROW) {
    rows.push_back({text_column(statement.get(), 0),
                    text_column(statement.get(), 1)});
  }
  return rows;
}

std::vector<GameMoveErrorRow> Database::move_errors_for_statistics(
    const std::string& profile_id, const int before_ply) const {
  // A game can hold several runs (other engine settings); only the latest
  // classified one speaks for it, so a re-analysis replaces old verdicts.
  auto statement = prepare(
      db_,
      "SELECT g.id,m.ply,m.category,gm.san,gm.fen_before,"
      "COALESCE(m.recommended_move,'') "
      "FROM games g "
      "JOIN analysis_runs r ON r.id=(SELECT id FROM analysis_runs "
      "  WHERE game_id=g.id AND status='complete' AND classifier_version>0 "
      "  ORDER BY completed_at DESC LIMIT 1) "
      "JOIN move_analysis m ON m.analysis_run_id=r.id AND m.ply<? "
      "  AND m.category IN ('miss','mistake','blunder') "
      "JOIN game_moves gm ON gm.game_id=g.id AND gm.ply_index=m.ply "
      "WHERE g.profile_id=? ORDER BY g.id,m.ply;");
  sqlite3_bind_int(statement.get(), 1, before_ply);
  sqlite3_bind_text(statement.get(), 2, profile_id.c_str(), -1, SQLITE_TRANSIENT);
  std::vector<GameMoveErrorRow> result;
  while (sqlite3_step(statement.get()) == SQLITE_ROW) {
    result.push_back(GameMoveErrorRow{
        .game_id = text_column(statement.get(), 0),
        .ply = sqlite3_column_int(statement.get(), 1),
        .category = text_column(statement.get(), 2),
        .san = text_column(statement.get(), 3),
        .fen_before = text_column(statement.get(), 4),
        .recommended_move = text_column(statement.get(), 5),
    });
  }
  return result;
}

std::vector<ProfileGameMetadataRow> Database::profile_games_metadata(
    const std::string& profile_id) const {
  // One cheap sweep over the library. Existing analysis summaries are reused;
  // no PGN text, engine lines or new engine work are requested here.
  const std::string sql = std::string(
      "SELECT g.id,"
      "CASE WHEN g.provider_ended_at>0 THEN g.provider_ended_at ELSE g.created_at END,"
      "CASE "
      "WHEN lower(trim(g.white_name))=lower(trim(COALESCE(NULLIF(p.provider_username,''),p.display_name))) THEN 'white' "
      "WHEN lower(trim(g.black_name))=lower(trim(COALESCE(NULLIF(p.provider_username,''),p.display_name))) THEN 'black' "
      "ELSE 'unknown' END,"
      "g.provider_outcome,g.time_control_type,COALESCE(g.opening_eco,''),COALESCE(g.opening_name,''),"
      "COALESCE(g.termination_type,'unknown'),"
      "CASE "
      "WHEN lower(trim(g.white_name))=lower(trim(COALESCE(NULLIF(p.provider_username,''),p.display_name))) THEN g.white_rating "
      "WHEN lower(trim(g.black_name))=lower(trim(COALESCE(NULLIF(p.provider_username,''),p.display_name))) THEN g.black_rating END,"
      "CASE "
      "WHEN lower(trim(g.white_name))=lower(trim(COALESCE(NULLIF(p.provider_username,''),p.display_name))) THEN g.black_rating "
      "WHEN lower(trim(g.black_name))=lower(trim(COALESCE(NULLIF(p.provider_username,''),p.display_name))) THEN g.white_rating END,"
      "COALESCE(ar.total_plies,(SELECT COUNT(*) FROM game_moves all_moves WHERE all_moves.game_id=g.id),0),"
      "CASE WHEN ar.id IS NULL THEN 0 ELSE 1 END,"
      "CASE "
      "WHEN lower(trim(g.white_name))=lower(trim(COALESCE(NULLIF(p.provider_username,''),p.display_name))) "
      "THEN COALESCE(ar.white_local_accuracy,g.provider_accuracy_white) "
      "WHEN lower(trim(g.black_name))=lower(trim(COALESCE(NULLIF(p.provider_username,''),p.display_name))) "
      "THEN COALESCE(ar.black_local_accuracy,g.provider_accuracy_black) END,"
      "SUM(CASE WHEN ma.category='miss' AND gm.side_to_move=(CASE "
      "WHEN lower(trim(g.white_name))=lower(trim(COALESCE(NULLIF(p.provider_username,''),p.display_name))) THEN 'white' "
      "WHEN lower(trim(g.black_name))=lower(trim(COALESCE(NULLIF(p.provider_username,''),p.display_name))) THEN 'black' ELSE '' END) THEN 1 ELSE 0 END),"
      "SUM(CASE WHEN ma.category='mistake' AND gm.side_to_move=(CASE "
      "WHEN lower(trim(g.white_name))=lower(trim(COALESCE(NULLIF(p.provider_username,''),p.display_name))) THEN 'white' "
      "WHEN lower(trim(g.black_name))=lower(trim(COALESCE(NULLIF(p.provider_username,''),p.display_name))) THEN 'black' ELSE '' END) THEN 1 ELSE 0 END),"
      "SUM(CASE WHEN ma.category='blunder' AND gm.side_to_move=(CASE "
      "WHEN lower(trim(g.white_name))=lower(trim(COALESCE(NULLIF(p.provider_username,''),p.display_name))) THEN 'white' "
      "WHEN lower(trim(g.black_name))=lower(trim(COALESCE(NULLIF(p.provider_username,''),p.display_name))) THEN 'black' ELSE '' END) THEN 1 ELSE 0 END) "
      "FROM games g JOIN profiles p ON p.id=g.profile_id "
      "LEFT JOIN analysis_runs ar ON ar.id=(SELECT a.id FROM analysis_runs a "
      "WHERE a.game_id=g.id AND a.status='complete' "
      "ORDER BY COALESCE(a.completed_at,0) DESC,a.started_at DESC LIMIT 1) "
      "LEFT JOIN move_analysis ma ON ma.analysis_run_id=ar.id "
      "LEFT JOIN game_moves gm ON gm.game_id=g.id AND gm.ply_index=ma.ply "
      "WHERE ") + kPlayerProfileGameScopeSql +
      " GROUP BY g.id "
      "ORDER BY CASE WHEN g.provider_ended_at>0 THEN g.provider_ended_at ELSE g.created_at END DESC;";
  auto statement = prepare(db_, sql.c_str());
  sqlite3_bind_text(statement.get(), 1, profile_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(statement.get(), 2, profile_id.c_str(), -1, SQLITE_TRANSIENT);

  std::vector<ProfileGameMetadataRow> result;
  while (sqlite3_step(statement.get()) == SQLITE_ROW) {
    result.push_back(ProfileGameMetadataRow{
        .game_id = text_column(statement.get(), 0),
        .played_at = sqlite3_column_int64(statement.get(), 1),
        .player_color = text_column(statement.get(), 2),
        .provider_outcome = text_column(statement.get(), 3),
        .time_control_type = text_column(statement.get(), 4),
        .opening_eco = text_column(statement.get(), 5),
        .opening_name = text_column(statement.get(), 6),
        .termination_type = text_column(statement.get(), 7),
        .player_rating = optional_int_column(statement.get(), 8),
        .opponent_rating = optional_int_column(statement.get(), 9),
        .plies = sqlite3_column_int(statement.get(), 10),
        .has_complete_analysis = sqlite3_column_int(statement.get(), 11) != 0,
        .accuracy = optional_double_column(statement.get(), 12),
        .miss_count = sqlite3_column_int(statement.get(), 13),
        .mistake_count = sqlite3_column_int(statement.get(), 14),
        .blunder_count = sqlite3_column_int(statement.get(), 15),
    });
  }
  return result;
}

std::vector<GamePhaseRow> Database::games_for_phases(
    const std::string& profile_id) const {
  auto statement = prepare(
      db_,
      "SELECT provider_outcome,result,white_name,black_name,"
      "(SELECT MAX(ply_index) FROM game_moves m WHERE m.game_id=games.id) "
      "FROM games WHERE profile_id=?;");
  sqlite3_bind_text(statement.get(), 1, profile_id.c_str(), -1, SQLITE_TRANSIENT);
  std::vector<GamePhaseRow> result;
  while (sqlite3_step(statement.get()) == SQLITE_ROW) {
    const int max_ply = sqlite3_column_type(statement.get(), 4) == SQLITE_NULL
        ? -1
        : sqlite3_column_int(statement.get(), 4);
    result.push_back(GamePhaseRow{
        .provider_outcome = text_column(statement.get(), 0),
        .result = text_column(statement.get(), 1),
        .white_name = text_column(statement.get(), 2),
        .black_name = text_column(statement.get(), 3),
        .max_ply = max_ply,
    });
  }
  return result;
}


PlayerLearningStats Database::player_learning_stats(
    const std::string& profile_id) const {
  PlayerLearningStats result;

  const std::string overview_sql = std::string(
      "SELECT count(*),"
      "CAST(round(avg(CASE "
      "WHEN lower(trim(g.white_name))=lower(trim(COALESCE(NULLIF(p.provider_username,''),p.display_name))) "
      "THEN g.white_rating "
      "WHEN lower(trim(g.black_name))=lower(trim(COALESCE(NULLIF(p.provider_username,''),p.display_name))) "
      "THEN g.black_rating END)) AS INTEGER),"
      "avg(CASE "
      "WHEN lower(trim(g.white_name))=lower(trim(COALESCE(NULLIF(p.provider_username,''),p.display_name))) "
      "THEN (SELECT ar.white_local_accuracy FROM analysis_runs ar "
      "WHERE ar.game_id=g.id AND ar.status='complete' AND ar.white_local_accuracy IS NOT NULL "
      "ORDER BY COALESCE(ar.completed_at,0) DESC,ar.started_at DESC LIMIT 1) "
      "WHEN lower(trim(g.black_name))=lower(trim(COALESCE(NULLIF(p.provider_username,''),p.display_name))) "
      "THEN (SELECT ar.black_local_accuracy FROM analysis_runs ar "
      "WHERE ar.game_id=g.id AND ar.status='complete' AND ar.black_local_accuracy IS NOT NULL "
      "ORDER BY COALESCE(ar.completed_at,0) DESC,ar.started_at DESC LIMIT 1) END) "
      "FROM games g JOIN profiles p ON p.id=g.profile_id WHERE ") +
      kPlayerProfileGameScopeSql + ";";
  auto overview = prepare(db_, overview_sql.c_str());
  sqlite3_bind_text(overview.get(), 1, profile_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(overview.get(), 2, profile_id.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(overview.get()) == SQLITE_ROW) {
    result.games = sqlite3_column_int(overview.get(), 0);
    result.average_rating = optional_int_column(overview.get(), 1);
    result.average_accuracy = optional_double_column(overview.get(), 2);
  }

  const std::string categories_sql = std::string(
      "SELECT ma.category,count(*) FROM move_analysis ma "
      "JOIN analysis_runs ar ON ar.id=ma.analysis_run_id AND ar.status='complete' "
      "JOIN games g ON g.id=ar.game_id "
      "JOIN profiles p ON p.id=g.profile_id "
      "JOIN game_moves gm ON gm.game_id=g.id AND gm.ply_index=ma.ply "
      "WHERE ") + kPlayerProfileGameScopeSql +
      " AND ma.classifier_version IS NOT NULL AND ("
      "(lower(trim(g.white_name))=lower(trim(COALESCE(NULLIF(p.provider_username,''),p.display_name))) "
      "AND gm.side_to_move='white') OR "
      "(lower(trim(g.black_name))=lower(trim(COALESCE(NULLIF(p.provider_username,''),p.display_name))) "
      "AND gm.side_to_move='black')) "
      "GROUP BY ma.category;";
  auto categories = prepare(db_, categories_sql.c_str());
  sqlite3_bind_text(categories.get(), 1, profile_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(categories.get(), 2, profile_id.c_str(), -1, SQLITE_TRANSIENT);
  while (sqlite3_step(categories.get()) == SQLITE_ROW) {
    const auto category = parse_category(text_column(categories.get(), 0));
    const int count = sqlite3_column_int(categories.get(), 1);
    result.analyzed_moves += count;
    switch (category) {
      case MoveCategory::theory: result.theory += count; break;
      case MoveCategory::brilliant: result.brilliant += count; break;
      case MoveCategory::critical: result.critical += count; break;
      case MoveCategory::best: result.best += count; break;
      case MoveCategory::excellent: result.excellent += count; break;
      case MoveCategory::miss: result.miss += count; break;
      case MoveCategory::mistake: result.mistake += count; break;
      case MoveCategory::blunder: result.blunder += count; break;
      default: break;
    }
  }

  const std::string openings_sql = std::string(
      "SELECT COALESCE(g.opening_eco,''),COALESCE(g.opening_name,''),count(*),"
      "sum(CASE WHEN provider_outcome='win' THEN 1 ELSE 0 END),"
      "sum(CASE WHEN provider_outcome='draw' THEN 1 ELSE 0 END),"
      "sum(CASE WHEN provider_outcome='loss' THEN 1 ELSE 0 END) "
      "FROM games g JOIN profiles p ON p.id=g.profile_id WHERE ") +
      kPlayerProfileGameScopeSql +
      " AND (COALESCE(g.opening_eco,'')<>'' OR COALESCE(g.opening_name,'')<>'') "
      "GROUP BY g.opening_eco,g.opening_name ORDER BY count(*) DESC LIMIT 8;";
  auto openings = prepare(db_, openings_sql.c_str());
  sqlite3_bind_text(openings.get(), 1, profile_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(openings.get(), 2, profile_id.c_str(), -1, SQLITE_TRANSIENT);
  while (sqlite3_step(openings.get()) == SQLITE_ROW) {
    result.openings.push_back(PlayerOpeningLearningStat{
        .eco = text_column(openings.get(), 0),
        .name = text_column(openings.get(), 1),
        .games = sqlite3_column_int(openings.get(), 2),
        .wins = sqlite3_column_int(openings.get(), 3),
        .draws = sqlite3_column_int(openings.get(), 4),
        .losses = sqlite3_column_int(openings.get(), 5),
    });
  }
  return result;
}


// -----------------------------------------------------------------------------
// Section: AI player-profile source evidence and queue
// -----------------------------------------------------------------------------

namespace {

std::vector<PlayerProfileGameSourceRow> read_player_profile_game_sources(
    sqlite3* db, const std::string& profile_id,
    const std::optional<std::string>& game_id) {
  std::vector<PlayerProfileGameSourceRow> result;
  const std::string sql = std::string(
      "SELECT g.id,"
      "COALESCE(NULLIF(g.provider_ended_at,0),g.created_at),"
      "CASE WHEN ar.completed_at IS NOT NULL THEN ar.completed_at "
      "ELSE COALESCE(NULLIF(g.provider_ended_at,0),g.created_at) END,"
      "COALESCE(g.time_control_type,'unknown'),g.result,COALESCE(g.provider_outcome,'unknown'),"
      "CASE WHEN lower(trim(g.white_name))=lower(trim(COALESCE(NULLIF(p.provider_username,''),p.display_name))) "
      "THEN 'white' WHEN lower(trim(g.black_name))=lower(trim(COALESCE(NULLIF(p.provider_username,''),p.display_name))) "
      "THEN 'black' ELSE 'unknown' END,"
      "CASE WHEN lower(trim(g.white_name))=lower(trim(COALESCE(NULLIF(p.provider_username,''),p.display_name))) "
      "THEN g.white_rating WHEN lower(trim(g.black_name))=lower(trim(COALESCE(NULLIF(p.provider_username,''),p.display_name))) "
      "THEN g.black_rating END,"
      "CASE WHEN lower(trim(g.white_name))=lower(trim(COALESCE(NULLIF(p.provider_username,''),p.display_name))) "
      "THEN g.black_rating WHEN lower(trim(g.black_name))=lower(trim(COALESCE(NULLIF(p.provider_username,''),p.display_name))) "
      "THEN g.white_rating END,"
      "CASE WHEN lower(trim(g.white_name))=lower(trim(COALESCE(NULLIF(p.provider_username,''),p.display_name))) "
      "THEN COALESCE(ar.white_local_accuracy,g.provider_accuracy_white) "
      "WHEN lower(trim(g.black_name))=lower(trim(COALESCE(NULLIF(p.provider_username,''),p.display_name))) "
      "THEN COALESCE(ar.black_local_accuracy,g.provider_accuracy_black) END,"
      "COALESCE(g.opening_eco,''),COALESCE(g.opening_name,''),"
      "(SELECT count(*) FROM game_moves all_moves WHERE all_moves.game_id=g.id),"
      "CASE WHEN ar.id IS NULL THEN 0 ELSE 1 END,"
      "count(ma.ply),"
      "sum(CASE WHEN ma.category='theory' THEN 1 ELSE 0 END),"
      "sum(CASE WHEN ma.category='brilliant' THEN 1 ELSE 0 END),"
      "sum(CASE WHEN ma.category='critical' THEN 1 ELSE 0 END),"
      "sum(CASE WHEN ma.category='best' THEN 1 ELSE 0 END),"
      "sum(CASE WHEN ma.category='excellent' THEN 1 ELSE 0 END),"
      "sum(CASE WHEN ma.category='miss' THEN 1 ELSE 0 END),"
      "sum(CASE WHEN ma.category='mistake' THEN 1 ELSE 0 END),"
      "sum(CASE WHEN ma.category='blunder' THEN 1 ELSE 0 END),"
      "sum(CASE WHEN ma.ply IS NOT NULL AND gm.ply_index/2+1<=12 THEN 1 ELSE 0 END),"
      "sum(CASE WHEN gm.ply_index/2+1<=12 AND ma.category='miss' THEN 1 ELSE 0 END),"
      "sum(CASE WHEN gm.ply_index/2+1<=12 AND ma.category='mistake' THEN 1 ELSE 0 END),"
      "sum(CASE WHEN gm.ply_index/2+1<=12 AND ma.category='blunder' THEN 1 ELSE 0 END),"
      "sum(CASE WHEN ma.ply IS NOT NULL AND gm.ply_index/2+1 BETWEEN 13 AND 30 THEN 1 ELSE 0 END),"
      "sum(CASE WHEN gm.ply_index/2+1 BETWEEN 13 AND 30 AND ma.category='miss' THEN 1 ELSE 0 END),"
      "sum(CASE WHEN gm.ply_index/2+1 BETWEEN 13 AND 30 AND ma.category='mistake' THEN 1 ELSE 0 END),"
      "sum(CASE WHEN gm.ply_index/2+1 BETWEEN 13 AND 30 AND ma.category='blunder' THEN 1 ELSE 0 END),"
      "sum(CASE WHEN ma.ply IS NOT NULL AND gm.ply_index/2+1>=31 THEN 1 ELSE 0 END),"
      "sum(CASE WHEN gm.ply_index/2+1>=31 AND ma.category='miss' THEN 1 ELSE 0 END),"
      "sum(CASE WHEN gm.ply_index/2+1>=31 AND ma.category='mistake' THEN 1 ELSE 0 END),"
      "sum(CASE WHEN gm.ply_index/2+1>=31 AND ma.category='blunder' THEN 1 ELSE 0 END),"
      "avg(ma.expected_score_loss),max(ma.expected_score_loss),g.profile_id,COALESCE(g.termination_type,'unknown') "
      "FROM games g JOIN profiles p ON p.id=g.profile_id "
      "LEFT JOIN analysis_runs ar ON ar.id=(SELECT candidate.id FROM analysis_runs candidate "
      "WHERE candidate.game_id=g.id AND candidate.status='complete' "
      "ORDER BY candidate.depth DESC,COALESCE(candidate.completed_at,0) DESC LIMIT 1) "
      "LEFT JOIN game_moves gm ON gm.game_id=g.id AND (("
      "lower(trim(g.white_name))=lower(trim(COALESCE(NULLIF(p.provider_username,''),p.display_name))) "
      "AND gm.side_to_move='white') OR ("
      "lower(trim(g.black_name))=lower(trim(COALESCE(NULLIF(p.provider_username,''),p.display_name))) "
      "AND gm.side_to_move='black')) "
      "LEFT JOIN move_analysis ma ON ma.analysis_run_id=ar.id AND ma.ply=gm.ply_index "
      "AND ma.classifier_version IS NOT NULL "
      "WHERE ") + kPlayerProfileGameScopeSql +
      (game_id.has_value() ? " AND g.id=? " : " ") +
      "GROUP BY g.id "
      "ORDER BY COALESCE(NULLIF(g.provider_ended_at,0),g.created_at) DESC;";
  auto statement = prepare(db, sql.c_str());
  sqlite3_bind_text(statement.get(), 1, profile_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(statement.get(), 2, profile_id.c_str(), -1, SQLITE_TRANSIENT);
  if (game_id.has_value()) {
    sqlite3_bind_text(statement.get(), 3, game_id->c_str(), -1, SQLITE_TRANSIENT);
  }
  while (sqlite3_step(statement.get()) == SQLITE_ROW) {
    PlayerProfileGameSourceRow row;
    row.game_id = text_column(statement.get(), 0);
    row.played_at = sqlite3_column_int64(statement.get(), 1);
    row.source_version = sqlite3_column_int64(statement.get(), 2);
    row.time_control_type = text_column(statement.get(), 3);
    row.result = text_column(statement.get(), 4);
    row.provider_outcome = text_column(statement.get(), 5);
    row.player_color = text_column(statement.get(), 6);
    row.player_rating = optional_int_column(statement.get(), 7);
    row.opponent_rating = optional_int_column(statement.get(), 8);
    row.accuracy = optional_double_column(statement.get(), 9);
    row.opening_eco = text_column(statement.get(), 10);
    row.opening_name = text_column(statement.get(), 11);
    row.move_count = sqlite3_column_int(statement.get(), 12);
    row.analysis_complete = sqlite3_column_int(statement.get(), 13) != 0;
    row.analyzed_moves = sqlite3_column_int(statement.get(), 14);
    row.theory = sqlite3_column_int(statement.get(), 15);
    row.brilliant = sqlite3_column_int(statement.get(), 16);
    row.critical = sqlite3_column_int(statement.get(), 17);
    row.best = sqlite3_column_int(statement.get(), 18);
    row.excellent = sqlite3_column_int(statement.get(), 19);
    row.miss = sqlite3_column_int(statement.get(), 20);
    row.mistake = sqlite3_column_int(statement.get(), 21);
    row.blunder = sqlite3_column_int(statement.get(), 22);
    row.opening_analyzed_moves = sqlite3_column_int(statement.get(), 23);
    row.opening_miss = sqlite3_column_int(statement.get(), 24);
    row.opening_mistake = sqlite3_column_int(statement.get(), 25);
    row.opening_blunder = sqlite3_column_int(statement.get(), 26);
    row.middlegame_analyzed_moves = sqlite3_column_int(statement.get(), 27);
    row.middlegame_miss = sqlite3_column_int(statement.get(), 28);
    row.middlegame_mistake = sqlite3_column_int(statement.get(), 29);
    row.middlegame_blunder = sqlite3_column_int(statement.get(), 30);
    row.endgame_analyzed_moves = sqlite3_column_int(statement.get(), 31);
    row.endgame_miss = sqlite3_column_int(statement.get(), 32);
    row.endgame_mistake = sqlite3_column_int(statement.get(), 33);
    row.endgame_blunder = sqlite3_column_int(statement.get(), 34);
    row.average_expected_score_loss = optional_double_column(statement.get(), 35);
    row.maximum_expected_score_loss = optional_double_column(statement.get(), 36);
    row.source_profile_id = text_column(statement.get(), 37);
    row.termination_type = text_column(statement.get(), 38);
    result.push_back(std::move(row));
  }
  return result;
}


}  // namespace

std::vector<PlayerProfileGameSourceRow> Database::player_profile_game_sources(
    const std::string& profile_id) const {
  return read_player_profile_game_sources(db_, profile_id, std::nullopt);
}

std::uint64_t Database::statistics_source_revision() const noexcept {
  return statistics_source_revision_.load(std::memory_order_relaxed);
}

std::optional<PlayerProfileGameSourceRow> Database::player_profile_game_source(
    const std::string& profile_id, const std::string& game_id) const {
  auto rows = read_player_profile_game_sources(db_, profile_id, game_id);
  if (rows.empty()) return std::nullopt;
  return std::move(rows.front());
}

std::vector<PlayerProfileMoveSourceRow> Database::player_profile_move_sources(
    const std::string& game_id, const std::string& player_color) const {
  if (player_color != "white" && player_color != "black") return {};
  std::vector<PlayerProfileMoveSourceRow> result;
  auto statement = prepare(
      db_,
      "SELECT gm.ply_index,ma.category,ma.expected_score_before,ma.expected_score_best,"
      "ma.expected_score_played,ma.expected_score_loss,ma.is_theory,gm.fen_before,gm.uci "
      "FROM game_moves gm "
      "JOIN analysis_runs ar ON ar.id=(SELECT candidate.id FROM analysis_runs candidate "
      "WHERE candidate.game_id=gm.game_id AND candidate.status='complete' "
      "ORDER BY candidate.depth DESC,COALESCE(candidate.completed_at,0) DESC LIMIT 1) "
      "JOIN move_analysis ma ON ma.analysis_run_id=ar.id AND ma.ply=gm.ply_index "
      "WHERE gm.game_id=? AND gm.side_to_move=? AND ma.classifier_version IS NOT NULL "
      "ORDER BY gm.ply_index ASC;");
  sqlite3_bind_text(statement.get(), 1, game_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(statement.get(), 2, player_color.c_str(), -1, SQLITE_TRANSIENT);
  while (sqlite3_step(statement.get()) == SQLITE_ROW) {
    result.push_back(PlayerProfileMoveSourceRow{
        .ply = sqlite3_column_int(statement.get(), 0),
        .classification = text_column(statement.get(), 1),
        .expected_score_before = optional_double_column(statement.get(), 2),
        .expected_score_best = optional_double_column(statement.get(), 3),
        .expected_score_played = optional_double_column(statement.get(), 4),
        .expected_score_loss = optional_double_column(statement.get(), 5),
        .theory = sqlite3_column_int(statement.get(), 6) != 0,
        .fen_before = text_column(statement.get(), 7),
        .uci = text_column(statement.get(), 8),
    });
  }
  return result;
}



void Database::sync_ai_profile_evidence_registry(const std::string& profile_id) {
  SqliteConnectionTransactionLock transaction_lock(db_);
  execute("BEGIN IMMEDIATE;");
  try {
    // Stage-1 metadata is always available for a persisted game. Keep only a
    // locator; the row deliberately does not copy the metadata payload.
    const std::string metadata_sql = std::string(
        "INSERT INTO ai_profile_evidence_registry("
        "profile_id,evidence_id,source_kind,source_table,source_key,game_id,ply,"
        "evidence_tier,evidence_level,source_version,updated_at) "
        "SELECT ?,('game:'||g.id||':metadata'),'game_metadata','games',g.id,g.id,NULL,"
        "'metadata',0,COALESCE(NULLIF(g.provider_ended_at,0),g.created_at),"
        "COALESCE(NULLIF(g.provider_ended_at,0),g.created_at) "
        "FROM games g JOIN profiles p ON p.id=g.profile_id WHERE ") +
        kPlayerProfileGameScopeSql +
        " "
        "ON CONFLICT(profile_id,evidence_id) DO UPDATE SET "
        "source_key=excluded.source_key,source_version=excluded.source_version,"
        "updated_at=excluded.updated_at;";
    auto metadata = prepare(db_, metadata_sql.c_str());
    sqlite3_bind_text(metadata.get(), 1, profile_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(metadata.get(), 2, profile_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(metadata.get(), 3, profile_id.c_str(), -1, SQLITE_TRANSIENT);
    check(sqlite3_step(metadata.get()), db_, "index profile metadata evidence");

    // Statistics remain computed from the existing library/analysis data. A
    // single profile-level locator tells the graph that aggregate statistics
    // are available without persisting a second copy of those values.
    const std::string statistics_sql = std::string(
        "INSERT INTO ai_profile_evidence_registry("
        "profile_id,evidence_id,source_kind,source_table,source_key,game_id,ply,"
        "evidence_tier,evidence_level,source_version,updated_at) "
        "SELECT ?,('profile:'||?||':statistics'),'statistics_aggregate','games',?,"
        "NULL,NULL,'aggregate',1,"
        "COALESCE(MAX(COALESCE(NULLIF(g.provider_ended_at,0),g.created_at)),0),"
        "COALESCE(MAX(COALESCE(NULLIF(g.provider_ended_at,0),g.created_at)),0) "
        "FROM games g JOIN profiles p ON p.id=g.profile_id WHERE ") +
        kPlayerProfileGameScopeSql +
        " "
        "ON CONFLICT(profile_id,evidence_id) DO UPDATE SET "
        "source_version=excluded.source_version,updated_at=excluded.updated_at;";
    auto statistics = prepare(db_, statistics_sql.c_str());
    sqlite3_bind_text(statistics.get(), 1, profile_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statistics.get(), 2, profile_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statistics.get(), 3, profile_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statistics.get(), 4, profile_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statistics.get(), 5, profile_id.c_str(), -1, SQLITE_TRANSIENT);
    check(sqlite3_step(statistics.get()), db_, "index profile statistics source");

    // A complete normal KChess analysis is already authoritative evidence. The
    // locator stays game-based so replacing the saved analysis run never
    // leaves a stale graph reference.
    const std::string analysis_sql = std::string(
        "INSERT INTO ai_profile_evidence_registry("
        "profile_id,evidence_id,source_kind,source_table,source_key,game_id,ply,"
        "evidence_tier,evidence_level,source_version,updated_at) "
        "SELECT ?,('game:'||g.id||':analysis'),'authoritative_analysis','analysis_runs',"
        "g.id,g.id,NULL,'analysis',3,"
        "MAX(COALESCE(ar.completed_at,0),COALESCE(ar.started_at,0)),"
        "MAX(COALESCE(ar.completed_at,0),COALESCE(ar.started_at,0)) "
        "FROM games g JOIN analysis_runs ar ON ar.id=(SELECT candidate.id FROM analysis_runs candidate "
        "WHERE candidate.game_id=g.id AND candidate.status='complete' "
        "ORDER BY candidate.depth DESC,COALESCE(candidate.completed_at,0) DESC LIMIT 1) "
        "JOIN profiles p ON p.id=g.profile_id WHERE ") +
        kPlayerProfileGameScopeSql +
        " "
        "ON CONFLICT(profile_id,evidence_id) DO UPDATE SET "
        "source_key=excluded.source_key,evidence_tier=excluded.evidence_tier,"
        "evidence_level=excluded.evidence_level,source_version=excluded.source_version,"
        "updated_at=excluded.updated_at;";
    auto analysis = prepare(db_, analysis_sql.c_str());
    sqlite3_bind_text(analysis.get(), 1, profile_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(analysis.get(), 2, profile_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(analysis.get(), 3, profile_id.c_str(), -1, SQLITE_TRANSIENT);
    check(sqlite3_step(analysis.get()), db_, "index authoritative profile analysis");

    // The compact learned profile itself is a source for derived patterns,
    // trends and hypotheses. Later graph nodes can point at this model row and
    // still resolve the detailed game evidence through the entries above.
    auto learned = prepare(
        db_,
        "INSERT INTO ai_profile_evidence_registry("
        "profile_id,evidence_id,source_kind,source_table,source_key,game_id,ply,"
        "evidence_tier,evidence_level,source_version,updated_at) "
        "SELECT profile_id,('profile:'||profile_id||':model'),'learned_profile',"
        "'ai_chess_profiles',profile_id,NULL,NULL,'derived',1,updated_at,updated_at "
        "FROM ai_chess_profiles WHERE profile_id=? "
        "ON CONFLICT(profile_id,evidence_id) DO UPDATE SET "
        "source_version=excluded.source_version,updated_at=excluded.updated_at;");
    sqlite3_bind_text(learned.get(), 1, profile_id.c_str(), -1, SQLITE_TRANSIENT);
    check(sqlite3_step(learned.get()), db_, "index learned profile model");

    auto prune_analysis = prepare(
        db_,
        "DELETE FROM ai_profile_evidence_registry WHERE profile_id=? "
        "AND source_kind='authoritative_analysis' AND game_id IS NOT NULL "
        "AND NOT EXISTS(SELECT 1 FROM analysis_runs ar "
        "WHERE ar.game_id=ai_profile_evidence_registry.game_id AND ar.status='complete');");
    sqlite3_bind_text(prune_analysis.get(), 1, profile_id.c_str(), -1, SQLITE_TRANSIENT);
    check(sqlite3_step(prune_analysis.get()), db_, "prune stale profile analysis locators");
    execute("COMMIT;");
  } catch (...) {
    sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
    throw;
  }
}

std::vector<AiProfileEvidenceRegistryRow> Database::ai_profile_evidence_registry(
    const std::string& profile_id) const {
  auto statement = prepare(
      db_,
      "SELECT evidence_id,source_kind,source_table,source_key,game_id,ply,"
      "evidence_tier,evidence_level,source_version,updated_at "
      "FROM ai_profile_evidence_registry WHERE profile_id=? "
      "ORDER BY evidence_level DESC,updated_at DESC,evidence_id ASC;");
  sqlite3_bind_text(statement.get(), 1, profile_id.c_str(), -1, SQLITE_TRANSIENT);
  std::vector<AiProfileEvidenceRegistryRow> result;
  while (sqlite3_step(statement.get()) == SQLITE_ROW) {
    result.push_back(AiProfileEvidenceRegistryRow{
        .evidence_id = text_column(statement.get(), 0),
        .source_kind = text_column(statement.get(), 1),
        .source_table = text_column(statement.get(), 2),
        .source_key = text_column(statement.get(), 3),
        .game_id = optional_text_column(statement.get(), 4),
        .ply = optional_int_column(statement.get(), 5),
        .evidence_tier = text_column(statement.get(), 6),
        .evidence_level = sqlite3_column_int(statement.get(), 7),
        .source_version = sqlite3_column_int64(statement.get(), 8),
        .updated_at = sqlite3_column_int64(statement.get(), 9),
    });
  }
  return result;
}


std::optional<std::string> Database::ai_chess_profile_payload(
    const std::string& profile_id) const {
  auto statement = prepare(
      db_, "SELECT payload_json FROM ai_chess_profiles WHERE profile_id=?;");
  sqlite3_bind_text(statement.get(), 1, profile_id.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(statement.get()) != SQLITE_ROW) return std::nullopt;
  return text_column(statement.get(), 0);
}

void Database::set_ai_chess_profile_payload(
    const std::string& profile_id, const std::string& payload_json) {
  auto statement = prepare(
      db_,
      "INSERT INTO ai_chess_profiles(profile_id,payload_json,updated_at) VALUES(?,?,?) "
      "ON CONFLICT(profile_id) DO UPDATE SET "
      "payload_json=excluded.payload_json,updated_at=excluded.updated_at;");
  sqlite3_bind_text(statement.get(), 1, profile_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(statement.get(), 2, payload_json.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(statement.get(), 3, unix_time_seconds());
  check(sqlite3_step(statement.get()), db_, "persist ai chess profile");
}

void Database::record_ai_coach_skill_attempt(
    const std::string& profile_id, const std::string& motif_id,
    const bool independent_success, const int verified_weak_attempts,
    const int guided_successes, const int success_streak,
    const int schedule_level, const std::int64_t interval_seconds,
    const std::int64_t next_practice_at, const std::int64_t practiced_at) {
  if (profile_id.empty() || motif_id.empty()) return;
  auto statement = prepare(db_,
      "INSERT INTO ai_coach_skill_progress(profile_id,motif_id,"
      "independent_successes,other_attempts,last_practiced_at,"
      "verified_weak_attempts,guided_successes,success_streak,schedule_level,"
      "interval_seconds,next_practice_at) VALUES(?,?,?,?,?,?,?,?,?,?,?) "
      "ON CONFLICT(profile_id,motif_id) DO UPDATE SET "
      "independent_successes=independent_successes+excluded.independent_successes,"
      "other_attempts=other_attempts+excluded.other_attempts,"
      "last_practiced_at=excluded.last_practiced_at,"
      "verified_weak_attempts=excluded.verified_weak_attempts,"
      "guided_successes=excluded.guided_successes,"
      "success_streak=excluded.success_streak,"
      "schedule_level=excluded.schedule_level,"
      "interval_seconds=excluded.interval_seconds,"
      "next_practice_at=excluded.next_practice_at;");
  sqlite3_bind_text(statement.get(), 1, profile_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(statement.get(), 2, motif_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(statement.get(), 3, independent_success ? 1 : 0);
  sqlite3_bind_int(statement.get(), 4, independent_success ? 0 : 1);
  sqlite3_bind_int64(statement.get(), 5, practiced_at);
  sqlite3_bind_int(statement.get(), 6, std::max(0, verified_weak_attempts));
  sqlite3_bind_int(statement.get(), 7, std::max(0, guided_successes));
  sqlite3_bind_int(statement.get(), 8, std::max(0, success_streak));
  sqlite3_bind_int(statement.get(), 9, std::max(0, schedule_level));
  sqlite3_bind_int64(statement.get(), 10, std::max<std::int64_t>(0, interval_seconds));
  sqlite3_bind_int64(statement.get(), 11, std::max<std::int64_t>(0, next_practice_at));
  check(sqlite3_step(statement.get()), db_, "record coach skill attempt");
}

std::vector<AiCoachSkillProgressRow> Database::ai_coach_skill_progress(
    const std::string& profile_id) const {
  auto statement = prepare(db_,
      "SELECT motif_id,independent_successes,other_attempts,last_practiced_at,"
      "verified_weak_attempts,guided_successes,success_streak,schedule_level,"
      "interval_seconds,next_practice_at "
      "FROM ai_coach_skill_progress WHERE profile_id=? ORDER BY motif_id;");
  sqlite3_bind_text(statement.get(), 1, profile_id.c_str(), -1, SQLITE_TRANSIENT);
  std::vector<AiCoachSkillProgressRow> result;
  while (sqlite3_step(statement.get()) == SQLITE_ROW) {
    result.push_back({.motif_id = text_column(statement.get(), 0),
        .independent_successes = sqlite3_column_int(statement.get(), 1),
        .other_attempts = sqlite3_column_int(statement.get(), 2),
        .last_practiced_at = sqlite3_column_int64(statement.get(), 3),
        .verified_weak_attempts = sqlite3_column_int(statement.get(), 4),
        .guided_successes = sqlite3_column_int(statement.get(), 5),
        .success_streak = sqlite3_column_int(statement.get(), 6),
        .schedule_level = sqlite3_column_int(statement.get(), 7),
        .interval_seconds = sqlite3_column_int64(statement.get(), 8),
        .next_practice_at = sqlite3_column_int64(statement.get(), 9)});
  }
  return result;
}


std::int64_t Database::ensure_ai_profile_sampling_bootstrap_cutoff(
    const std::string& profile_id,
    const std::int64_t latest_known_played_at) {
  if (latest_known_played_at <= 0) return 0;

  auto insert = prepare(
      db_,
      "INSERT OR IGNORE INTO ai_profile_sampling_state("
      "profile_id,bootstrap_cutoff_played_at,updated_at) VALUES(?,?,?);");
  sqlite3_bind_text(insert.get(), 1, profile_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(insert.get(), 2, latest_known_played_at);
  sqlite3_bind_int64(insert.get(), 3, unix_time_seconds());
  check(sqlite3_step(insert.get()), db_, "persist profile sampling bootstrap cutoff");

  auto read = prepare(
      db_,
      "SELECT bootstrap_cutoff_played_at FROM ai_profile_sampling_state "
      "WHERE profile_id=?;");
  sqlite3_bind_text(read.get(), 1, profile_id.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(read.get()) != SQLITE_ROW) return 0;
  return sqlite3_column_int64(read.get(), 0);
}

void Database::update_ai_profile_sampling_state(
    const std::string& profile_id,
    const AiProfileSamplingState& state) {
  auto statement = prepare(
      db_,
      "UPDATE ai_profile_sampling_state SET "
      "total_games=?,eligible_games=?,excluded_games=?,minimum_games=?,"
      "recommended_games=?,maximum_games=?,selected_games=?,required_strata=?,"
      "covered_strata=?,diversity_score=?,covered_population_share=?,capped=?,"
      "updated_at=? WHERE profile_id=?;");
  sqlite3_bind_int(statement.get(), 1, state.total_games);
  sqlite3_bind_int(statement.get(), 2, state.eligible_games);
  sqlite3_bind_int(statement.get(), 3, state.excluded_games);
  sqlite3_bind_int(statement.get(), 4, state.minimum_games);
  sqlite3_bind_int(statement.get(), 5, state.recommended_games);
  sqlite3_bind_int(statement.get(), 6, state.maximum_games);
  sqlite3_bind_int(statement.get(), 7, state.selected_games);
  sqlite3_bind_int(statement.get(), 8, state.required_strata);
  sqlite3_bind_int(statement.get(), 9, state.covered_strata);
  sqlite3_bind_double(statement.get(), 10, state.diversity_score);
  sqlite3_bind_double(statement.get(), 11, state.covered_population_share);
  sqlite3_bind_int(statement.get(), 12, state.capped ? 1 : 0);
  sqlite3_bind_int64(
      statement.get(), 13,
      state.updated_at > 0 ? state.updated_at : unix_time_seconds());
  sqlite3_bind_text(statement.get(), 14, profile_id.c_str(), -1, SQLITE_TRANSIENT);
  check(sqlite3_step(statement.get()), db_, "update profile sampling state");
}

std::optional<AiProfileSamplingState> Database::ai_profile_sampling_state(
    const std::string& profile_id) const {
  auto statement = prepare(
      db_,
      "SELECT bootstrap_cutoff_played_at,total_games,eligible_games,excluded_games,"
      "minimum_games,recommended_games,maximum_games,selected_games,required_strata,"
      "covered_strata,diversity_score,covered_population_share,capped,updated_at "
      "FROM ai_profile_sampling_state WHERE profile_id=?;");
  sqlite3_bind_text(statement.get(), 1, profile_id.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(statement.get()) != SQLITE_ROW) return std::nullopt;
  return AiProfileSamplingState{
      .bootstrap_cutoff_played_at = sqlite3_column_int64(statement.get(), 0),
      .total_games = sqlite3_column_int(statement.get(), 1),
      .eligible_games = sqlite3_column_int(statement.get(), 2),
      .excluded_games = sqlite3_column_int(statement.get(), 3),
      .minimum_games = sqlite3_column_int(statement.get(), 4),
      .recommended_games = sqlite3_column_int(statement.get(), 5),
      .maximum_games = sqlite3_column_int(statement.get(), 6),
      .selected_games = sqlite3_column_int(statement.get(), 7),
      .required_strata = sqlite3_column_int(statement.get(), 8),
      .covered_strata = sqlite3_column_int(statement.get(), 9),
      .diversity_score = sqlite3_column_double(statement.get(), 10),
      .covered_population_share = sqlite3_column_double(statement.get(), 11),
      .capped = sqlite3_column_int(statement.get(), 12) != 0,
      .updated_at = sqlite3_column_int64(statement.get(), 13),
  };
}


void Database::upsert_ai_profile_queue(
    const std::string& profile_id,
    const std::string& game_id,
    const double priority,
    const std::string& reason,
    const int pipeline_stage,
    const bool historical_sample,
    const bool interesting_selected,
    const std::int64_t source_version) {
  auto statement = prepare(
      db_,
      "INSERT INTO ai_profile_queue(profile_id,game_id,state,priority,reason,pipeline_stage,"
      "historical_sample,interesting_selected,source_version,processed_version,attempts,updated_at) "
      "VALUES(?,?,'queued',?,?,?,?,?,?,-1,0,?) "
      "ON CONFLICT(profile_id,game_id) DO UPDATE SET "
      "priority=MAX(ai_profile_queue.priority,excluded.priority),reason=excluded.reason,"
      "pipeline_stage=CASE "
      "WHEN ai_profile_queue.source_version<>excluded.source_version THEN excluded.pipeline_stage "
      "ELSE MAX(ai_profile_queue.pipeline_stage,excluded.pipeline_stage) END,"
      "historical_sample=excluded.historical_sample,"
      "interesting_selected=excluded.interesting_selected,"
      "source_version=excluded.source_version,"
      // Preserve genuine in-flight states during periodic queue synchronization.
      // Requeue only when the authoritative source changed, or when a game that
      // was previously metadata-only is promoted into relevant evidence work.
      "state=CASE "
      "WHEN ai_profile_queue.source_version<>excluded.source_version THEN 'queued' "
      "WHEN ai_profile_queue.reason='metadata_only' AND excluded.reason<>'metadata_only' "
      "THEN 'queued' "
      "WHEN ai_profile_queue.processed_version<>excluded.source_version "
      "AND ai_profile_queue.state IN ('indexed','done') THEN 'queued' "
      "ELSE ai_profile_queue.state END,"
      "attempts=CASE "
      "WHEN ai_profile_queue.source_version<>excluded.source_version "
      "OR (ai_profile_queue.reason='metadata_only' AND excluded.reason<>'metadata_only') "
      "THEN 0 ELSE ai_profile_queue.attempts END,"
      "updated_at=CASE "
      "WHEN ai_profile_queue.source_version<>excluded.source_version "
      "OR (ai_profile_queue.reason='metadata_only' AND excluded.reason<>'metadata_only') "
      "THEN excluded.updated_at ELSE ai_profile_queue.updated_at END;");
  sqlite3_bind_text(statement.get(), 1, profile_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(statement.get(), 2, game_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_double(statement.get(), 3, priority);
  sqlite3_bind_text(statement.get(), 4, reason.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(statement.get(), 5, pipeline_stage);
  sqlite3_bind_int(statement.get(), 6, historical_sample ? 1 : 0);
  sqlite3_bind_int(statement.get(), 7, interesting_selected ? 1 : 0);
  sqlite3_bind_int64(statement.get(), 8, source_version);
  sqlite3_bind_int64(statement.get(), 9, unix_time_seconds());
  check(sqlite3_step(statement.get()), db_, "upsert ai profile queue");
}

std::optional<AiProfileQueueRecord> Database::next_ai_profile_queue(
    const std::string& profile_id) const {
  auto statement = prepare(
      db_,
      "SELECT profile_id,game_id,state,priority,reason,pipeline_stage,source_version,processed_version,"
      "attempts,updated_at FROM ai_profile_queue WHERE profile_id=? "
      "AND (state='engine_pending' "
      "OR (state='queued' AND updated_at<=strftime('%s','now')-CASE "
      "WHEN attempts<=0 THEN 0 WHEN attempts=1 THEN 2 WHEN attempts=2 THEN 10 "
      "WHEN attempts=3 THEN 30 ELSE 120 END) "
      "OR (state='processing' AND updated_at<strftime('%s','now')-30)) "
      "ORDER BY priority DESC,updated_at ASC LIMIT 1;");
  sqlite3_bind_text(statement.get(), 1, profile_id.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(statement.get()) != SQLITE_ROW) return std::nullopt;
  return AiProfileQueueRecord{
      .profile_id = text_column(statement.get(), 0),
      .game_id = text_column(statement.get(), 1),
      .state = text_column(statement.get(), 2),
      .priority = sqlite3_column_double(statement.get(), 3),
      .reason = text_column(statement.get(), 4),
      .pipeline_stage = sqlite3_column_int(statement.get(), 5),
      .source_version = sqlite3_column_int64(statement.get(), 6),
      .processed_version = sqlite3_column_int64(statement.get(), 7),
      .attempts = sqlite3_column_int(statement.get(), 8),
      .updated_at = sqlite3_column_int64(statement.get(), 9),
  };
}

void Database::recover_ai_profile_queue_inflight(const std::string& profile_id) {
  // `processing` only covers synchronous orchestration around an engine request
  // and cannot represent surviving work after process restart. Requeue it
  // immediately on cold owner activation instead of waiting for the stale-row
  // timeout. `engine_pending` is intentionally untouched: it is already a
  // resumable state and may still correspond to a live shared AnalysisService
  // job during an in-process profile/account switch.
  auto statement = prepare(
      db_,
      "UPDATE ai_profile_queue SET state='queued',updated_at=? "
      "WHERE profile_id=? AND state='processing';");
  sqlite3_bind_int64(statement.get(), 1, unix_time_seconds());
  sqlite3_bind_text(statement.get(), 2, profile_id.c_str(), -1, SQLITE_TRANSIENT);
  check(sqlite3_step(statement.get()), db_, "recover ai profile queue inflight");
}

void Database::set_ai_profile_queue_state(
    const std::string& profile_id,
    const std::string& game_id,
    const std::string& state,
    const std::int64_t processed_version,
    const bool increment_attempts,
    const int pipeline_stage) {
  auto statement = prepare(
      db_,
      "UPDATE ai_profile_queue SET state=?,"
      "processed_version=CASE WHEN ?>=0 THEN ? ELSE processed_version END,"
      "pipeline_stage=CASE WHEN ?>=1 THEN MAX(pipeline_stage,?) ELSE pipeline_stage END,"
      "attempts=attempts+?,updated_at=? WHERE profile_id=? AND game_id=?;");
  sqlite3_bind_text(statement.get(), 1, state.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(statement.get(), 2, processed_version);
  sqlite3_bind_int64(statement.get(), 3, processed_version);
  sqlite3_bind_int(statement.get(), 4, pipeline_stage);
  sqlite3_bind_int(statement.get(), 5, pipeline_stage);
  sqlite3_bind_int(statement.get(), 6, increment_attempts ? 1 : 0);
  sqlite3_bind_int64(statement.get(), 7, unix_time_seconds());
  sqlite3_bind_text(statement.get(), 8, profile_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(statement.get(), 9, game_id.c_str(), -1, SQLITE_TRANSIENT);
  check(sqlite3_step(statement.get()), db_, "set ai profile queue state");
}


void Database::notify_ai_profile_analysis_changed(const std::string& game_id) {
  auto statement = prepare(
      db_,
      "UPDATE ai_profile_queue SET state='queued',processed_version=-1,updated_at=? "
      "WHERE game_id=?;");
  sqlite3_bind_int64(statement.get(), 1, unix_time_seconds());
  sqlite3_bind_text(statement.get(), 2, game_id.c_str(), -1, SQLITE_TRANSIENT);
  check(sqlite3_step(statement.get()), db_, "notify ai profile analysis changed");
}

AiProfileQueueProgress Database::ai_profile_queue_progress(
    const std::string& profile_id) const {
  AiProfileQueueProgress progress;
  auto statement = prepare(
      db_,
      "SELECT count(*),"
      "sum(CASE WHEN state IN ('indexed','done') AND processed_version=source_version THEN 1 ELSE 0 END),"
      "sum(historical_sample),"
      "sum(interesting_selected),"
      "sum(CASE WHEN reason<>'metadata_only' THEN 1 ELSE 0 END),"
      "sum(CASE WHEN reason<>'metadata_only' AND state='done' "
      "AND processed_version=source_version THEN 1 ELSE 0 END),"
      "sum(CASE WHEN state IN ('queued','engine_pending') THEN 1 ELSE 0 END),"
      "sum(CASE WHEN state='engine_pending' THEN 1 ELSE 0 END),"
      "sum(CASE WHEN state='processing' THEN 1 ELSE 0 END),"
      "sum(CASE WHEN pipeline_stage>=7 THEN 1 ELSE 0 END) "
      "FROM ai_profile_queue WHERE profile_id=?;");
  sqlite3_bind_text(statement.get(), 1, profile_id.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(statement.get()) == SQLITE_ROW) {
    progress.total = sqlite3_column_int(statement.get(), 0);
    progress.considered = sqlite3_column_int(statement.get(), 1);
    progress.historical_sample = sqlite3_column_int(statement.get(), 2);
    progress.interesting = sqlite3_column_int(statement.get(), 3);
    progress.relevant = sqlite3_column_int(statement.get(), 4);
    progress.resolved_relevant = sqlite3_column_int(statement.get(), 5);
    progress.queued = sqlite3_column_int(statement.get(), 6);
    progress.engine_pending = sqlite3_column_int(statement.get(), 7);
    progress.processing = sqlite3_column_int(statement.get(), 8);
    progress.engine_promoted_games = sqlite3_column_int(statement.get(), 9);
  }
  return progress;
}

std::vector<std::string> Database::ai_profile_processed_game_ids(
    const std::string& profile_id) const {
  std::vector<std::string> result;
  auto statement = prepare(
      db_,
      "SELECT game_id FROM ai_profile_queue WHERE profile_id=? "
      "AND state IN ('indexed','done') AND processed_version=source_version "
      "ORDER BY priority DESC;");
  sqlite3_bind_text(statement.get(), 1, profile_id.c_str(), -1, SQLITE_TRANSIENT);
  while (sqlite3_step(statement.get()) == SQLITE_ROW) {
    result.push_back(text_column(statement.get(), 0));
  }
  return result;
}

void Database::delete_local_game(
    const std::string& profile_id, const std::string& game_id) {
  auto statement = prepare(
      db_,
      "DELETE FROM games WHERE id=? AND profile_id=? AND provider_game_id IS NULL;");
  sqlite3_bind_text(statement.get(), 1, game_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(statement.get(), 2, profile_id.c_str(), -1, SQLITE_TRANSIENT);
  check(sqlite3_step(statement.get()), db_, "delete local game");
  if (sqlite3_changes(db_) == 0) {
    throw std::runtime_error("Local game not found");
  }
}

int Database::clear_cached_month(
    const std::string& profile_id, const std::string& month) {
  SqliteConnectionTransactionLock transaction_lock(db_);
  execute("BEGIN IMMEDIATE;");
  try {
    auto cache = prepare(
        db_, "DELETE FROM provider_month_cache WHERE profile_id=? AND month=?;");
    sqlite3_bind_text(cache.get(), 1, profile_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(cache.get(), 2, month.c_str(), -1, SQLITE_TRANSIENT);
    check(sqlite3_step(cache.get()), db_, "clear provider month cache");

    // Keep anything the user favorited (including the Downloads collection) or
    // already analysed. Only disposable synced rows from this month are removed.
    auto games = prepare(
        db_,
        "DELETE FROM games WHERE profile_id=? AND provider_game_id IS NOT NULL "
        "AND strftime('%Y-%m',provider_ended_at,'unixepoch')=? "
        "AND NOT EXISTS(SELECT 1 FROM favorites f WHERE f.profile_id=games.profile_id "
        "AND f.game_id=games.id) "
        "AND NOT EXISTS(SELECT 1 FROM analysis_runs a WHERE a.game_id=games.id);");
    sqlite3_bind_text(games.get(), 1, profile_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(games.get(), 2, month.c_str(), -1, SQLITE_TRANSIENT);
    check(sqlite3_step(games.get()), db_, "prune provider month games");
    const int removed = sqlite3_changes(db_);
    execute("COMMIT;");
    return removed;
  } catch (...) {
    sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
    throw;
  }
}

void Database::set_profile_avatar_file(
    const std::string& profile_id, const std::optional<std::string>& file_path) {
  auto statement = prepare(db_, "UPDATE profiles SET avatar_file=? WHERE id=?;");
  bind_optional_text(statement.get(), 1, file_path);
  sqlite3_bind_text(statement.get(), 2, profile_id.c_str(), -1, SQLITE_TRANSIENT);
  check(sqlite3_step(statement.get()), db_, "cache profile avatar path");
}

void Database::set_provider_sync_state(
    const std::string& profile_id,
    const ProviderType provider,
    const std::string& status,
    const std::string& last_error,
    const std::int64_t retry_after) {
  auto statement = prepare(
      db_, "INSERT INTO provider_sync_state(profile_id,last_sync_at,last_error,provider,status,"
           "retry_after) VALUES(?,?,?,?,?,?) ON CONFLICT(profile_id) DO UPDATE SET "
           "last_sync_at=excluded.last_sync_at,last_error=excluded.last_error,"
           "provider=excluded.provider,status=excluded.status,retry_after=excluded.retry_after;");
  sqlite3_bind_text(statement.get(), 1, profile_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(statement.get(), 2, unix_time_seconds());
  sqlite3_bind_text(statement.get(), 3, last_error.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(statement.get(), 4, static_cast<int>(provider));
  sqlite3_bind_text(statement.get(), 5, status.c_str(), -1, SQLITE_TRANSIENT);
  if (retry_after > 0) sqlite3_bind_int64(statement.get(), 6, retry_after);
  else sqlite3_bind_null(statement.get(), 6);
  check(sqlite3_step(statement.get()), db_, "update provider sync state");
}

std::vector<GameRecord> Database::games(const std::string& profile_id) const {
  const std::string sql = std::string("SELECT ") + kGameColumns + kGameJoins
      + "WHERE g.profile_id=? ORDER BY COALESCE(NULLIF(g.provider_ended_at,0),g.created_at) DESC;";
  auto statement = prepare(db_, sql.c_str());
  sqlite3_bind_text(statement.get(), 1, profile_id.c_str(), -1, SQLITE_TRANSIENT);
  std::vector<GameRecord> result;
  while (sqlite3_step(statement.get()) == SQLITE_ROW) {
    result.push_back(read_game_record(statement.get()));
  }
  return result;
}

std::optional<std::string> Database::latest_game_month_at_or_before(
    const std::string& profile_id, const std::string& maximum_month) const {
  auto statement = prepare(
      db_,
      "SELECT strftime('%Y-%m',COALESCE(NULLIF(provider_ended_at,0),created_at),'unixepoch') "
      "FROM games WHERE profile_id=? "
      "AND strftime('%Y-%m',COALESCE(NULLIF(provider_ended_at,0),created_at),'unixepoch')<=? "
      "ORDER BY COALESCE(NULLIF(provider_ended_at,0),created_at) DESC LIMIT 1;");
  sqlite3_bind_text(statement.get(), 1, profile_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(statement.get(), 2, maximum_month.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(statement.get()) != SQLITE_ROW) return std::nullopt;
  return text_column(statement.get(), 0);
}

std::vector<GameRecord> Database::games_for_month(
    const std::string& profile_id, const std::string& month) const {
  const std::string sql = std::string("SELECT ") + kGameColumns + kGameJoins
      + "WHERE g.profile_id=? "
        "AND strftime('%Y-%m',COALESCE(NULLIF(g.provider_ended_at,0),g.created_at),'unixepoch')=? "
        "ORDER BY COALESCE(NULLIF(g.provider_ended_at,0),g.created_at) DESC;";
  auto statement = prepare(db_, sql.c_str());
  sqlite3_bind_text(statement.get(), 1, profile_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(statement.get(), 2, month.c_str(), -1, SQLITE_TRANSIENT);
  std::vector<GameRecord> result;
  while (sqlite3_step(statement.get()) == SQLITE_ROW) {
    result.push_back(read_game_record(statement.get()));
  }
  return result;
}

std::vector<GameRecord> Database::favorite_games() const {
  const std::string sql = std::string("SELECT ") + kGameColumns + kGameJoins +
      "WHERE EXISTS(SELECT 1 FROM favorites fav WHERE fav.game_id=g.id) "
      "ORDER BY CASE WHEN g.provider_ended_at>0 THEN g.provider_ended_at ELSE g.created_at END DESC;";
  auto statement = prepare(db_, sql.c_str());
  std::vector<GameRecord> result;
  while (sqlite3_step(statement.get()) == SQLITE_ROW) {
    result.push_back(read_game_record(statement.get()));
  }
  return result;
}

std::optional<GameRecord> Database::game(const std::string& game_id) const {
  const std::string sql = std::string("SELECT ") + kGameColumns + kGameJoins
      + "WHERE g.id=?;";
  auto statement = prepare(db_, sql.c_str());
  sqlite3_bind_text(statement.get(), 1, game_id.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(statement.get()) != SQLITE_ROW) return std::nullopt;
  GameRecord result = read_game_record(statement.get());

  auto moves = prepare(
      db_,
      "SELECT ply_index,move_number,side_to_move,san,uci,fen_before,fen_after "
      "FROM game_moves WHERE game_id=? ORDER BY ply_index;");
  sqlite3_bind_text(moves.get(), 1, game_id.c_str(), -1, SQLITE_TRANSIENT);
  while (sqlite3_step(moves.get()) == SQLITE_ROW) {
    result.moves.push_back({
        .ply_index = sqlite3_column_int(moves.get(), 0),
        .move_number = sqlite3_column_int(moves.get(), 1),
        .side_to_move = text_column(moves.get(), 2),
        .san = text_column(moves.get(), 3),
        .uci = text_column(moves.get(), 4),
        .fen_before = text_column(moves.get(), 5),
        .fen_after = text_column(moves.get(), 6),
    });
  }
  return result;
}


// -----------------------------------------------------------------------------
// Section: Persisted local bot game sessions
// -----------------------------------------------------------------------------

BotGameRecord Database::create_bot_game(
    const int bot_elo, const std::string& starting_fen) {
  if (bot_elo < 100 || bot_elo > 3200) {
    throw std::invalid_argument("Bot Elo must be between 100 and 3200");
  }
  if (active_bot_game().has_value()) {
    throw std::runtime_error("An unfinished bot game already exists");
  }
  const auto id = make_uuid();
  const auto now = unix_time_seconds();
  auto statement = prepare(
      db_,
      "INSERT INTO bot_games(id,bot_elo,player_color,bot_color,status,result,"
      "starting_fen,current_fen,created_at,updated_at) "
      "VALUES(?,?,'white','black','active','*',?,?,?,?);");
  sqlite3_bind_text(statement.get(), 1, id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(statement.get(), 2, bot_elo);
  sqlite3_bind_text(statement.get(), 3, starting_fen.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(statement.get(), 4, starting_fen.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(statement.get(), 5, now);
  sqlite3_bind_int64(statement.get(), 6, now);
  check(sqlite3_step(statement.get()), db_, "create bot game");
  return bot_game(id).value();
}

std::optional<BotGameRecord> Database::active_bot_game() const {
  auto statement = prepare(
      db_, "SELECT id FROM bot_games WHERE status='active' ORDER BY updated_at DESC LIMIT 1;");
  if (sqlite3_step(statement.get()) != SQLITE_ROW) return std::nullopt;
  return bot_game(text_column(statement.get(), 0));
}

std::optional<BotGameRecord> Database::bot_game(const std::string& game_id) const {
  auto statement = prepare(
      db_,
      "SELECT id,bot_elo,player_color,bot_color,status,result,starting_fen,current_fen,"
      "created_at,updated_at,show_eval_bar,analysis_game_id FROM bot_games WHERE id=?;");
  sqlite3_bind_text(statement.get(), 1, game_id.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(statement.get()) != SQLITE_ROW) return std::nullopt;

  BotGameRecord game{
      .id = text_column(statement.get(), 0),
      .bot_elo = sqlite3_column_int(statement.get(), 1),
      .player_color = text_column(statement.get(), 2),
      .bot_color = text_column(statement.get(), 3),
      .status = text_column(statement.get(), 4),
      .result = text_column(statement.get(), 5),
      .starting_fen = text_column(statement.get(), 6),
      .current_fen = text_column(statement.get(), 7),
      .created_at = sqlite3_column_int64(statement.get(), 8),
      .updated_at = sqlite3_column_int64(statement.get(), 9),
      .show_eval_bar = sqlite3_column_int(statement.get(), 10) != 0,
      .analysis_game_id = optional_text_column(statement.get(), 11),
  };

  auto moves = prepare(
      db_,
      "SELECT ply,uci,san,fen_after FROM bot_game_moves WHERE game_id=? ORDER BY ply ASC;");
  sqlite3_bind_text(moves.get(), 1, game_id.c_str(), -1, SQLITE_TRANSIENT);
  while (sqlite3_step(moves.get()) == SQLITE_ROW) {
    game.moves.push_back({
        .ply = sqlite3_column_int(moves.get(), 0),
        .uci = text_column(moves.get(), 1),
        .san = text_column(moves.get(), 2),
        .fen_after = text_column(moves.get(), 3),
    });
  }
  return game;
}

std::vector<BotGameSummaryRecord> Database::bot_games() const {
  auto statement = prepare(
      db_,
      "SELECT g.id,g.bot_elo,g.player_color,g.bot_color,g.status,g.result,"
      "g.created_at,g.updated_at,COUNT(m.ply),g.analysis_game_id "
      "FROM bot_games g LEFT JOIN bot_game_moves m ON m.game_id=g.id "
      "GROUP BY g.id ORDER BY g.updated_at DESC,g.created_at DESC;");
  std::vector<BotGameSummaryRecord> result;
  while (sqlite3_step(statement.get()) == SQLITE_ROW) {
    result.push_back({
        .id = text_column(statement.get(), 0),
        .bot_elo = sqlite3_column_int(statement.get(), 1),
        .player_color = text_column(statement.get(), 2),
        .bot_color = text_column(statement.get(), 3),
        .status = text_column(statement.get(), 4),
        .result = text_column(statement.get(), 5),
        .created_at = sqlite3_column_int64(statement.get(), 6),
        .updated_at = sqlite3_column_int64(statement.get(), 7),
        .move_count = sqlite3_column_int(statement.get(), 8),
        .analysis_game_id = optional_text_column(statement.get(), 9),
    });
  }
  return result;
}

void Database::append_bot_game_move(
    const std::string& game_id,
    const int base_ply,
    const std::string& expected_fen_before,
    const BotGameMoveRecord& move) {
  if (base_ply < 0) {
    throw std::invalid_argument("Bot game branch ply must be non-negative");
  }
  if (move.uci.empty() || move.san.empty() || move.fen_after.empty()) {
    throw std::invalid_argument("Bot game move is incomplete");
  }
  SqliteConnectionTransactionLock transaction_lock(db_);
  execute("BEGIN IMMEDIATE;");
  try {
    auto game = prepare(
        db_, "SELECT status,starting_fen FROM bot_games WHERE id=?;");
    sqlite3_bind_text(game.get(), 1, game_id.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(game.get()) != SQLITE_ROW) {
      throw std::invalid_argument("Bot game not found");
    }
    if (text_column(game.get(), 0) != "active") {
      throw std::runtime_error("Bot game is no longer active");
    }

    std::string branch_fen;
    if (base_ply == 0) {
      branch_fen = text_column(game.get(), 1);
    } else {
      auto branch = prepare(
          db_, "SELECT fen_after FROM bot_game_moves WHERE game_id=? AND ply=?;");
      sqlite3_bind_text(branch.get(), 1, game_id.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_int(branch.get(), 2, base_ply);
      if (sqlite3_step(branch.get()) != SQLITE_ROW) {
        throw std::invalid_argument("Bot game branch ply does not exist");
      }
      branch_fen = text_column(branch.get(), 0);
    }
    if (branch_fen != expected_fen_before) {
      throw std::runtime_error("Bot game branch position changed before move was saved");
    }

    auto erase_future = prepare(
        db_, "DELETE FROM bot_game_moves WHERE game_id=? AND ply>?;");
    sqlite3_bind_text(erase_future.get(), 1, game_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(erase_future.get(), 2, base_ply);
    check(sqlite3_step(erase_future.get()), db_, "truncate bot game continuation");

    const int next_ply = base_ply + 1;
    auto insert = prepare(
        db_, "INSERT INTO bot_game_moves(game_id,ply,uci,san,fen_after) VALUES(?,?,?,?,?);");
    sqlite3_bind_text(insert.get(), 1, game_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(insert.get(), 2, next_ply);
    sqlite3_bind_text(insert.get(), 3, move.uci.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(insert.get(), 4, move.san.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(insert.get(), 5, move.fen_after.c_str(), -1, SQLITE_TRANSIENT);
    check(sqlite3_step(insert.get()), db_, "append bot game branch move");

    auto update = prepare(
        db_, "UPDATE bot_games SET current_fen=?,updated_at=?,analysis_game_id=NULL WHERE id=?;");
    sqlite3_bind_text(update.get(), 1, move.fen_after.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(update.get(), 2, unix_time_seconds());
    sqlite3_bind_text(update.get(), 3, game_id.c_str(), -1, SQLITE_TRANSIENT);
    check(sqlite3_step(update.get()), db_, "update bot game branch position");
    execute("COMMIT;");
  } catch (...) {
    sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
    throw;
  }
}

void Database::finish_bot_game(
    const std::string& game_id,
    const std::string& status,
    const std::string& result) {
  if (status != "resigned" && status != "complete") {
    throw std::invalid_argument("Unsupported bot game final status");
  }
  auto statement = prepare(
      db_, "UPDATE bot_games SET status=?,result=?,updated_at=? WHERE id=? AND status='active';");
  sqlite3_bind_text(statement.get(), 1, status.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(statement.get(), 2, result.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(statement.get(), 3, unix_time_seconds());
  sqlite3_bind_text(statement.get(), 4, game_id.c_str(), -1, SQLITE_TRANSIENT);
  check(sqlite3_step(statement.get()), db_, "finish bot game");
  if (sqlite3_changes(db_) == 0) throw std::runtime_error("Active bot game not found");
}

void Database::delete_bot_game(const std::string& game_id) {
  auto statement = prepare(db_, "DELETE FROM bot_games WHERE id=?;");
  sqlite3_bind_text(statement.get(), 1, game_id.c_str(), -1, SQLITE_TRANSIENT);
  check(sqlite3_step(statement.get()), db_, "delete bot game");
  if (sqlite3_changes(db_) == 0) throw std::invalid_argument("Bot game not found");
}

void Database::set_bot_game_show_eval_bar(
    const std::string& game_id, const bool enabled) {
  auto statement = prepare(
      db_, "UPDATE bot_games SET show_eval_bar=?,updated_at=? WHERE id=?;");
  sqlite3_bind_int(statement.get(), 1, enabled ? 1 : 0);
  sqlite3_bind_int64(statement.get(), 2, unix_time_seconds());
  sqlite3_bind_text(statement.get(), 3, game_id.c_str(), -1, SQLITE_TRANSIENT);
  check(sqlite3_step(statement.get()), db_, "update bot game display settings");
  if (sqlite3_changes(db_) == 0) throw std::invalid_argument("Bot game not found");
}

void Database::set_bot_game_analysis_game_id(
    const std::string& game_id, const std::string& analysis_game_id) {
  auto statement = prepare(
      db_, "UPDATE bot_games SET analysis_game_id=? WHERE id=?;");
  sqlite3_bind_text(
      statement.get(), 1, analysis_game_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(statement.get(), 2, game_id.c_str(), -1, SQLITE_TRANSIENT);
  check(sqlite3_step(statement.get()), db_, "link bot game analysis game");
  if (sqlite3_changes(db_) == 0) throw std::invalid_argument("Bot game not found");
}

// -----------------------------------------------------------------------------
// Section: Training progress persistence
// -----------------------------------------------------------------------------

std::vector<TrainingProgressRecord> Database::training_progress() const {
  auto statement = prepare(
      db_,
      "SELECT exercise_id,mastered,success_streak,success_count,attempt_count,"
      "best_depth,last_attempt_at FROM training_progress ORDER BY exercise_id;");
  std::vector<TrainingProgressRecord> result;
  while (sqlite3_step(statement.get()) == SQLITE_ROW) {
    result.push_back(TrainingProgressRecord{
        .exercise_id = text_column(statement.get(), 0),
        .mastered = sqlite3_column_int(statement.get(), 1) != 0,
        .success_streak = sqlite3_column_int(statement.get(), 2),
        .success_count = sqlite3_column_int(statement.get(), 3),
        .attempt_count = sqlite3_column_int(statement.get(), 4),
        .best_depth = sqlite3_column_int(statement.get(), 5),
        .last_attempt_at = optional_int64_column(statement.get(), 6),
    });
  }
  return result;
}

void Database::put_training_progress(const TrainingProgressRecord& progress) {
  auto statement = prepare(
      db_,
      "INSERT INTO training_progress(exercise_id,mastered,success_streak,"
      "success_count,attempt_count,best_depth,last_attempt_at) VALUES(?,?,?,?,?,?,?) "
      "ON CONFLICT(exercise_id) DO UPDATE SET mastered=excluded.mastered,"
      "success_streak=excluded.success_streak,success_count=excluded.success_count,"
      "attempt_count=excluded.attempt_count,best_depth=excluded.best_depth,"
      "last_attempt_at=excluded.last_attempt_at;");
  sqlite3_bind_text(
      statement.get(), 1, progress.exercise_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(statement.get(), 2, progress.mastered ? 1 : 0);
  sqlite3_bind_int(statement.get(), 3, progress.success_streak);
  sqlite3_bind_int(statement.get(), 4, progress.success_count);
  sqlite3_bind_int(statement.get(), 5, progress.attempt_count);
  sqlite3_bind_int(statement.get(), 6, progress.best_depth);
  if (progress.last_attempt_at.has_value()) {
    sqlite3_bind_int64(statement.get(), 7, *progress.last_attempt_at);
  } else {
    sqlite3_bind_null(statement.get(), 7);
  }
  check(sqlite3_step(statement.get()), db_, "persist training progress");
}

PersistedAnalysis Database::prepare_analysis(
    const std::string& game_id,
    const std::string& config_hash,
    const std::string& engine_version,
    const int total_plies,
    const int depth,
    const int multi_pv,
    const int time_limit_seconds,
    const bool adaptive_early_stop) {
  if (const auto existing = analysis(game_id, config_hash); existing.has_value()) {
    if (existing->status != "complete") {
      set_analysis_status(game_id, config_hash, "running");
      auto resumed = *existing;
      resumed.status = "running";
      resumed.error.clear();
      return resumed;
    }
    return *existing;
  }
  const std::string id = make_uuid();
  auto statement = prepare(
      db_,
      "INSERT INTO analysis_runs(id,game_id,analysis_version,engine_name,engine_version,"
      "depth,multi_pv,status,total_plies,completed_plies,started_at,config_hash,"
      "engine_analysis_version,engine_config_hash,time_limit_seconds,adaptive_early_stop) "
      "VALUES(?,? ,?,'Stockfish',?,?,?,'running',?,0,?,?,'2',?,?,?);");
  sqlite3_bind_text(statement.get(), 1, id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(statement.get(), 2, game_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(statement.get(), 3, config_hash.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(statement.get(), 4, engine_version.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(statement.get(), 5, depth);
  sqlite3_bind_int(statement.get(), 6, multi_pv);
  sqlite3_bind_int(statement.get(), 7, total_plies);
  sqlite3_bind_int64(statement.get(), 8, unix_time_seconds());
  sqlite3_bind_text(statement.get(), 9, config_hash.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(statement.get(), 10, config_hash.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(statement.get(), 11, time_limit_seconds);
  sqlite3_bind_int(statement.get(), 12, adaptive_early_stop ? 1 : 0);
  check(sqlite3_step(statement.get()), db_, "create analysis run");
  return analysis(game_id, config_hash).value();
}

void Database::persist_engine_result(
    const std::string& game_id,
    const std::string& config_hash,
    const int ply,
    const int completed_plies,
    const AnalysisResult& result,
    const std::int64_t analysis_timestamp) {
  auto run = prepare(
      db_, "SELECT id,engine_version FROM analysis_runs WHERE game_id=? AND config_hash=?;");
  sqlite3_bind_text(run.get(), 1, game_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(run.get(), 2, config_hash.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(run.get()) != SQLITE_ROW) throw std::runtime_error("Analysis run not found");
  const std::string run_id = text_column(run.get(), 0);
  const std::string engine_version = text_column(run.get(), 1);
  run.reset();
  const EngineLine* primary = result.lines.empty() ? nullptr : &result.lines.front();

  SqliteConnectionTransactionLock transaction_lock(db_);
  execute("BEGIN IMMEDIATE;");
  try {

  auto statement = prepare(
      db_,
      "INSERT INTO move_analysis(analysis_run_id,ply,category,best_move,engine_score,"
      "is_theory,engine_depth,evaluation_cp,mate_in,wdl_wins,wdl_draws,wdl_losses,"
      "nodes,analysis_timestamp,stockfish_version,config_hash) "
      "VALUES(?,?,'unknown',?,'',0,?,?,?,?,?,?,?,?,?,?) "
      "ON CONFLICT(analysis_run_id,ply) DO UPDATE SET best_move=excluded.best_move,"
      "engine_depth=excluded.engine_depth,evaluation_cp=excluded.evaluation_cp,"
      "mate_in=excluded.mate_in,wdl_wins=excluded.wdl_wins,wdl_draws=excluded.wdl_draws,"
      "wdl_losses=excluded.wdl_losses,nodes=excluded.nodes,"
      "analysis_timestamp=excluded.analysis_timestamp,stockfish_version=excluded.stockfish_version,"
      "config_hash=excluded.config_hash;");
  sqlite3_bind_text(statement.get(), 1, run_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(statement.get(), 2, ply);
  sqlite3_bind_text(statement.get(), 3, result.best_move.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(statement.get(), 4, result.reached_depth);
  bind_optional_int(statement.get(), 5, primary == nullptr ? std::nullopt : primary->evaluation_cp);
  bind_optional_int(statement.get(), 6, primary == nullptr ? std::nullopt : primary->mate_in);
  bind_optional_int(
      statement.get(), 7,
      primary != nullptr && primary->wdl.has_value()
          ? std::optional<int>(primary->wdl->wins)
          : std::nullopt);
  bind_optional_int(
      statement.get(), 8,
      primary != nullptr && primary->wdl.has_value()
          ? std::optional<int>(primary->wdl->draws)
          : std::nullopt);
  bind_optional_int(
      statement.get(), 9,
      primary != nullptr && primary->wdl.has_value()
          ? std::optional<int>(primary->wdl->losses)
          : std::nullopt);
  sqlite3_bind_int64(statement.get(), 10, static_cast<sqlite3_int64>(result.nodes));
  sqlite3_bind_int64(statement.get(), 11, analysis_timestamp);
  sqlite3_bind_text(statement.get(), 12, engine_version.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(statement.get(), 13, config_hash.c_str(), -1, SQLITE_TRANSIENT);
  check(sqlite3_step(statement.get()), db_, "persist Stockfish move result");

  auto clear = prepare(db_, "DELETE FROM engine_lines WHERE analysis_run_id=? AND ply=?;");
  sqlite3_bind_text(clear.get(), 1, run_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(clear.get(), 2, ply);
  check(sqlite3_step(clear.get()), db_, "replace engine lines");
  auto line_statement = prepare(
      db_,
      "INSERT INTO engine_lines(analysis_run_id,ply,rank,engine_depth,evaluation_cp,"
      "mate_in,wdl_wins,wdl_draws,wdl_losses,nodes,best_move,principal_variation) "
      "VALUES(?,?,?,?,?,?,?,?,?,?,?,?);");
  for (const auto& line : result.lines) {
    sqlite3_reset(line_statement.get());
    sqlite3_clear_bindings(line_statement.get());
    sqlite3_bind_text(line_statement.get(), 1, run_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(line_statement.get(), 2, ply);
    sqlite3_bind_int(line_statement.get(), 3, line.rank);
    sqlite3_bind_int(line_statement.get(), 4, line.depth);
    bind_optional_int(line_statement.get(), 5, line.evaluation_cp);
    bind_optional_int(line_statement.get(), 6, line.mate_in);
    bind_optional_int(
        line_statement.get(), 7,
        line.wdl.has_value() ? std::optional<int>(line.wdl->wins) : std::nullopt);
    bind_optional_int(
        line_statement.get(), 8,
        line.wdl.has_value() ? std::optional<int>(line.wdl->draws) : std::nullopt);
    bind_optional_int(
        line_statement.get(), 9,
        line.wdl.has_value() ? std::optional<int>(line.wdl->losses) : std::nullopt);
    sqlite3_bind_int64(line_statement.get(), 10, static_cast<sqlite3_int64>(line.nodes));
    const auto best_move = line.best_move();
    const auto pv = join_moves(line.moves);
    sqlite3_bind_text(line_statement.get(), 11, best_move.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(line_statement.get(), 12, pv.c_str(), -1, SQLITE_TRANSIENT);
    check(sqlite3_step(line_statement.get()), db_, "persist MultiPV line");
  }
  auto progress = prepare(
      db_,
      "UPDATE analysis_runs SET completed_plies=?,status='running',"
      "error=NULL WHERE id=?;");
  sqlite3_bind_int(progress.get(), 1, completed_plies);
  sqlite3_bind_text(progress.get(), 2, run_id.c_str(), -1, SQLITE_TRANSIENT);
  check(sqlite3_step(progress.get()), db_, "update Stockfish analysis progress");
    execute("COMMIT;");
  } catch (...) {
    sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
    throw;
  }
}

void Database::set_analysis_status(
    const std::string& game_id,
    const std::string& config_hash,
    const std::string& status,
    const std::string& error) {
  auto statement = prepare(
      db_,
      "UPDATE analysis_runs SET status=?,error=?,completed_at=CASE WHEN ?='complete' "
      "THEN ? ELSE completed_at END WHERE game_id=? AND config_hash=?;");
  sqlite3_bind_text(statement.get(), 1, status.c_str(), -1, SQLITE_TRANSIENT);
  if (error.empty()) {
    sqlite3_bind_null(statement.get(), 2);
  } else {
    sqlite3_bind_text(statement.get(), 2, error.c_str(), -1, SQLITE_TRANSIENT);
  }
  sqlite3_bind_text(statement.get(), 3, status.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(statement.get(), 4, unix_time_seconds());
  sqlite3_bind_text(statement.get(), 5, game_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(statement.get(), 6, config_hash.c_str(), -1, SQLITE_TRANSIENT);
  check(sqlite3_step(statement.get()), db_, "set analysis status");
  // Engine progress mutates analysis_runs frequently, but only the transition
  // to a completed run changes the source selected by player_knowledge_json.
  // Invalidate exactly at that semantic boundary instead of on every ply.
  if (status == "complete" && sqlite3_changes(db_) > 0) {
    statistics_source_revision_.fetch_add(1, std::memory_order_relaxed);
  }
}
std::vector<int> Database::analyzed_position_slots(
    const std::string& game_id,
    const std::string& config_hash) const {
  auto statement = prepare(
      db_,
      "SELECT ma.ply FROM move_analysis ma "
      "JOIN analysis_runs ar ON ar.id=ma.analysis_run_id "
      "WHERE ar.game_id=? AND ar.config_hash=? "
      "ORDER BY ma.ply;");
  sqlite3_bind_text(statement.get(), 1, game_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(statement.get(), 2, config_hash.c_str(), -1, SQLITE_TRANSIENT);
  std::vector<int> slots;
  while (sqlite3_step(statement.get()) == SQLITE_ROW) {
    slots.push_back(sqlite3_column_int(statement.get(), 0));
  }
  return slots;
}


std::pair<std::optional<AnalysisResult>, std::optional<AnalysisResult>>
Database::adjacent_analysis_results(
    const std::string& game_id,
    const std::string& config_hash,
    const int ply) const {
  auto statement = prepare(
      db_,
      "SELECT ma.ply,ma.best_move,ma.engine_depth,ma.nodes,"
      "el.rank,el.engine_depth,el.evaluation_cp,el.mate_in,el.wdl_wins,"
      "el.wdl_draws,el.wdl_losses,el.nodes,el.principal_variation "
      "FROM analysis_runs ar "
      "JOIN move_analysis ma ON ma.analysis_run_id=ar.id "
      "LEFT JOIN engine_lines el ON el.analysis_run_id=ma.analysis_run_id "
      "AND el.ply=ma.ply "
      "WHERE ar.game_id=? AND ar.config_hash=? AND ma.ply IN (?,?) "
      "ORDER BY ma.ply,el.rank;");
  sqlite3_bind_text(statement.get(), 1, game_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(statement.get(), 2, config_hash.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(statement.get(), 3, ply);
  sqlite3_bind_int(statement.get(), 4, ply + 1);

  std::pair<std::optional<AnalysisResult>, std::optional<AnalysisResult>> results;
  auto result_for = [&](const int row_ply) -> std::optional<AnalysisResult>& {
    return row_ply == ply ? results.first : results.second;
  };

  while (sqlite3_step(statement.get()) == SQLITE_ROW) {
    const int row_ply = sqlite3_column_int(statement.get(), 0);
    if (row_ply != ply && row_ply != ply + 1) continue;
    auto& result = result_for(row_ply);
    if (!result.has_value()) {
      result = AnalysisResult{};
      result->best_move = text_column(statement.get(), 1);
      result->reached_depth = sqlite3_column_int(statement.get(), 2);
      result->nodes = static_cast<std::uint64_t>(sqlite3_column_int64(statement.get(), 3));
    }
    if (sqlite3_column_type(statement.get(), 4) == SQLITE_NULL) continue;

    EngineLine line;
    line.rank = sqlite3_column_int(statement.get(), 4);
    line.depth = sqlite3_column_int(statement.get(), 5);
    line.evaluation_cp = optional_int_column(statement.get(), 6);
    line.mate_in = optional_int_column(statement.get(), 7);
    if (sqlite3_column_type(statement.get(), 8) != SQLITE_NULL) {
      line.wdl = WdlScore{
          .wins = sqlite3_column_int(statement.get(), 8),
          .draws = sqlite3_column_int(statement.get(), 9),
          .losses = sqlite3_column_int(statement.get(), 10),
      };
    }
    line.nodes = static_cast<std::uint64_t>(sqlite3_column_int64(statement.get(), 11));
    std::istringstream pv(text_column(statement.get(), 12));
    std::string move;
    while (pv >> move) line.moves.push_back(move);
    result->lines.push_back(std::move(line));
  }
  return results;
}


bool Database::classification_is_current(
    const std::string& game_id,
    const std::string& config_hash,
    const int classifier_version,
    const int accuracy_version,
    const std::string& opening_book_version) const {
  auto statement = prepare(
      db_,
      "SELECT 1 FROM analysis_runs WHERE game_id=? AND config_hash=? "
      "AND classifier_version=? AND accuracy_algorithm_version=? "
      "AND opening_book_version=? LIMIT 1;");
  sqlite3_bind_text(statement.get(), 1, game_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(statement.get(), 2, config_hash.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(statement.get(), 3, classifier_version);
  sqlite3_bind_int(statement.get(), 4, accuracy_version);
  sqlite3_bind_text(
      statement.get(), 5, opening_book_version.c_str(), -1, SQLITE_TRANSIENT);
  return sqlite3_step(statement.get()) == SQLITE_ROW;
}

void Database::persist_classifications(
    const std::string& game_id,
    const std::string& config_hash,
    const std::vector<MoveClassificationRecord>& records,
    const std::optional<double>& white_accuracy,
    const std::optional<double>& black_accuracy,
    const int classifier_version,
    const int accuracy_version,
    const std::string& opening_book_version,
    const bool finalize) {
  auto run = prepare(
      db_, "SELECT id FROM analysis_runs WHERE game_id=? AND config_hash=?;");
  sqlite3_bind_text(run.get(), 1, game_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(run.get(), 2, config_hash.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(run.get()) != SQLITE_ROW) throw std::runtime_error("Analysis run not found");
  const std::string run_id = text_column(run.get(), 0);

  std::array<int, 11> counts{};
  auto count_index = [](const MoveCategory category) -> int {
    switch (category) {
      case MoveCategory::theory: return 0;
      case MoveCategory::forced: return 1;
      case MoveCategory::brilliant: return 2;
      case MoveCategory::critical: return 3;
      case MoveCategory::best: return 4;
      case MoveCategory::excellent: return 5;
      case MoveCategory::good: return 6;
      case MoveCategory::okay: return 7;
      case MoveCategory::miss: return 8;
      case MoveCategory::mistake: return 9;
      case MoveCategory::blunder: return 10;
      case MoveCategory::unknown: return -1;
    }
    return -1;
  };
  SqliteConnectionTransactionLock transaction_lock(db_);
  execute("BEGIN IMMEDIATE;");
  try {
    auto update = prepare(
        db_,
        "UPDATE move_analysis SET category=?,is_theory=?,classifier_version=?,"
        "expected_score_before=?,expected_score_best=?,expected_score_played=?,"
        "expected_score_loss=?,recommended_move=?,theory_games=?,theory_white_wins=?,"
        "theory_draws=?,theory_black_wins=?,move_accuracy=?,accuracy_weight=? "
        "WHERE analysis_run_id=? AND ply=?;");
    for (const auto& record : records) {
      sqlite3_reset(update.get());
      sqlite3_clear_bindings(update.get());
      const auto name = category_name(record.classification);
      sqlite3_bind_text(update.get(), 1, name.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_int(update.get(), 2, record.theory.is_theory ? 1 : 0);
      sqlite3_bind_int(update.get(), 3, record.classifier_version);
      bind_optional_double(update.get(), 4, record.expected_score_before);
      bind_optional_double(update.get(), 5, record.expected_score_best);
      bind_optional_double(update.get(), 6, record.expected_score_played);
      bind_optional_double(update.get(), 7, record.expected_score_loss);
      if (record.recommended_move.empty()) sqlite3_bind_null(update.get(), 8);
      else sqlite3_bind_text(
          update.get(), 8, record.recommended_move.c_str(), -1, SQLITE_TRANSIENT);
      if (record.theory.is_theory) {
        sqlite3_bind_int64(update.get(), 9, record.theory.games);
        sqlite3_bind_int64(update.get(), 10, record.theory.white_wins);
        sqlite3_bind_int64(update.get(), 11, record.theory.draws);
        sqlite3_bind_int64(update.get(), 12, record.theory.black_wins);
      } else {
        for (int index = 9; index <= 12; ++index) sqlite3_bind_null(update.get(), index);
      }
      bind_optional_double(update.get(), 13, record.move_accuracy);
      // Always written, zero for theory or an unscorable move: a NULL weight
      // then means only "classified before per-move accuracy existed".
      sqlite3_bind_double(update.get(), 14, record.accuracy_weight);
      sqlite3_bind_text(update.get(), 15, run_id.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_int(update.get(), 16, record.ply);
      check(sqlite3_step(update.get()), db_, "persist move classification");
      const int index = count_index(record.classification);
      if (index >= 0) ++counts[static_cast<std::size_t>(index)];
    }
    std::optional<double> combined_accuracy;
    if (white_accuracy.has_value() && black_accuracy.has_value()) {
      combined_accuracy = (*white_accuracy + *black_accuracy) / 2.0;
    } else if (white_accuracy.has_value()) combined_accuracy = white_accuracy;
    else if (black_accuracy.has_value()) combined_accuracy = black_accuracy;

    if (finalize) {
      // Category totals are derived from move_analysis when an analysis is read.
      // Keep the legacy aggregate columns updated only for the final complete
      // pass, where `records` contains the whole game. Incremental classification
      // batches must never overwrite these compatibility counters with a one-move
      // subset.
      auto summary = prepare(
          db_,
          "UPDATE analysis_runs SET classifier_version=?,accuracy_algorithm_version=?,"
          "opening_book_version=?,white_local_accuracy=?,black_local_accuracy=?,"
          "local_accuracy=?,theory_count=?,brilliant_count=?,best_count=?,excellent_count=?,"
          "okay_count=?,miss_count=?,mistake_count=?,blunder_count=? WHERE id=?;");
      sqlite3_bind_int(summary.get(), 1, classifier_version);
      sqlite3_bind_int(summary.get(), 2, accuracy_version);
      sqlite3_bind_text(
          summary.get(), 3, opening_book_version.c_str(), -1, SQLITE_TRANSIENT);
      bind_optional_double(summary.get(), 4, white_accuracy);
      bind_optional_double(summary.get(), 5, black_accuracy);
      bind_optional_double(summary.get(), 6, combined_accuracy);
      // Legacy aggregate columns do not have forced/good fields. move_analysis is
      // authoritative, so fold Forced into Best and Good into Okay only for these
      // compatibility counters. Read-time summaries keep all classifier categories apart.
      sqlite3_bind_int(summary.get(), 7, counts[0]);
      sqlite3_bind_int(summary.get(), 8, counts[2]);
      sqlite3_bind_int(summary.get(), 9, counts[4] + counts[1]);
      sqlite3_bind_int(summary.get(), 10, counts[5]);
      sqlite3_bind_int(summary.get(), 11, counts[7] + counts[6]);
      sqlite3_bind_int(summary.get(), 12, counts[8]);
      sqlite3_bind_int(summary.get(), 13, counts[9]);
      sqlite3_bind_int(summary.get(), 14, counts[10]);
      sqlite3_bind_text(summary.get(), 15, run_id.c_str(), -1, SQLITE_TRANSIENT);
      check(sqlite3_step(summary.get()), db_, "persist classification summary");
    } else {
      // Partial move labels are publishable immediately, but the run-level
      // classifier/accuracy contract becomes current only after the final
      // game-wide pass. Keep the transaction short and leave compatibility
      // aggregate counters untouched until then.
      auto summary = prepare(
          db_,
          "UPDATE analysis_runs SET classifier_version=0,accuracy_algorithm_version=0,"
          "opening_book_version=?,white_local_accuracy=NULL,black_local_accuracy=NULL,"
          "local_accuracy=NULL WHERE id=?;");
      sqlite3_bind_text(
          summary.get(), 1, opening_book_version.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(summary.get(), 2, run_id.c_str(), -1, SQLITE_TRANSIENT);
      check(sqlite3_step(summary.get()), db_, "persist partial classification state");
    }
    if (finalize) {
      auto game = prepare(db_, "UPDATE games SET local_accuracy=? WHERE id=?;");
      bind_optional_double(game.get(), 1, combined_accuracy);
      sqlite3_bind_text(game.get(), 2, game_id.c_str(), -1, SQLITE_TRANSIENT);
      check(sqlite3_step(game.get()), db_, "persist game accuracy");
    }
    execute("COMMIT;");
  } catch (...) {
    sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
    throw;
  }
}

std::optional<PersistedAnalysis> Database::analysis(
    const std::string& game_id,
    const std::string& config_hash,
    const int requested_ply) const {
  auto statement = prepare(
      db_,
      "SELECT id,status,config_hash,engine_version,completed_plies,total_plies,"
      "error,depth,classifier_version,accuracy_algorithm_version,opening_book_version,"
      "white_local_accuracy,black_local_accuracy FROM analysis_runs "
      "WHERE game_id=? AND config_hash=?;");
  sqlite3_bind_text(statement.get(), 1, game_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(statement.get(), 2, config_hash.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(statement.get()) != SQLITE_ROW) return std::nullopt;
  const std::string run_id = text_column(statement.get(), 0);
  PersistedAnalysis result;
  result.status = text_column(statement.get(), 1);
  result.config_hash = text_column(statement.get(), 2);
  result.engine_version = text_column(statement.get(), 3);
  result.completed_plies = sqlite3_column_int(statement.get(), 4);
  result.total_plies = sqlite3_column_int(statement.get(), 5);
  result.error = text_column(statement.get(), 6);
  result.summary.engine_depth = sqlite3_column_int(statement.get(), 7);
  result.summary.classifier_version = sqlite3_column_int(statement.get(), 8);
  result.summary.accuracy_algorithm_version = sqlite3_column_int(statement.get(), 9);
  result.summary.opening_book_version = text_column(statement.get(), 10);
  result.summary.white.local_accuracy = optional_double_column(statement.get(), 11);
  result.summary.black.local_accuracy = optional_double_column(statement.get(), 12);

  auto totals = prepare(
      db_, "SELECT side_to_move,count(*) FROM game_moves WHERE game_id=? GROUP BY side_to_move;");
  sqlite3_bind_text(totals.get(), 1, game_id.c_str(), -1, SQLITE_TRANSIENT);
  while (sqlite3_step(totals.get()) == SQLITE_ROW) {
    auto& player = text_column(totals.get(), 0) == "black"
        ? result.summary.black : result.summary.white;
    player.total_moves = sqlite3_column_int(totals.get(), 1);
  }
  auto category_counts = prepare(
      db_,
      "SELECT gm.side_to_move,ma.category,count(*) FROM move_analysis ma "
      "JOIN game_moves gm ON gm.game_id=? AND gm.ply_index=ma.ply "
      "WHERE ma.analysis_run_id=? AND ma.classifier_version IS NOT NULL "
      "GROUP BY gm.side_to_move,ma.category;");
  sqlite3_bind_text(category_counts.get(), 1, game_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(category_counts.get(), 2, run_id.c_str(), -1, SQLITE_TRANSIENT);
  while (sqlite3_step(category_counts.get()) == SQLITE_ROW) {
    auto& player = text_column(category_counts.get(), 0) == "black"
        ? result.summary.black : result.summary.white;
    const auto category = parse_category(text_column(category_counts.get(), 1));
    const int count = sqlite3_column_int(category_counts.get(), 2);
    increment_category(player, category, count);
    if (category != MoveCategory::unknown) player.analyzed_moves += count;
  }

  // Engine rows use position slots: slot 0 is the position before move 0,
  // slot N is the position after move N-1.  Public move analysis remains
  // move-indexed, so the default snapshot must never expose the synthetic
  // final position slot as an extra ply.
  int display_ply = requested_ply;
  if (display_ply < 0) {
    if (result.total_plies <= 0) display_ply = 0;
    else if (result.completed_plies <= 0) display_ply = 0;
    else display_ply = std::min(result.completed_plies - 1, result.total_plies - 1);
  }
  auto latest = prepare(
      db_,
      "SELECT ply,best_move,category,classifier_version,expected_score_before,"
      "expected_score_best,expected_score_played,expected_score_loss,recommended_move,"
      "is_theory,theory_games,theory_white_wins,theory_draws,theory_black_wins "
      "FROM move_analysis WHERE analysis_run_id=? AND ply=? LIMIT 1;");
  sqlite3_bind_text(latest.get(), 1, run_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(latest.get(), 2, display_ply);
  if (sqlite3_step(latest.get()) == SQLITE_ROW) {
    result.latest_ply = sqlite3_column_int(latest.get(), 0);
    result.best_move = text_column(latest.get(), 1);
    if (sqlite3_column_type(latest.get(), 3) != SQLITE_NULL) {
      result.classification = parse_category(text_column(latest.get(), 2));
      result.classifier_version = sqlite3_column_int(latest.get(), 3);
    }
    result.expected_score_before = optional_double_column(latest.get(), 4);
    result.expected_score_best = optional_double_column(latest.get(), 5);
    result.expected_score_played = optional_double_column(latest.get(), 6);
    result.expected_score_loss = optional_double_column(latest.get(), 7);
    result.recommended_move = text_column(latest.get(), 8);
    if (sqlite3_column_int(latest.get(), 9) != 0) {
      result.theory = TheoryMoveInfo{
          .is_theory = true,
          .games = static_cast<std::uint32_t>(sqlite3_column_int64(latest.get(), 10)),
          .white_wins = static_cast<std::uint32_t>(sqlite3_column_int64(latest.get(), 11)),
          .draws = static_cast<std::uint32_t>(sqlite3_column_int64(latest.get(), 12)),
          .black_wins = static_cast<std::uint32_t>(sqlite3_column_int64(latest.get(), 13)),
      };
    }
    auto lines = prepare(
        db_,
        "SELECT rank,engine_depth,evaluation_cp,mate_in,wdl_wins,wdl_draws,"
        "wdl_losses,nodes,principal_variation FROM engine_lines WHERE "
        "analysis_run_id=? AND ply=? ORDER BY rank;");
    sqlite3_bind_text(lines.get(), 1, run_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(lines.get(), 2, result.latest_ply);
    while (sqlite3_step(lines.get()) == SQLITE_ROW) {
      EngineLine line;
      line.rank = sqlite3_column_int(lines.get(), 0);
      line.depth = sqlite3_column_int(lines.get(), 1);
      line.evaluation_cp = optional_int_column(lines.get(), 2);
      line.mate_in = optional_int_column(lines.get(), 3);
      if (sqlite3_column_type(lines.get(), 4) != SQLITE_NULL) {
        line.wdl = WdlScore{
            .wins = sqlite3_column_int(lines.get(), 4),
            .draws = sqlite3_column_int(lines.get(), 5),
            .losses = sqlite3_column_int(lines.get(), 6),
        };
      }
      line.nodes = static_cast<std::uint64_t>(sqlite3_column_int64(lines.get(), 7));
      std::istringstream pv(text_column(lines.get(), 8));
      std::string move;
      while (pv >> move) line.moves.push_back(move);
      result.lines.push_back(std::move(line));
    }
  }
  return result;
}

std::optional<PersistedAnalysis> Database::compatible_analysis(
    const std::string& game_id,
    const std::string& engine_version,
    const AppSettings& requested,
    const int requested_ply) const {
  auto statement = prepare(
      db_,
      "SELECT config_hash FROM analysis_runs WHERE game_id=? AND status='complete' "
      "AND engine_analysis_version='2' AND engine_version=? AND depth>=? AND multi_pv>=? "
      "AND (time_limit_seconds=0 OR (? > 0 AND time_limit_seconds>=?)) "
      "AND (adaptive_early_stop=0 OR ?=1) "
      "ORDER BY depth DESC,multi_pv DESC,"
      "CASE WHEN time_limit_seconds=0 THEN 2147483647 ELSE time_limit_seconds END DESC,"
      "adaptive_early_stop ASC,completed_at DESC LIMIT 1;");
  sqlite3_bind_text(statement.get(), 1, game_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(statement.get(), 2, engine_version.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(statement.get(), 3, requested.depth);
  sqlite3_bind_int(statement.get(), 4, requested.multi_pv);
  sqlite3_bind_int(statement.get(), 5, requested.time_limit_seconds);
  sqlite3_bind_int(statement.get(), 6, requested.time_limit_seconds);
  sqlite3_bind_int(statement.get(), 7, requested.adaptive_early_stop ? 1 : 0);
  if (sqlite3_step(statement.get()) != SQLITE_ROW) return std::nullopt;
  return analysis(game_id, text_column(statement.get(), 0), requested_ply);
}


void Database::prune_game_analyses_except(
    const std::string& game_id, const std::string& keep_config_hash) {
  SqliteConnectionTransactionLock transaction_lock(db_);
  execute("BEGIN IMMEDIATE;");
  try {
    auto prune = prepare(
        db_, "DELETE FROM analysis_runs WHERE game_id=? AND config_hash<>?;");
    sqlite3_bind_text(prune.get(), 1, game_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(
        prune.get(), 2, keep_config_hash.c_str(), -1, SQLITE_TRANSIENT);
    check(sqlite3_step(prune.get()), db_, "prune superseded game analyses");
    execute("COMMIT;");
  } catch (...) {
    sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
    throw;
  }
}

void Database::delete_game_analyses(const std::string& game_id) {
  SqliteConnectionTransactionLock transaction_lock(db_);
  execute("BEGIN IMMEDIATE;");
  try {
    auto remove = prepare(db_, "DELETE FROM analysis_runs WHERE game_id=?;");
    sqlite3_bind_text(remove.get(), 1, game_id.c_str(), -1, SQLITE_TRANSIENT);
    check(sqlite3_step(remove.get()), db_, "delete game analyses");

    auto clear_accuracy = prepare(
        db_, "UPDATE games SET local_accuracy=NULL WHERE id=?;");
    sqlite3_bind_text(
        clear_accuracy.get(), 1, game_id.c_str(), -1, SQLITE_TRANSIENT);
    check(sqlite3_step(clear_accuracy.get()), db_, "clear deleted analysis accuracy");
    execute("COMMIT;");
    // User deletion is authoritative too: derived profile evidence must be
    // rebuilt from the now-weaker shared cache state instead of retaining a
    // stale analysis-derived conclusion.
    notify_ai_profile_analysis_changed(game_id);
  } catch (...) {
    sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
    throw;
  }
}

std::optional<AnalysisResult> Database::compatible_position_analysis(
    const std::string& position_fen,
    const std::string& engine_version,
    const AppSettings& requested) const {
  auto statement = prepare(
      db_,
      "SELECT id,reached_depth,nodes,best_move FROM engine_position_cache "
      "WHERE position_fen=? AND stockfish_version=? AND depth>=? AND multi_pv>=? "
      "AND (time_limit_seconds=0 OR (? > 0 AND time_limit_seconds>=?)) "
      "ORDER BY depth ASC,multi_pv ASC,"
      "CASE WHEN time_limit_seconds=0 THEN 2147483647 ELSE time_limit_seconds END ASC,"
      "analyzed_at DESC LIMIT 1;");
  sqlite3_bind_text(statement.get(), 1, position_fen.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(statement.get(), 2, engine_version.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(statement.get(), 3, requested.depth);
  sqlite3_bind_int(statement.get(), 4, requested.multi_pv);
  sqlite3_bind_int(statement.get(), 5, requested.time_limit_seconds);
  sqlite3_bind_int(statement.get(), 6, requested.time_limit_seconds);
  if (sqlite3_step(statement.get()) != SQLITE_ROW) return std::nullopt;

  const auto cache_id = sqlite3_column_int64(statement.get(), 0);
  {
    auto touch = prepare(
        db_,
        "UPDATE engine_position_cache SET last_used_at=? WHERE id=?;");
    sqlite3_bind_int64(touch.get(), 1, unix_time_seconds());
    sqlite3_bind_int64(touch.get(), 2, cache_id);
    step_nonessential_cache_touch(touch.get(), db_, "touch global position cache");
  }
  AnalysisResult result;
  result.reached_depth = sqlite3_column_int(statement.get(), 1);
  result.nodes = static_cast<std::uint64_t>(sqlite3_column_int64(statement.get(), 2));
  result.best_move = text_column(statement.get(), 3);

  auto lines = prepare(
      db_,
      "SELECT rank,engine_depth,evaluation_cp,mate_in,wdl_wins,wdl_draws,"
      "wdl_losses,nodes,principal_variation FROM engine_position_cache_lines "
      "WHERE cache_id=? ORDER BY rank LIMIT ?;");
  sqlite3_bind_int64(lines.get(), 1, cache_id);
  sqlite3_bind_int(lines.get(), 2, requested.multi_pv);
  while (sqlite3_step(lines.get()) == SQLITE_ROW) {
    EngineLine line;
    line.rank = sqlite3_column_int(lines.get(), 0);
    line.depth = sqlite3_column_int(lines.get(), 1);
    line.evaluation_cp = optional_int_column(lines.get(), 2);
    line.mate_in = optional_int_column(lines.get(), 3);
    if (sqlite3_column_type(lines.get(), 4) != SQLITE_NULL) {
      line.wdl = WdlScore{
          .wins = sqlite3_column_int(lines.get(), 4),
          .draws = sqlite3_column_int(lines.get(), 5),
          .losses = sqlite3_column_int(lines.get(), 6),
      };
    }
    line.nodes = static_cast<std::uint64_t>(sqlite3_column_int64(lines.get(), 7));
    std::istringstream pv(text_column(lines.get(), 8));
    std::string move;
    while (pv >> move) line.moves.push_back(move);
    result.lines.push_back(std::move(line));
  }
  return result;
}


std::optional<AnalysisResult> Database::best_position_checkpoint(
    const std::string& position_fen,
    const std::string& engine_version,
    const int maximum_depth,
    const int multi_pv) const {
  auto statement = prepare(
      db_,
      "SELECT id,reached_depth,nodes,best_move FROM engine_position_cache "
      "WHERE position_fen=? AND stockfish_version=? AND depth<=? AND multi_pv>=? "
      "ORDER BY reached_depth DESC,depth DESC,analyzed_at DESC LIMIT 1;");
  sqlite3_bind_text(statement.get(), 1, position_fen.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(statement.get(), 2, engine_version.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(statement.get(), 3, maximum_depth);
  sqlite3_bind_int(statement.get(), 4, multi_pv);
  if (sqlite3_step(statement.get()) != SQLITE_ROW) return std::nullopt;

  const auto cache_id = sqlite3_column_int64(statement.get(), 0);
  {
    auto touch = prepare(
        db_,
        "UPDATE engine_position_cache SET last_used_at=? WHERE id=?;");
    sqlite3_bind_int64(touch.get(), 1, unix_time_seconds());
    sqlite3_bind_int64(touch.get(), 2, cache_id);
    step_nonessential_cache_touch(touch.get(), db_, "touch position checkpoint");
  }
  AnalysisResult result;
  result.reached_depth = sqlite3_column_int(statement.get(), 1);
  result.nodes = static_cast<std::uint64_t>(sqlite3_column_int64(statement.get(), 2));
  result.best_move = text_column(statement.get(), 3);

  auto lines = prepare(
      db_,
      "SELECT rank,engine_depth,evaluation_cp,mate_in,wdl_wins,wdl_draws,"
      "wdl_losses,nodes,principal_variation FROM engine_position_cache_lines "
      "WHERE cache_id=? ORDER BY rank LIMIT ?;");
  sqlite3_bind_int64(lines.get(), 1, cache_id);
  sqlite3_bind_int(lines.get(), 2, multi_pv);
  while (sqlite3_step(lines.get()) == SQLITE_ROW) {
    EngineLine line;
    line.rank = sqlite3_column_int(lines.get(), 0);
    line.depth = sqlite3_column_int(lines.get(), 1);
    line.evaluation_cp = optional_int_column(lines.get(), 2);
    line.mate_in = optional_int_column(lines.get(), 3);
    if (sqlite3_column_type(lines.get(), 4) != SQLITE_NULL) {
      line.wdl = WdlScore{
          .wins = sqlite3_column_int(lines.get(), 4),
          .draws = sqlite3_column_int(lines.get(), 5),
          .losses = sqlite3_column_int(lines.get(), 6),
      };
    }
    line.nodes = static_cast<std::uint64_t>(sqlite3_column_int64(lines.get(), 7));
    std::istringstream pv(text_column(lines.get(), 8));
    std::string move;
    while (pv >> move) line.moves.push_back(move);
    result.lines.push_back(std::move(line));
  }
  return result;
}

void Database::persist_position_analysis(
    const std::string& position_fen,
    const std::string& engine_version,
    const AppSettings& settings,
    const AnalysisResult& result,
    const std::int64_t analysis_timestamp) {
  SqliteConnectionTransactionLock transaction_lock(db_);
  execute("BEGIN IMMEDIATE;");
  try {
    auto cache = prepare(
        db_,
        "INSERT INTO engine_position_cache(position_fen,stockfish_version,depth,multi_pv,"
        "time_limit_seconds,reached_depth,nodes,best_move,analyzed_at,last_used_at) "
        "VALUES(?,?,?,?,?,?,?,?,?,?) "
        "ON CONFLICT(position_fen,stockfish_version,depth,multi_pv,time_limit_seconds) "
        "DO UPDATE SET reached_depth=excluded.reached_depth,nodes=excluded.nodes,"
        "best_move=excluded.best_move,analyzed_at=excluded.analyzed_at,"
        "last_used_at=excluded.last_used_at;");
    sqlite3_bind_text(cache.get(), 1, position_fen.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(cache.get(), 2, engine_version.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(cache.get(), 3, settings.depth);
    sqlite3_bind_int(cache.get(), 4, settings.multi_pv);
    sqlite3_bind_int(cache.get(), 5, settings.time_limit_seconds);
    sqlite3_bind_int(cache.get(), 6, result.reached_depth);
    sqlite3_bind_int64(cache.get(), 7, static_cast<sqlite3_int64>(result.nodes));
    sqlite3_bind_text(cache.get(), 8, result.best_move.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(cache.get(), 9, analysis_timestamp);
    sqlite3_bind_int64(cache.get(), 10, analysis_timestamp);
    check(sqlite3_step(cache.get()), db_, "persist global position cache");

    auto find_id = prepare(
        db_,
        "SELECT id FROM engine_position_cache WHERE position_fen=? AND stockfish_version=? "
        "AND depth=? AND multi_pv=? AND time_limit_seconds=?;");
    sqlite3_bind_text(find_id.get(), 1, position_fen.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(find_id.get(), 2, engine_version.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(find_id.get(), 3, settings.depth);
    sqlite3_bind_int(find_id.get(), 4, settings.multi_pv);
    sqlite3_bind_int(find_id.get(), 5, settings.time_limit_seconds);
    if (sqlite3_step(find_id.get()) != SQLITE_ROW) {
      throw std::runtime_error("Global position cache row not found after upsert");
    }
    const auto cache_id = sqlite3_column_int64(find_id.get(), 0);

    auto clear_lines = prepare(db_, "DELETE FROM engine_position_cache_lines WHERE cache_id=?;");
    sqlite3_bind_int64(clear_lines.get(), 1, cache_id);
    check(sqlite3_step(clear_lines.get()), db_, "clear global position cache lines");

    auto line_statement = prepare(
        db_,
        "INSERT INTO engine_position_cache_lines(cache_id,rank,engine_depth,evaluation_cp,"
        "mate_in,wdl_wins,wdl_draws,wdl_losses,nodes,principal_variation) "
        "VALUES(?,?,?,?,?,?,?,?,?,?);");
    for (const auto& line : result.lines) {
      sqlite3_reset(line_statement.get());
      sqlite3_clear_bindings(line_statement.get());
      sqlite3_bind_int64(line_statement.get(), 1, cache_id);
      sqlite3_bind_int(line_statement.get(), 2, line.rank);
      sqlite3_bind_int(line_statement.get(), 3, line.depth);
      bind_optional_int(line_statement.get(), 4, line.evaluation_cp);
      bind_optional_int(line_statement.get(), 5, line.mate_in);
      bind_optional_int(
          line_statement.get(), 6,
          line.wdl.has_value() ? std::optional<int>(line.wdl->wins) : std::nullopt);
      bind_optional_int(
          line_statement.get(), 7,
          line.wdl.has_value() ? std::optional<int>(line.wdl->draws) : std::nullopt);
      bind_optional_int(
          line_statement.get(), 8,
          line.wdl.has_value() ? std::optional<int>(line.wdl->losses) : std::nullopt);
      sqlite3_bind_int64(line_statement.get(), 9, static_cast<sqlite3_int64>(line.nodes));
      std::ostringstream pv;
      for (std::size_t index = 0; index < line.moves.size(); ++index) {
        if (index != 0) pv << ' ';
        pv << line.moves[index];
      }
      const auto pv_text = pv.str();
      sqlite3_bind_text(line_statement.get(), 10, pv_text.c_str(), -1, SQLITE_TRANSIENT);
      check(sqlite3_step(line_statement.get()), db_, "persist global position cache line");
    }
    execute("COMMIT;");
  } catch (...) {
    sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
    throw;
  }
}


void Database::clear_global_position_cache() {
  execute("DELETE FROM engine_position_cache;");
}

void Database::run_maintenance() {
  // Keep cache growth predictable without touching user-owned library data.
  // 100k positions is large enough to retain common transpositions/openings while
  // preventing a long-lived installation from accumulating an unbounded cache.
  constexpr std::int64_t kMaxPositionCacheEntries = 100000;
  constexpr std::int64_t kFailedRunRetentionSeconds = 30LL * 24LL * 60LL * 60LL;

  SqliteConnectionTransactionLock transaction_lock(db_);
  execute("BEGIN IMMEDIATE;");
  try {
    // Failed/cancelled runs are resumable for a while, then become disposable
    // diagnostics. Completed analyses and active/running partial analyses remain.
    auto prune_runs = prepare(
        db_,
        "DELETE FROM analysis_runs "
        "WHERE status IN ('error','cancelled') AND started_at < ?;");
    sqlite3_bind_int64(
        prune_runs.get(), 1, unix_time_seconds() - kFailedRunRetentionSeconds);
    check(sqlite3_step(prune_runs.get()), db_, "prune stale failed analysis runs");

    auto count = prepare(db_, "SELECT COUNT(*) FROM engine_position_cache;");
    std::int64_t cache_count = 0;
    if (sqlite3_step(count.get()) == SQLITE_ROW) {
      cache_count = sqlite3_column_int64(count.get(), 0);
    }
    if (cache_count > kMaxPositionCacheEntries) {
      const auto remove_count = cache_count - kMaxPositionCacheEntries;
      auto prune_cache = prepare(
          db_,
          "DELETE FROM engine_position_cache WHERE id IN ("
          "SELECT id FROM engine_position_cache "
          "ORDER BY last_used_at ASC, analyzed_at ASC LIMIT ?"
          ");");
      sqlite3_bind_int64(prune_cache.get(), 1, remove_count);
      check(sqlite3_step(prune_cache.get()), db_, "prune global position cache");
    }

    execute("COMMIT;");
  } catch (...) {
    sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
    throw;
  }

  // Let SQLite checkpoint old WAL pages without blocking readers for a full VACUUM.
  // The existing logger already rotates at 1 MiB, so log growth is independently bounded.
  sqlite3_exec(db_, "PRAGMA wal_checkpoint(PASSIVE);", nullptr, nullptr, nullptr);
}

}  // namespace kchess
