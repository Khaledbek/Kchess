-- Phase 4 is embedded as kMigration4 in database.cpp for mobile/desktop startup.
-- This source copy documents the migration contract; keep both definitions aligned.
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
  ON games(profile_id, provider, provider_game_id)
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
  PRIMARY KEY(profile_id, month)
);

ALTER TABLE provider_sync_state ADD COLUMN provider INTEGER;
ALTER TABLE provider_sync_state ADD COLUMN status TEXT NOT NULL DEFAULT 'idle';
ALTER TABLE provider_sync_state ADD COLUMN retry_after INTEGER;
PRAGMA user_version = 4;
