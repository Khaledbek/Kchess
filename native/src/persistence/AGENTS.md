# Persistence Agent

Ali integration: migrations 41/42 add `training_progress.best_depth` and per-move accuracy components to the existing SQLite database. Completed `analysis_runs` and `move_analysis` remain the single source for statistics; migration numbers 21/22 are already occupied by Khaled's profile schema.

`statistics_games_missing_move_accuracy` selects old completed runs with null move weights from that same cache. Reclassification writes back to the existing `move_analysis` rows; Statistics never maintains a separate result store.

## Scope

`database.cpp/.h`, Schema, Migrationen und persistente Kompatibilität.

## Token-Regel

`database.cpp` niemals pauschal komplett lesen. Tabelle/Spalte/Funktion mit `rg` suchen und nur Schema-/Migration-/Methodenabschnitte öffnen, die dieselben Daten betreffen.

## Regeln

- bestehende Nutzer-Daten müssen erhalten bleiben
- Migrationen idempotent/versionssicher halten
- Legacy-Spalten/Aliase können weiterhin für alte DBs nötig sein
- vor Löschen nach Schema, Migration, Reads, Writes und Tests/Fixtures suchen
- seitenbezogene lokale Accuracy kommt aus `analysis_runs.white_local_accuracy` / `black_local_accuracy`; `games.local_accuracy` ist nur der kombinierte Legacy-Wert
- keine UI-Anforderung direkt als SQL-Sonderfall modellieren, wenn Service-/Domainlogik geeigneter ist

## AI Chess Profile Persistence

`ai_chess_profiles` stores one versioned JSON payload per existing KChess profile. SQLite owns only persistence and aggregated source observations; interpretation/learning belongs to `native/ai/profile/`. Preserve cascade deletion through the profile foreign key.


## Player Profile Evidence Queue

- `player_profile_game_sources` / `player_profile_move_sources` expose existing KChess data as transport rows only; SQL must not decide strengths, weaknesses, confidence or coach goals.
- `ai_profile_queue` is the persistent resume point for background personalization. Keep `(profile_id, game_id)` unique, source versions monotonic enough to requeue changed evidence, and cascade deletion through profile/game foreign keys.
- A stale `processing` row must be recovered immediately when its persisted profile owner is cold-activated after an interrupted app process; never require a clean shutdown to resume personalization. `engine_pending` remains directly selectable/resumable and must not be rewritten merely because periodic queue sync ran.
- Queue promotion is semantic work: when a row changes from `reason='metadata_only'`/`state='indexed'` to a relevant reason, requeue it even if `source_version` did not change. A repeated processing failure must stay retryable with bounded backoff and must never be converted to `done` merely to stop retries.
- Completed native analysis/classification rows are reused. Profile persistence must not create a second analysis cache.
- `analysis_runs`/`move_analysis` are the single authoritative persisted game-analysis cache for both normal Analysis UI and profile maintenance. Equal-or-better compatible runs are reused, weaker work must never downgrade a stronger run, and only explicit user deletion may remove saved game analysis.
- Every completed shared-cache upgrade and explicit user deletion must invalidate the corresponding `ai_profile_queue` generation via `notify_ai_profile_analysis_changed(game_id)`. The queue stores derived-profile progress only; invalidation never deletes or duplicates engine analysis.
### Profile metadata sweep

`profile_games_metadata` is the cheap one-row-per-game source for the staged profile funnel. It may join existing analysis summaries/classifications, but it must not load PGN/engine lines or create engine work. Keep the existing `player_profile_game_sources`, `player_profile_move_sources` and persistent `ai_profile_queue`; the staged funnel extends them rather than replacing the resume architecture.


### Retired sparse profile-probe storage

Migration 35 deletes `ai_profile_probe_evidence`. Profile-triggered engine work is persisted only in the shared `analysis_runs` / `move_analysis` cache. Do not recreate a profile-owned engine-evidence table.

### Profile queue terminal states

`ai_profile_queue.state='indexed'` is a terminal metadata-only state for the current `source_version`; `state='done'` is the terminal state for promoted/relevant profile work. Both may contribute to the learned profile, but progress queries must report them separately so metadata coverage cannot masquerade as engine-analysis completion. Retired fast/verification/deep probe-position counters are not part of queue progress. Shared-analysis promotion is represented by queue stage/state plus authoritative analysis rows.


## Profile Evidence Registry

`ai_profile_evidence_registry` is the payload-free catalog for profile knowledge sources. It stores stable evidence IDs plus source locators/quality only; PGN, Statistics values, engine lines, classifications and learned profile payloads remain in their authoritative existing tables. Index shared completed analysis at game level and the learned profile model as a derived source. Only active authoritative source locators belong in the registry. Graph/retrieval code must resolve these references instead of creating a second analysis/profile data store.

## General Knowledge Graph persistence

The retired `ai_profile_graph_nodes` / `ai_profile_graph_edges` tables are deleted by migration 35. General graph persistence is owned by the `knowledge_*` schema and `native/src/knowledge/GraphStore`; the profile evidence registry remains only a payload-free locator catalog.

## Update 85 - single authoritative analysis cache

- Normal Analysis and Player Profile Maintenance are clients of the same persisted `analysis_runs` / `move_analysis` cache.
- Profile maintenance may request cheaper settings, but it MUST reuse any equal-or-better saved analysis and MUST upgrade the same cache when stronger work is required.
- A weaker run MUST NOT replace a stronger saved run. Engine analysis is never duplicated into a profile-owned store.
- Profile-triggered engine evidence exists only in the shared analysis cache; the old sparse probe table is removed by migration 35.
- The profile database stores derived player knowledge only; graph relationships live in the general Knowledge Graph.
- Shared analysis is deleted only through the existing user analysis-deletion flow; derived profile state is refreshed when that authoritative cache changes.

## Update 86 - shared online personalization scope

- `player_profile_owner_id(profile_id)` resolves every online provider profile to one stable persistence owner (oldest surviving online profile). Local PGN/FEN profiles own only themselves.
- `profile_games_metadata`, `player_learning_stats`, `player_profile_game_sources` and profile evidence-registry synchronization read the complete online-account scope when the requested profile is online. Player color/rating must still be resolved against each game's owning provider profile/username.
- `ai_chess_profiles`, `ai_profile_queue` and the evidence registry remain the profile-specific stores; graph state lives only in the general Knowledge Graph. Services must use the resolved owner for shared personalization persistence.


## Update 88 - resilient profile queue

- `ai_profile_queue` is recovered in place: stale `processing` rows return to retryable work when the native profile owner is activated, while `engine_pending` remains directly resumable.
- Promotion from `metadata_only`/`indexed` to relevant evidence must requeue the row even when the game source version did not change. Repeated failures retain retry state/backoff and must not be converted to `done`.

## Update 94 - persisted pipeline stage

`ai_profile_queue.pipeline_stage` stores the highest Stage 1..9 reached by the current persistent profile work row. It is resume/diagnostic metadata, not a second scheduler and not evidence itself. Stage values advance monotonically for a queue generation; existing queue state/reason/source-version semantics remain authoritative for retry and invalidation.


## Update 99 - current funnel telemetry

`ai_profile_queue.historical_sample` and `interesting_selected` are current-membership telemetry for the active Stage-3/Stage-4 selection. They are recomputed on every queue sync and may move both ways; unlike `pipeline_stage`, they are not monotonic evidence. Progress queries may expose these counts plus games that have ever reached Stage 7, but they must not use telemetry flags to schedule engine work or to claim verification/deep evidence that was not persisted by the authoritative analysis path.

For a changed `source_version`, reset `pipeline_stage` to the newly synchronized stage before that queue generation advances again. Monotonic stage progression applies within one source generation, not forever across changed game evidence.

## Update 105 - general Knowledge Graph storage

- Schema migration 28 owns `knowledge_nodes`, `knowledge_edges` and their typed compact-property tables. They are the persistence substrate for `native/src/knowledge/graph_store.*`.
- General graph tables contain routing structure and compact metadata only. Do not copy PGNs, engine lines, statistics payloads or learned-profile JSON into generic graph properties.
- Node deletion cascades incident general graph edges/properties. Edge endpoints are strict foreign keys to `knowledge_nodes`.
- `GraphStore` uses a focused FULLMUTEX connection to the existing `kchess.sqlite3`; the normal `Database::open_and_migrate()` path must run first so graph code never owns schema migration independently.

## Update 106 - Knowledge Graph provenance/dependencies

- Schema migration 29 adds `knowledge_sources`, `knowledge_provenance` and `knowledge_dependencies` to the existing `kchess.sqlite3`; no second database is allowed.
- These tables persist only authoritative source locators, source versions, first/last-seen timestamps and targeted invalidation state. PGNs, statistics, engine analysis and learned-profile payloads stay in their existing stores.
- `knowledge_dependencies.source_version` is the exact version used to derive one graph entry. A later source-version change marks only mismatched dependents invalid; it must not force a full graph rebuild.
- Cleanup triggers remove provenance/dependency rows when their owning graph node/edge is deleted. Preserve those triggers or equivalent referential cleanup in future schema changes.


## Knowledge Graph migrations 28-30

- `knowledge_nodes`/`knowledge_edges` and compact typed property tables are the general Knowledge Graph routing store.
- `knowledge_sources`, `knowledge_provenance` and `knowledge_dependencies` own source/version tracking and targeted invalidation for nodes, edges and, since migration 30, chunks.
- `knowledge_chunks` stores concise semantic retrieval content plus metadata; `knowledge_chunk_sources` stores source locators and `knowledge_chunk_graph_links` stores graph routing references. Raw PGNs, full engine analysis and duplicated statistics do not belong in these tables.
- Schema changes for Knowledge Graph contracts must stay synchronized with `native/src/knowledge/AGENTS.md` and the corresponding C++ persistence-facing enums.

## Update 111 - persisted expected-score read model

- `PlayerProfileMoveSourceRow` is still a read-only transport over the authoritative completed `analysis_runs` / `move_analysis` cache. Update 111 exposes the already persisted `expected_score_before`, `expected_score_best` and `expected_score_played` columns alongside `expected_score_loss` so result/transition graph logic can distinguish missed wins and defensive saves without starting Stockfish or inferring them from move labels.
- Keep this query side-effect free and bound to the strongest completed analysis run selected by the existing ordering. Do not add a second analysis table or graph-owned copy of these values.

## Update 113 - Knowledge Graph quality metadata

- Schema migration 31 adds `knowledge_quality` to the existing `kchess.sqlite3`. It stores only derived routing metrics (`confidence`, `coverage`, optional `freshness`, source quality/diversity, importance, sample/evidence counts and temporal/freshness basis).
- These values do not replace confidence in `ai_chess_profiles`, statistics aggregates, analysis cache or other authoritative sources. They are Knowledge-Graph retrieval metadata only.
- Separate cleanup triggers delete quality rows when their owning node/edge/chunk is deleted. Preserve that lifecycle if graph tables are rebuilt in later migrations.
- `source_observation` freshness is source/version observation recency, not necessarily behavioral game recency. Consumers must inspect `freshness_basis` and `temporal_scope` rather than treating every freshness number as a recent chess sample.
- At Update 113, Knowledge Graph migrations cover 28-31; Updates 114-115 extend this through migration 33.

## Update 114 - Knowledge Graph history/conflicts persistence

- Schema migration 32 owns `knowledge_versions`, version properties/sources and `knowledge_conflicts`. These tables preserve compact historical graph state and conflict-resolution metadata only; they do not duplicate authoritative source payloads.

## Update 115 - Knowledge Graph vector persistence

- Schema migration 33 owns `knowledge_embeddings_metadata` plus `knowledge_embedding_vectors`. Metadata is the stable runtime contract; float32 vector bytes are a replaceable index implementation detail.
- Embedding owners are existing Knowledge Graph chunks or nodes. Cleanup triggers remove embedding rows when owners disappear and clear stale `knowledge_chunks.embedding_id` references when an embedding is deleted.
- `player_id` is a routing/privacy scope, not learned content. Non-empty player-scoped retrieval must never match another player's vectors; empty scope is reserved for intentionally global reusable knowledge.
- Model ID/version, vector space and dimensions are persisted explicitly so incompatible vectors are never compared. Runtime schema migrations are now 28-33 for the general Knowledge Graph series.

## Update 123 — Query Trace Storage

Schema v34 ergänzt `knowledge_query_traces` für begrenzte Graph-Routing-/Retrievaldiagnostik. Gespeichert werden IDs, Counts, Scores, Answerability und ein begrenzter Trace; niemals Providerprompts, vollständige PGNs oder Enginepayloads. Die Tabelle ist Diagnose-/Derived-State und keine neue Source of Truth.


## Cleanup Update 124 - migration 35

Migration 35 permanently removes `ai_profile_probe_evidence`, `ai_profile_graph_nodes` and `ai_profile_graph_edges` after deleting obsolete `profile_probe` evidence-registry locators. Historical migrations 23/25 remain in the chain so old databases can advance safely to the cleanup migration. Runtime APIs for the retired profile graph are removed from `Database`.

## Update 127 - persisted sampling metadata

- Schema v36 adds `games.termination_type` as cheap persisted metadata. Provider/PGN imports populate it with `services/termination.h`; existing databases are backfilled once during migration with the same classifier.
- `profile_games_metadata(...)` must read `termination_type` directly and must remain a payload-free one-row-per-game sweep. Do not reopen PGN text in Stage 3 to classify termination.
- Very short games are not deleted from persistence. Sampling eligibility is a profile-domain decision derived from persisted ply count; all game metadata remains available to Statistics and the Knowledge Graph.

## Update 130 - persisted sampling bootstrap watermark

- Schema v37 adds `ai_profile_sampling_state` with one `bootstrap_cutoff_played_at` per profile owner. It is initialized once from the newest already-known game timestamp and must not advance on later queue synchronizations.
- The watermark separates historical bootstrap data from genuinely post-bootstrap games. Import time is irrelevant: an older provider archive game discovered tomorrow is still historical if its played-at timestamp is at or before the frozen cutoff.
- This table is orchestration metadata only; it contains no statistics, PGN or analysis payload and cascades with profile deletion.


## Update 131 - persisted Stage-3 sampling telemetry

- Schema migration 38 extends `ai_profile_sampling_state` with the adaptive sampling requirement and selected structural-coverage telemetry. The table remains lightweight orchestration/diagnostic state; it is not a second statistics store and never contains PGNs or engine evidence.
- `recommended_games` is the current Stage-3 target, `maximum_games` is only the hard cap, and `covered_population_share` is metadata-representation coverage. Keep these values separate from learned profile confidence and Knowledge-Graph entry coverage.

## Fix Update 134 - shared SQLite writer priority

- `persistence/sqlite_write_priority.h` coordinates writers across the main `Database` connection and the Knowledge Graph's separate connections to the same `kchess.sqlite3` WAL file.
- Foreground analysis priority is session-scoped: while a user-driven main-line/refinement job is active, Knowledge/diagnostic writers may not begin new writes. Existing short background writes drain first.
- Global position-cache `last_used_at` touches are non-essential LRU metadata. `SQLITE_BUSY`/`SQLITE_LOCKED` on those touch-only updates must not invalidate an otherwise valid cache hit or fail foreground analysis.
- Keep explicit transactions short. `PRAGMA busy_timeout` is a fallback, not the mechanism for scheduling foreground vs. background work.

## Update 135 - source metadata and diagnostic writes

`PlayerProfileGameSourceRow` now also exposes the existing game's `profile_id` as `source_profile_id` and persisted `termination_type`. These are transport fields over the same shared source query, not new persisted analysis or a schema migration. They allow per-account recorded ratings and termination statistics without N+1 PGN/game loads.

`BackgroundSqliteWriteGuard(std::try_to_lock)` is reserved for optional work such as query diagnostics; callers inspect `owns_lock()` and skip when foreground/background writers own the slot. Authoritative/background maintenance still uses the blocking guard. Diagnostics must never delay a foreground Coach answer behind an analysis session.

## Update 136 - scoped Coach practice counters

Schema migration 39 adds `ai_coach_skill_progress` under the shared player-profile owner with cascading deletion. Only native-verified quiz answer counts and last-practice time are stored; this is learner practice telemetry, not a new analysis cache or graph payload. Keep old profile JSON and schema migrations compatible.

## Update 140 - focused classification reads and partial writes

- `Database::adjacent_analysis_results(...)` is the read-only classification transport for position slots `ply` and `ply+1`. It returns only persisted engine result data/lines from the existing `analysis_runs`/`move_analysis`/`engine_lines` source and must not build game-wide summaries or introduce another cache.
- `persist_classifications(..., finalize=false)` may update the supplied move rows and must keep run-level classifier/accuracy markers non-current. It must not overwrite legacy game-wide category counters with a partial batch. Those compatibility aggregates plus local accuracy are written only by the final full-game classification pass.
- No schema migration is introduced by Update 140; `move_analysis` remains the authoritative per-move classification store and `analysis_runs` remains the authoritative run-level completion/version contract.

## Update 141 - focused player-profile source transport

- `Database::player_profile_game_source(profile_id, game_id)` is the focused read contract for one queued profile game. It must use the same SQL projection, shared-player scope and strongest-completed-analysis selection as `player_profile_game_sources(...)`; the two methods must not drift into separate evidence semantics.
- The focused query is read-only and introduces no schema/cache table. The all-game query remains the authoritative source for queue/relevance synchronization and learned-profile materialization.

## Update 142 - statistics source revision

- `Database::statistics_source_revision()` is an in-process monotonic invalidation generation for the existing shared-player statistics source; it is orchestration metadata only and is never persisted as a new table/cache.
- SQLite update-hook invalidation is deliberately limited to authoritative `profiles`/`games` row changes. High-frequency running analysis writes must not invalidate follow-up Coach statistics on every ply.
- The semantic transition of an `analysis_runs` row to `status='complete'` advances the revision explicitly because that is when `player_profile_game_sources(...)` can select the new analysis. Final classification also updates the owning game accuracy row, so reclassification of an already-complete run invalidates through the same game source path.
- Deleting analysis clears the game accuracy row and therefore invalidates through `games`. Keep this generation synchronized with future changes to fields/tables read by `player_profile_game_sources(...)` rather than introducing manual cache invalidation in Flutter or KnowledgeRuntime.


## Update 144 - SQLite writer-priority telemetry

- `SqliteWritePriorityGate` remains the authoritative in-process coordinator for separate SQLite writer connections. Its snapshot is read-only telemetry over the existing gate; no persisted lock/metrics table is allowed.
- Foreground wait metrics count only cases where Analysis actually had to wait for an already-running background writer. Background wait metrics cover waits behind foreground ownership or another serialized background writer.
- Snapshot reads take only the gate's short in-process mutex and must not acquire SQLite locks. The telemetry is diagnostic and must not feed scheduling policy.


## Cleanup Update 145 - final persistence contracts

- This performance series adds no SQLite schema migration. `statistics_source_revision()` is in-process invalidation state; focused profile/classification reads are views over existing authoritative tables; partial classification writes become globally current only at finalization.
- `SqliteWritePriorityGate` remains the only in-process writer-priority coordinator for the shared WAL database. Telemetry snapshots are read-only and must not become persisted scheduling state.

## Update 152 - focused month reads

`Database::latest_game_month_at_or_before(...)` and `games_for_month(...)` are focused read-only views over the existing `games` source. They exist so the ordinary Games UI does not materialize the full library just to display one month. No schema/cache table is added; full-library consumers continue to use `games(...)` deliberately.

## Update 155 - namespaced Coach skill IDs

No schema migration is introduced. The historical `ai_coach_skill_progress.motif_id` column remains the compatibility storage key, but new verified attempts may use stable namespaced skill IDs such as `tactics.fork` or `strategy.secure_king`. Do not create a parallel skill-progress table merely to rename this legacy column; a later explicit migration may extend the same table with richer scheduling telemetry.

## Update 156 - native Coach spaced repetition

- `ai_coach_skill_progress` remains the single learner-practice persistence store; schema migration 40 extends that same row with verified-weak counters plus streak/interval/due scheduling metadata. No parallel training table or Flutter-owned scheduler is allowed.
- `native/ai/teaching/spaced_repetition_scheduler.*` owns interval and due-policy. Scheduling metadata expresses when to revisit a verified exercise, never a measured player rating or proof of mastery.
- Only natively verified quiz attempts update the schedule. An independent success advances the interval; a natively verified weak move resets the streak and schedules a near-term revisit. Ungraded legal alternatives still do not become attempts.
- Historical coarse motif rows remain compatible cold-start priors. New namespaced skill IDs share the same table and scheduling contract.
