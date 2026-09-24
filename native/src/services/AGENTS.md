# Native Services Agent

Ali integration: `PlayerProfileService` is the sole background analysis scheduler; `StatisticsService` reads completed shared analysis through `Database`. The compatibility background status/switch endpoints in Core refer to the profile worker, not a second worker. Accuracy/trend and opening-weakness interpretation stay native.

Phase-accuracy backfill: `AnalysisService::refresh_cached_accuracy_for_statistics` recalculates missing move weights from complete saved runs only. Core invokes a bounded pass for an explicit Statistics read; the existing profile worker drains remaining old runs when its queue is idle. With profile background analysis disabled, that worker may still derive Statistics from saved runs, but never starts profile queue or engine work. No new engine request or cache table is permitted.

## Routing

- Analyse → `analysis_service.*`
- Bot → `bot_service.*`
- Games/Favorites → `game_library_service.*`
- Profile → `profile_service.*`
- Provider-Sync → `provider_service.*`
- Settings → `settings_service.*`
- Statistics → `statistics_service.*`, `statistics_details.cpp`
- AI Chess Coach App-Bridge → `coach_service.*`
- Coach User-Profile Bridge / learned-profile mapping → `coach_profile_bridge.*`

## Token-Regel

Nur den betroffenen Service lesen; andere Services erst bei konkretem Call. Header zuerst verwenden, um API/Verantwortung zu verstehen, dann nur relevante Implementierungsfunktionen öffnen.

## Regeln

Services orchestrieren Domain-/Persistence-Komponenten. Keine DB-SQL-Duplikate in Services einführen, wenn `Database` bereits eine passende Operation besitzt. Fehler-/Kompatibilitätspfade nicht ohne Aufrufer- und Migrationsprüfung löschen.


## Coach Service

`coach_service.*` ist nur die App-/Persistenzbrücke zum `native/ai/CoachOrchestrator`: Transport-JSON parsen, vorhandenen PGN-/Profilkontext anbinden und strukturierte Antwort serialisieren. Keine zweite Coach-Pipeline, Prompt-Logik oder Schachalgorithmen im Service anlegen.

## Automatic Coach

`CoachService::automatic_json` maps stored game/analysis data into `AutomaticCoachEvent`, while event classification stays in `native/ai/automatic/`. Every valid just-played move may start one automatic coach turn. Automatic jobs use latest-position-wins semantics: starting a newer automatic job marks all older automatic jobs cancelled so rapid move sequences never build an LLM backlog.

## Coach User Profile

`coach_profile_bridge.*` is the only service-side mapping from persisted KChess profile/game/analysis data into `native/ai/profile/`. It may refresh the versioned learned JSON and map it into coach evidence/practicality, but learning rules stay in `native/ai/profile/`.


## Coach Context Preview

`CoachService::context_json` may only validate/parse a pasted FEN or PGN through the authoritative chess helpers and return presentation-ready board DTOs. For PGN it returns the initial position plus main-line positions so Flutter can navigate without reparsing chess notation. It must not persist pasted context or invoke the LLM.

## Coach Jobs

Provider-Inferenz läuft über `CoachService`-Jobs (`start/status/cancel`), damit C-ABI/Flutter niemals während eines LLM-/API-Laufs blockieren. Der Orchestrator wird innerhalb des Services serialisiert, damit Session-State nicht parallel mutiert wird. Für Automatic-Coach-Jobs gilt latest-position-wins: ein neuer Zug cancelt ältere noch laufende/queued Automatic-Jobs; gecancelte Jobs dürfen keine Antwort veröffentlichen. Der Service wählt den konfigurierten nativen Provider-Factory-Pfad; API-Schlüssel bleiben außerhalb des Services und außerhalb von Flutter.
Provider-Quota-/Transportfehler bleiben native Statuswerte: `provider_rate_limited`, `provider_daily_limit` und `provider_deferred` werden im Service nur aus strukturierten Provider-Fehlercodes abgeleitet. Der Service berechnet keine eigene Gemini-Quote und darf einen Rate-Limit-Zustand nicht pauschal als „Modell nicht verfügbar“ umdeuten.

## Coach Hint Job

- The shared single-position hint cache supplies the existing candidate-retrieval hook. Key it by FEN and relevant engine/hint settings; never trust UI-provided hint moves as evidence.
- Manual coach questions may reuse the bounded AnalysisService hint search when native engine budgeting requires fresh candidates. Automatic turns must consume existing analysis and never start this search.
- Automatic checks may refer to a native variation job. Verify its played move against previous/current FEN before coaching; classification stays native.

Der Engine-Hint läuft als nicht blockierender Coach-Job. `CoachService` delegiert die Schachsuche an den vorhandenen `AnalysisService`; bis zu zwei nahezu gleichwertige Kandidaten dürfen als strukturierte UCI-/Eval-Daten an Flutter gehen. Das LLM bestimmt niemals den Hint-Zug.


### Shared analysis cache contract

`AnalysisService::shared_cached_analysis(...)` is the common read gate for persisted game analysis. Normal analysis and player-profile maintenance must both ask this gate before starting Stockfish. A complete run at equal-or-higher engine quality is reused; weaker requests must never create a duplicate run or downgrade/prune a stronger compatible result. Only explicit user deletion may remove the authoritative saved game analysis. Profile orchestration may request work through `AnalysisService`, but it must not own a second engine-analysis store. Whenever a shared run completes/upgrades, `AnalysisService` invalidates the corresponding profile queue generation so `PlayerProfileService` re-reads the stronger cache and rebuilds derived evidence/graph state without another engine call.

## Background Player Profile

`player_profile_service.*` owns orchestration around the evidence-driven profile: representative initial sample, persistent queue synchronization, pause/resume, reuse-first evidence loading and hand-off of missing work to the existing `AnalysisService`. It must not duplicate statistics queries, chess classification or Stockfish implementation. Queue order is: new games, games relevant to open hypotheses, recent missing evidence, initial/high-relevance material, then remaining history. Every available game is eventually marked considered, but only relevant missing evidence may trigger engine work.
Cold-profile queue synchronization must not require analysis-derived signals from an unanalysed old game before it can become relevant. The native funnel supplies an age-balanced bounded interesting set; service code must preserve that selection rather than reapplying a recent-only gate.
Persistent queue recovery is part of this service contract: cold owner activation immediately requeues abandoned `processing` claims; `engine_pending` remains resumable; metadata-only rows promoted to relevant work must leave `indexed`; and orchestration failures remain retryable with persistence-level backoff rather than being mislabeled `done`.

`coach_profile_bridge.*` still owns learned-profile/practicality mapping and may create a conservative legacy seed only when no valid persisted profile exists; it must not overwrite an in-progress learned profile with aggregate fallback data. The active Coach LLM profile path is exclusively `KnowledgeRuntime::coach_evidence(...)`, which emits `profile.context.v3`. The removed direct whole-profile serializer must not be restored.
### Staged funnel bridge

`coach_profile_bridge.*` may expose cheap metadata/sample/relevance/candidate stages to services. Candidate selection loads only persisted move evidence; it never starts Stockfish. Coach profile delivery uses only the bounded `profile.context.v3` KnowledgeRuntime path. `PlayerProfileService` remains the owner of background orchestration and persistent resume state. Fast/verification/deep budgets are pure `native/ai/profile/` planning signals; when the funnel promotes a game to engine work, `PlayerProfileService` requests the cheapest sufficient **full-game** quality through `AnalysisService::ensure_shared_profile_analysis(...)`. That request reads/writes only the same `analysis_runs`/`move_analysis` cache used by the Analysis UI. Foreground engine work always wins; profile maintenance must never persist a second engine result.


### Profile progress semantics

`PlayerProfileService` must keep metadata indexing distinct from relevant evidence resolution. Metadata-only rows finish as `indexed`; promoted rows finish as `done`. The profile snapshot exposes library-index coverage, relevant-game resolution and reused full analyses separately. Never infer "all games analyzed" from a completed metadata sweep. A profile-triggered shared AnalysisService job keeps `currentGameId` ownership until that job actually stops. Snapshot reads may overlay fresh queue counters plus read-only live AnalysisService progress (`overallProgress`, `currentGameProgress`, ply/depth fields) without rebuilding or persisting the learned model; this is presentation telemetry only and must not change queue/evidence semantics.


### Profile evidence-library synchronization

After refreshing the learned profile, `PlayerProfileService` synchronizes the payload-free evidence registry. The service may orchestrate registry freshness, but it must not copy analysis payloads into it or invent graph semantics. New profile-triggered engine work is authoritative only through the shared completed analysis cache. Retired sparse probe storage is removed by migration 35.

## Knowledge Graph refresh and Coach context

`PlayerProfileService` owns orchestration only: after updating the learned profile it synchronizes the payload-free evidence registry and triggers the shared `knowledge::KnowledgeRuntime` refresh. It does not build or persist a profile-specific graph.

`CoachService` obtains `EvidenceKind::user_profile` exclusively from `KnowledgeRuntime::coach_evidence(...)`. The runtime emits bounded `profile.context.v3` chunks from authoritative sources and preserves query scope/provenance for validation. Services must not re-classify profile intent, query profile payloads directly for Coach answers, or recreate the deleted `coach_profile_context_resolver`.

## Update 85 - single authoritative analysis cache

- Normal Analysis and Player Profile Maintenance are clients of the same persisted `analysis_runs` / `move_analysis` cache.
- Profile maintenance may request cheaper settings, but it MUST reuse any equal-or-better saved analysis and MUST upgrade the same cache when stronger work is required.
- A weaker run MUST NOT replace a stronger saved run. Engine analysis is never duplicated into a profile-owned store.
- Profile-triggered engine evidence uses only the shared analysis cache; sparse profile-probe persistence is removed by migration 35.
- The profile database stores derived player knowledge only; graph relationships/retrieval live in `native/src/knowledge/`.
- Shared analysis is deleted only through the existing user analysis-deletion flow; derived profile state is refreshed when that authoritative cache changes.

## Update 86 - provider-independent player profile

`PlayerProfileService` resolves the active account through `Database::player_profile_owner_id(...)` before queue/profile/graph work. All online accounts therefore feed one native background profile and switching between Chess.com/Lichess accounts does not start a second personalization stream. `coach_profile_bridge.*` must read/seed the same resolved owner. Local PGN/FEN profiles stay independent unless merged through the existing profile merge flow.

## Update 87 - profile-owned history discovery

`PlayerProfileService` also orchestrates provider-history completeness for online personalization. Its background worker calls `ProviderService::backfill_player_profile_history_once(...)` independently of Flutter and then immediately resynchronizes the existing profile queue when a new month was imported. The provider service fetches at most one missing persisted archive month per call and reuses the existing profile/stats/archive/month cache + normalization path. Do not move archive traversal into Flutter and do not add a second provider importer.

## Update 88 - resilient profile queue

`PlayerProfileService` resumes the existing persistent queue after interrupted processes and semantic metadata-to-relevant promotion. Queue retries/backoff remain native; Flutter navigation, screen mounting, or a second in-memory scheduler must never be required to release stuck profile work.

## Update 89 - AnalysisService/ProfileService completion hand-off

`AnalysisService` owns the engine lifecycle and exposes a lightweight terminal observer to `PlayerProfileService`. The observer is wake-only: completion/error/cancellation state stays authoritative in AnalysisService + persistence, while the profile worker merely resynchronizes/retries its existing persistent queue. Register/unregister the observer with service lifetime so no callback can outlive `PlayerProfileService`.

`ensure_shared_profile_analysis(...)` returns `cache_ready`, `started`, or `deferred`. Only `started` grants the profile worker ownership of `currentGameId`; `deferred` means foreground work won the shared engine slot. Profile engine pause/resume is native and must not depend on Flutter navigation or status polling.

## Update 90 - profile progress snapshot

`PlayerProfileService::snapshot_json()` is the authoritative presentation snapshot for profile preparation. It overlays persisted learned-profile data with current queue counters, provider-history coverage from `ProviderService`, and read-only live `AnalysisService` progress. `complete` requires both history completeness and relevant-evidence completion; do not derive this state from Games-screen activity or the currently known game count. Progress reads must stay side-effect free.

### Update 92 - queue forward progress

A shared-analysis `cache_ready` result resolves the current `ai_profile_queue` generation immediately: the requested equal-or-better authoritative analysis is already available, so requeueing the same unchanged game is forbidden. Queue sync/claim/engine hand-off must make forward progress independently from the heavier learned-profile/evidence-registry/graph refresh. Profile refresh is derived/retryable work; a refresh exception must be logged and retried without preventing the next persistent queue item from being claimed. Prefer performing a pending refresh while a profile-triggered AnalysisService job is already running, rather than before starting useful engine work.

## Update 94 - nine-stage orchestration

`PlayerProfileService` is the only runtime orchestrator that advances persisted `ai_profile_queue.pipeline_stage`. The domain funnel defines Stage 1..9; service code records reached boundaries while continuing to execute all engine work through `AnalysisService`. Stage 3 is reserved for the representative historical sampler introduced by the next updates; Flutter navigation must never advance a stage.


## Update 97 - Stage-3 gate for expensive profile work

`PlayerProfileService::sync_queue(...)` must build Stage 4 only from the Stage-3 representative historical sample plus genuinely new post-bootstrap games. Stage 1 is a subset of that representative historical sample and is not an extra bypass. Historical games outside the hard Stage-3 budget remain metadata/index work even when generic complexity/relevance signals are high; they must not reach move-candidate, existing-evidence or engine stages merely because the player is strong. The existing Stage-2 relevance score is allowed to steer Stage-3 sampling, not bypass it. New games remain outside the historical ceiling so personalization can keep learning incrementally.

## Update 98 - profile engine escalation budgets

`PlayerProfileService` must treat Stage 7-9 as bounded escalation, not as permission to deepen every complex sampled game. The pure profile funnel owns the per-game escalation ceilings and relevance gates; service code may request only the resulting cheapest sufficient quality through the shared `AnalysisService`. Do not scale background thread count from player Elo or from library size. A server may execute more independent jobs, but the persisted nine-stage selection semantics and Stage-3 sample boundary stay identical.

## Update 99 - nine-stage progress diagnostics

`PlayerProfileService::snapshot_json()` exposes native funnel telemetry only: total metadata-scanned games, current historical Stage-3 sample size/budget, current Stage-4 interesting-game count, games that reached the shared-engine promotion stage, relevant/resolved queue work and live shared-analysis progress. Flutter may display these values but must not recompute sample membership or pipeline stages. Overall work progress remains separate from Coach-readiness confidence.


## Update 100 - profile-owned engine resources

`AnalysisService::ensure_shared_profile_analysis(...)` must overwrite every engine-relevant mutable user setting for profile background work. The profile path is fixed to Stockfish 18 and receives the native profile tier's depth/MultiPV/thread/hash budget; changing normal Analysis settings during preparation must not change later profile jobs. The same authoritative analysis cache is still reused, and opening-theory skipping remains in the existing AnalysisService preparation path.

Analysis- und Variation-JSON geben `bestMove` aus der publizierten Rang-1-PV aus, wenn diese vorhanden ist. Klassifikation, Empfehlung und Brett-Pfeile sollen denselben vollständig publizierten Engine-Stand sehen; der rohe SF18-`bestmove` bleibt intern als Diagnose für schwierige Stellungen erhalten.

## Update 121 - Knowledge-Gap active learning bridge

`PlayerProfileService` may open the shared Knowledge Graph quality/conflict/gap views after `Database::open_and_migrate()` and fold their bounded active-learning hints into `ProfileRelevanceContext`. The gap layer is optional derived orchestration: failure or absence of materialized graph quality must leave ordinary profile maintenance fully functional. Knowledge gaps never own queue rows or engine jobs; the existing `ai_profile_queue` and `AnalysisService::ensure_shared_profile_analysis(...)` remain authoritative. Historical gap targets must still pass Stage 3.

## Update 123 — Knowledge-Graph-Servicegrenzen

`PlayerProfileService` besitzt weiterhin ausschließlich die 9-stufige Profil-/Engine-Orchestrierung. Graphqualität, Konflikte und Knowledge Gaps kommen über die gemeinsame `knowledge::KnowledgeRuntime`; der Service öffnet keine eigenen Knowledge-Stores mehr. Nach dem Schreiben des gelernten `ChessProfile` und der Evidence Registry triggert er den inkrementellen Runtime-Refresh.

`CoachService` verwendet für `EvidenceKind::user_profile` ausschließlich `KnowledgeRuntime::coach_evidence(...)`. Der frühere `coach_profile_context_resolver.*`-Pfad ist gelöscht. Neue persönliche Coach-Retrieval-Logik gehört in `src/knowledge`, nicht zurück in die Service-Schicht.


## Cleanup Update 124 - retired profile stores removed

- `coach_profile_context_resolver.*` is deleted and must not return.
- `PlayerProfileService` no longer emits retired fast/verification/deep probe-position counters or legacy `knownGames`/`processedGames` snapshot aliases.
- Existing historical profile JSON may still be read with a `processedGames` fallback inside the profile parser; new snapshots/serialized profiles use `indexedGames`.

## Update 127 - sampling eligibility bridge

- `PlayerProfileService::sync_queue` maps persisted `termination_type` into `ProfileGameMetadata` and marks games below 16 plies as ineligible for the historical Stage-3 sample. This does not change metadata indexing or remove queue/source rows.
- Service code must not derive termination from PGN or recalculate statistics; persistence supplies the already-normalized metadata and `native/ai/profile/` owns sampling decisions.


## Update 129 - hierarchical Stage-3 selection boundary

- `PlayerProfileService` continues to supply persisted metadata/relevance only. The pure `native/ai/profile/` sampler now owns the conditional hierarchy and concrete historical selection; services must not recreate quotas, opening/result/termination grouping or statistics calculations.
- Stage 4+ may consume only the returned adaptive historical sample plus genuinely post-bootstrap incremental games. Stage 1 is chosen from inside that historical sample. Historical games outside that sample remain metadata/index work even when they have high generic relevance.

## Update 130 - adaptive Stage-3 orchestration and bootstrap boundary

- Initial historical preparation uses the dynamic Stage-3 recommendation; 500 is only the hard policy ceiling. Stage 1 is selected from inside the Stage-3 historical sample and may not add extra historical engine work beyond that ceiling.
- `ai_profile_sampling_state.bootstrap_cutoff_played_at` freezes the newest already-known game timestamp on first bootstrap. Older provider archive games discovered later remain historical even if imported later; only games actually played after that watermark are incremental learning outside Stage 3.
- The old `recent <= 30 days` shortcut is not a definition of a new game and must never bypass the historical sample cap. Incremental games must also satisfy sampling eligibility, so very short games remain metadata-only.
- A lifetime profile may eventually contain evidence from more than 500 games because genuinely new post-bootstrap games continue learning. The *initial historical bootstrap* itself must never exceed the Stage-3 maximum.


## Update 131 - profile completion/readiness telemetry

- `PlayerProfileService` publishes the persisted adaptive sampling recommendation as `historicalSampleBudget` and exposes structural `samplingCoverage` separately from learned `ChessProfile::confidence`.
- `initialPreparationComplete` is native work-state truth: provider history is complete, there is no active/processing profile job, and every currently relevant queue item is resolved. It must not wait for confidence to reach 100%.
- Snapshot reads may derive only cheap presentation fractions from persisted queue/sampling state. They must not rerun the Stage-3 sampler or recalculate profile confidence.

## Update 132 - profile background diagnostics

- `PlayerProfileService::diagnostics_json()` is the authoritative read-only background snapshot for the Graph Inspector. It reads queue, adaptive Stage-3 sampling state, provider history and shared AnalysisService state directly, so diagnostics also work before the first learned-profile payload exists.
- The diagnostic snapshot exposes current/pending operation (`provider_history_sync`, `profile_engine_analysis`, `waiting_for_foreground_analysis`, `profile_knowledge_refresh`, `profile_queue`, `idle`) plus adaptive sample counts, 500-game historical hard cap, cap state and live profile-engine progress.
- Diagnostics must not advance the queue, rerun the sampler, trigger provider sync or start engine work.


## Update 133 - profile sampling/diagnostic cleanup

- `PlayerProfileService` must use `ai::kProfileSamplingMinimumPlies` for Stage-3 eligibility and `ProfileSamplingPolicy{}.maximum_games` for diagnostic fallback; do not reintroduce duplicate numeric constants.
- `diagnostics_json()` exposes `samplingMinimumPlies` plus persisted adaptive sampling state. Core may compose those values for the inspector but must not derive sampling decisions.
- Historical initialization remains capped by Stage 3; genuinely post-bootstrap games are the only normal path by which lifetime evidence can exceed the historical hard cap.

## Fix Update 134 - foreground analysis owns DB write priority

- A user-started main-line analysis and user-started move refinement acquire `persistence::ForegroundSqlitePriorityLease` before cache/persistence work and retain it until the worker reaches a terminal state.
- `ensure_shared_profile_analysis(...)` remains background work and must never acquire this foreground lease.
- The lease protects SQLite writer availability, not Stockfish ownership. Existing `PlayerProfileService::pause_engine_work()` remains responsible for engine hand-off/cancellation; do not merge the two responsibilities.
- A finished/cancelled/failed worker must release its foreground lease even through early-return/error paths. Keep release scoped to the worker lifetime so Knowledge refresh resumes automatically.

## Update 135 - shared-player statistics read model

`StatisticsService::player_knowledge_json(profile_id, time_controls, player_colors, since)` is the common native aggregation boundary for Knowledge Graph projection and live Coach statistics. It reuses `player_profile_game_sources` plus the shared statistics outcome/tally helpers. Empty axes mean the full known library; explicit values create separate comparison cells. It exposes opening results, analyzed phase counters, termination/result splits, observed accuracy and the latest recorded game rating per source account/time control. No PGNs, engine work, profile preparation or active-account switching are required. The existing provider-specific Statistics UI methods retain their own scope.

Every statistic keeps its denominator and unknown-data semantics. Phase move-number ranges do not classify rook/pawn endgames. Results are associations, not loss causes; an absent analysis must not become a zero-error skill assertion. A `since` timestamp is a transparent lower bound; Coach's unqualified recent window is explicitly 90 days.

## Update 136 - service hand-off for adaptive coaching

`CoachService` maps a completed native move/variation event to the automatic trigger, lets a pending verified quiz answer bypass the ordinary criticality gate, and passes the played UCI and session ID to `CoachOrchestrator`. It persists resolved attempts through the database owner ID. A new automatic job cancels only unfinished automatic jobs in the same Coach session. Quiz practice counters are loaded here; the orchestrator chooses the teaching target after routing identifies the actual exercise topic. Provider/graph work stays native; Flutter only renders status and move overlays.

## Update 137 - completed-move proof and cautious grading

The UI's ephemeral `playedMoveFenBefore`/`playedMoveUci` markers are checked against the authoritative stored or completed variation move and resulting FEN. Only this match sets `CoachRequest::user_move_uci`; navigation cannot satisfy a pending quiz. `user_move_error_confirmed` comes only from native mistake/blunder/miss classification or large WDL loss. Quiz pacing is selected later by the orchestrator from same-topic counts, a Beta(1,1) posterior and conservative bounds; it is not a skill/rating estimate.

## Update 139 - native performance baseline diagnostics

- Performance instrumentation for classification, profile source sweeps and the shared-player statistics read model stays inside the owning native services. It is read-only telemetry and must not become a second scheduler, cache or source of truth.
- `AnalysisService` counts classification rebuild requests/current-cache hits, processed plies, the two persisted before/after analysis reads per processed move and rebuild duration. These counters may explain cost but may not influence classification policy.
- `PlayerProfileService` measures only its own existing full `player_profile_game_sources(...)` sweeps so later queue/query work can be compared against the same baseline. The authoritative rows still come from `Database`.
- `StatisticsService` exposes request/row/duration counters for `player_knowledge_json(...)`. Update 139 deliberately reports the statistics read cache as disabled; a later cache must reuse this read model rather than duplicate statistics logic.

## Update 140 - incremental classification scheduling

- Preparation analysis classifies only newly decidable moves: both persisted position slots `ply` and `ply+1` must exist. A newly written position can unlock only its two adjacent moves, and the worker deduplicates those ply updates instead of rebuilding the whole prefix.
- `run_analysis` keeps one `GameRecord` for the worker and uses one `Database::adjacent_analysis_results(...)` query for both classification position slots. Do not reintroduce repeated `Database::game(...)` loads or full `Database::analysis(...)` summary reads inside the per-position classification path. `sqliteAnalysisReads` counts these focused read operations after Update 140.
- Partial classification publishes move-level category/evidence only. The final `rebuild_classification(...)` remains the single game-wide pass that computes white/black accuracy and publishes current run-level classifier/accuracy versions. Refinement still waits for its entire queue before reclassification.

## Update 141 - targeted profile source reads and refresh hand-off

- `PlayerProfileService` consumes a selected persistent queue item with `Database::player_profile_game_source(profile_id, game_id)`. Do not reintroduce a full `player_profile_game_sources(...)` sweep merely to resolve one queued game.
- Full profile source sweeps remain the authoritative input for queue/relevance synchronization. The resulting vector may be handed once to the immediately pending learned-profile refresh so that the same expensive SQLite projection is not executed twice back-to-back. This hand-off is ephemeral process memory, never a second cache/source of truth.
- Native mutation notifications and AnalysisService terminal events are the primary invalidation path. The periodic full synchronization is only a five-minute safety net for missed/external changes, not a 15-second polling loop.
- Profile diagnostics expose focused source lookups and refresh-snapshot reuse beside the existing full-sweep counters. These counters are observation-only and must not drive sampling/relevance decisions.

## Update 142 - shared-player statistics read cache

- `StatisticsService::player_knowledge_json(...)` owns a bounded in-memory read cache over its existing native aggregation result. The cache stores serialized derived facts only; it is not persisted and does not become a second statistics source of truth.
- Cache keys include the shared player owner plus the requested time-control/color axes and `since` bound. A hit must return the exact same native read-model payload that a fresh aggregation would produce.
- `Database::statistics_source_revision()` is the invalidation generation for this cache. Profile/game row changes advance it through SQLite's update hook; a completed analysis transition advances it explicitly because running per-ply engine persistence must not churn the statistics cache.
- Cache insertion is allowed only when the source revision stayed unchanged across the aggregation. Concurrent source changes may still satisfy the current caller but the stale result must not be retained for a later Coach question.
- Performance diagnostics expose cache hits/misses and the current source revision. Flutter must not implement another cache or invent cache state.


## Cleanup Update 145 - final service boundaries

- `AnalysisService` owns incremental move classification plus the single final game-wide accuracy/classification pass; `PlayerProfileService` owns queue/profile orchestration and one-shot source-snapshot hand-off; `StatisticsService` owns the bounded revision-invalidated read cache. None of these responsibilities move to Flutter or KnowledgeRuntime.
- Performance counters are observational only. Do not feed them back into engine breadth, sampling, queue order or classification decisions without a separate architecture change.

## Update 149 - startup-safe profile maintenance diagnostics

- `PlayerProfileService` exposes lock-independent `workerActivity` diagnostics with operation, phase, detail, elapsed time and bounded progress counters. The existing persistent queue/profile state remains authoritative; diagnostics must not become a second scheduler.
- Native profile maintenance observes a short startup grace so Flutter/Core startup gets CPU/SQLite priority before background work begins.
- On the first owner activation after process start, a persisted profile that is already complete and has a learned-profile payload may resume from that authoritative state without immediately rescanning the full game library. Provider imports, analysis invalidations, explicit native wake events and the existing five-minute safety resync still use the normal queue-sync/profile-refresh path.

## Update 152 - Games UI bootstrap ownership

`GameLibraryService::initial_games_json()` owns the initial online Games display window: use the current UTC month when it contains games, otherwise the newest earlier persisted month, and current month with an empty list when no game exists. The fallback must be selected natively from SQLite; Flutter only renders the returned month/games. `query_games_json(...)` must push an explicit month restriction into the focused DB read instead of loading the full profile library first.

## Update 153 - Coach pipeline performance diagnostics

- `CoachService::performance_diagnostics_json()` is the authoritative read-only aggregate for Coach runtime telemetry and is surfaced through the existing Knowledge Inspector. Flutter must not derive Coach timings or job counts.
- `CoachOrchestrator` may emit privacy-safe per-turn timing metadata through the diagnostics callback only. Diagnostics contain stage durations, evidence counts, provider/repair counts and acceptance/validation state, never user text, FEN/PGN, session IDs or profile IDs.
- Coach diagnostics are observational only. They must not change routing, evidence budgets, provider admission, validation, scheduling or training decisions.

## Update 156 - native Coach spaced repetition

- `ai_coach_skill_progress` remains the single learner-practice persistence store; schema migration 40 extends that same row with verified-weak counters plus streak/interval/due scheduling metadata. No parallel training table or Flutter-owned scheduler is allowed.
- `native/ai/teaching/spaced_repetition_scheduler.*` owns interval and due-policy. Scheduling metadata expresses when to revisit a verified exercise, never a measured player rating or proof of mastery.
- Only natively verified quiz attempts update the schedule. An independent success advances the interval; a natively verified weak move resets the streak and schedules a near-term revisit. Ungraded legal alternatives still do not become attempts.
- Historical coarse motif rows remain compatible cold-start priors. New namespaced skill IDs share the same table and scheduling contract.


## Update 157 - foreground-first Coach execution

- `CoachService` remains the single serialization boundary for Coach provider/session execution. Manual asks and explicit hints are foreground work; Automatic Coach is background work.
- A foreground Coach request cancels unfinished Automatic Coach jobs before joining the execution gate. The gate gives waiting foreground work the next available slot, so Automatic jobs cannot build a queue ahead of an interactive question.
- An Automatic provider call that is already inside `CoachOrchestrator::handle(...)` is allowed to finish because the current provider contract has no safe mid-request cancellation. Its result is discarded when the job was preempted; no new parallel provider path is introduced.
- Coach performance diagnostics expose scheduler class/waiters, foreground wait time and automatic preemption counts. These counters are observation-only and must not alter teaching/evidence policy.

## Update 161 - Automatic Coach learner-state context

- `CoachService` supplies the Automatic Coach gate with bounded due-practice relevance from the existing `ai_coach_skill_progress` rows and a bounded process-local last-delivery timestamp keyed by session/profile. No second persistence store is introduced.
- Only a successfully delivered unsolicited automatic response updates the recency history. Verified quiz-answer feedback does not count as an interruption. Provider errors, cancelled jobs and skipped events do not update it.

## Update 162 - Coach small-model wiring

- `CoachService` may load one optional `SmallModelSuite` at construction and pass it into the existing orchestrator. The service does not perform inference or duplicate route/context policy.
- Coach diagnostics may report model presence, availability, id/version and stable load error codes only; filesystem paths and model inputs are not diagnostic payload.

## Update 164 - personal Coach training hydration

- `CoachService` is the integration boundary for explicit `personalTraining` quiz requests. It loads the shared player owner's existing skill-progress rows and learned `ChessProfile`, asks the native teaching selector for one real example position, then reuses existing `game(...)` / `player_profile_game_source(...)` lookups to hydrate FEN, PGN and player color.
- Do not persist a second exercise queue or copy profile example payloads into Coach-owned storage. If no suitable learned example exists, the normal supplied-board quiz path remains available.

## Update 167 - final Coach service boundary

`CoachService` is the only app-integration/scheduling boundary for Coach turns. It hydrates existing persistence/profile/analysis context, loads optional small-model assets, applies foreground-over-automatic serialization, records read-only diagnostics and delegates chess/teaching/provider policy to `native/ai/`. It must not grow a second router, teaching policy, response cache or engine client.


## Fix Update 168 - personal training hydration boundary

After `CoachService` has selected and hydrated one own-game exercise from the learned profile, it marks that request internally so the Coach planner does not perform a second full personal Knowledge retrieval for the same quiz. The profile remains the selector source of truth; the exercise turn itself is grounded by the selected board and native candidates.

## Fix Update 171 - interactive feedback priority

A move that answers an open Coach board question may arrive through `coachAutomatic`, but it is interactive foreground feedback. After native session proof confirms the pending question/FEN, `CoachService` must bypass the unsolicited Automatic gate, use foreground execution/provider priority, and must not let Automatic-only RPM/TPM/RPD soft guards silently suppress that learner feedback. Ordinary unsolicited Automatic turns remain background priority.

## Update 173 - read-only Coach performance access

`CoachService::performance_diagnostics_json()` remains the single source for LLM pipeline diagnostics. Core/C-ABI expose it directly for the Coach UI without starting a job, loading the graph inspector or logging prompts, transcripts, board positions or secrets.

## Update 175 - completed move cache bridge

`CoachService` may read the authoritative `AnalysisService` move record for a proved learner attempt only after matching persisted game ID, ply, UCI and before/after FEN. Require completed quality and existing score/classification fields. Supply that record privately to native `move_contrast.*`; the Coach provider sees only the bounded contrast packet. This callback must never request engine work or create Coach-owned analysis persistence.
