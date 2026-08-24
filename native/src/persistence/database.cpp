#include "persistence/database.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <iomanip>
#include <memory>
#include <random>
#include <sstream>
#include <stdexcept>

#include "sqlite3.h"

namespace kchess {
namespace {

using Statement = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>;

void check(const int result, sqlite3* db, const char* operation) {
  if (result != SQLITE_OK && result != SQLITE_DONE && result != SQLITE_ROW) {
    throw std::runtime_error(
        std::string(operation) + ": " + (db == nullptr ? "unknown" : sqlite3_errmsg(db)));
  }
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
  if (value == "brilliant") return MoveCategory::brilliant;
  if (value == "critical") return MoveCategory::critical;
  if (value == "best") return MoveCategory::best;
  if (value == "excellent") return MoveCategory::excellent;
  if (value == "okay") return MoveCategory::okay;
  if (value == "miss") return MoveCategory::miss;
  if (value == "mistake") return MoveCategory::mistake;
  if (value == "blunder") return MoveCategory::blunder;
  return MoveCategory::unknown;
}

std::string category_name(const MoveCategory value) {
  switch (value) {
    case MoveCategory::theory: return "theory";
    case MoveCategory::brilliant: return "brilliant";
    case MoveCategory::critical: return "critical";
    case MoveCategory::best: return "best";
    case MoveCategory::excellent: return "excellent";
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
    case MoveCategory::brilliant: summary.brilliant += count; break;
    case MoveCategory::critical: summary.critical += count; break;
    case MoveCategory::best: summary.best += count; break;
    case MoveCategory::excellent: summary.excellent += count; break;
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

constexpr const char* kGameColumns =
    "g.id,g.profile_id,g.kind,g.white_name,g.black_name,g.white_rating,g.black_rating,"
    "g.result,g.event,g.site,g.game_date,g.time_control,g.pgn,g.starting_fen,g.created_at,"
    "g.provider_game_id,gs.provider_url,g.provider_accuracy_white,g.provider_accuracy_black,"
    "ar.white_local_accuracy,ar.black_local_accuracy,g.provider_outcome,g.time_control_type,"
    "g.provider_ended_at,EXISTS(SELECT 1 FROM favorites f WHERE f.profile_id=g.profile_id "
    "AND f.game_id=g.id),(SELECT f.collection_id FROM favorites f WHERE "
    "f.profile_id=g.profile_id AND f.game_id=g.id),"
    "EXISTS(SELECT 1 FROM favorites f JOIN favorite_collections c ON c.id=f.collection_id "
    "WHERE f.game_id=g.id AND c.name='Downloads' COLLATE NOCASE),(ar.id IS NOT NULL)";

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
  ('locale', 'de');
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
  const int version = schema_version();
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
  result.show_board_coordinates = setting("showBoardCoordinates").value_or("true") == "true";
  result.highlight_last_move = setting("highlightLastMove").value_or("true") == "true";
  result.highlight_selected_square = setting("highlightSelectedSquare").value_or("true") == "true";
  result.auto_sync_online = setting("autoSyncOnline").value_or("true") == "true";
  result.confirm_before_delete = setting("confirmBeforeDelete").value_or("true") == "true";
  result.use_global_analysis_cache = setting("useGlobalAnalysisCache").value_or("true") == "true";
  result.diagnostic_logging = setting("diagnosticLogging").value_or("true") == "true";
  result.theme_mode = setting("themeMode").value_or("system");
  result.locale = setting("locale").value_or("de");
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
  execute("BEGIN IMMEDIATE;");
  try {
    auto statement = prepare(
        db_,
        "INSERT INTO games(id,profile_id,white_name,black_name,result,pgn,created_at,"
        "kind,starting_fen,white_rating,black_rating,event,site,game_date,time_control) "
        "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);");
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
          "provider_outcome,time_control_type,provider_ended_at,provider_rules) "
          "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?) "
          "ON CONFLICT(profile_id,provider,provider_game_id) WHERE provider_game_id IS NOT NULL "
          "DO UPDATE SET white_name=excluded.white_name,black_name=excluded.black_name,"
          "result=excluded.result,pgn=excluded.pgn,white_rating=excluded.white_rating,"
          "black_rating=excluded.black_rating,event=excluded.event,site=excluded.site,"
          "game_date=excluded.game_date,time_control=excluded.time_control,"
          "provider_accuracy_white=excluded.provider_accuracy_white,"
          "provider_accuracy_black=excluded.provider_accuracy_black,"
          "provider_outcome=excluded.provider_outcome,time_control_type=excluded.time_control_type,"
          "provider_ended_at=excluded.provider_ended_at,provider_rules=excluded.provider_rules;");
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
      "opening_eco,opening_name "
      "FROM games WHERE profile_id=? "
      "ORDER BY provider_ended_at DESC, created_at DESC;");
  sqlite3_bind_text(statement.get(), 1, profile_id.c_str(), -1, SQLITE_TRANSIENT);
  std::vector<GameStatRow> result;
  while (sqlite3_step(statement.get()) == SQLITE_ROW) {
    result.push_back(GameStatRow{
        .provider_outcome = text_column(statement.get(), 0),
        .result = text_column(statement.get(), 1),
        .white_name = text_column(statement.get(), 2),
        .black_name = text_column(statement.get(), 3),
        .time_control_type = text_column(statement.get(), 4),
        .opening_eco = text_column(statement.get(), 5),
        .opening_name = text_column(statement.get(), 6),
    });
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

PersistedAnalysis Database::prepare_analysis(
    const std::string& game_id,
    const std::string& config_hash,
    const std::string& engine_version,
    const int total_plies,
    const int depth,
    const int multi_pv,
    const int time_limit_seconds) {
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
      "engine_analysis_version,engine_config_hash,time_limit_seconds) "
      "VALUES(?,? ,?,'Stockfish',?,?,?,'running',?,0,?,?,'2',?,?);");
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

  std::array<int, 9> counts{};
  auto count_index = [](const MoveCategory category) -> int {
    switch (category) {
      case MoveCategory::theory: return 0;
      case MoveCategory::brilliant: return 1;
      case MoveCategory::critical: return 2;
      case MoveCategory::best: return 3;
      case MoveCategory::excellent: return 4;
      case MoveCategory::okay: return 5;
      case MoveCategory::miss: return 6;
      case MoveCategory::mistake: return 7;
      case MoveCategory::blunder: return 8;
      case MoveCategory::unknown: return -1;
    }
    return -1;
  };
  execute("BEGIN IMMEDIATE;");
  try {
    auto update = prepare(
        db_,
        "UPDATE move_analysis SET category=?,is_theory=?,classifier_version=?,"
        "expected_score_before=?,expected_score_best=?,expected_score_played=?,"
        "expected_score_loss=?,recommended_move=?,theory_games=?,theory_white_wins=?,"
        "theory_draws=?,theory_black_wins=? WHERE analysis_run_id=? AND ply=?;");
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
      sqlite3_bind_text(update.get(), 13, run_id.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_int(update.get(), 14, record.ply);
      check(sqlite3_step(update.get()), db_, "persist move classification");
      const int index = count_index(record.classification);
      if (index >= 0) ++counts[static_cast<std::size_t>(index)];
    }
    std::optional<double> combined_accuracy;
    if (white_accuracy.has_value() && black_accuracy.has_value()) {
      combined_accuracy = (*white_accuracy + *black_accuracy) / 2.0;
    } else if (white_accuracy.has_value()) combined_accuracy = white_accuracy;
    else if (black_accuracy.has_value()) combined_accuracy = black_accuracy;
    // Category totals are derived from move_analysis when an analysis is read.
    // Keep the legacy aggregate columns updated for compatibility, but do not make
    // persistence depend on the optional critical_count column. This makes databases
    // created by older/intermediate builds safe to use without destructive migration.
    auto summary = prepare(
        db_,
        "UPDATE analysis_runs SET classifier_version=?,accuracy_algorithm_version=?,"
        "opening_book_version=?,white_local_accuracy=?,black_local_accuracy=?,"
        "local_accuracy=?,theory_count=?,brilliant_count=?,best_count=?,excellent_count=?,"
        "okay_count=?,miss_count=?,mistake_count=?,blunder_count=? WHERE id=?;");
    sqlite3_bind_int(summary.get(), 1, finalize ? classifier_version : 0);
    sqlite3_bind_int(summary.get(), 2, finalize ? accuracy_version : 0);
    sqlite3_bind_text(
        summary.get(), 3, opening_book_version.c_str(), -1, SQLITE_TRANSIENT);
    bind_optional_double(summary.get(), 4, finalize ? white_accuracy : std::nullopt);
    bind_optional_double(summary.get(), 5, finalize ? black_accuracy : std::nullopt);
    bind_optional_double(summary.get(), 6, finalize ? combined_accuracy : std::nullopt);
    // counts: theory, brilliant, critical, best, excellent, okay, miss, mistake, blunder.
    // critical is intentionally not written to analysis_runs; analysis() derives all
    // category totals from move_analysis, which is the authoritative source.
    sqlite3_bind_int(summary.get(), 7, counts[0]);
    sqlite3_bind_int(summary.get(), 8, counts[1]);
    sqlite3_bind_int(summary.get(), 9, counts[3]);
    sqlite3_bind_int(summary.get(), 10, counts[4]);
    sqlite3_bind_int(summary.get(), 11, counts[5]);
    sqlite3_bind_int(summary.get(), 12, counts[6]);
    sqlite3_bind_int(summary.get(), 13, counts[7]);
    sqlite3_bind_int(summary.get(), 14, counts[8]);
    sqlite3_bind_text(summary.get(), 15, run_id.c_str(), -1, SQLITE_TRANSIENT);
    check(sqlite3_step(summary.get()), db_, "persist classification summary");
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
      "ORDER BY depth ASC,multi_pv ASC,"
      "CASE WHEN time_limit_seconds=0 THEN 2147483647 ELSE time_limit_seconds END ASC,"
      "completed_at DESC LIMIT 1;");
  sqlite3_bind_text(statement.get(), 1, game_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(statement.get(), 2, engine_version.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(statement.get(), 3, requested.depth);
  sqlite3_bind_int(statement.get(), 4, requested.multi_pv);
  sqlite3_bind_int(statement.get(), 5, requested.time_limit_seconds);
  sqlite3_bind_int(statement.get(), 6, requested.time_limit_seconds);
  if (sqlite3_step(statement.get()) != SQLITE_ROW) return std::nullopt;
  return analysis(game_id, text_column(statement.get(), 0), requested_ply);
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
    check(sqlite3_step(touch.get()), db_, "touch global position cache");
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
    check(sqlite3_step(touch.get()), db_, "touch position checkpoint");
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
