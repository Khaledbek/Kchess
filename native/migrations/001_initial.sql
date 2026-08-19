PRAGMA foreign_keys = ON;

CREATE TABLE profiles (
  id TEXT PRIMARY KEY,
  type INTEGER NOT NULL CHECK(type BETWEEN 0 AND 2),
  display_name TEXT NOT NULL CHECK(length(trim(display_name)) > 0),
  provider_username TEXT,
  avatar_asset TEXT NOT NULL,
  created_at INTEGER NOT NULL,
  last_opened_at INTEGER NOT NULL
);

CREATE TABLE games (
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

CREATE TABLE game_sources (
  game_id TEXT PRIMARY KEY REFERENCES games(id) ON DELETE CASCADE,
  provider INTEGER NOT NULL,
  provider_url TEXT,
  etag TEXT,
  last_modified TEXT
);

CREATE TABLE downloads (
  profile_id TEXT NOT NULL REFERENCES profiles(id) ON DELETE CASCADE,
  game_id TEXT NOT NULL REFERENCES games(id) ON DELETE CASCADE,
  downloaded_at INTEGER NOT NULL,
  PRIMARY KEY(profile_id, game_id)
);

CREATE TABLE favorites (
  profile_id TEXT NOT NULL REFERENCES profiles(id) ON DELETE CASCADE,
  game_id TEXT NOT NULL REFERENCES games(id) ON DELETE CASCADE,
  created_at INTEGER NOT NULL,
  PRIMARY KEY(profile_id, game_id)
);

CREATE TABLE analysis_runs (
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

CREATE TABLE move_analysis (
  analysis_run_id TEXT NOT NULL REFERENCES analysis_runs(id) ON DELETE CASCADE,
  ply INTEGER NOT NULL,
  category TEXT NOT NULL,
  best_move TEXT,
  engine_score TEXT,
  is_theory INTEGER NOT NULL DEFAULT 0,
  PRIMARY KEY(analysis_run_id, ply)
);

CREATE TABLE provider_stats_cache (
  profile_id TEXT NOT NULL REFERENCES profiles(id) ON DELETE CASCADE,
  cache_key TEXT NOT NULL,
  payload_json TEXT NOT NULL,
  expires_at INTEGER NOT NULL,
  PRIMARY KEY(profile_id, cache_key)
);

CREATE TABLE provider_sync_state (
  profile_id TEXT PRIMARY KEY REFERENCES profiles(id) ON DELETE CASCADE,
  cursor TEXT,
  etag TEXT,
  last_modified TEXT,
  last_sync_at INTEGER,
  last_error TEXT
);

CREATE TABLE engine_settings (
  profile_id TEXT PRIMARY KEY REFERENCES profiles(id) ON DELETE CASCADE,
  preset TEXT NOT NULL DEFAULT 'medium',
  depth INTEGER NOT NULL DEFAULT 18,
  multi_pv INTEGER NOT NULL DEFAULT 3,
  threads INTEGER NOT NULL DEFAULT 2,
  hash_mb INTEGER NOT NULL DEFAULT 128,
  ponder INTEGER NOT NULL DEFAULT 0
);

CREATE TABLE app_settings (
  key TEXT PRIMARY KEY,
  value TEXT NOT NULL
);

INSERT INTO app_settings(key, value) VALUES
  ('showBoardArrows', 'true'),
  ('themeMode', 'system'),
  ('locale', 'de');

PRAGMA user_version = 1;
