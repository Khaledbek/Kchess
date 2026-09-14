# Knowledge Graph Agent

## Scope

`native/src/knowledge/` owns the product-runtime Knowledge Graph contracts and, in later updates, graph storage, chunks, dependency tracking, retrieval, ranking, confidence/coverage and evidence-packet assembly.

## Architectural boundary

- The Knowledge Graph is a routing/relationship layer, not a replacement database for PGNs, statistics, engine analysis or the learned profile.
- Authoritative payloads remain in their existing stores/services. Graph nodes and edges keep stable identities, relationships, compact properties and references only.
- Flutter may display graph/retrieval state but must not implement graph, ranking or retrieval logic.
- Python may benchmark/evaluate models and tooling, but runtime graph ownership remains C++.
- The general Knowledge Graph is the only graph runtime for profile/Coach routing. Do not create a profile-specific parallel graph or competing profile truth.

## Update 104 contract

- `knowledge_graph_contract.*` is the versioned base contract for the general property graph.
- Node/edge IDs must be deterministic from canonical machine identities. Never derive identity from localized labels or mutable prose.
- `KnowledgeNodeKind` and `KnowledgeEdgeKind` names are persistence-facing contract values. Renaming/removing them requires a schema/version migration.
- Node/edge properties are compact metadata only. Large payloads, PGNs, complete engine lines and duplicated statistics do not belong in generic properties.
- Facts, observations and hypotheses remain explicitly distinct through `KnowledgeAssertionKind`.
- Storage-specific provenance/dependency behavior starts in Update 106; do not prematurely make this base contract depend on SQLite or service layers.

## Workflow

Read this file plus `native/AGENTS.md` before changing this folder. Extend the existing contract rather than creating another graph model. Update this file whenever ownership, schema or contract invariants change.

## Update 105 storage

- `graph_store.*` is the SQLite runtime store for the general Knowledge Graph and operates on the shared `kchess.sqlite3` after `Database::open_and_migrate()` has established the schema.
- `knowledge_nodes` / `knowledge_edges` persist stable graph identities and relation kinds. Compact generic properties use typed child tables so bool/int/real/text values round-trip without turning SQLite into a duplicate payload store.
- Endpoint foreign keys are strict. Removing a node cascades its incident edges and their compact properties; upserting an edge requires both endpoint nodes to exist.
- `GraphStore::adjacent` and `GraphStore::traverse` are the bounded basic traversal primitives. Higher-level retrieval budgets/ranking belong to later retrieval updates, not this storage layer.
- Persistence-facing enum names round-trip through the parsing helpers in `knowledge_graph_contract.*`; changing those names is a migration-sensitive contract change.

## Update 106 provenance and dependency tracking

- `dependency_tracker.*` owns payload-free source provenance and version expectations for general Knowledge Graph nodes/edges.
- `KnowledgeSourceRef` identifies an authoritative source by stable `source_type` + `source_id` and records the source version used to derive an entry. Source payloads must never be copied into these records.
- `knowledge_sources` stores the latest observed source version plus monotonic `first_seen_ms` / `last_seen_ms`. `knowledge_provenance` records which sources support a graph entry; `knowledge_dependencies` records which source version that entry was built against.
- Source changes must go through `DependencyTracker::invalidate_source_change(...)`. Only dependencies whose expected version differs from the new authoritative version are marked stale; later chunk/graph refresh work consumes this targeted invalidation set.
- Rebuilding an entry replaces its dependency expectations with the versions actually used. Do not clear stale state merely to silence work; `clear_invalidation` only clears dependencies already aligned with the current source version.
- Node/edge deletion automatically removes associated provenance/dependency rows through schema triggers. Generic graph properties remain compact metadata and are not a substitute for provenance tables.

## Update 107 chunk registry

- `chunk_registry.*` owns semantic retrieval chunks plus compact retrieval metadata. Chunk content is concise derived knowledge for retrieval; raw PGNs, complete engine lines and duplicated statistics remain forbidden.
- Chunk IDs are deterministic from player scope, type, topic and granularity. Supported granularities are `summary`, `topic`, `entity` and `evidence`; retrieval may later combine several granularities instead of forcing one oversized profile blob.
- `knowledge_chunk_sources` stores only source locators/versions required to explain a chunk. `KnowledgeEntryKind::kChunk` extends the Update-106 provenance/dependency contract so chunk refresh can participate in the same targeted invalidation pipeline.
- `knowledge_chunk_graph_links` is the normalized graph-to-chunk routing layer. It may target existing graph nodes/edges only and uses `describes`, `evidence_for` or `summarizes`; it must not become a second graph implementation.
- Confidence, coverage, freshness, importance and source quality in chunk metadata are bounded `[0,1]` retrieval metadata. Their actual computation remains owned by later confidence/coverage/ranking updates.
- `embedding_id` is only a nullable future vector reference. Update 107 does not create embeddings or a vector index.

## Update 108 statistics graph

- `statistics_graph_projector.*` projects existing native statistics into compact graph routing facts. It must consume `StatisticsService`/persisted learning aggregates and must not independently recompute game statistics or trigger engine analysis.
- Statistics sources are versioned per active-profile facet (`overview`, `openings`, `terminations`, `learning_aggregates`) using a deterministic fingerprint of the authoritative aggregate payload. A changed facet invalidates only entries that depended on that facet.
- The projection creates `Statistic`, `Color`, `TimeControl`, `TerminationType` and lifetime `RatingPeriod` routing nodes plus `HAS_STATISTICS`/`DEPENDS_ON` edges. Raw games, PGNs and analysis lines remain outside the graph.
- Update 108 intentionally keeps opening information as statistic metadata. Structural `OpeningFamily`/`Opening`/`Variation`, move-order and transposition modeling belongs to Update 109; do not duplicate that graph here.
- Average rating/accuracy may be projected only when the existing persisted learning aggregates provide them. Missing values stay missing; the projector must not synthesize them.

## Update 109 opening, variation and transposition graph

- `opening_graph_projector.*` projects the existing persisted KCO opening classification plus only the opening-prefix moves needed for topology. The Games DB remains authoritative; the projector never reclassifies openings, recomputes statistics or copies PGNs/engine analysis.
- `OpeningFamily` is the stable base family before the first `:`/`,` in the persisted name. `Opening` is the persisted ECO + full named opening. `Variation` represents a distinct observed UCI move order for that named opening, so several move orders may belong to the same `Opening` without forcing a tree.
- `Position` identity is the canonical Stockfish-18 position key with FEN clocks excluded through `theory/position_key.*`. Opening paths use `PRECEDES`; different paths therefore converge on one Position node when they transpose.
- Distinct Variation nodes that reach the same canonical opening endpoint are connected with deterministic `TRANSPOSITION_OF` star links rather than an O(n^2) clique. `REACHES` keeps the variation/game -> canonical endpoint relation explicit.
- Source versions are derived from each authoritative game's persisted opening metadata and opening-prefix moves. A profile-wide opening manifest detects additions/removals. Shared semantic entries record the manifest plus their contributing game source locators through Update-106 provenance/dependency tracking.
- Opening projection edges use an `opening_graph` discriminator/property so obsolete edges invalidated by a changed manifest can be removed without touching unrelated graph layers. Shared Player/Game identities are only ensured as routing endpoints; their payload remains in existing stores.
- When an existing `OpeningTheoryProvider` is supplied, the Variation stores only the compact contiguous `theory_end_ply`; absence of a provider leaves that metadata unknown rather than inventing it.

## Update 110 position and structure graph

- `position_structure_graph_projector.*` is the only Update-110 projection path from persisted game positions into `PawnStructure`, `MiddlegameStructure`, `EndgameType`, `MaterialConfiguration`, `PieceConfiguration`, `KingSafetyPattern` and center-state `PositionFamily` nodes.
- Canonical `Position` identity continues to use `theory/position_key.*`; do not introduce a second position identity or a position tree. Structure nodes are shared semantic identities and are connected from positions through `HAS_STRUCTURE`.
- Board parsing/feature extraction remains owned by `ai/position/PositionFeatureExtractor`. The Knowledge Graph may classify those deterministic DTO values into stable structure signatures but must not add another FEN parser or run engine search.
- `games_db_positions` and the profile-wide `position_structure_manifest` are provenance/version locators only. PGNs/FEN payloads remain in the Games DB; graph properties contain compact signatures/metrics, never full game payloads.
- Center state is represented through `PositionFamily` with `family_type=center_state` because the persistence-facing base contract intentionally has no separate CenterState node kind. Changing that choice requires an explicit contract/schema version decision.

## Update 111 result, time and transition graph

- `result_transition_graph_projector.*` is the only Update-111 projection path for result causes, clock-pressure observations, phase transitions and persisted expected-score conversion/defense evidence.
- Game result and termination remain authoritative in the Games DB/PGN metadata. Player-perspective outcome comes from the existing `player_profile_game_sources` read path; termination classification reuses `services/termination.h`. The graph stores only compact routing facts and source references.
- Time trouble is derived only from existing PGN clock comments via `extract_mainline_clock_millis`; absent clocks mean absent time-trouble evidence. It must never synthesize clock history from time-control metadata.
- `PlayerProfileMoveSourceRow` exposes the already-persisted `expected_score_before`, `expected_score_best` and `expected_score_played` fields in addition to loss. This is a read-only extension of the shared analysis cache and must not trigger Stockfish or create a second move-classification path.
- `MISSED_WIN` and `SAVED_FROM` are emitted only when persisted expected-score evidence crosses the explicit Update-111 thresholds. Missing expected-score fields remain missing rather than being inferred from labels alone.
- Opening -> middlegame and middlegame -> endgame `TRANSITIONS_TO` relations reuse structural nodes already projected by Update 110 through Position `HAS_STRUCTURE` traversal. Do not duplicate PositionFeatureExtractor classification in this projector.
- `ConversionPattern` and `DefensePattern` are compact per-game routing summaries over existing expected-score evidence. Later profile/evidence aggregation may generalize them; Update 111 does not create profile-level strengths or weaknesses.

## Update 112 profile knowledge and evidence graph

- `profile_knowledge_graph_projector.*` is the general Knowledge-Graph projection boundary for the already learned `ChessProfile`, its payload-free `ai_profile_evidence_registry` locators, and persisted shared `move_analysis` observations. It must not learn a second profile, read engine output through a new cache, or start Stockfish.
- `Strength`, `Weakness`, `Behavior`, `Habit`, `Trend` and `Hypothesis` nodes are projections of explicit existing profile fields/patterns. Hypotheses remain `KnowledgeAssertionKind::kHypothesis`; derived strengths/weaknesses/habits/trends remain observations rather than facts.
- `Evidence` nodes contain source locators/tiers/versions only. Pattern example positions may create `SUPPORTED_BY` routes to the existing authoritative-analysis locator; PGNs, FEN payloads, PVs and full engine analysis remain in their authoritative stores.
- Recovery is a profile-level `RecoveryPattern` observation over consecutive persisted player-move analysis: a major error followed by a saved best/excellent/critical/brilliant move with no more than 0.05 persisted expected-score loss counts as a stabilization. Error cascades use `TransitionPattern` with `transition=error_cascade` and require at least two consecutive analyzed player moves classified as miss/mistake/blunder. These are routing observations only and never alter move classification.
- Generic misses/blunders must not be renamed into chess motifs. `TacticalMotif` / `StrategicMotif` are materialized only when an upstream profile identifier explicitly carries the `tactical_motif:`/`.` or `strategic_motif:`/`.` namespace. Missing motif evidence stays missing.
- The profile projection uses source-specific dependencies for targeted invalidation. Its manifest fingerprints source membership, not every source version, so a changed analysis/model version invalidates only entries that actually depended on that source; manifest invalidation is reserved for source/concept membership changes and stale-entry cleanup.
- Shared `Player`, `Game` and `TimeControl` identities are routing endpoints only and are not projection-owned. Stale cleanup may remove only nodes/edges that were registered through this projector's dependency sources.
- The learned `ChessProfile` remains authoritative profile truth; this graph only projects/routs that knowledge and source evidence.

## Update 113 confidence, coverage and freshness

- `confidence_engine.*`, `coverage_engine.*`, `knowledge_quality.*`, `knowledge_quality_store.*` and `knowledge_quality_engine.*` own derived routing-quality metadata for the general Knowledge Graph.
- Knowledge quality is separate from upstream domain confidence. In particular, the learned `ChessProfile` remains authoritative for its own pattern/hypothesis confidence; the graph may consume that value as one input but must never write a replacement profile confidence back to the profile store.
- Confidence and Coverage are distinct. Confidence measures support for one assertion. Coverage in Update 113 is **entry-level support coverage**, not global topic/profile coverage; Update 121 may aggregate entry metrics to identify Knowledge Gaps.
- Freshness is stored together with an explicit `freshness_basis`. `evidence_timestamp`/`explicit_recent_scope` may represent behavioral recency; `source_observation` only means the source/version was recently observed and must not be misread as a recent chess event.
- `KnowledgeTemporalScope` keeps `recent`, `lifetime`, `mixed` and `unknown` distinct. Do not infer historical-vs-current conflict resolution here; Update 114 owns contradictions and temporal conflict semantics.
- Manifest sources are dependency/control metadata and must not inflate evidence count, source diversity or confidence. Independent source locators and source families contribute separately to evidence diversity.
- `knowledge_quality` (migration 31) persists bounded derived metrics for node/edge routing. Deleting a node/edge/chunk removes its quality row through cleanup triggers. The table contains no PGN, engine line, statistics payload or learned-profile payload.
- Chunk quality columns from Update 107 remain the chunk retrieval contract. Update 113 does not create a competing chunk-quality source; chunk population is wired when chunk/retrieval generation becomes active.

## Update 114 conflicts and temporal development

- `graph_store.*` now preserves the previous compact node/edge state in `knowledge_versions` before a changed upsert or deletion. History contains graph metadata/properties plus payload-free source locators only; it must never become a duplicate PGN/statistics/analysis/profile store.
- Node deletion snapshots its incident live edges before SQLite cascade removal so former relationships remain explainable. Unchanged upserts do not create redundant history revisions.
- `conflict_resolver.*` owns historical-vs-current conflict semantics. It compares current profile assertions with immutable graph versions, records explicit `knowledge_conflicts`, and may materialize a compact historical assertion node only when that past assertion actually conflicts with the current one.
- `CONTRADICTED_BY` is a first-class explainable graph relation. Conflict metadata must keep the historical version ID, current entry ID, confidence on both sides, resolution state and temporal-change classification.
- A former `Weakness` becoming a current `Strength` for the same `profile_id` + `pattern_id` is an `improved` temporal change; the inverse is `declined`. Same-kind revisions are history, not contradictions.
- Conflict resolution must not rewrite or delete the learned `ChessProfile`. `current_supersedes_historical` means the current graph assertion is preferred for retrieval while the historical assertion remains queryable; `balanced`, `unresolved` and `insufficient_evidence` remain explicit states.
- Profile pattern nodes must carry stable `profile_id` + `pattern_id` routing metadata so temporal comparisons never mix different players. Existing upstream `recent`/`trend` and Update-113 quality may influence resolution, but missing evidence must not be invented.

## Update 115 embedding and vector interface

- `vector_index.*` owns the runtime vector contract. Callers depend on `VectorIndex`, never on a specific ANN library or Python implementation.
- `KnowledgeVectorSpace` keeps text-semantic and chess-position vectors explicitly separate. Update 115 only provides the storage/search contract; Update 116 selects text models/reranking and Update 117 defines position features/embeddings.
- `SqliteVectorIndex` is the persistent exact-search baseline. It stores deterministic embedding metadata in `knowledge_embeddings_metadata` and encoded float32 payloads in `knowledge_embedding_vectors`; an ANN implementation may replace search later without changing IDs/metadata semantics.
- Every embedding is owned by an existing `Chunk` or `Node`, carries a player scope (empty only for intentionally global reusable knowledge), exact model ID/version, dimensions and source version. Player-scoped search may include the same player plus explicitly global vectors but must never cross into another player's scope.
- Chunk embedding linkage uses the existing `knowledge_chunks.embedding_id`; vector upsert updates that active link and embedding deletion clears it. Historical/model-alternative embeddings may coexist because search is model/version-specific.
- Exact search suppresses stale chunk vectors unless the chunk still points to that embedding and its `source_version` matches. Chunk regeneration can therefore invalidate semantic vectors without deleting graph truth; node-owned position-vector freshness remains the responsibility of the position embedding producer in Update 117.
- Embedding vectors are retrieval artifacts, never truth. Facts/statistics/profile/analysis stay authoritative in their existing stores, and deleting/re-embedding a vector must not delete graph knowledge.
- Python evaluation uses `tools/ai/embeddings/` only as an offline interchange/benchmark path. Shipped KChess runtime must not require Python.

## Update 116 text embeddings and semantic reranking

- `text_semantic_retrieval.*` owns Knowledge-Graph text-vector ingestion and semantic chunk search. It reuses the existing provider-neutral `ai::EmbeddingModel`; do not create a second embedding-model contract under `src/knowledge/`.
- `ChunkTextEmbeddingWriter` embeds only current concise `KnowledgeChunk::content` and persists through `VectorIndex`. The active vector inherits the chunk `source_version`, player scope, exact model ID/version and dimensions; changing the chunk makes the previous vector stale through the Update-115 contract.
- Query embeddings are ephemeral and are never persisted as Knowledge Graph truth.
- `TextReranker` is an optional candidate-scoring interface. A reranker may only reorder/score candidates already returned by text-vector retrieval; unknown IDs, non-finite scores and missing scores are ignored/fall back to vector order. It may never broaden player scope or invent graph/chunk evidence.
- Semantic chunk search is deliberately narrow in Update 116: text vector candidates are capped at 100 and returned semantic hits at 40. Hybrid exact/statistics/graph/lexical retrieval and the final cross-channel ranking/query planner remain owned by Updates 119-120.
- No concrete model vendor, Python runtime or model binary is required by this layer. Candidate evaluation/export lives under `tools/ai/embeddings/`; a selected ONNX encoder still needs a native adapter before product use.

## Update 117 chess-position similarity

- `position_similarity.*` owns the deterministic chess-position feature-vector space and position-similarity search. It reuses `ai::PositionFeatureExtractor` and `theory/position_key.*`; it must not parse FEN independently, run engine search or duplicate Update-110 structure classification.
- Chess-position vectors use `KnowledgeVectorSpace::kChessPosition`, node ownership, model ID `kchess.position_features` and an explicit encoder version. They are structurally separate from Update-116 text-semantic chunk embeddings.
- The feature vector represents board structure only: pawn geometry/passed pawns, material/piece inventory, file topology, development/activity/space, king placement/safety, center/phase and existing weakness-oriented board facts. These are deterministic retrieval features, never evaluations or profile truth.
- Canonical Position vectors are globally reusable because the board state is player-independent. Player/history eligibility must be intersected later through graph/hybrid retrieval; the vector layer must never claim that every similar global Position belongs to the active player.
- Node-vector freshness is checked against the Position node's canonical `position_key` plus encoder version. Changing the feature schema requires a new model version so old vectors can coexist without being compared to the new space.
- Similarity is retrieval evidence only. It may suggest related Position nodes but must not create `SIMILAR_TO` truth edges merely from a cosine score; persistent semantic relationships require an explicit later policy/evidence decision.

## Update 118 query routing and entity extraction

- `query_router.*` is the Knowledge-Graph retrieval router. It consumes the already classified native `ai::QueryPlan`; it must not create a second Coach intent/family classifier or allow Flutter to infer retrieval scope.
- Current-board availability and current-board relevance are separate. An open board is never enough to force position retrieval. The board becomes `required` only when the authoritative plan needs position context or the user explicitly references the current position/move; historical/personal questions therefore cannot be dominated by incidental UI board state.
- Mixed position+personal questions may legitimately require both current-board and historical-profile scope because `QueryPlan::needs_position` and `needs_profile` remain independent.
- `entity_extractor.*` emits normalized deterministic query hints (ECO, opening-name hints, time control, phase, color, result/termination, temporal scope, statistic metric, current-position reference and proof/evidence request). These hints are not graph truth and must still resolve against authoritative graph/chunk data during retrieval.
- Profile-scope entities already classified in `ProfileQueryScope` have precedence and are forwarded directly. Lexical aliases only improve query normalization; unknown named openings/concepts must remain discoverable by Update-119 lexical/vector retrieval rather than requiring an exhaustive hard-coded catalog.
- `KnowledgeRetrievalChannels` expresses which Update-119 channels are eligible: exact/statistics, graph, lexical, vector and chess-position similarity. It is a routing contract only; candidate collection/ranking budgets are owned by Updates 119-120.
- General-chess requests must not trigger player-history graph traversal solely because player data exists. The existing concept/theory path remains authoritative until Update 123 integrates Knowledge Retrieval with the Coach.
## Update 119 hybrid retrieval

- `hybrid_retrieval.*` is the bounded candidate-collection layer across exact/statistical facts, graph traversal, lexical chunk search, text-vector search and optional chess-position similarity. It consumes the Update-118 `KnowledgeQueryRoute`; it must not create a second intent classifier or perform final cross-channel ranking.
- Default retrieval budgets are explicit and local: at most 8 graph seeds, depth 2 (depth 3 only for complex relationship/causal/evidence questions), 100 expanded graph nodes and 40 merged chunk candidates; 20 is reserved as the default rerank ceiling for Update 120. Per-channel chunk caps prevent one retrieval mode from unboundedly consuming the shared candidate budget.
- `GraphStore::find_nodes(...)` is the deterministic graph-entity/statistic resolver for compact text properties. `ChunkRegistry::lexical_search(...)` and `linked_chunks(...)` are retrieval views over existing chunks/graph links; they do not create a second text store or copy source payloads.
- Exact/statistical retrieval may order deterministic statistic candidates by the requested stored metric, but final hybrid relevance/ranking belongs to Update 120. Missing statistics remain missing; retrieval must never recompute StatisticsService facts.
- Player-scoped traversal must not cross through graph nodes explicitly owned by a different `profile_id`. Shared semantic nodes (openings, canonical positions, structures) remain reusable, while returned chunks are always constrained to the requested player scope.
- Canonical position vectors are global, but personal position-similarity retrieval must intersect matches with positions actually reached by a game owned by the requested player. Similarity remains a retrieval signal only and never materializes graph truth.
- Hybrid candidates retain channel-specific signals (`exact_statistics`, graph distance, lexical score, vector score, position score) for Update 120. Update 119 must not collapse these into an opaque final score.


## Update 120 hybrid ranking and Knowledge query planning

- `retrieval_ranking.*` owns final cross-channel ranking after `HybridRetrievalEngine` candidate collection. Ranking is retrieval policy only and must never become a source of chess/profile truth.
- `KnowledgeQueryPlanner` refines the already-authoritative Update-118 `KnowledgeQueryRoute` into bounded retrieval/ranking policy. It may tighten budgets and weights for exact, causal, trend, evidence, similarity or current-position questions, but it must never reclassify Coach intent/query family or re-enable a disabled retrieval channel.
- Ranking remains explainable through separate query-relevance, exact/statistical, graph-distance, lexical, text-vector, position-similarity, confidence, coverage, freshness, source-quality and importance components. Missing quality metadata is omitted from the weighted mean rather than treated as negative evidence.
- Node quality comes only from Update-113 `KnowledgeQualityStore`; chunk quality comes from the existing chunk metadata contract. Confidence, coverage and freshness remain distinct signals and ranking must not write them back to authoritative sources.
- Optional final text reranking reuses Update-116 `TextReranker`, is capped by the Update-119 20-candidate ceiling and may only reorder already collected chunks. Arbitrary finite reranker score scales are normalized within that bounded candidate set; unknown IDs and invalid scores are ignored.
- Default final chunk output remains token-oriented (normally at most 8 before Update-122 token-budget assembly). Evidence-packet construction, answerability and final Coach integration remain Updates 122-123.

## Update 121 knowledge gaps and active learning

- `knowledge_gap_engine.*` turns already-computed quality/conflict/provenance metadata into bounded `KnowledgeGap` observations. It does not inspect PGNs, recompute statistics, start Stockfish or create a second profile scheduler.
- A gap remains explainable: it stores the target graph entry, separate low-coverage / low-confidence / stale / conflicting reasons, priority and compact quality values. The player owns the gap through `CONTAINS`; the gap points to its target through `DEPENDS_ON`.
- Only player-scoped actionable knowledge is eligible. Gap nodes themselves, raw Game/Position/Evidence routing nodes and global reusable semantic nodes are not recursively turned into gaps merely because quality metadata exists.
- Source game IDs are recovered only from Update-106 provenance locators (`game:<id>`). They are hints for evidence acquisition, not copied game payloads.
- `KnowledgeGapActiveLearningPlan` is deliberately scheduler-free. It may nominate a bounded set of games/topics for the existing nine-stage profile funnel, but `PlayerProfileService` remains the sole owner of persistent queue state, retries and AnalysisService engine hand-off.
- Active learning must preserve the Stage-3 historical sampling boundary. A Knowledge Gap can steer the bounded discretionary/priority portion of the representative sample; it cannot bypass the 500-game historical bootstrap ceiling or make Flutter start analysis.

## Update 122 Coach evidence packets

- `evidence_packet_builder.*` is the sole Knowledge-Graph-to-Coach packet assembly layer. It consumes the already-routed, retrieved and Update-120-ranked material; it must not rerun retrieval, recompute statistics, start engine analysis or invent missing knowledge.
- The packet keeps `FACTS`, `OBSERVATIONS`, `EVIDENCE` and `UNCERTAINTIES` structurally separate. Node `KnowledgeAssertionKind` is authoritative for fact/observation/hypothesis classification. Unlinked or ambiguous semantic chunks stay evidence rather than being promoted to facts; hypothesis-linked material is surfaced as uncertainty.
- Provider context is bounded by intent-specific token budgets: deterministic statistics target about 600 tokens, ordinary personal/position questions about 1000-1800, relationship/evidence questions about 1800-2200, causal analysis up to about 3000, and the hard packet ceiling is 4000. Budget exhaustion must remain explicit rather than silently implying complete coverage.
- `CURRENT POSITION` is copied into the packet only when Update-118 routing marks the board relevant. Merely having a board open never injects it into historical/personal questions. `RECENT CONTEXT` is included only for recent/trend-routed questions.
- `KnowledgeAnswerability` is a gate over retrieved support, not a chess truth score. Exact-statistic questions require exact support; causal/relationship questions require graph support; evidence questions require source trace; required current-position context must actually be present. A failed gate tells Update 123 not to present unsupported Knowledge-Graph claims as answered.
- Provenance remains payload-free and explainable. Packet source traces carry stable `KnowledgeSourceRef` locators, chunk/graph entry IDs and retrieval-channel signals; graph traces carry traversal edge/node IDs and depths. Raw PGNs, complete engine lines and duplicated statistics must not be copied into trace metadata.
- Update 122 does not yet replace the existing `ai::EvidenceRetriever`/provider path. Update 123 owns Coach integration, packet serialization/prompt hand-off, persisted query traces and UI diagnostics.

## Update 123 — integrierte Runtime, Coach und Inspector

`KnowledgeRuntime` ist ab Update 123 der einzige aktive Owner der allgemeinen Knowledge-Graph-Runtime. Er öffnet und teilt `GraphStore`, `DependencyTracker`, `ChunkRegistry`, `KnowledgeQualityStore`, `SqliteVectorIndex`, `PositionSimilarityIndex`, `ConflictResolver`, `KnowledgeGapEngine` und `QueryTraceStore` zwischen Profilpflege, Coach und Diagnostik. Keine Service-Schicht darf dafür eigene parallele Stores öffnen.

Nach einem erfolgreichen `ChessProfile`-Refresh projiziert `PlayerProfileService` über `KnowledgeRuntime::refresh_active_profile(...)` bestehende Statistik-, Opening-, Positions-, Resultat-/Transition- und Profilquellen. Semantische `graph_node`-Chunks werden inkrementell aus Graphknoten materialisiert; Roh-PGNs und vollständige Enginepayloads bleiben in den bestehenden Stores. Online-Accounts verwenden den gemeinsamen `player_profile_owner_id` als Personalisierungs-Scope.

Der Coach bezieht persönliche Evidenz ausschließlich über `KnowledgeRuntime::coach_evidence(...)`. Die interne Retrievalkette ist Router -> Planner -> Hybrid Retrieval -> Ranking -> Evidence Packet. Der Provider-Vertrag bleibt absichtlich `profile.context.v3`, damit Provider-Optimierung und Claim-Validierung nicht parallel neu implementiert werden. Die früheren `ai/profile/profile_graph*`- und `coach_profile_context_resolver`-Pfade wurden in Cleanup 124 vollständig gelöscht.

`QueryTraceStore` persistiert nur begrenzte Routing-/Retrievaldiagnostik (IDs, Scores, Quellenpfade, Budgets/Counts). Providerprompts, vollständige PGNs und Enginepayloads dürfen dort nicht landen. `KnowledgeRuntime::inspector_json(...)` ist der read-only Diagnosevertrag für Graph Inspector und Live Query Traces.


## Cleanup Update 124 - single graph runtime

The legacy profile-only graph/resolver source files and SQLite tables are removed. `KnowledgeRuntime`, `GraphStore`, the evidence registry and authoritative profile/analysis sources are the only supported path. Do not add compatibility writes back to the retired tables.


## Fix Update 126 - non-blocking Knowledge Inspector

The Graph Inspector is diagnostic and must never wait behind a long KnowledgeRuntime refresh. `KnowledgeRuntime::inspector_json(...)` uses a non-blocking runtime lock and returns `status: busy`/`retryable: true` immediately when refresh/Coach work owns the runtime. Inspector candidate expansion, relations, chunks and recent traces stay tightly bounded. Flutter remains read-only and must not move graph/query logic into Dart.

## Update 132 - live KnowledgeRuntime diagnostics

- `KnowledgeRuntime` publishes a small lock-independent `runtimeActivity` snapshot so `knowledge.inspector.v1` remains informative even when the main runtime mutex is busy.
- Long graph refreshes expose the current native phase (`statistics`, `openings`, `position_structures`, `result_transitions`, `profile_knowledge`, `quality`, `chunks`, `chunk_cleanup`, `conflicts`, `knowledge_gaps`) plus bounded completed/total counters where meaningful.
- Active-learning conflict/gap scans use the same activity channel. This is diagnostics only; it must not become a scheduler, cancellation path or second source of profile state.
- The inspector still uses `try_lock`; diagnostic visibility must never reintroduce a wait behind graph refresh work.

## Fix Update 134 - Knowledge writes yield to foreground analysis

- All Knowledge stores that own separate SQLite connections (`GraphStore`, `DependencyTracker`, `ChunkRegistry`, `SqliteVectorIndex`, `KnowledgeQualityStore`, `ConflictResolver`, `QueryTraceStore`) participate in `persistence::BackgroundSqliteWriteGuard` for runtime writes.
- Background write guards are intentionally fine-grained around one transaction/statement. Never hold the gate for an entire `KnowledgeRuntime::refresh_active_profile()` pass; doing so would make foreground analysis wait for a full graph rebuild.
- When a foreground analysis session is waiting/active, new Knowledge writes pause between batches. Reads may continue, and runtime activity/diagnostics remain read-only.
- Do not add independent SQLite retry loops in individual Knowledge stores to compete with foreground analysis; use the shared priority gate.

## Update 135 - current retrieval and packet contract (supersedes earlier integration details)

- Statistics projection uses `StatisticsService::player_knowledge_json` for the shared player owner. This supersedes Update 108's active-account-only facets and lifetime-average rating fallback. Live query facts and projected facts use the same deterministic node/source identities; recorded ratings carry account/provider/time-control context. Opening statistics link to existing Opening/OpeningFamily identities; statistic scope links reference Color, TimeControl and TerminationType, with Player/Account/Provider ownership links for ratings.
- `KnowledgeQueryRoute::profile_scope` preserves the authoritative Coach scope. `knowledge_node_matches_scope` is the shared eligibility predicate before final ranking for exact, graph, lexical and vector candidates; containers may be traversed but cannot themselves establish a personal assertion. Values on the same entity axis are alternatives; different axes compose. Missing required board context remains required and fails answerability.
- Fresh StatisticsService candidates carry their source references in `HybridNodeCandidate`; they supersede persisted statistics for that query. The source read is outside the runtime mutex. `coach_evidence` uses `try_lock`; if maintenance owns the graph, the same ranker/packet builder processes only live candidates without accessing graph stores.
- No 1000-chunk embedding warm-up runs inside a Coach question. Semantic search reuses current persisted vectors and can embed at most eight already scoped graph/lexical candidates in memory. It never writes query vectors or a corpus during foreground retrieval. `ChunkTextEmbeddingWriter` remains available for explicit/background indexing clients.
- `EvidencePacketBuilder` carries complete typed properties, not a truncated string as factual authority. It sends each graph assertion once, preserves required scope cells, rejects invalidated nodes/chunks/edges, and only emits relationships whose endpoints are supplied. Traversal reachability alone is not relationship support or causation.
- Packet estimates include the envelope, identifiers, typed properties and uncertainty text. Exact queries start near 600 tokens, comparisons/rating lists grow with required evidence, and the hard packet ceiling is 4000 estimated tokens. The final provider optimizer enforces serialized JSON size and removes internal `requiredChunkGroups`/budget metadata after validating coverage.
- `profile.context.v3` retains scalar-claim grounding. It exposes source statistical denominators, separate supportConfidence/supportCoverage/sourceFreshness, meaningful uncertainty details and explicit missing scopes. Graph quality must not overwrite a learned profile's own confidence.
- `DependencyTracker::is_invalidated` is a read-only per-entry check. Diagnostic `QueryTraceStore::upsert` is best-effort and non-blocking at the shared writer-priority gate; trace failures must not discard a grounded answer.

## Update 136 - practice and similarity evidence

Live Coach retrieval may include the shared player's graded quiz-practice counters as scoped statistic facts, marked `graded_quiz_moves_not_independent_skill`; the source remains the native database table. A legal alternative outside the small candidate set is ungraded unless native analysis confirms a clear error. Internal legacy column names do not justify an independent-skill claim. Weakness-pattern uncertainty is projected as compact graph metadata alongside its analyzed-move denominator. Position-similarity scores are retrieval signals and may be sent as packet metadata, never as proof of a personal conclusion. Existing player-scope intersection remains mandatory for similar positions.
Position-family quiz routes with a required board may use the current-position channel even if their conversational intent is a generic concept. A historical/personal query with an incidental open board still must not acquire current-board retrieval.

## Update 139 - KnowledgeRuntime performance baseline diagnostics

- `KnowledgeRuntime` records lock-independent refresh duration/lock-wait counters and Coach evidence graph-availability/fallback timing. The counters are diagnostics only and must never gate retrieval, refresh or foreground priority.
- `knowledge.inspector.v1` may expose this telemetry as `runtimePerformance`, including while the runtime mutex is busy. Reading performance telemetry must never wait on the main KnowledgeRuntime lock.
- Update 139 does not change projection/retrieval semantics and does not add a second metrics database or persistent telemetry store.

## Update 143 - incremental Knowledge maintenance

- `KnowledgeRuntime::refresh_active_profile(...)` still owns one serialized graph refresh, but downstream quality/chunk maintenance is incremental between low-frequency safety passes. Projectors report the graph/dependency entries they actually changed or removed; Runtime must not rescan/materialize the full ~2200-node player graph after every ordinary profile update.
- `GraphStore::upsert_node/edge` return whether persisted graph state changed. `DependencyTracker::dependencies_match(...)` is the read-only guard for provenance/dependency rewrites. Projectors update provenance/dependencies only when the expected source set/version changed; do not reintroduce unconditional dependency writes for every projected entry.
- Dirty node/edge entries plus source invalidations drive targeted quality refresh. Dirty nodes drive owner-scoped `graph_node` chunk rematerialization. Removed nodes delete their deterministic owner chunk immediately. A six-hour in-memory full maintenance pass remains the safety net for freshness decay, stale legacy chunks and out-of-band state; restart/profile switch also forces a full pass.
- Incremental maintenance remains derived Knowledge work only. Authoritative Games/Analysis/Statistics/Profile stores are unchanged, no second scheduler/cache/database is introduced, and Coach retrieval continues using `try_lock` fallback behavior while refresh owns the runtime mutex.
- `knowledge.performance.v1` exposes full-vs-incremental refresh counts plus changed-entry, quality-entry and chunk-node work so the optimization can be measured without Flutter-derived telemetry.


## Cleanup Update 145 - final Knowledge maintenance boundary

- Normal profile refresh keeps one shared projection pipeline and performs quality/chunk maintenance only for dirty graph/dependency entries. The periodic/restart full pass is the safety fallback for freshness and legacy/out-of-band state, not a second refresh architecture.
- Projector change reports, dependency matching and GraphStore changed-state returns are internal maintenance contracts. Authoritative chess facts remain in Games/Analysis/Statistics/Profile persistence and must not be copied into a new Knowledge-owned source.

## Update 149 - precise KnowledgeRuntime activity diagnostics

- `runtimeActivity` exposes phase detail, operation/phase elapsed time, progress-known state, last completed phase duration and the fact that active refreshes own the KnowledgeRuntime mutex while fine-grained SQLite writes still yield through the shared foreground-priority gate.
- A diagnostic `status=busy` means the graph runtime lock is occupied, not that profile preparation is incomplete. `profileBackground.status` and `workerActivity` remain separate native states.
