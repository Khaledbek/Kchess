# Native C++ AI Instructions

## Rolle

`native/` ist die fachliche Runtime-Wahrheit: Schach, Analyse, Stockfish, Persistenz, Provider, Statistik, Bots und Training.

## Kontext sparen

- Für AI-Coach-Aufgaben `native/ai/AGENTS.md` bzw. die nächstgelegene Child-`AGENTS.md` lesen.
- Ansonsten die nächstgelegene `native/src/**/AGENTS.md` lesen.
- Mit `rg` zuerst Deklaration, Definition und Aufrufer eines Symbols finden.
- Große Dateien (`core_api.cpp`, `database.cpp`, große Services) nur an relevanten Funktionen öffnen.
- Nicht automatisch alle Services oder Engine-Dateien lesen.
- `third_party/`, Prebuilt- und Build-Caches nur bei expliziten Engine-/Dependency-Aufgaben öffnen.

## Kritische Regeln

- C-ABI stabil halten; Exceptions nicht über ABI-Grenze lassen.
- JSON/DTO-Verträge und Speicherbesitz explizit behandeln.
- Migrationen, Legacy-Spalten, Settings-Aliase und alte ABI-Exports können absichtliche Kompatibilität sein.
- Stockfish 18 und 19 bleiben getrennte aktive Runtime-Optionen.
- Side-Lines nicht in autoritative Hauptanalyse persistieren.
- Keine zweite fachliche Implementierung in Dart/Python erzeugen.

## Cleanup

Vor Löschen mindestens Deklaration, Definition, C-ABI/JSON-Vertrag und alle internen Aufrufer prüfen. Geringe Nutzung allein ist kein Beweis für toten Code.

Keine automatischen Builds/Tests starten, wenn der Benutzer sie selbst ausführt.
## Update 104 - Knowledge Graph ownership

- `src/knowledge/` owns the general versioned C++ Knowledge Graph contract and its later runtime layers. It must reuse authoritative Games/Statistics/Analysis/Profile sources rather than duplicating their payloads.
- Knowledge node/edge identities are deterministic machine contracts. Persistence-facing enum names and schema versions may only change with an explicit migration/versioning decision.
- `src/knowledge/` is the only graph/retrieval runtime for profile and Coach knowledge. The retired `ai/profile/profile_graph*` stack must not be recreated.


## Update 106 - source-aware Knowledge Graph invalidation

- General Knowledge Graph provenance/dependency state is native C++ runtime logic under `src/knowledge/dependency_tracker.*`, persisted by migration 29.
- Source version changes produce targeted stale node/edge references; callers must recompute those entries from authoritative stores rather than rebuilding the whole graph or copying source payloads into graph metadata.

## Update 108 - statistics Knowledge Graph projection

- `src/knowledge/statistics_graph_projector.*` is the only general-graph projection path for existing StatisticsService facts introduced by Update 108. It references current aggregates and source versions; it does not create a second statistics engine.
- Opening statistics stay compact statistic facts until the dedicated opening/transposition graph in Update 109.

## Update 109 - opening topology ownership

- `src/knowledge/opening_graph_projector.*` owns general-graph OpeningFamily/Opening/Variation topology and reuses the existing Games DB, KCO classification and `theory/position_key.*` canonical position identity.
- Transpositions must converge through shared canonical Position nodes. Never model the opening repertoire as a tree or introduce a second position-key implementation.
- Opening theory may contribute only compact metadata/provenance such as contiguous `theory_end_ply`; theory lookup remains owned by the existing `OpeningTheoryProvider`.

## Update 110 - position/structure Knowledge Graph ownership

- `src/knowledge/position_structure_graph_projector.*` projects shared structural identities from existing persisted positions; it reuses `ai/position/PositionFeatureExtractor` and canonical `theory/position_key.*` identity.
- Position feature extraction remains the native deterministic board-feature source. Update-110 additions to that DTO (piece counts and pawn-square lists) exist to prevent duplicate FEN parsing in the graph layer.

## Update 111 - result/time/transition Knowledge Graph ownership

- `src/knowledge/result_transition_graph_projector.*` projects authoritative game outcomes/terminations, PGN clock-pressure evidence, persisted expected-score conversion/defense signals and phase transitions into the general graph.
- It must reuse `services/termination.h`, `chess/pgn.*`, the shared analysis cache read models and Update-110 structure edges. It must never run engine analysis, infer missing clock data or reimplement structure classification.
- The persisted profile move source DTO may expose additional existing expected-score columns for graph/profile consumers, but those fields remain read-only views over `move_analysis`.

## Update 113 - Knowledge Graph quality ownership

- `src/knowledge/confidence_engine.*` and `coverage_engine.*` compute native routing confidence/coverage from existing graph properties and provenance only; they must not start analysis work or rewrite learned-profile confidence.
- `src/knowledge/knowledge_quality_store.*` persists those derived node/edge metrics in migration 31. Temporal scope and freshness basis stay explicit so source-observation recency is not confused with recent player behavior.

## Update 114 - Knowledge Graph temporal/conflict ownership

- `src/knowledge/graph_store.*` snapshots changed/deleted compact graph states into migration-32 history before mutation; unchanged upserts must not create duplicate revisions.
- `src/knowledge/conflict_resolver.*` is the native owner of explicit historical/current contradiction resolution, `CONTRADICTED_BY`, and improvement/decline classification. Flutter/Python must not duplicate this runtime logic.
- Historical graph state remains metadata-only and may reference source locators; raw Games/Statistics/Analysis/Profile payloads stay in their authoritative stores.

## Update 116 - Knowledge Graph text semantic retrieval

- `src/knowledge/text_semantic_retrieval.*` reuses the existing `ai::EmbeddingModel` contract to write/search text-semantic chunk vectors through `VectorIndex`; it must not introduce another model API or persist query embeddings.
- Optional reranking is candidate-bounded and may only reorder already eligible semantic chunks. Hybrid multi-channel ranking remains a later Knowledge Graph responsibility.
- Python/Hugging Face tooling under `tools/ai/embeddings/` is evaluation/export-only; the shipped native runtime must not depend on Python.

## Update 117 - Knowledge Graph position similarity ownership

- `src/knowledge/position_similarity.*` owns the second, chess-position vector space and reuses the existing deterministic `ai/position/PositionFeatureExtractor` plus canonical `theory/position_key.*` identity.
- Position vectors are node-owned retrieval artifacts only; they must not trigger Stockfish, duplicate FEN parsing, alter graph truth or mix with text-semantic embeddings.
- Player-specific filtering remains a graph/retrieval responsibility because canonical board vectors are globally reusable representations of Position nodes.

## Update 118 - Knowledge Graph query routing ownership

- `src/knowledge/query_router.*` consumes the existing native `ai::QueryPlan` and owns only Knowledge-Graph scope/channel refinement; it must not become another Coach family/intent classifier.
- Current-board availability never implies relevance. Position retrieval requires the authoritative plan or an explicit current-position/move reference, preventing incidental UI board state from overriding historical/personal retrieval.
- `src/knowledge/entity_extractor.*` provides normalized retrieval hints only. Authoritative entity resolution and multi-channel candidate retrieval remain in the graph/retrieval layers, not Flutter.
## Update 119 - Knowledge Graph hybrid retrieval ownership

- `src/knowledge/hybrid_retrieval.*` owns bounded multi-channel candidate collection after Update-118 routing. It combines existing exact/statistical graph facts, graph traversal, lexical chunk lookup, text-semantic retrieval and chess-position similarity without recomputing authoritative data.
- Personal graph traversal is profile-safe: explicitly foreign `profile_id` nodes are neither returned nor traversed through, and player-scoped chunks remain filtered by `player_id`. Shared semantic graph identities may still be reused.
- Retrieval preserves per-channel evidence/signals and hard budgets; final hybrid ranking, query planning and reranking remain Update 120 responsibilities. Flutter and Python must not duplicate this runtime logic.


## Update 120 - Knowledge Graph ranking/query-planning ownership

- `src/knowledge/retrieval_ranking.*` owns final native ranking over Update-119 candidates and the Knowledge-Graph-specific execution plan. It consumes the existing Update-118 route and must not duplicate/reclassify the Coach `ai::QueryPlan`.
- Hybrid rank is explainable and combines query/channel relevance with Update-113 confidence/coverage/freshness/source quality and graph distance. Missing quality metadata is neutral, not silently interpreted as low quality.
- Final text reranking reuses the existing bounded `TextReranker` contract and cannot broaden player scope, add candidate IDs or create graph truth. Flutter/Python must not implement a competing runtime ranking policy.

## Update 121 - Knowledge-gap active learning

The general Knowledge Graph may identify player-scoped knowledge gaps from persisted quality, provenance and conflict metadata. Runtime follow-up remains native and must feed only bounded priority hints into the existing nine-stage `PlayerProfileService` funnel; it must not introduce a second queue, second engine-analysis cache or Flutter-owned learning policy.

## Update 122 - Coach evidence packet ownership

- `src/knowledge/evidence_packet_builder.*` owns the provider-neutral, token-bounded Knowledge Graph evidence packet (`FACTS`, `OBSERVATIONS`, `EVIDENCE`, `UNCERTAINTIES`, optional current/recent context), source trace and answerability gate.
- The builder only packages Update-120 ranked results and Update-106 provenance. It must not become another retrieval/ranking engine, analysis scheduler, statistics implementation or provider prompt layer. Final Coach/provider integration remains Update 123.

## Update 123 — Knowledge Runtime Ownership

`src/knowledge/knowledge_runtime.*` ist die gemeinsame native Integrationsgrenze für Knowledge Graph, Coach Retrieval, Active Learning und Inspector. Core konstruiert genau eine Runtime nach `StatisticsService` und vor `PlayerProfileService`/`CoachService`. Flutter bleibt Darstellung; Domainlogik und Query-Traces bleiben nativ.


## Cleanup Update 124 - retired profile graph removed

- The profile-only graph sources and service-side profile context resolver are deleted.
- Migration 35 removes their obsolete SQLite tables plus the old sparse profile-probe table.
- Historical profile JSON may still read `processedGames` as a compatibility fallback, but active native/Flutter contracts use `indexedGames` and no probe-position telemetry.


## Update 146/148 - Windows build-cache ownership

- `native/cmake/kchess_windows_build_cache.cmake` and `ensure_kchess_windows_build_cache.cmake` own the persistent Windows/MSVC `kchess_core` build tree. They do not implement source invalidation themselves; every Flutter build asks the child CMake/MSBuild tree for `kchess_core`, and CMake/MSBuild decides which translation units are stale.
- `native/cmake/windows_build_cache_root.cmake` is the shared locator for large Windows build intermediates. KChess-core and Stockfish intermediate build trees live outside the repository under `%LOCALAPPDATA%\KChess\build-cache\<checkout-id>`. The checkout id is derived from the normalized native source path so different project copies do not share one CMake cache.
- `native/prebuilt/windows/` contains the stable SF18/SF19 `.lib` artifacts and remains inside the project as a reusable build input. Normal KChess-core rebuilds and `flutter clean` must not delete these libraries.
- Do not copy native runtime/domain logic into build scripts. Build scripts may only locate, configure, build or reset generated build state.

## Update 151 - process-runtime diagnostics

- Core may expose a passive `processRuntime` snapshot through the existing Knowledge Inspector. On Windows it reports cumulative process CPU time plus normalized CPU use over the interval since Core creation and since the previous inspector sample, logical processor count and process handle count.
- Process diagnostics sample only on inspector requests. Do not add a monitoring thread, timer, scheduler feedback or resource-policy decisions from these values.
