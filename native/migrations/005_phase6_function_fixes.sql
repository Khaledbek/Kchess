ALTER TABLE engine_settings
  ADD COLUMN time_limit_seconds INTEGER NOT NULL DEFAULT 0;
ALTER TABLE analysis_runs
  ADD COLUMN time_limit_seconds INTEGER NOT NULL DEFAULT 0;
PRAGMA user_version = 5;
