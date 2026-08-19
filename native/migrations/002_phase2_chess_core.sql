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
