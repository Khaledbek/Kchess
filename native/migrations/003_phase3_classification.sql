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

UPDATE analysis_runs
SET engine_analysis_version='1', engine_config_hash=config_hash
WHERE engine_analysis_version IS NULL;
PRAGMA user_version = 3;
