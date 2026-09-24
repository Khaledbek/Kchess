# KChess AI Router

## Ali integration - shared background analysis and statistics

- `PlayerProfileService` remains the only background game-analysis orchestrator. It requests engine work through `AnalysisService` and the authoritative `analysis_runs` / `move_analysis` cache; no separate statistics worker starts Stockfish.
- Ali's accuracy, trend and opening-weakness statistics read completed, classified games from that cache. Manual and profile-triggered analysis contribute through the same source. Flutter only presents native DTOs, and visible text lives in EN/DE/AR ARB files.
- Ali's opening-drill progress and per-move accuracy schema changes use migrations 41/42 after Khaled's existing migration 40; schema numbers 21/22 remain owned by the original profile migrations.
- Old complete runs with missing per-move accuracy are reclassified from their saved engine slots by `AnalysisService`; the existing profile worker drains this derived backfill after queue work. Statistics reads all completed classified runs for the active game profile, including manually started analyses, and observes the shared cache revision for refresh.

## Scope

Diese Datei enthält nur globale Regeln. Für jede Aufgabe zusätzlich die **nächstgelegene** `AGENTS.md` im betroffenen Ordner lesen. Lokale `AGENTS.md`-Dateien enthalten nur bereichsspezifische Hinweise und sollen unnötigen Repository-Kontext vermeiden.

## Architektur

```text
Flutter/Dart = UI, Navigation, View-State, Darstellung, dünne FFI-Adapter
C++20        = Runtime-Domainlogik, Schach, Engine, Persistenz, Provider, Training
Python       = Development-/Builder-/Diagnosewerkzeuge, keine App-Runtime
ARB          = alle sichtbaren Übersetzungen
```

Keine fachliche Runtime-Logik neu in Flutter oder Python duplizieren.

## AI-Kontextbudget

Bei jeder Aufgabe:

1. Diese Datei + nächstgelegene lokale `AGENTS.md` lesen.
2. Mit Symbol-/Textsuche (`rg`) die betroffenen Stellen finden.
3. Zuerst nur Zieldatei + maximal 2–4 direkte Abhängigkeiten öffnen.
4. Große Dateien nicht komplett lesen, wenn ein Abschnitt per Symbolsuche reicht.
5. Kontext nur erweitern, wenn ein konkreter Aufrufer, Vertrag oder Datenfluss es verlangt.
6. Bereits gelesene unveränderte Dateien im selben Task nicht erneut vollständig laden.

Standardmäßig **nicht** durchsuchen/lesen:

- `third_party/`
- `build/`, `.dart_tool/`, CMake-Buildverzeichnisse
- `native/prebuilt/`
- `flutter_app/lib/localization/generated/`
- große generierte Trainingsdaten unter `native/src/training/data/*.inc`

Ausnahme nur, wenn die Aufgabe genau diesen Bereich betrifft.

## Task-Routing

- Analyse UI → `flutter_app/lib/features/analysis/AGENTS.md`
- Coach UI → `flutter_app/lib/features/coach/AGENTS.md`
- Play/Bots UI → `flutter_app/lib/features/play/AGENTS.md`
- Training UI → `flutter_app/lib/features/training/AGENTS.md`
- Games/Import → `flutter_app/lib/features/games/AGENTS.md`
- Favorites → `flutter_app/lib/features/favorites/AGENTS.md`
- Settings → `flutter_app/lib/features/settings/AGENTS.md`
- Statistics → `flutter_app/lib/features/statistics/AGENTS.md`
- Profile → `flutter_app/lib/features/profile/AGENTS.md`
- Shared Board/UI → `flutter_app/lib/shared/AGENTS.md`
- FFI → `flutter_app/lib/ffi/AGENTS.md`
- App-Shell/Startup → `flutter_app/lib/ui/AGENTS.md`
- Native Analyse → `native/src/analysis/AGENTS.md`
- Stockfish/Bot Engine → `native/src/engine/AGENTS.md`
- Native Services → `native/src/services/AGENTS.md`
- Coach App-Integration/FFI → `native/src/services/coach_service.*` + `flutter_app/lib/features/coach/`
- Datenbank → `native/src/persistence/AGENTS.md`
- C-ABI → `native/src/api/AGENTS.md`
- Schachmodell/PGN/FEN → `native/src/chess/AGENTS.md`
- Native Training → `native/src/training/AGENTS.md`
- Knowledge Graph → `native/src/knowledge/AGENTS.md`
- Theory/Openings → `native/src/theory/AGENTS.md`
- Provider → `native/src/providers/AGENTS.md`
- AI Chess Coach → `native/ai/AGENTS.md`
- Coach Position Intelligence → `native/ai/position/AGENTS.md`
- Automatic Coach Trigger → `native/ai/automatic/AGENTS.md`
- Coach Practicality → `native/ai/practicality/AGENTS.md`
- User Chess Profile AI → `native/ai/profile/AGENTS.md`
- Coach Concepts → `native/ai/concepts/AGENTS.md`
- Coach Conversation State → `native/ai/conversation/AGENTS.md`
- Coach LLM Provider → `native/ai/providers/AGENTS.md`
- Coach Runtime Config → `config/AGENTS.md`
- Local Secrets → `secrets/AGENTS.md`
- Small AI Models/Embeddings → `native/ai/models/AGENTS.md`
- Coach Input Optimization → `native/ai/optimization/AGENTS.md`
- Coach Validation → `native/ai/validation/AGENTS.md`
- Python/Builder → `tools/AGENTS.md`
- AI Dataset/Training/Evaluation Tools → `tools/ai/AGENTS.md`

## Globale Regeln

- `third_party/` bei normalen KChess-Aufgaben nicht ändern.
- Sichtbare Flutter-Texte ausschließlich in `flutter_app/l10n/app_en.arb`, `app_de.arb`, `app_ar.arb` pflegen; alle drei gemeinsam ändern.
- Generierte Lokalisierungsdateien nie manuell editieren.
- Bestehende ABI-/JSON-/DB-Kompatibilität nicht als „toten Code“ entfernen, ohne alte Daten/Clients auszuschließen.
- Stockfish 18 und 19 sind beide aktive Engines; Engine-spezifische Logik bleibt nativ.
- Side-Line-Analyse ist flüchtig und darf Hauptlinienpersistenz nicht überschreiben.
- Handgeschriebene nicht-triviale Dateien mit klaren Sections strukturieren.
- Keine Builds, Tests, `flutter analyze` oder App-Runs automatisch starten, solange der Benutzer sie selbst ausführen möchte.

## Änderungsstil

Bevor Code geändert wird: Aufrufer suchen, Verantwortung bestimmen, kleinste sichere Änderung wählen. Bei Cleanup nur nachweislich tote/redundante Pfade entfernen. Keine Parallelimplementierung erstellen, wenn eine bestehende Schnittstelle erweitert werden kann.

## Gemini API

Der Coach verwendet standardmäßig den nativen Gemini-Remote-Provider. API-Schlüssel liegen ausschließlich in `secrets/gemini_api_key.txt`, niemals in Dart/C++-Quelltext. `config/coach_provider.json` enthält nur nicht-geheime Provider-/Quota-Konfiguration. Lokale GGUF-Dateien dürfen vorhanden bleiben, werden aber vom Standard-Coach-Pfad nicht verwendet.
Die Free-Tier-Steuerung verwendet getrennte RPM/TPM/RPD-Soft-/Hard-Limits. Automatic Coach gibt bei Soft-Limits Kapazität für manuelle Fragen/Follow-ups und den einmaligen Repair-Pass frei; RPD folgt dem Pacific-Reset von Google AI Studio. HTTP 429 startet nur einen persistenten Backoff und keine Retry-Schleife.

## AI Chess Coach UI-Ziel

Die Coach-UI liegt unter `flutter_app/lib/features/coach/` und übernimmt die visuelle Sprache des Analysis-Screens: Board links; rechts oben Coach-Ausgabe; rechts unten die Eingabeleiste mit Hint/Depth sowie FEN/PGN-Kontextwahl. Flutter bleibt dabei reine UI, sichtbare feste Texte bleiben ARB-basiert. Die native Anbindung erfolgt über kontrollierte Callbacks/FFI und darf keine Coach-Domainlogik nach Dart verlagern.

## Gemini-only Coach provider

- The main Coach LLM uses Gemini API only. Do not restore the old local GGUF/llama.cpp Coach fallback.
- The Gemini key stays only in `secrets/gemini_api_key.txt`, which is git-ignored.

## Update 85 - single authoritative analysis cache

- Normal Analysis and Player Profile Maintenance are clients of the same persisted `analysis_runs` / `move_analysis` cache.
- Profile maintenance may request cheaper settings, but it MUST reuse any equal-or-better saved analysis and MUST upgrade the same cache when stronger work is required.
- A weaker run MUST NOT replace a stronger saved run. Engine analysis is never duplicated into a profile-owned store.
- Profile-triggered engine work uses only the shared `analysis_runs` / `move_analysis` cache. Retired sparse probe storage is removed by migration 35.
- The profile database stores derived player knowledge; graph relationships live only in the general `native/src/knowledge/` Knowledge Graph.
- Shared analysis is deleted only through the existing user analysis-deletion flow; derived profile state is refreshed when that authoritative cache changes.

## Update 86 - shared online player-profile scope

- Player personalization is scoped to the human player, not to one provider account: every persisted online provider profile participates in one shared player-profile library.
- The oldest surviving online profile is the native persistence owner for the shared learned profile, queue and graph; switching the active Chess.com/Lichess account must not fork background personalization.
- Local PGN/FEN profiles remain separate until the existing merge flow moves their games into an online profile.
- Per-provider Games/Statistics UI behavior is not reimplemented in Flutter by this rule; the combined scope is native profile-personalization data flow.

## Update 87 - UI-independent provider history backfill

- Historical online games are discovered by native `PlayerProfileService`; opening or switching to the Games UI is never a prerequisite for profile preparation.
- `PlayerProfileService` asks the existing `ProviderService` for at most one still-missing provider month per background pass. Provider month caches are the persistent resume point, so app restarts continue the same history backfill instead of restarting it.
- All online accounts in the shared player-profile scope are backfilled. `ProviderService` remains the only owner of Chess.com/Lichess transport, provider scheduling, normalization and game persistence; Flutter must not reproduce archive traversal.
- The backfill extends the existing provider metadata/month sync path rather than creating another importer. Provider rate-limit cooldowns are honored before background requests.

## Update 88 - resilient profile queue

- `ai_profile_queue` remains the single persistent resume point for profile preparation. Interrupted `processing` work is recovered on native owner activation, and `engine_pending` stays resumable without UI navigation.
- A game promoted from metadata-only indexing to relevant evidence is requeued even when its source version is unchanged. Processing failures remain retryable with bounded backoff and are never relabeled as completed work.
- Queue recovery extends the existing persistence/service flow; no second scheduler or Flutter-owned recovery path may be introduced.

## Update 89 - native analysis/profile hand-off

- `AnalysisService` emits a native terminal-state wake event for main-line analysis; `PlayerProfileService` uses it only to wake/resync its persistent queue. Profile continuation must never depend on Flutter polling, opening Games, or changing sections.
- `ensure_shared_profile_analysis(...)` distinguishes `cache_ready`, `started`, and `deferred`. A deferred request means foreground engine work owns the slot and must not create a phantom profile `currentGameId`.
- Foreground analysis still wins: `pause_engine_work()` keeps profile engine work paused while AnalysisService is active, then the native terminal event releases background continuation automatically.

## Update 90 - truthful native profile progress

- Player-profile progress is native telemetry over the shared player scope; Flutter must not infer provider-history completion from the currently loaded game count.
- `ProviderService::player_profile_history_progress(...)` reports discovered online accounts and persisted archive-month coverage using the existing provider caches. `PlayerProfileService` combines those counters with persistent queue/evidence counters and live shared-analysis progress.
- `background.status='complete'` is valid only when provider history is fully discovered/synced and all currently relevant evidence is resolved. While provider history is incomplete the native status is `syncing_history`.
- `overallProgress` is based on explicit native work units (provider-account discovery + provider months + relevant-game resolution); detailed `historyProgress` and `relevantProgress` remain separate so the UI can present the real phase instead of manufacturing progress locally.

## Update 91 - profile UI is observation-only

- Profile preparation is fully native and UI-independent. Flutter never starts provider-history traversal or advances the profile queue; opening Games/Profile is not part of the processing contract.
- The Profile screen displays the native `syncing_history`, history coverage, queue counters and shared-analysis live progress without deriving domain state in Dart.
- Visible progress/status wording remains in all three ARB files.

## Update 92 - profile queue forward-progress invariant

- `AnalysisService::ensure_shared_profile_analysis(...) == cache_ready` is terminal for the current profile-queue source generation. `PlayerProfileService` must mark that relevant game `done`; it must never requeue an unchanged equal-or-better authoritative analysis.
- Queue synchronization/consumption is scheduler-critical. Learned-profile/evidence-registry/graph materialization is derived work and must not prevent the persistent queue from advancing when a refresh is slow or recoverably fails.
- Full profile/graph refresh remains native and may be deferred/throttled while the queue or Stockfish continues; Flutter remains a read-only observer of native progress.

## Update 94 - nine-stage profile pipeline contract

- The player-profile preparation funnel is now numbered Stage 1 through Stage 9. Stage 3 is reserved for the representative/prioritized historical sample; all former stages after relevance ranking move up by one.
- The intended order is: 1 initial fast sample -> 2 global metadata/relevance sweep -> 3 representative prioritized historical sample -> 4 interesting-game filter -> 5 position candidates -> 6 existing-evidence reuse -> 7 fast probe -> 8 verification -> 9 deep analysis.
- `ai_profile_queue.pipeline_stage` persists the highest stage reached for resume/diagnostics. Flutter must not own or infer pipeline advancement.
- Update 94 establishes numbering/persistence only; the Stage-3 sampler itself is implemented by the subsequent sampling updates.

## Update 95 - Stage-3 sampling strata

- The nine-stage profile funnel now has a native metadata-only Stage-3 population model before interesting-game filtering.
- Stage 3 describes the player's real library distribution using coarse existing metadata/statistics; it does not start engine work or read PGNs.
- The actual maximum-500 weighted historical selection is intentionally owned by the next sampling update; do not let Stage 4 consume the full library once that selector is active.
## Update 96 - bounded historical profile sample (superseded target semantics)

- This update originally introduced the 500-game historical ceiling. Updates 127-133 supersede the old fixed-target interpretation: 500 is only the hard safety cap.
- Current Stage 3 is metadata/statistics-only and adaptively derives its recommendation between the native minimum and hard maximum from observed library diversity.
- New post-bootstrap games and later user-driven analysis may contribute additional evidence without reopening the complete historical library.
- No UI navigation may influence selection.

## Update 98 - bounded verification/deep work

- Stage 7-9 now use explicit hard escalation ceilings so strong-player complexity cannot turn the 500-game historical sample into hundreds of deep analyses.
- Default background work is PC-safe and conservative; server speedups come from execution capacity, not from weakening Stage-3 sampling or promoting more historical games.
- Flutter remains observational only; no UI route may alter probe budgets or pipeline promotion.

## Update 99 - nine-stage profile telemetry

- Coach-readiness confidence and background work progress remain separate native concepts.
- Profile snapshots expose truthful nine-stage funnel diagnostics: metadata-scanned library size, current Stage-3 historical sample/budget, Stage-4 interesting-game count, shared-engine-promoted games, relevant/resolved work and live analysis progress.
- Stage diagnostics are observation-only. Current sample/interesting membership may change when the library changes; highest `pipeline_stage` is historical diagnostic state. Flutter displays native values and never derives sampling or escalation decisions.

## Update 100 - private profile-engine budget

- Player-profile preparation has a native, non-user-configurable engine contract and must not inherit mutable Analysis-screen engine settings. Profile evidence uses Stockfish 18 for stable comparability, while normal Analysis may still use SF18 or SF19 according to user settings.
- Stage 7/8/9 quality ceilings are Depth 4 / 8 / 12. Stage 7 uses MultiPV 1; Stage 8 and 9 use MultiPV 2. Profile hash is 1024 MB; Stage 7/8 use 2 threads and Stage 9 uses 4 threads. Adaptive early-stop remains enabled.
- Known opening-theory work continues to be skipped by the existing book-driven AnalysisService preparation path; the boundary needed for the first non-theory move remains analyzable.
- These private resources still write/reuse the single authoritative `analysis_runs` / `move_analysis` cache. A stronger compatible saved analysis must be reused rather than recomputed.

## Update 101 - profile-engine thread budget

- The private profile-engine contract keeps the fixed Depth 4 / 8 / 12 and MultiPV 1 / 2 / 2 quality ceilings, but Stage 7 and Stage 8 now use 2 threads and Stage 9 uses 4 threads.
- Profile preparation continues to run a single shared profile-engine job at a time; thread increases accelerate one staged job instead of introducing parallel profile analyses.
- Hash remains 1024 MB, Stockfish 18 remains fixed for profile preparation, and normal user Analysis settings remain isolated from this private budget.

## Update 102 - compact evidence profile and live preparation UI

- The learned player profile exposes compact evidence-derived summary metrics: weighted analyzed-game Accuracy, a conservative estimated playing-strength rating, estimate confidence, and analytically-backed game count. Estimated playing strength is a profile estimate, never an official FIDE/provider rating.
- Playing-strength estimation uses existing stored player/opponent ratings and outcomes only; it does not start additional Stockfish work. Accuracy is weighted by analyzed move evidence already present in the authoritative analysis cache.
- The Profile UI remains observational. It presents a small set of useful summary metrics and a smoothly animated live preparation card driven entirely by native background telemetry; Flutter does not infer or advance pipeline stages.
- While the Profile screen is visible, lightweight native snapshot polling may be more frequent for live presentation, but profile preparation itself remains fully UI-independent.

## Update 103 - refined profile presentation

- Profile UI keeps the evidence summary compact and presents the native preparation funnel in a readable live layout. `enginePromotedGames` may be shown directly from native telemetry; Flutter must not infer funnel membership or stage advancement.
## Update 104 - general Knowledge Graph contract

- The general personal chess Knowledge Graph runtime lives under `native/src/knowledge/`; Flutter remains presentation-only and Python remains evaluation/tooling-only.
- `knowledge_graph_contract.*` defines the versioned property-graph base contract, stable deterministic node/edge identities, persistence-facing node/relation kinds and the explicit Fact/Observation/Hypothesis distinction.
- Canonical graph identity must come from stable machine/source identities, never translated labels or mutable prose.
- The graph is a routing/relationship layer over existing authoritative Games, Statistics, Analysis and Profile stores; generic graph properties must not become a duplicate payload database.
- The general `native/src/knowledge/` graph is the only profile/Coach graph runtime. Do not recreate a profile-specific parallel graph.

## Update 105 - Knowledge Graph SQLite storage

- The general Knowledge Graph now has `native/src/knowledge/graph_store.*` backed by schema migration 28 in the existing `kchess.sqlite3`; no second database or payload store is introduced.
- Graph storage owns stable nodes, edges, compact typed properties and bounded basic traversal. Authoritative Games/Statistics/Analysis/Profile payloads remain in their existing stores.
- Knowledge runtime must initialize after the existing `Database::open_and_migrate()` path; schema migration remains owned by native persistence rather than GraphStore.

## Update 106 - Knowledge Graph provenance and dependency tracking

- The general Knowledge Graph now tracks payload-free provenance and source-version dependencies through `native/src/knowledge/dependency_tracker.*` and schema migration 29.
- Stable `source_type` / `source_id` locators point back to authoritative Games/Statistics/Analysis/Profile data. `first_seen`/`last_seen` and expected source versions are metadata only; source payloads remain outside the graph.
- A changed source invalidates only graph nodes/edges derived from an older version. Later graph/chunk pipelines must consume this targeted invalidation instead of forcing complete rebuilds.

## Update 108 - Statistics Graph projection

- The general Knowledge Graph now projects existing native statistics through `native/src/knowledge/statistics_graph_projector.*`; StatisticsService and persisted learning aggregates remain authoritative.
- Statistics projection is source-versioned and targeted for invalidation. It must never trigger duplicate statistics aggregation, game re-analysis or engine work.
- Opening values in Update 108 are statistics metadata only; the dedicated opening/variation/transposition topology begins in Update 109.

## Update 109 - Opening/Variation/Transposition Graph

- `native/src/knowledge/opening_graph_projector.*` projects persisted Games-DB opening classifications into the general graph. Opening classification itself stays owned by the existing KCO pipeline.
- Opening topology is graph-shaped, not a tree: stable canonical Stockfish-18 Position nodes are shared across move orders; distinct observed Variation move orders may converge on the same Position and receive explicit `TRANSPOSITION_OF` links.
- Opening-prefix `PRECEDES`, `REACHES`, family/opening containment and compact theory-end metadata are routing facts only. PGNs, full game payloads and engine analysis stay in their existing authoritative stores.
- Per-game opening source versions plus a profile opening manifest drive Update-106 invalidation. Do not create an independent opening database or Flutter-side opening graph logic.

## Update 112 - Profile Knowledge and Evidence Graph

- `native/src/knowledge/profile_knowledge_graph_projector.*` projects the existing learned `ChessProfile`, payload-free evidence registry and saved shared move-analysis observations into the general Knowledge Graph. The learned profile and shared analysis cache remain authoritative; no second profile-learning or engine path is introduced.
- Explicit profile strengths, weaknesses, habits, behaviors, trends and hypotheses become typed graph observations/hypotheses with provenance. Evidence nodes store only locators/versions and can route profile patterns back to authoritative shared analysis.
- Recovery and error-cascade observations are derived only from already persisted player move classifications/expected-score loss. They never trigger Stockfish or reclassify moves.
- Tactical/strategic motif nodes require explicit upstream motif IDs; ordinary misses/blunders are not promoted to motifs without supporting source data.
- Update-112 invalidation is source-specific; the profile manifest tracks source membership for stale-entry cleanup rather than turning every source-version change into a full profile-graph rebuild.
- The general Knowledge Graph is the only Coach routing graph and remains a projection/routing layer, not a new profile source of truth.

## Update 113 - Knowledge Graph Confidence/Coverage/Freshness

- General Knowledge Graph quality is native C++ logic under `native/src/knowledge/`; Flutter must not derive confidence, coverage, freshness, source quality, evidence diversity or importance.
- Graph quality is a retrieval/routing layer over existing authoritative confidence/evidence. It must not overwrite `ChessProfile` confidence semantics or duplicate Statistics/Analysis payloads.
- Confidence and Coverage remain separate. Freshness always carries a basis, and `recent`/`lifetime`/`mixed`/`unknown` temporal scope remains explicit for later conflict resolution and ranking.

## Update 114 - Knowledge Graph conflicts and temporal history

- The general Knowledge Graph preserves compact prior node/edge revisions in migration 32 instead of silently losing changed/deleted assertions. Version rows contain only graph metadata/properties and source locators; authoritative source payloads remain outside the graph.
- `native/src/knowledge/conflict_resolver.*` owns historical-vs-current contradiction semantics and explicit `CONTRADICTED_BY` relations. It does not alter the learned profile or re-run analysis.
- Strength/weakness changes are scoped by `profile_id` + `pattern_id`: weakness -> strength is improvement; strength -> weakness is decline. Old assertions remain explainable and retrieval can prefer current evidence without erasing history.

## Update 115 - Knowledge Graph Embedding/Vector Interface

- General Knowledge Graph vectors are owned natively through `native/src/knowledge/vector_index.*`. Runtime callers use the model-agnostic `VectorIndex` contract; Python remains offline evaluation/conversion tooling only.
- Migration 33 persists embedding metadata and vector payloads in the existing `kchess.sqlite3`. Embeddings reference existing chunks/nodes and never replace authoritative Games/Statistics/Analysis/Profile knowledge.
- Text-semantic and chess-position vector spaces are contractually separate. Model ID/version, dimensions, source version and player scope must match before similarity comparison.
- The initial SQLite exact-search implementation is the correctness baseline and may later be replaced by an ANN backend without changing retrieval-facing IDs or metadata contracts.

## Update 116 - text-semantic Knowledge Graph layer

- Update 116 does not implement final hybrid retrieval/ranking: exact/statistical/graph/lexical fusion and cross-channel ranking remain Updates 119-120.

## Update 117 - chess-position similarity vector space

- KChess now has a second Knowledge Graph vector space for deterministic chess-position similarity under `native/src/knowledge/position_similarity.*`. It reuses native position features and canonical position identity; no Flutter/Python runtime logic or second FEN parser is allowed.
- Text-semantic and chess-position vectors remain strictly separate model/version contracts. Position similarity is a retrieval aid, not authoritative chess/profile truth and not by itself a reason to persist `SIMILAR_TO` edges.
- Canonical Position vectors are globally reusable; later hybrid retrieval must intersect them with the active player's graph/game scope before Coach evidence is assembled.

## Update 118 - Knowledge Graph Query Router und Entity Extraction

- `native/src/knowledge/query_router.*` verfeinert den bereits einmal erzeugten nativen `ai::QueryPlan` ausschließlich für Knowledge-Graph-Retrieval. Es darf keine zweite Coach-Intent-/QueryFamily-Klassifikation in Flutter oder im Graph-Layer entstehen.
- Ein vorhandenes aktuelles Brett ist nur verfügbare Kontextinformation und darf historische/persönliche Fragen nicht dominieren. Current-board-Retrieval wird erst bei explizitem Positionsbezug oder `QueryPlan::needs_position` verpflichtend; gemischte Position+Profil-Fragen dürfen beide Scopes benötigen.
- `native/src/knowledge/entity_extractor.*` erzeugt normalisierte Query-Hinweise für ECO/Opening, Zeitkontrolle, Phase, Farbe, Resultat/Termination, zeitlichen Scope, Statistikmetrik, Positionsreferenz und Evidenzwunsch. Diese Hinweise sind keine Graph-Wahrheit und werden erst durch Retrieval gegen autoritative Graph-/Chunk-Daten aufgelöst.
- Update 118 definiert nur Channel Eligibility für Exact/Statistics, Graph, Lexical, Vector und Position Similarity. Candidate-Sammlung, Budgets und Fusion beginnen in Update 119; finales Ranking/Query Planning bleibt Update 120.
## Update 119 - Knowledge Graph Hybrid Retrieval

- `native/src/knowledge/hybrid_retrieval.*` collects bounded candidates from exact/statistical facts, graph traversal, lexical chunks, text vectors and optional position similarity according to the existing native query route. It does not perform final cross-channel ranking or create another intent classifier.
- Default retrieval remains token-oriented: 8 seed nodes, graph depth 2 (max 3 for complex questions), 100 expanded nodes, 40 merged chunk candidates and a reserved 20-candidate rerank ceiling. Channel-specific raw signals stay explainable for Update 120 ranking.
- Player-scoped retrieval must not cross explicitly foreign profile-owned graph nodes; position similarity must intersect global canonical position vectors with the active player's reached positions before they can become personal evidence. Flutter remains UI-only.

## Update 120 - Knowledge Graph Hybrid Ranking

- Native C++ `native/src/knowledge/retrieval_ranking.*` now owns Knowledge-Graph query execution policy and final cross-channel ranking after bounded hybrid candidate retrieval.
- The Knowledge planner refines, but never replaces, the existing Coach `QueryPlan`/Knowledge route. Ranking keeps exact/statistical relevance, graph distance, lexical/vector/position signals and confidence/coverage/freshness explainable as separate components.
- Optional reranking is bounded to already eligible chunks and cannot broaden scope or become authoritative knowledge. Final token budgeting/evidence packet assembly remains Update 122.

## Update 121 - Knowledge Gaps

- Knowledge gaps are native C++ derived observations under `native/src/knowledge/knowledge_gap_engine.*`; Flutter has no gap-detection or active-learning logic.
- Gap-driven evidence acquisition reuses the existing nine-stage `PlayerProfileService` queue and shared AnalysisService cache. No second scheduler, engine cache or profile store is allowed.
- Low coverage, low confidence, stale evidence and conflicting evidence remain distinct signals; gap priority must not collapse them into a replacement truth/confidence value.

## Knowledge Graph Updates 104–123 abgeschlossen

Der allgemeine lokale Knowledge Graph ist über `native/src/knowledge/KnowledgeRuntime` in Profilpipeline und Coach integriert. C++ bleibt Owner von Projektion, Storage, Chunks/Vektoren, Retrieval, Ranking, Confidence/Coverage/Freshness, Konflikten, Knowledge Gaps, Evidence Packets und Query Traces. Flutter enthält nur den read-only Graph Inspector und rendert native Ergebnisse; sichtbare feste Texte bleiben ARB-basiert. Der frühere profil-spezifische Graph ist kein aktiver Retrievalpfad mehr.

## Cleanup Update 124 - dead Knowledge/Profile paths removed

- The retired `native/ai/profile/profile_graph*` sources and `native/src/services/coach_profile_context_resolver.*` are deleted; they must not be restored.
- Schema migration 35 deletes obsolete `ai_profile_graph_nodes`, `ai_profile_graph_edges` and `ai_profile_probe_evidence` state after pruning old probe locators from the evidence registry.
- Profile progress no longer exposes `knownGames`, legacy `processedGames` output, or fast/verification/deep probe-position counters. Old persisted `processedGames` is accepted only as an input fallback while reading historical profile JSON.
- Cleanup must preserve the active boundaries: shared Analysis cache for engine evidence, learned `ChessProfile` for profile truth, evidence registry for source locators, and `KnowledgeRuntime` for graph/retrieval/Coach context.

## Update 127 - adaptive profile sampling foundation

- Profile Stage 3 is being converted from a fixed 500-game target to statistics-driven adaptive sampling. Update 127 establishes the authoritative metadata contract: color, result, time control, opening, termination, recency/game length and ratings remain available for every game, while very short games (<16 plies) are sampling-ineligible only.
- Schema v36 persists `termination_type` so Stage 3 can use the same termination semantics as Statistics without loading PGNs. Full metadata/statistical ownership is unchanged; the profile sampler is a consumer, not a second statistics engine.
- The 500 value remains only a future hard cap. Update 127 does not yet change the target-size estimator; that occurs in Update 128.

## Update 128 - adaptive Stage-3 sample budget

- Historical profile initialization no longer targets 500 games. Native Stage 3 derives `recommended_games` from observed Statistics/Games metadata diversity with a default floor of 40 eligible games and a hard ceiling of 500. Raw account size alone does not increase the target.
- Confidence/coverage and completion semantics remain separate from this sample-size requirement; Flutter must not reconstruct the budget.

## Update 129 - hierarchical Stage-3 sample selection

- Stage 3 samples the existing metadata distribution hierarchically rather than creating a full cross-product. The representative order is color -> opening family -> result -> time control -> termination -> recency/opponent/game-length context, with a bounded relevance/Knowledge-Gap tail.
- Persisted opening names are preferred for opening-family sampling so distinct openings such as Italian Game and Ruy Lopez remain distinct when Statistics already knows them. No PGN is reopened and no statistics or engine work is duplicated.
- Short sampling-ineligible games remain in Games/Statistics/Knowledge Graph metadata and are excluded only from expensive historical profile analysis. The 500 value is a hard safety cap, never the desired sample size.

## Update 131 - profile coverage and completion semantics

- Adaptive Stage-3 sample coverage, Coach-readiness confidence and background completion are three different native values. Coverage describes statistical representation of the eligible historical metadata population; confidence describes evidence certainty; completion describes whether the required initial work is currently resolved.
- A profile may be initially complete below 100% confidence. The 500-game value is only a native safety ceiling and must not appear as the default Stage-3 target in Flutter.
- Schema v38 persists sampling diagnostics so Profile UI and the upcoming background-work inspector can read them without rerunning sampling/statistics logic.

## Update 132 - diagnostics contract

- The Profile Diagnose surface is a read-only composition of native KnowledgeRuntime activity and PlayerProfileService background telemetry.
- A busy KnowledgeRuntime must report what owns it instead of blocking Flutter. Stage-3 adaptive sampling diagnostics expose recommendation/min/max/cap/coverage from persisted native state; Flutter never recomputes them.

## Update 133 - adaptive sampling cleanup

- The adaptive Stage-3 contract has one centralized short-game threshold (`kProfileSamplingMinimumPlies`), one native minimum/maximum policy, and no duplicate hard-coded 500 fallback in Core/diagnostics.
- Removed dead sampler DTO/policy fields that were not consumed by persistence, diagnostics or later stages. `ProfileHistoricalSample` now exposes only the requirement, selected games and coverage actually used downstream.
- Diagnose exposes the centralized minimum eligible plies together with adaptive recommendation/cap/coverage; Flutter remains observation-only.
- Earlier fixed-500 wording is historical only. The binding rule is: statistics-derived recommendation, fixed floor, 500 hard cap, and post-bootstrap incremental learning outside the initial historical sample.

## Fix Update 134 - foreground SQLite write priority

- User-started main-line analysis/refinement has priority over Knowledge Graph persistence for the shared `kchess.sqlite3` WAL writer.
- `AnalysisService` owns a foreground SQLite-priority lease for the complete lifetime of a user-driven analysis job. Profile-triggered shared analysis does not take foreground priority.
- Knowledge/diagnostic stores using separate SQLite connections must enter the shared background-write gate before starting a write transaction/statement. Once foreground analysis is waiting or active, no new Knowledge writer may begin; only an already-running short background write may finish.
- Do not replace this contract with retry loops or larger `busy_timeout` values. Foreground priority is cooperative scheduling; WAL/busy timeout remains only a fallback for unrelated transient contention.

## Update 135 - grounded, bounded Coach knowledge

- `KnowledgeRuntime::coach_evidence` combines the general graph with the current shared-player StatisticsService read model. Profile/graph completion is never a prerequisite for already recorded metadata/statistical facts.
- `StatisticsService::player_knowledge_json` is the common statistical source for projection and live Coach facts; it uses the existing shared Games/Analysis source, not a second database or engine path. Recorded ratings remain distinct per account and time control.
- Retrieval carries the authoritative `ProfileQueryScope` across every channel. Packets deduplicate graph/chunk representations, exclude invalidated dependencies and require supplied scope/relationship evidence. Provider trimming must recheck native required chunk groups.
- The Coach query path does not index a corpus or wait for the KnowledgeRuntime maintenance mutex. Busy graph maintenance permits live statistical evidence through the same packet builder; unsupported graph claims remain unavailable. Diagnostic trace writes yield immediately to foreground analysis.
- Personal evidence budgets are demand-based and capped at 4000 estimated tokens (4500 including prepared context, excluding fixed provider instructions/schema). No token-reduction or latency percentage is claimed without measurement.

## Update 136 - adaptive evidence-grounded coaching

- Native Automatic Coach spends unsolicited LLM calls on significant verified teaching events; a legal answer to a pending board quiz gets feedback regardless of criticality. Flutter sends only completed native-resolved move context and never scores taps.
- Accepted native candidate-based quizzes remember their original FEN, candidate set and opponent reply in ephemeral session state. Only a legal played move whose resulting FEN matches is counted once. SQLite migration 39 persists scoped per-motif success/other counters; these are practice observations, not engine truth or an Elo estimate.
- Quiz difficulty is chosen natively from same-topic graded attempts with a smoothed Beta posterior and conservative bounds. Learned profile weaknesses expose bounded Beta posterior error-rate uncertainty alongside analyzed-move denominators. Personal similarity retrieval reuses the existing scoped Knowledge Graph/position vector path; it does not create a second graph or engine cache.
- Provider wording may vary naturally, but structured tactical/opening/engine claims and recommended arrows need matching provider-visible native evidence. A second invalid answer after the single repair pass is rejected wholesale. Free prose remains model-generated and must not be presented as deterministic proof.

## Update 137 - confirmed move attempts and calibrated practice

- Opening a saved move through navigation is observation, never a newly played answer. Flutter sends ephemeral completed-move FEN/UCI context only after native board resolution; native CoachService must match it against the authoritative move and resulting FEN before bypassing automatic criticality or scoring a quiz.
- A legal answer outside the small candidate set is ungraded unless native classification or WDL loss confirms a clear error. Quiz counters are observations of graded move choices, not proof of independent mastery; provider-facing fields must say this despite legacy storage names.
- Difficulty changes only after enough graded attempts and conservative posterior bounds. An accepted quiz requires a provider-visible native candidate, a follow-up question and no move recommendation; malformed quiz output gets the same single repair limit as other responses.

## Update 138 - coherent native analysis arrows and classification

- SF18 publishes only complete exact MultiPV iterations to live analysis and persisted analysis; its final `bestmove` callback remains available to the native difficult-position audit. SF19 retains its separate exact snapshot path and rejects empty or duplicate root moves.
- Native AnalysisService emits the published rank-1 move as `bestMove` for main-line and variation JSON, matching the classification recommendation and board-arrow source. Flutter analysis UI and translation files are unchanged.

## Update 139 - cross-pipeline performance baseline

- The existing Graph Inspector developer surface now composes native, lock-free/read-only performance snapshots from AnalysisService, PlayerProfileService, StatisticsService and KnowledgeRuntime. Flutter does not derive these metrics and no new persistence source is introduced.
- Update 139 is measurement-only: classification, profile preparation, statistics and Knowledge Graph behavior remain authoritative in their existing owners. Later optimization updates must compare against these counters instead of adding parallel implementations.

## Update 140 - incremental main-line classification

- `AnalysisService` no longer rebuilds classification from ply 0 after every newly persisted position. During the preparation pass, only a move whose before/after position slots have just become available is classified; out-of-order preferred-slot work is deduplicated in-memory for the worker lifetime.
- The completed game still receives one authoritative full classification/accuracy pass before the analysis run is marked `complete`. Run-level classifier/accuracy versions and game accuracy therefore become current only at that final boundary. Maximum-depth refinement keeps its existing atomic publication behavior.
- Classification reads use one focused native persistence query for the two adjacent position slots instead of constructing two full `PersistedAnalysis` summaries per move. The performance snapshot keeps `sqliteAnalysisReads` as the count of these focused classification read operations. SF18/SF19 classifier rules and SF19 MultiPV search breadth are unchanged.

## Update 141 - incremental profile preparation I/O

- Player-profile queue consumption resolves only the selected game from SQLite instead of reloading the full shared-player source projection for every queue step.
- A full source projection produced by queue synchronization is reused once by the pending learned-profile refresh, eliminating the immediate duplicate full query without introducing another persisted cache or profile data store.
- Explicit native invalidation remains primary; the all-library safety resync is low-frequency. Flutter is unchanged and continues to observe native progress only.

## Update 142 - invalidated native statistics read cache

- Repeated Coach/Knowledge statistics requests reuse a bounded `StatisticsService` in-memory cache keyed by the existing native query scope. The aggregation and SQLite source remain unchanged and authoritative.
- Cache validity follows `Database::statistics_source_revision()`: profile/game mutations and completed-analysis publication advance the generation. Running per-ply analysis writes do not evict the cache until they become visible to the statistics source.
- A result computed across a concurrent source change is returned only to that caller and is not cached. No persisted statistics cache, Flutter cache, duplicate aggregation path or schema migration is introduced.

## Update 143 - incremental Knowledge Graph refresh

- Knowledge projection keeps the existing shared `KnowledgeRuntime`/GraphStore/DependencyTracker ownership. Projectors now report changed/removed entries and skip provenance/dependency rewrites when the persisted dependency source set/version already matches.
- Normal profile refreshes update quality and semantic graph-node chunks only for dirty entries. Restart, player-owner switch, clock reset or six hours since the last safety pass triggers the bounded full maintenance sweep. This is a maintenance fallback, not a parallel refresh path.
- Removed graph nodes remove their owner-scoped semantic chunks immediately; full chunk cleanup remains periodic. Native runtime diagnostics count full/incremental maintenance and actual dirty work. Flutter remains unchanged.

## Update 144 - integrated foreground/background priority diagnostics

- The existing `persistence::SqliteWritePriorityGate` remains the single coordinator for foreground Analysis SQLite priority versus Knowledge/diagnostic background writers. Update 144 does not introduce another scheduler or lock layer.
- The developer Knowledge Inspector now exposes the gate's native current state plus cumulative foreground/background wait counts and wait duration. Flutter must not infer writer contention from UI progress.
- Profile engine preemption remains owned by `PlayerProfileService`/`AnalysisService`; Knowledge writers remain fine-grained users of `BackgroundSqliteWriteGuard`. Interactive work must not be made dependent on graph refresh completion.
- These counters are diagnostics only. They must never influence engine settings, queue order, statistics cache validity or Knowledge ranking.

## Cleanup Update 145 - native dataflow performance closeout

- Updates 139-144 remain one integrated native dataflow path: measurement, incremental classification, focused profile I/O, revision-invalidated statistics caching, incremental Knowledge maintenance and existing SQLite writer-priority telemetry. Do not split these responsibilities into parallel Flutter, Python, cache, queue or scheduler implementations.
- No schema migration, FFI contract or Flutter/ARB change is part of this series. Existing `analysis_runs`/`move_analysis`, player-profile sources, StatisticsService and the shared Knowledge Graph remain the respective sources of truth.
- The final service/persistence/Knowledge contracts are documented in their nearest `AGENTS.md`. The low-frequency full profile/Knowledge passes are safety fallbacks only; normal interactive/background flow stays incremental and foreground Analysis keeps priority over conflicting background writers/engine work.
- Cleanup found no series-owned obsolete file that can be removed safely. Future removals must still preserve historical migrations and compatibility contracts.

## Update 146/148 - external persistent incremental Windows native build

- Windows/MSVC x64 Flutter builds delegate `kchess_core` to a per-checkout CMake/MSBuild tree under `%LOCALAPPDATA%\KChess\build-cache\<checkout-id>\kchess-core`. `flutter clean` keeps its normal Flutter behavior and does not own or delete this external native cache. CMake/MSBuild remains the sole source/header dependency tracker.
- Temporary Stockfish 18/19 compilation state also lives below the same external per-checkout cache root. The stable SF18/SF19 `.lib` artifacts remain under `native/prebuilt/windows/` as permanent local build inputs and must never be deleted by the normal native rebuild flow.
- MSVC `/MP` is enabled for the large KChess core and Stockfish builds so cold compiles can use multiple compiler processes. No Runtime/domain behavior changes are part of this build optimization.
- `tools/build_dev.ps1` is the standard Windows development build and never performs a clean. `tools/rebuild_native.ps1` deletes only the external KChess-core cache for the current checkout and rebuilds it; it does not reset Stockfish. There is no custom `clean_all.ps1`. For a compact transport ZIP, use Flutter's normal `flutter clean`; external caches are outside the repository and therefore are not packed. Exact operator guidance lives in `BUILD.md`.

## Update 149 - startup/background diagnostics

- Core startup timing, profile-worker activity and KnowledgeRuntime activity are distinct native diagnostic domains exposed through the existing read-only Knowledge Inspector contract.
- Completed persisted profiles should not trigger an immediate full-library rescan solely because the process restarted. Background maintenance gets a short startup grace; real invalidations/imports and the existing safety resync remain authoritative triggers.

- Do not copy native runtime/domain logic into build scripts. Build scripts may only locate, configure, build or reset generated build state. `rebuild_native.ps1` is native-only: it rebuilds `kchess_core` through CMake/MSBuild and must never launch Flutter or the app.

## Update 151 - Flutter startup and process-runtime diagnostics

- Startup diagnostics are observation-only and span both sides of the existing boundary: Flutter owns UI/bootstrap timings (`main`, FFI bootstrap, controller reads, first/ready frame and bounded frame timings), while native Core owns process CPU snapshots exposed through the existing Knowledge Inspector.
- Flutter startup measurements must never become a scheduler, readiness gate or replacement for native domain state. Native process CPU diagnostics sample only when the read-only inspector is requested and must not add a monitoring thread.
- `knowledge.inspector.v1` remains the developer diagnostic surface. Flutter may append its explicitly UI-owned `flutterStartup` section for presentation; native `processRuntime`, `coreStartup`, profile/runtime activity and performance counters remain native-owned.

## Update 152 - month-scoped Games startup + analysis cancel UI

- The ordinary online Games surface starts from a native-selected month window, never by materializing the whole profile library in Flutter. Current UTC month wins when populated; otherwise native selects the newest earlier persisted month. Full-history statistics/profile/knowledge consumers are unchanged.
- Analysis cancellation remains owned by the existing native `AnalysisService::cancel_analysis`. Flutter exposes the action with an upper-right `X` during preparation/running analysis and does not fake cancellation by route dismissal alone.

## Update 154 - native Teaching Planner

- `native/ai/teaching/TeachingPlanner` is the single provider-neutral lesson-policy stage between practicality/evidence assembly and the LLM provider. It chooses one primary objective, delivery mode, reveal level and bounded recommendation/concept policy; Gemini formulates the lesson but does not choose a competing teaching strategy.
- Existing native quiz-practice counts feed this same planner. Quiz and early-hint plans use reveal level 0 and permit no move recommendations; the existing native validation/one-repair boundary enforces those output limits.
- Teaching policy is not a measured player-skill claim. Future skill modeling and spaced repetition must extend this planner/scheduler path rather than creating a second Flutter/provider-owned teaching policy.

## Update 155 - fine-grained native Coach skill taxonomy

- `native/ai/teaching/skill_taxonomy.*` is the single native resolver for practice-skill identity. It consumes only already-retrieved native evidence and never starts engine/search work or infers player ability.
- Verified tactical motifs (confidence >= the existing native claim threshold) may refine a quiz into stable namespaced IDs such as `tactics.fork`; strategic-plan evidence may refine strategy practice. Otherwise the resolver falls back to bounded stable families such as `opening.decision`, `endgame.decision`, `strategy.plan_choice` and `calculation.candidate_selection`.
- New verified attempts are grouped by the fine-grained skill ID. Existing coarse Update-136/137 counters are read only as cold-start priors for difficulty so old databases remain useful without rewriting historical telemetry.
- Skill identity is learning telemetry, not a measured strength/weakness claim and not provider-owned policy. Gemini does not choose or persist skill IDs.

## Update 156 - native Coach spaced repetition

- `ai_coach_skill_progress` remains the single learner-practice persistence store; schema migration 40 extends that same row with verified-weak counters plus streak/interval/due scheduling metadata. No parallel training table or Flutter-owned scheduler is allowed.
- `native/ai/teaching/spaced_repetition_scheduler.*` owns interval and due-policy. Scheduling metadata expresses when to revisit a verified exercise, never a measured player rating or proof of mastery.
- Only natively verified quiz attempts update the schedule. An independent success advances the interval; a natively verified weak move resets the streak and schedules a near-term revisit. Ungraded legal alternatives still do not become attempts.
- Historical coarse motif rows remain compatible cold-start priors. New namespaced skill IDs share the same table and scheduling contract.

## Update 157 - interactive Coach priority

- `CoachService` is the single Coach execution scheduler: manual asks and explicit hints are foreground; Automatic Coach turns are background. Foreground work gets the next available serialized provider/session slot and cancels unfinished Automatic jobs rather than waiting behind an Automatic backlog.
- Provider/session execution remains serialized. An Automatic request already inside the current provider call may finish because provider cancellation is not yet part of the contract; a preempted job discards that result. Do not add parallel Gemini calls to simulate preemption.

## Update 158 - cached native position intelligence

- `native/ai/position/position_analysis_stage.*` is the single Coach stage for deterministic position features, weaknesses, exploitation plans, strategic plans and tactical motifs. The historical `analyze_stub` path is removed; do not recreate deterministic Coach position analysis inside the orchestrator or Flutter.
- The stage owns a bounded in-memory per-position cache (128 exact-FEN entries). It is read-through and version-local: no SQLite persistence, no second source of truth and no engine results are stored there. Requested evidence is still emitted through the existing provider/validator pipeline.
- `EvidenceRetriever` no longer computes `PositionFeatureExtractor` separately; position intelligence is computed once in the dedicated stage and reused across follow-up questions on the same position.
- Coach performance diagnostics expose per-turn and aggregate position-cache hits plus bounded entry/eviction counters. Cache telemetry is diagnostic only and must not alter teaching/routing policy.

## Update 159 - exact validated Coach response cache

- `native/ai/optimization/ValidatedResponseCache` is the only provider-response cache. It is process-local, bounded and keyed by the exact provider-neutral request plus both provider-visible and full native validation evidence, provider id, teaching policy, profile scope, session context and Coach prompt/schema version. It is never persisted as player knowledge.
- Only structured content that already passed the existing native `ResponseValidator` (including the one allowed repair pass) may be stored. A cache hit may skip the remote provider and repeated validation because the exact validation inputs are part of the key; session remembering and verified learning-attempt handling still run normally.
- Evidence/profile/analysis changes naturally miss the cache because their payloads are part of the key. Do not add time-based stale-answer heuristics or a second Flutter/provider cache. Cache telemetry is diagnostic only.

## Update 160 - expected-score-aware Coach practicality

- Coach candidate/practicality scoring reuses existing engine WDL as root-side expected score. No fresh engine work is introduced.
- Expected-score loss is the preferred objective gate for practical alternatives; the prior 80 cp limit remains a fallback only when WDL is unavailable. This changes Practicality to v2 while preserving engine rank as source of truth.

## Update 161 - Automatic Coach teaching value

- The existing native `AutomaticCoachTrigger` remains the only unsolicited-Coach gate, but its final threshold now uses bounded `teaching_value` rather than event criticality alone.
- Objective importance still comes from the existing classification/WDL/motif/phase/repeated-mistake signals. `CoachService` may add only native learner-state scheduling context from the existing `ai_coach_skill_progress` store and short process-local recency of prior successful automatic deliveries.
- Due-practice relevance is scheduling context, never proof of weakness or chess evidence. Recent automatic delivery adds an interruption cost so weak events can be deferred; severe objective events can still pass. Verified answers to an open board question continue to bypass the unsolicited gate and are not counted as interruptions.
- Automatic history is bounded, process-local and session/profile scoped. It is not persisted and must not become a second learner model.

## Retired local-model runtime

- The former tiny routing/context/evidence/embedding model stack is fully retired. Production Coach routing and action planning are deterministic C++20; Gemini is the only LLM and is used only when `AnswerGate` requires language generation.
- Do not reintroduce `smallModels` diagnostics, local model discovery, KCEP/ONNX runtime loading, local embedding/reranker inference or a second learned planner.

## Update 165 - Coach provider-input optimization

- The first dedicated AI-Coach optimization pass keeps full native evidence authoritative while deduplicating exact provider-visible evidence before Gemini serialization/budgeting.
- Coach diagnostics expose provider-evidence item/token reduction so provider payload tuning remains measurable; no validation, Knowledge, Engine or player-profile evidence is deleted from the native truth path.

## Update 167 - AI Chess Trainer final architecture

- The AI Trainer remains native-first. C++ owns routing, evidence, position intelligence, objective/practical move context, teaching policy, skill identity, spaced repetition, personal-training selection, Automatic-Coach teaching value, provider scheduling, validation and caches. Flutter only presents/collects input and renders native results.
- Gemini is the language/dialog layer after native lesson planning and bounded evidence selection; it never becomes the source of engine truth, player-skill truth or scheduling policy.
- The existing `ai_coach_skill_progress` table is the single persisted practice store. Position/response caches are optimizations, not parallel knowledge stores.
- Manual questions/hints have priority over Automatic Coach. Full validation and the single repair boundary remain mandatory even after provider-input optimization; only exact already-validated responses may be reused.
- Final architecture and operator-level behavior are summarized in `KChess_Coach_Konzept.md`; local `AGENTS.md` files remain the ownership source for implementation work.

## Update 170 - native rebuild output + MSVC environment warning cleanup

- `tools/rebuild_native.ps1` keeps its recovery semantics: it resets only the external per-checkout KChess-core tree and performs a full native rebuild, but suppresses ordinary compile-file spam. It surfaces warnings/errors and prints only local native source/build inputs changed since the previous successful native build using developer-only SHA-256 manifest metadata outside the reset tree.
- The changed-file report is presentation/diagnostics only; CMake/MSBuild remains authoritative for configure/build behavior. `third_party/` and stable `native/prebuilt/` inputs are not scanned or modified by the report.

## Fix Update 171 - interactive Automatic-Coach questions

- A validated Automatic-Coach response that exposes a concrete `followUpQuestion` while native candidate moves are available becomes an ephemeral open board question in the existing Coach session. A legal move from that original FEN may therefore receive immediate feedback instead of being treated as an unrelated unsolicited event.
- Only explicit `CoachMode::quiz` questions are scored into `ai_coach_skill_progress`; an Automatic follow-up question may reuse candidate matching for feedback but must not mutate spaced-repetition state.
- When a played move answers an open trainer question, `CoachService` treats that provider turn as interactive foreground feedback even though it entered through the Automatic endpoint. It bypasses Automatic-only Gemini soft guards and joins the foreground execution class; ordinary unsolicited Automatic turns keep their existing quota/teaching-value policy.

## Update 172 - focused, grounded Coach language

- Gemini receives only prompt clauses relevant to the native query, board, training and profile state. Common grounding and typed-claim rules, provider-visible evidence scope and native validation remain mandatory.
- A typed verified-learner-feedback flag controls move-answer wording and exact response-cache identity. Previous answer prose cannot activate the flag.
- Off-topic Coach requests stop after native routing. `CoachPrompt v3` prevents reuse of responses validated under the previous instruction. Flutter UI, ARB and chess truth sources are unchanged.

## Update 173 - Coach diagnostics and adaptive dialogue

- Coach UI may display the existing read-only native LLM pipeline trace through a dedicated C-ABI/FFI endpoint and copy the currently visible conversation on explicit user action. Diagnostics must stay free of prompt, transcript, FEN and secret payloads; Flutter does not infer provider or teaching policy.
- Provider-visible candidate/PV moves and natively validated move claims bound coordinate-move mentions in answer prose. This narrow check does not certify arbitrary free prose or SAN.
- Verified learner-attempt state takes priority in the native teaching plan. Personal practice may add a small exploration bonus only after multiple candidate skills have enough verified graded outcomes; profile weakness and due scheduling remain the primary signals.
- Synthetic dialogue scenarios and a manual review rubric live under `tools/ai/evaluation/`. They are offline quality material, not app runtime or evidence of passed tests.

## Update 174 - claim-linked Coach answers and move contrast

- Gemini's `coach_response.v3` asks for an exact `answer_quote` on every non-general typed claim. Native validation checks that quote against the returned answer/question and independently checks the cited source; `CoachPrompt v4` keeps older cached responses out of this contract. Quote linkage does not prove every uncited free-prose sentence.
- A completed, native-verified board answer may add `move.contrast.v1` from the original question FEN, the played move and the already remembered candidate. It reuses deterministic position features and never starts a new engine search or treats feature deltas as Stockfish evaluation. Numeric contrast claims validate exact provider-visible scalars.
- The offline dialogue-review script summarizes human verdicts against the synthetic scenario catalogue. Human-edited teaching examples remain private until their rights, scope and quality are established. The next prioritized work is recorded in `docs/Coach_Naechste_Verbesserungen.md`.

## Update 175 - segmentierte Coach-Antworten und belegte Zugurteile

- Gemini liefert `coach_response.v4` mit geordneten Antwort-/Rückfragesegmenten. Der sichtbare Text wird nativ daraus zusammengesetzt. Jedes faktische Segment verweist auf genau einen typisierten Claim mit identischem `answer_quote`; die Validierung nutzt nur provider-sichtbare Evidenz. Das ist eine strukturelle Grenze und ersetzt keine menschliche semantische Prüfung.
- `move.contrast.v1` enthält objektive Klassifikation/Score nur aus einer bereits gespeicherten, vollständigen Analyse, wenn Partie, Halbzug, UCI, Ausgangs- und Ziel-FEN übereinstimmen. Ohne diesen Cache bleibt der Vergleich statisch. Keine zweite Engine-Suche und kein zweiter Analysespeicher.
- Echte Coach-Dialoge und menschliche Korrekturen bleiben private lokale Evaluationsdaten. `tools/ai/evaluation/` fasst menschliche Urteile zusammen und trennt Spieler-/Partiegruppen in Entwicklung/Held-out. Es trainiert kein Modell und liefert keine App-Runtime-Wahrheit.

## Update 176 - Coach error taxonomy

- Coach failures are classified natively before Flutter presentation. Validation rejection, malformed provider output, missing verified exercise evidence, quota/backoff, provider transport errors and true provider unavailability are distinct machine states.
- Coach diagnostics expose privacy-safe status classes plus prompt/response/status contract versions so a captured diagnostic can identify the active Coach contract without logging prompts, boards, users or secrets.

### Update 177 - Coach response contract v5

Gemini no longer authors global claim indices or duplicate `answer_quote` strings. Each factual segment carries its own grounding metadata and native parsing derives the provider-neutral `CoachClaim` plus internal segment link. The request-specific structured schema permits `profile_status=not_used` only for non-profile questions, and permits only `grounded`/`insufficient_evidence` for profile questions. The response/cache identity is `coach_response.v5` with `CoachPrompt v6`.

## Update 178 - native Coach candidate references

- `engine.candidates.v1` assigns stable per-response `candidate_id` values to the native best/alternative/user candidates. These IDs are transport references only; UCI remains native chess data.
- Gemini may select a concrete recommendation or move-bound factual claim only through a provider-visible `candidate_id`. The provider adapter resolves that ID back to the exact native UCI before the response reaches validation/UI; provider-authored recommendation UCI is no longer part of the wire contract.
- Native validation cross-checks candidate ID and resolved UCI against the exact provider-visible candidate evidence. Legacy internal/local DTO callers without candidate IDs remain supported, but the production Gemini path uses `coach_response.v6` / `CoachPrompt v7`.

## Update 179 - position follow-up routing

- Concrete follow-up questions now outrank the previous trainer topic: a new explicit plan/move question keeps its own native intent while still receiving bounded session context. Only short elliptical follow-ups inherit the old intent.
- Board questions that ask how to prevent/avoid a concrete continuation request native probe/candidate grounding, so the provider explains verified choices instead of continuing an unrelated prior teaching skill.

## Update 180 - single pending trainer question

- A scored quiz remains the only authoritative open exercise for its original FEN until a legal learner move resolves it. Reissuing quiz mode on the same board returns the native `quiz_question_pending` state instead of generating and storing a duplicate question.
- Hint/explain requests can still advance assistance, but they do not replace the scored expected move. Flutter localizes the pending state and suppresses duplicate visible notices.

## Update 182 - provider/cache identity hardening

- Provider failures carry provider-neutral failure classes so malformed structured output, quota/transport problems and true unavailability are not conflated. Detailed Gemini parse codes remain diagnostics-only.
- Exact validated-response cache identity includes provider+model identity and the `CoachValidator` contract version in addition to prompt/schema versions, preventing stale validated prose from crossing model or validator changes.

## Update 181 - native safe fallback and repair boundary

- A rejected provider answer for a position-bound request may fall back to native candidate evidence instead of being presented as model unavailability. `position_explanation` exposes at most the verified native best candidate; `candidate_comparison` exposes at most two verified native candidates. Flutter only localizes the fallback wording and renders the returned board moves.
- Quiz and hint validation failures with a deterministic fallback do not spend a repair-provider call. Move-authority violations (`claim_move_illegal`, `recommendation_move_illegal`, or candidate-reference mismatches) are discarded immediately and never receive an LLM repair pass.
- Profile/grounding/schema problems that can actually be corrected may still use the single existing repair pass. No fallback is allowed to invent move text, evaluation, profile facts or board state.

## Update 183 - Coach candidate-integrity gate

- Before Coach candidate evidence reaches Gemini, native fallback or trainer-session scoring, `EvidenceRetriever` rechecks every root move against the request's exact FEN and retains only legal candidates. PVs are truncated to their legal sequential prefix and duplicate root moves are removed before candidate IDs are assigned.
- This is a defensive integration boundary over existing Analysis/Stockfish sources, not a new chess engine or cache. If a cached/source candidate no longer matches the current board, it is discarded and cannot become a provider recommendation, board arrow or scored expected move.

## Update 184 - Coach generation-time constraint optimization

Gemini structured-output schemas mirror deterministic native teaching limits before inference: recommendation count is bounded by the current teaching plan and is zero for quiz/hint/reveal-level-0 turns. Move-bound claim kinds are unavailable when no native candidate IDs are provider-visible. Native response validation remains mandatory; optimization must prevent avoidable repair calls rather than bypass validation.

## Update 185 - bounded Coach provider evidence

- The Coach provider-input optimizer now compacts only the disposable provider-visible copy of high-volume candidate/PV and tactical-motif evidence before applying the existing evidence token budget. Full native evidence is unchanged and remains the source for legality, validation, trainer-session state and safe fallbacks.
- Candidate compaction preserves native candidate IDs and root moves while bounding PV depth and surplus alternatives according to answer depth/mode/teaching policy. Tactical motif compaction only removes surplus tail entries. Neither optimization may create chess facts or expand provider authority.
- Coach performance diagnostics report `compactedItems` and `estimatedCompactionSavingsTokens` per turn and in aggregate so provider-input reduction can be measured independently from ordinary budget trimming.
## Update 186 - AI Trainer reliability series cleanup

Updates 176-185 form one cumulative reliability contract: native C++ owns board legality, candidate identity, session scoring, fallback chess data, provider failure classification and exact-cache authority; the LLM is limited to structured explanation/selection over provider-visible native evidence; Flutter remains presentation/localization only. Keep `CoachPrompt v7`, `coach_response.v6`, `CoachValidator v1` and the documented fallback/status names synchronized when any side of this contract changes. `third_party`, secrets and build products remain local and are never part of update archives.

## Updates 187-198 - Grounded Coach Architecture

The Coach now treats explicit current-turn chess questions as authoritative and resolves them into native position-analysis modes before retrieval or teaching. Worst-move/fastest-loss questions use bounded complete-root Stockfish analysis; concrete chess truth is exposed as stable native candidate/fact references rather than provider-authored move notation. `coach_response.v7` / `CoachPrompt v9` use grounded fact-reference segments, native rendering and native fallback/validation. Automatic turns are one-provider-call maximum, deterministic native fallbacks bypass unnecessary repair, and provider-visible evidence is compacted without changing full native truth. Exact-FEN/settings engine-result reuse is process-local only. Flutter remains presentation/localization; secrets, `third_party` and build outputs remain local and are excluded from update archives.

## kchess_update_serie_1 - Update 11 repository hygiene

- Local `secrets/` and `third_party/` trees are intentionally excluded from Git/update archives. Do not weaken those ignores to package local credentials, vendor payloads, Stockfish binaries or Planner weights.

## Stateful Coach planner series

- The Coach interaction path uses deterministic native semantic classification plus deterministic resolution; native interaction state and actions remain authoritative.
- Native `ai/interaction/` owns semantic IDs, primitive actions, compact conversation state and deterministic follow-ups. Flutter remains presentation-only.
- Follow-up UI controls carry structured native action IDs; clicking one must not require another natural-language planner call.
- Full chat history is not planner context. Only compact state/references may be supplied to the future planner path.

## Cleanup Update Series 1 - Update 1 repository hygiene

- Removed one-off Flutter patch scripts (`flutter_app/fix.py`, `fix2.py`, `fix3.py`) and committed Python bytecode/cache directories. They are developer residue and are not part of the Flutter UI, native runtime, training pipeline or supported tooling.
- Cleanup work must stay conservative: generated or demonstrably unreferenced residue may be removed immediately, while runtime, ABI, migration, model and dependency paths require reference/ownership verification in the later cleanup updates before deletion.
## Cleanup Update Series 1 - Update 2 Flutter dependency cleanup

- Removed the unused direct Dart dependency `path`; no production or test Dart source imports it. Its lockfile entry remains transitive because Flutter/tooling dependencies still resolve it indirectly.
- Verified all 90 Dart files under `flutter_app/lib` are reachable from `lib/main.dart` through imports/exports; this update therefore removes no Flutter source files or FFI adapters on heuristic grounds.
- Direct Flutter packages `ffi`, `file_picker`, `fl_chart`, `flutter_svg`, `intl` and `path_provider` remain because each has concrete production imports. Flutter remains UI/View-State plus thin FFI only; dependency cleanup must not move domain logic into Dart.

## Cleanup Update Series 1 - Update 6 final audit

- Final static audit completed without running builds, tests, `flutter analyze` or `flutter run`, per project policy.
- No productive C++/Flutter references remain to the removed lightweight scout API (`kc_start_scout_json`, `Core::start_scout_json`, `ProviderService::start_scout_json`, `ProviderService::run_scout`). The single supported path is the scout-report pipeline.
- Flutter `path` is no longer a direct dependency. Its lockfile entry is retained only as a transitive dependency resolved by the remaining Flutter toolchain/packages.
- Physical deletions for repository residue and retired branches are applied by the series cleanup PowerShell script shipped with the cumulative ZIP. The script also removes Python bytecode/cache residue and common generated Flutter/native build caches, while preserving migrations, third-party sources, prebuilt runtime assets and accepted planner generations.


## Cleanup Series 2 - local planner/model retirement

- The experimental Evidence Planner/KCEP pipeline and all local semantic/router/text-encoder/reranker model runtimes are retired and removed.
- Production planning is deterministic C++20 (`PrimaryInteractionPlanner`, `InteractionRequirementResolver`, `QueryPlanner` and native evidence/engine policy). Do not recreate a parallel learned planner or local-model fallback.
- `tools/ai/` is limited to non-private dataset preparation and Coach dialogue evaluation; model checkpoints, ONNX/KCEP artifacts, tokenizer bundles and training-run directories are not part of the repository.
- `secrets/` is intentionally outside this cleanup decision and must not be altered by cleanup scripts.

## LLM Interaction Series 1 - action-aware routing

- Direct product/session requests are classified before legacy evidence planning with `InteractionRequestKind`: `question`, `analysis`, `app_action`, or `conversation_control`.
- `start_game`, game continuation, and game stop language must not become `position_overview` merely because the start/current board is present. For app/session actions, the interaction layer suppresses legacy positional evidence and engine planning.
- Game-action recognition is deterministic C++20 for German, English, and Arabic. Do not reintroduce a local intent model or duplicate Flutter-side routing.
- This update only establishes the routing contract; action execution remains owned by the existing native `ActionCatalog`/`ActionChainEngine` path in the next update of the series.

## LLM Interaction Series 1 - Update 2 action fulfillment

- Native C++ owns product/session action selection. `CoachResponse::client_actions` is the machine-only transport from the resolved `ActionChain` to Flutter; visible coach prose must never be parsed to infer navigation or mutations.
- Direct game actions complete without Gemini. `start_game` resolves to `open_bot_game_setup` or `open_bot_game` (when an explicit Elo is present); continue/stop resolve to `resume_bot_game` / `resign_active_bot_game`.
- Flutter is limited to thin execution of the supplied native command: navigation plus existing FFI calls (`activeBotGame`, `resignBotGame`). It may not reclassify user text or choose a different product action.
- Action-only Coach responses are valid successful responses even when `answer` is empty. Do not turn an empty-prose native action into `provider_unavailable`.

### LLM Interaction Series 1 - Update 3 mixed action/language turns

KChess now treats product/session action intent and language-answer intent as orthogonal native contracts. Native `clientActions` are materialized from the resolved C++ action chain even when the same turn also calls Gemini. Gemini receives `coach_llm_context.v5` with native `requestKind`, session/product `requestedActions`, explicit action parameters and `answerIntent`; it may render only the language portion and cannot select or execute those actions. Future live-coaching wording attached to `start_game` is not reclassified as an immediate position explanation. `CoachPrompt v14` invalidates pre-contract response-cache entries. Flutter remains a thin executor of supplied `clientActions` only.

## LLM Interaction Series 1 - Update 4

Product-action success is no longer inferred from a valid Coach text response. The deterministic C++ interaction plan is checked against the final native `clientActions` by `ActionFulfillmentValidator`, and production Coach diagnostics report that result separately from provider grounding validation. The unused standalone interaction-diagnostics implementation was removed so diagnostics describe the actual `CoachOrchestrator`/`CoachService` path. Flutter remains a thin executor of native-approved client actions.

## LLM Interaction Series 1 - Update 5 regression and parameter contract

- Direct game-action regression coverage is executable native test code, not documentation-only fixtures. `kchess_interaction_action_routing_tests` covers German, English and Arabic start-game requests, negative non-action examples, mixed start+coaching turns, explicit Elo and explicit color.
- Explicit opponent Elo on a start-game request is an action parameter and must not be reclassified as rating prediction. German `1800er` opponent wording is parsed deterministically when bounded game-opponent language is present.
- Explicit player color is part of native `CoachClientAction` transport. `open_bot_game` carries optional `elo` and `color`; Flutter only forwards them to the existing bot-game FFI. Native bot-session creation owns validation and persistence of `white`/`black`.
- The C ABI changed for `kc_create_bot_game_json` and is therefore ABI version 10. Keep the Flutter supported ABI version in lockstep with `KCHESS_CORE_ABI_VERSION`.

## Coach Series 2 - Update 1 best-move response fulfillment

- Provider-free `best_move` analysis is a successful native Coach reply. C++ transports the verified move in `board_moves` plus machine-only `native_answer_kind=best_move`; it must not be mislabeled `provider_unavailable` merely because Gemini prose is intentionally skipped.
- Flutter remains presentation-only and localizes the native move through the existing ARB `bestMoveText(move)` string. It must never recompute the best move or infer a deterministic answer from free text.
- Provider validation and deterministic native-answer fulfillment remain separate boundaries. This update does not add an LLM call to best-move questions.

## Coach-Serie 2 / Update 2 - Automatic Coach Trigger Gate

- Unsolicited Coach turns now require a primary native teaching reason and a recency-aware threshold. Motif and phase-transition detection remain supporting evidence and cannot trigger provider work on their own.
- A successful automatic delivery creates a short quiet window; only severe events may interrupt again immediately. This is deterministic C++ policy in `native/ai/automatic/` with delivery recency owned by `CoachService`; Flutter must not implement cooldown logic.
- `inaccuracy` is an authoritative native category. Automatic Coach consumes it directly as a low-priority teaching signal; `okay` is not reinterpreted as an inaccuracy surrogate.


### Coach automatic move attribution (Series 2 Update 3)
Automatic Coach turns carry a native `CoachMoveAttribution` contract from `CoachService` through `LLMProviderRequest`. The mover color comes from the authoritative pre-move FEN; learner color comes from the game request context and yields `moverRole=learner|opponent|unknown`. Only `moverRole=learner` plus a verified completed UI move may populate `user_move_uci` or learning-attempt success/error state. Provider context `coach_llm_context.v5` exposes the role metadata and an evidence-backed move reference when one exists; it must never call an opponent move "your move". Concrete evaluation/quality claims remain evidence-gated. `CoachPrompt v14` invalidates older cached prose.

## Coach Series 2 Update 4 - persistent AI Coach sessions

AI Coach conversations now have a durable native persistence contract in the existing SQLite database. Sessions are profile-owned, dated, resume-capable and receive monotonic default names `Sitzung N` (`N > 0`); numbers are not reused after deletion. Visible user/assistant messages are stored separately from the bounded compact `CoachSessionState`, so persistence never turns the full transcript into planner/provider evidence. Rename/delete/list UI and FFI are delivered in the following update; Flutter remains presentation-only.

## Coach-Serie 2 / Update 5 - persistente Sitzungsverwaltung

Die in Update 4 eingeführten nativen Coach-Sitzungen sind jetzt vollständig über die bestehende C++/SQLite -> Core -> C-ABI -> Flutter-FFI-Kette verwaltbar. Der allgemeine Coach öffnet beim App-/Tab-Einstieg die zuletzt aktualisierte Profilsitzung; kontextgebundene Einstiege aus Analyse/Training/Partie erzeugen eine neue Sitzung, damit kein alter Brettkontext unbemerkt übernommen wird. Sitzungen können gelistet, geöffnet/weitergeführt, neu erstellt, umbenannt und gelöscht werden. Löschen der aktiven Sitzung erzeugt sofort eine neue monotone `Sitzung N`; gelöschte Nummern werden nicht wiederverwendet. Beim Wiederöffnen werden Transcript und die letzte native `currentBoard`-Stellung wiederhergestellt. Flutter bleibt Präsentation/View-State und führt keine Session-Namens-, Nummerierungs- oder Persistenzlogik selbst aus. Die C-/Flutter-ABI ist für die neuen additiven Exporte auf Version 11 angehoben. Für dieses Update wurden keine Builds, Tests, `flutter analyze` oder `flutter run` ausgeführt.

## Coach-Serie 2 / Update 6 - budgetierte Knowledge-Maintenance

- Ein Profil-/Kontowechsel darf weiterhin `profile_refresh` auslösen; der Trigger selbst ist korrekt. Teure abgeleitete Knowledge-Projektionen dürfen daraus aber keinen ungebremsten minutenlangen CPU-Scan machen.
- `PlayerProfileService` bildet aus den bereits geladenen autoritativen Game-Source-Zeilen einen stabilen Graph-Source-Fingerprint und übergibt ihn an `KnowledgeRuntime`.
- Der letzte erfolgreich projizierte Fingerprint sowie der Zeitpunkt der letzten Full-Maintenance werden profilbezogen in SQLite persistiert (Schema 44). Nach App-Neustart oder Rückwechsel auf ein unverändertes Profil werden Opening-, Position-Structure- und Result-Transition-Projektionen wiederverwendet statt erneut über alle Partien zu laufen.
- Wenn sich die Quellen wirklich geändert haben, bleibt die Position-Structure-Projektion vollständig deterministisch, gibt aber nach kleinen Spiel-Batches CPU-Zeit zurück. Diagnostics zählen `gameGraphProjectionSkips`, `backgroundYieldCount` und `backgroundYieldMs`.
- Foreground-Analyse behält über den bestehenden SQLite-/Profile-Worker-Prioritätspfad Vorrang. Der neue Fingerprint ersetzt keine Quelle und keine Analyse; er verhindert ausschließlich redundante derived-knowledge-Arbeit.
- Flutter bleibt unberührt; die Änderung ist native C++/SQLite-Runtime-Logik.

## Coach-Serie 2 / Update 7 - Legacy App Logo + Abschlussaudit

- Das kanonische KChess-Branding wird wieder auf den ursprünglichen App-Logo-Stand zurückgesetzt. `flutter_app/assets/analysis_img/app_logo.png`, alle fünf Android-`ic_launcher.png`-Dichten und `flutter_app/windows/runner/resources/app_icon.ico` stammen byte-identisch aus dem ursprünglichen KChess-Projektstand; keine neue Logo-Variante oder Generator-Abhängigkeit wird eingeführt.
- Der Abschlussaudit bestätigt als aktuellen nativen Vertrag ABI 11. Coach-Sitzungen bleiben profilgebunden und persistent, ihre `Sitzung N`-Nummern monoton; Flutter verwaltet nur Darstellung und FFI-Aufrufe.
- Provider-freie `best_move`-Antworten, das Automatic-Coach-Gate, Move-Attribution und die Knowledge-Maintenance-Drosselung bleiben getrennte native Verantwortlichkeiten. Ein Profil-/Kontowechsel darf weiterhin einen notwendigen Refresh auslösen; unveränderte Game-Graph-Quellen werden über den persistierten Fingerprint übersprungen und echte Projektionen kooperativ gedrosselt.
- `secrets/` ist ausdrücklich nicht Bestandteil dieser Serie und darf weder vom Paket noch von einem Cleanup-Schritt verändert werden.
- Der Serienabschluss wurde ausschließlich statisch auditiert. Es wurden keine Builds, Tests, `flutter analyze` oder `flutter run` ausgeführt.

## Coach Functional Hotfix 2 - direct native analysis fulfillment

- `best_move` and `worst_move` are provider-free native answers. C++ owns the verified move selection and transports `native_answer_kind` plus `board_moves`; Flutter only localizes the sentence through ARB (`bestMoveText` / `worstMoveText`).
- A direct best/worst/fastest-loss question overrides a persisted quiz/hint continuation inside the same durable session. The transcript/session survives, but stale expected-move, scoring and hint-ladder state is retired so delayed training prompts cannot appear after the explicit analysis answer.
- Direct best/worst current-board analysis bypasses conversation/concept/profile/opening/theory retrieval. Engine/cache/current-position evidence remains native authority; no historical profile query is required merely to answer the current-board extreme.
- `status=ok` and Coach performance `statusClass=ok` share one renderability contract: visible text/follow-up, validated client action, or a native answer kind with an actual board move. A machine kind without its native move is not success.



## Coach Answer Quality Serie 3 - Update 1 verbindlicher Chess Verdict Contract

- Bewertende Coach-Turns besitzen jetzt einen nativen `ChessVerdictContract`. C++ entscheidet `PositionVerdict` (`white_winning`, `white_clearly_better`, `white_slightly_better`, `equal`, spiegelbildlich fuer Schwarz) und, wenn belastbar, `MoveVerdict` (`best`, `excellent`, `good`, `playable`, `inaccuracy`, `mistake`, `blunder`, `losing`).
- Bereits vorhandene native Move-Klassifikation hat Vorrang. Kandidatenbasierte Urteile werden nur aus verifizierten Engine-Kandidaten abgeleitet. `worst_move`/`fastest_loss` bleiben Complete-Root-Extremanalysen; ein extremes Focus-Move wird nicht automatisch zu einem erfundenen Blunder-Label.
- Der Provider erhaelt den Vertrag ueber `coach_llm_context.v5`. Ein `authoritative=true` Verdict ist nicht verhandelbar: Nutzertext darf es nicht abschwaechen, umdrehen oder ersetzen; Gemini darf nur die native Entscheidung anhand gelieferter Fakten erklaeren.
- `CoachResponse` und Performance-Diagnostics transportieren das Verdict maschinenlesbar. Flutter darf daraus keine eigene Schachbewertung berechnen.

## Coach Answer Quality Serie 3 - Update 2 Challenge/Re-Evaluation

- Nutzereinwaende gegen ein zuvor autoritatives Schachurteil werden als Hypothese behandelt, nicht als neue Wahrheit. `verdict/chess_verdict_review.*` bindet den Einwand an das letzte native `ChessVerdictContract` derselben Coach-Sitzung.
- Die Sitzung persistiert dafuer das letzte autoritative Verdict, die zugehoerige Move-Attribution und die urspruengliche Bewertungs-FEN. Bei einem echten Boardwechsel werden diese Review-Referenzen fuer den aktuellen Turn verworfen.
- Ein erkannter Einwand erzwingt frische foreground Engine-Arbeit. Fuer einen konkreten bestrittenen Zug vergleicht `AnalysisService::coach_move_review_json()` den aktuellen nativen Bestzug mit genau diesem legalen Root-Zug via `search_moves`; das LLM entscheidet weder den Zielzug noch das Ergebnis.
- C++ klassifiziert den Recheck als `unchanged`, `changed` oder `inconclusive`. Nur `changed` darf eine vorherige Coach-Bewertung korrigieren; `unchanged` muss am bisherigen Urteil festhalten; `inconclusive` darf keinen Sieg des Nutzerarguments vortaeuschen.
- Provider-Kontext ist jetzt `coach_llm_context.v6`; `coach_response.v8`, ABI 11 und SQLite-Schema 44 bleiben unveraendert. Keine Builds oder Tests wurden fuer dieses Update ausgefuehrt.

### Coach Answer Quality Serie 3 - Update 3/6

Native chess judgments are now locked across the provider boundary. `coach_response.v9` contains a required `verdict_lock` echo whose schema permits only the exact native position/move verdict and native re-evaluation outcome for the turn; native validation checks the echo again. Gemini may explain those judgments but cannot soften, reverse or negotiate them into another technically valid response. `CoachPrompt v15` invalidates old exact-response cache entries. `coach_llm_context.v6`, ABI 11 and SQLite schema 44 remain unchanged. No builds or tests were executed for this update.

### Coach Answer Quality Serie 3 - Update 4

Evaluative current-board questions are fail-closed: move/position quality judgments must carry an authoritative native `ChessVerdictContract` before provider prose is allowed. Missing verdict evidence returns the localized `verdict_grounding_unavailable` safe fallback with zero Gemini calls. Keep Flutter presentation-only and keep the engine/verdict authority in C++20. No ABI or SQLite schema change in this update.



### Coach Answer Quality Serie 3 / Update 5
Authoritative Coach evaluations now render verdict-first. The provider response schema is `coach_response.v10` and carries a mandatory short localized `verdict_summary` whenever native `ChessVerdictContract.authoritative=true`; native code prepends it before explanation segments and rejects summaries without a native verdict. The semantic decision remains locked by `verdict_lock`; Gemini only phrases and explains it. `CoachPrompt` is now v16 so exact response caches cannot reuse pre-structure answers. `coach_llm_context.v6`, ABI 11 and SQLite schema 44 remain unchanged. No builds, tests, `flutter analyze` or `flutter run` were executed for this update.

## Coach Answer Quality Serie 3 / Update 6 — adversarial regression boundary

The six-update verdict series closes with the native `kchess_coach_verdict_regression_tests` CTest target. It covers DE/EN/AR evaluative wording, persuasion/objection follow-ups, same-move/same-FEN recheck binding, changed/unchanged/inconclusive native review outcomes, exact provider `verdict_lock` echo, and mandatory verdict-first summaries. Conversational agreement is never evidence: phrases such as "gibst du mir jetzt recht?", "do you agree with me now?" and Arabic equivalents trigger native review when a prior authoritative verdict exists. No provider prose may change a chess judgment without changed native evidence.

## Classification/Arrow Coherence Series - Update 2/9

`AnalysisService` now emits `analysis.snapshot.v1` provenance for main-line analysis. Engine/PV and move classification each have an explicit snapshot ID built from canonical FEN, engine/config identity, requested/reached depth, MultiPV, rank-1 move, search mode and live generation. The existing live-refinement freeze is therefore visible as `coherent=false` when a new live PV/arrow is paired with an older published pre-analysis classification. Flutter may parse this contract but must not repair or reinterpret it. Analysis config identity now includes `ChessEngine::cache_identity()` so SF18/SF19 NNUE evaluator changes cannot reuse old analysis snapshots.

## Classification/Arrow coherence series — Update 3/9

- SF18 sideline arrows and move classification now share one root snapshot width. Internal classification lines may be wider than the UI preference, but Flutter receives only the configured visible MultiPV count.
- Hard invariant: a user following the published SF18 rank-1 arrow cannot receive a negative classification merely because a wider classification search would have reordered rank 1.

## Classification publication contract — current

- Every completed legal move receives a normal native category. `pending`, `unknown`, and stability diagnostics are temporary running/diagnostic states only; they are not durable completed labels.
- SF19 root-vs-after contradiction remains native diagnostic evidence. It may trigger a bounded recheck, but it never withholds a finished category. Accuracy still follows the authoritative root decision snapshot.
- Negative severity remains outcome-first for SF18/SF19: normalized WDL/expected-score loss first, CP only when normalized outcome evidence is unavailable. Short-PV material uses final net material, not transient capture swings.
- Depth/budget comparability and boundary proximity may explain a later reclassification, but they do not gate publication. More depth is allowed to replace the earlier label.
- Current classifier generations for this contract are SF18 v18 and SF19 v1910. ABI 11 and SQLite schema 44 remain unchanged. Flutter stays presentation-only.

## Classification/Arrow Coherence Series - Update 7/9 (persistence/cache/reclassification)

- `analysis/classifier_contract.h` is the single native source of truth for the currently publishable classifier generation (SF18 v18, SF19 v1910). Services and SQLite reads must not duplicate these version numbers.
- Complete engine rows remain reusable even when their derived move labels are stale. Cache reuse rebuilds classification from persisted engine slots; it must not start Stockfish merely because the classifier generation changed.
- Statistics, Games analysis badges/local accuracy, PlayerProfile source rows, move-error samples and profile move evidence may consume only a run that satisfies the current classifier contract. `classifier_version > 0` or non-null move-row versions are no longer sufficient.
- `Database::analysis(...)` may still expose raw engine PV/best-move data for a stale run, but must suppress stale category/expected-score/theory/accuracy fields until reclassification commits the current contract.
- Final reclassification invalidates the statistics generation and requeues derived profile evidence. Partial per-move classification remains non-current.

## Classification/Arrow Coherence Series - Update 8/9

Arrow/classification consistency is native-owned. `analysis.arrow.v1` transports exact snapshot ID + FEN + published rank-1 move + renderability reason; `analysis.classification.v1` transports the classification snapshot, root FEN, played move, published rank-1 move, whether the played move matched that rank 1, and native renderability. Flutter never reconstructs rank 1 or decides a category. Main-line mixed live/classification generations may suppress the arrow, but a completed native classification remains publishable. A sideline arrow is published only from a completed after-position snapshot, and that exact widened internal result is cached under its real search settings so the next sideline ply reuses it as the BEFORE-position classification root. ABI 11 and SQLite schema 44 remain unchanged.

## Classification/Arrow Coherence Series - Update 9/9 final invariants

- The hard product invariant is now regression-covered for both SF18 and SF19: a completed legal move always has a normal native category. A later contradictory/deeper engine view may replace that category, but may not turn the completed result into `unknown/pending/unstable`.
- `native/tests/analysis_workflow_tests.cpp` covers the end-to-end follow-arrow invariant and the `analysis.arrow.v1` / `analysis.classification.v1` provenance for both engines. Existing classifier suites cover WDL/expected-score-first severity and net-material semantics.
- Flutter is fail-closed for the presentation contracts. An arrow requires `analysis.arrow.v1`, a non-empty snapshot ID, matching FEN and native `renderable=true`. A classification requires `analysis.classification.v1`, a non-empty snapshot ID and native `renderable=true`. Missing/legacy presentation provenance is not permission to display stale chess truth.
- Current derived-classification generations are SF18 v18 and SF19 v1910. Engine raw evidence/cache remains reusable across classifier generations; only current derived labels may feed UI/statistics/profile/knowledge.
- The series does not add an ABI or SQLite migration. Do not reintroduce independent Flutter classification/arrow policy, transient-PV material severity, or a second post-hoc engine truth that can punish the published rank-1 move.
## Best-Move / Reclassification Series — Update 3/6

- Maximum-depth refinement reclassifies each move as soon as both deeper adjacent position slots are available. A shallow completed category remains visible only until that deeper move becomes decidable; deeper analysis may then replace it in either direction.
- Move-level classifier currentness is checked from the move row itself, independently of the run-level final summary version. This permits progressive categories while game-wide accuracy/counters remain atomic until the final rebuild.
- `pending` is only a temporary state when neither a deeper category nor a previously published shallow category exists. Flutter does not preserve old labels or choose between depths.

## Best-Move / Reclassification Series — Update 4/6

- SF18 and SF19 are strict Analysis truth namespaces. The selected engine owns its live worker, final best move, WDL/PV evidence, classification/reclassification and reusable game analysis. No completed run may prune or substitute the other Stockfish generation's persisted analysis for the same game.
- `Database::prune_game_analyses_except(...)` is engine-scoped: a newer run supersedes only rows with the same `engine_version`. Switching SF18 -> SF19 -> SF18 therefore preserves each engine's compatible game cache instead of destructively deleting the inactive engine's truth.
- Live engine changes remain fail-safe: `Core::set_engine_id()` validates the candidate before persisting the setting, then `AnalysisService::prepare_for_engine_change()` cancels/joins main-line, refinement and variation work from the previous engine. The next request is created from the newly selected engine only.
- Position-cache and sideline reuse remain isolated by `ChessEngine::cache_identity()` / `position_cache_engine_identity(...)`; the in-memory sideline cache is cleared when its engine changes. Flutter contains no SF18/SF19 reconciliation policy.
- No ABI or SQLite schema migration is introduced by this update. No builds or tests are executed by the assistant.


## Best-Move / Reclassification Series — Update 5/6

- Persisted engine evidence and derived classification remain separate validity domains. A complete compatible SF18/SF19 run is reused even when its classifier generation is stale; native code rebuilds the current move labels from saved engine slots without scheduling Stockfish again.
- Progressive maximum-depth refinement may replace the shallow published label as soon as the deeper run has both adjacent position slots. The final full rebuild remains the atomic summary/accuracy publication boundary.
- Flutter remains a pure consumer of native truth. The analysis screen must perform the terminal adjacent-position refresh when refinement changes from running to complete, so the final native `bestmove`, PV and reclassification cannot remain hidden behind the penultimate running snapshot.
## Best-Move / Reclassification Series — Update 6/6 final contract

- Running analysis may expose the newest complete MultiPV rank-1 PV provisionally; completed analysis must expose the selected engine's final `bestmove` as the arrow/recommendation authority. Exact MultiPV lines remain snapshot evidence and are not rewritten into a synthetic ordering merely to match the final callback.
- Every completed legal move has a normal native classification. `pending`, `unknown`, and stability reasons are transient/diagnostic only. Maximum-depth refinement may replace a shallower label as soon as both adjacent deep position slots are available.
- SF18 and SF19 remain strict truth/cache namespaces across main-line, sideline, persistence, WDL/PV, best move and reclassification. Engine switching cancels old live workers but preserves the inactive engine's compatible persisted run.
- Final regression coverage includes completed-class publication, progressive reclassification, final-arrow authority, MultiPV snapshot coherence, rapid sideline supersession, cache reuse and SF18 -> SF19 -> SF18 isolation. Exchange/material semantics remain covered by the classifier-quality regressions.
- The obsolete SF19 helper that promoted a historical/final bestmove into MultiPV rank 1 was removed. There is no second synthetic result-coherence path beside the exact snapshot accumulator.
- This six-update series was closed by static audit only. No build, CTest, Flutter test, `flutter analyze`, `flutter run` or application execution was performed by the assistant. ABI 11 and SQLite schema 44 remain unchanged.


## Unified Move Classification Series — Update 1/7

- Move classification now starts from one native C++ `MoveAnalysisFacts` record shared by SF18 and SF19. Engine rank, final bestmove identity, expected score/WDL, CP/mate state and settled PV material belong to this common facts layer.
- Do not duplicate classifier facts in Flutter or in engine-specific parallel models. Engine-specific wrappers may contain only evidence that is genuinely specific to that classifier generation.
- No visible move category was added in Update 1. Later updates extend the facts layer for tactical exposure and compensation before changing Brilliant/Great/Best/Excellent policy.

## Unified Move Classification Series — Update 2/7

- Native classification facts now include immediate profitable material exposure after the played root move, including the important case where the move intentionally leaves a different own piece capturable.
- Exposure is computed in C++ from legal board moves; Flutter receives only the eventual native classification and never performs hanging-piece or sacrifice detection.
- The exposure layer is conservative: immediate recapture discounts a capture to its favorable exchange margin, while deeper material recovery, tactical compensation and mate justification are deferred to Update 3.
- The `f5g6` reference position is retained as a regression fixture for this ignored-threat pattern. No move class, icon contract, ABI or SQLite schema changes are introduced by Update 2.

## Unified Move Classification Series — Update 3/7

- `pv_material_evidence(...)` now tracks the full short-PV material path, not only peak/final balance: deepest concession, its ply, material recovered afterward, settled net loss/gain and whether the concession is fully recovered.
- `MoveAnalysisFacts` receives those recovery facts from the same played-move best-defense continuation used by classification. This lets later Brilliant policy distinguish a real sacrifice that is compensated/recovered from an ordinary hanging piece without adding a parallel analyzer.
- Mate/evaluation justification remains existing engine evidence and is not converted into a synthetic material score in this update. Update 4 owns the Brilliant policy that combines material concession/recovery with engine strength. Flutter remains presentation-only.
- No visible move category, icon contract, ABI, SQLite schema or build configuration changes are introduced by Update 3.

## Unified Move Classification Series — Update 4/7

- Brilliant classification no longer requires the moved piece itself to be the sacrificed material. Native C++ may also recognize a Best/Near-Best move that consciously leaves a different own non-pawn piece profitably capturable, using the Update-2 material-exposure facts and Update-3 PV compensation/recovery facts.
- A Brilliant candidate still requires at least minor-piece-scale exposure (>= 300cp piece value and >= 201cp estimated/net concession), a sound resulting engine outcome, more than one legal move, and a non-trivial/non-decided position. Pawn-only concessions remain Best/Critical rather than Brilliant.
- SF19 keeps its independent after-position confirmation requirement before publishing Brilliant. SF18/SF19 classifier generations are now v16/v1908 so persisted raw engine evidence is reused while stale derived labels are reclassified.
- Regression coverage includes the `f5g6` ignored-exposure pattern from `r4rk1/p4p1p/3p1npQ/1PpPpP2/4P3/2n4P/Pq2BP2/2R3RK w - - 0 22`. No visible move category or Flutter asset was added.

## Unified Move Classification Series — Update 5/7

- Great/Best/Excellent are now derived from native engine-quality clusters rather than rigid MultiPV positions. Several effectively equal engine candidates may all be Best; a lightly worse rank 3/4/5 candidate may be Excellent.
- `critical` remains the stored/native name for the visible Great class; no new visible class or asset name was introduced in this update.
- Classification-only MultiPV width is five. Flutter remains presentation-only and must not reproduce cluster thresholds or infer classes from line rank.
- Current classifier generations after this update are SF18 v17 and SF19 v1909. No ABI or SQLite schema migration is introduced.

## Unified Move Classification Series — Update 6/7

- The category model now includes native `inaccuracy` between `okay` and `mistake`. Persistence, analysis JSON, profile/game summaries, Coach verdict mapping, knowledge/profile consumers and Flutter rendering all consume that native category; no Dart-side classification policy is allowed.
- The custom icon slot is `assets/analysis_img/move_inaccuracy.png`. A temporary existing-icon copy keeps the asset valid until the project supplies final artwork.
- Forced mate is evaluated on a mate-distance axis before ordinary CP/WDL quality bands when both candidates preserve the same mate result. Missing a winning mate remains `miss`; newly allowing an opponent mate remains `blunder`.
- Shared severity thresholds are WDL/expected-score-first: Inaccuracy >= 0.08, Mistake >= 0.15, Blunder >= 0.25; CP fallback-only thresholds are 180/260/400cp. Current classifier generations are SF18 v18 and SF19 v1910.

## Unified Move Classification Series — Update 7/7 final audit

- The seven-update classifier rewrite is complete. Native C++ owns one shared move-facts layer, tactical material exposure, PV material recovery/compensation, Brilliant policy, quality clustering, mate-distance quality and negative severity.
- Brilliant is not restricted to sacrificing the moved piece. Best/Near-Best moves may qualify when they consciously accept a real >= minor-piece material concession elsewhere and engine evidence proves sufficient compensation. The `f5g6` reference FEN is retained as regression coverage for this ignored-threat form.
- Great (`critical`) is an isolated rank-1 decision; Best is an equivalence cluster rather than a single fixed rank; Excellent/Good/Okay are progressively wider quality clusters.
- Mate positions use mate distance before numeric WDL/CP precision. `inaccuracy` is a distinct durable visible class; `miss` is reserved for missed concrete opportunity. Current classifier generations are SF18 v18 and SF19 v1910.
- The visible class added by this series is `inaccuracy`. Its Flutter enum/string is `inaccuracy`; its asset contract is `assets/analysis_img/move_inaccuracy.png`. No other visible category was added.
- Final audit was static only. No build, CTest, Flutter test, `flutter analyze`, `flutter run`, or application execution was performed by the assistant.
### Opening graph integration series (2026-09-19)

- Update 1/7 repaired the native learning-stats/classifier contract after the unified move-classification migration: `PlayerLearningStats` now exposes `good`, `okay`, and `inaccuracy`, matching `Database::player_learning_stats()`.
- Removed the stale rank-1 conflict diagnostic local from `AnalysisService` instead of keeping a dead warning-only path.
- No build, test, Flutter analyze, or runtime command was executed; verification remains user-driven per project workflow.


## Opening Graph Integration Series — Update 3/7

- `opening_lines.kcl` and `opening_names.kco` use the same canonical Stockfish position-key contract and expose ordered fingerprints for diagnostics. KCL topology remains authoritative even if optional KCO coverage differs; a mismatch degrades naming/diagnostics, not graph availability.
- KCL move encoding is the existing shared KCB/KCL 16-bit UCI codec from `theory/position_key.*`, versioned by `kBookMoveEncodingVersion`; graph code must not introduce another move encoder.
- This update adds compatibility validation only. Graph navigation, KCB weighting and Training-Arena policy remain owned by Updates 4–6. No build/test command is executed by the assistant.

- KCL graph runtime: native `KclOpeningLineGraph` first replays the persisted root-to-terminal lines and derives position adjacency; `OpeningLineGraph::node_index`/`continuations` then own position-based navigation and transposition convergence. Flutter never parses or traverses KCL directly.


## Opening Graph Integration Series — Update 5/7

- Training continuation composition is now KCL topology + KCB statistics/weight + KCO destination identity. The rebuilt, broader `opening_book.kcb` is consumed through the unchanged KCB1 lookup contract; runtime contains no month-range assumption.
- KCB may weight only moves already present in KCL. Missing KCB statistics do not delete a KCL edge; that edge receives minimal fallback weight. KCO names the resulting canonical destination position.
- The legacy `openings_*.inc` path remains only as the stable scenario catalogue/id/hierarchy and setup source for existing progress compatibility. KCL1 does contain root-to-terminal move lines, but not the persisted catalogue IDs/hierarchy consumed by Practice. Flutter owns presentation only and does not parse/join KCL, KCB or KCO.
- No build, test, Flutter analyze, or runtime command was executed by the assistant.


## Opening Graph Integration Series — Update 7/7 final audit

- The seven-update opening-graph integration is complete. `opening_lines.kcl` is the native Training-Arena topology, `opening_book.kcb` remains the unchanged statistics source and may be rebuilt from broader history, and `opening_names.kco` remains the name/ECO source.
- Every KCL edge is a valid user continuation. KCB frequency only sorts/weights KCL edges for hints and opponent sampling; the removed legacy top-3 gate must not reject a graph-valid move.
- `openings_a.inc` through `openings_e.inc` remain only for stable catalogue hierarchy/IDs and deterministic setup compatibility. KCL1 stores terminal-keyed root-to-terminal lines; native runtime derives graph adjacency from those lines. The old PolyGlot fallback is not a productive Training-Arena continuation path.
- Native regression fixtures install bundled KCL/KCO/KCB assets before core initialization, matching Flutter's production asset installation contract.
- The native build-contract repair from Update 1 remains part of the series: `PlayerLearningStats` exposes `good`, `okay`, and `inaccuracy`, and the stale `classification_rank1_conflict` local is gone.
- Final audit was static only. No build, CTest, Flutter test, `flutter analyze`, `flutter run`, or application execution was performed by the assistant.

## Opening Graph Integration — post-series build hotfix

- Weighted continuation sampling is shared through neutral `core/weighted_choice.h`; Training Arena must not include or depend on `theory/polyglot.h` merely to draw a weighted KCL continuation.
- `practice_session.cpp` and PolyGlot reuse the same generic helper, so there is one weighted-sampling implementation without reintroducing the removed PolyGlot training fallback.
- Verification for this hotfix is static only; no build or test command was executed by the assistant.

### Opening Arena error visibility

- The Opening Arena must preserve concrete native `CoreGatewayException` details instead of mapping every failure to "line not in database". Flutter may present the native diagnostic text, but all KCL/KCO/KCB validation and opening-training semantics remain native C++.

## Opening arena runtime contract (2026-09-19 diagnostic fix)
- `opening_lines.kcl` is the sole authority for opening-training topology. A KCO coverage/fingerprint difference must never disable a valid KCL graph.
- `opening_names.kco` remains optional naming/ECO metadata. Missing/unmatched names degrade labels only, not trainability.
- Practice command `openingStatus` exposes graph/name metadata for UI diagnostics; Flutter must not derive opening topology itself.
- Opening UI surfaces the native graph status instead of collapsing every startup/load exception into a misleading “line not in database” diagnosis.

## Opening Arena spinner hotfix
- Removed the UI-blocking `openingStatus` preflight from the opening scenario first-render path.
- Keep graph diagnostics attached to the authoritative catalogue/session operations; do not add synchronous FFI diagnostics ahead of first paint.

## Opening KCL cumulative native build hotfix

- `practice_session.cpp` must include neutral `core/weighted_choice.h` whenever it uses `pick_weighted`; later KCL diagnostic patches must not regress this include.
- The cumulative hotfix carries `core/weighted_choice.h` plus the shared PolyGlot consumer and the newest KCL runtime diagnostics so a diagnostic patch cannot overwrite the earlier build-contract repair.
- This repair was verified statically only. No build, test, Flutter analyze, or runtime command was executed by the assistant.


## Opening KCL edge-offset parser hotfix

- KCL1 node `edge_offset` is persisted as a byte offset into the packed move table; `edge_count` is a move count. Native loading must validate 2-byte alignment and normalize the byte offset to a move index before storing runtime nodes.
- Do not interpret persisted KCL edge offsets as direct move indices. The shipped graph's final node ends exactly at the declared edge-table byte length when this contract is applied.
- This repair changes only KCL parsing/validation; graph topology, KCB weighting, KCO naming and Training-Arena policy are unchanged. Verification was static only; no build/test command was executed by the assistant.


## Opening line semantics correction series — Updates 1–3/3

- KCL1's 20-byte sorted records are terminal-position records. Each record references a complete standard-start-to-terminal move line in the packed move table. `edge_offset` is persisted as a byte offset; `edge_count` is the number of plies in that line. A persisted record range is never an outgoing-edge array.
- Native `KclOpeningLineGraph` validates/decodes the immutable file, replays every line from the standard start position, verifies the replayed terminal `stockfish_position_key(...)`, deduplicates shared prefix edges and merges move-order transpositions into one in-memory adjacency graph.
- KCO compatibility remains defined by the persisted sorted terminal-position key universe/fingerprint. KCB remains independently replaceable/broader and only weights derived KCL continuations; it never defines graph topology.
- `openings_*.inc` remains for stable catalogue IDs/hierarchy and persisted `opening_<id>` progress compatibility. Its setup position does not constrain continuation legality; the derived KCL graph does.
- Regression references: after `1.e4 e5 2.d4 exd4 3.Qxd4`, the graph must expose `...Nc6` and must never replay `e2e4/e7e5` as replies. Bishop's Opening shared prefixes and equivalent move-order transpositions must merge rather than overwrite or duplicate runtime nodes.
- Verification for this series is static only. No native build, CTest, Flutter test, `flutter analyze`, `flutter run`, or application execution was performed by the assistant.
