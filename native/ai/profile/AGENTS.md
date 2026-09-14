# User Chess Profile AI Agent

## Scope

`native/ai/profile/` owns the compact learned chess profile used by coaching and practicality.

## Rules

- Preferences and measured abilities are separate. Never infer a preference merely because a player is weak/strong at something.
- Unknown fields stay unknown with low/zero confidence; do not fabricate style, Elo, risk tolerance or strengths.
- Learn only from existing KChess chess data supplied as aggregated observations. This folder must not read SQLite or start Stockfish.
- Persisted JSON is versioned through `ChessProfile`; provider/UI code must not define a second profile schema.
- Weakness observations carry analyzed-move denominators and a smoothed Beta(1,1) posterior mean with approximate 90% uncertainty bounds. The bounds express uncertainty in the observed major-error rate, not a diagnosis or official skill/rating. Older profile JSON without these optional fields remains readable.
- `profile_updater.*` may derive stable machine IDs for strengths, weaknesses and common mistakes, but visible wording belongs to the coach/ARB layer.
- Player-aware practicality consumes a compact mapping from this profile; engine ranking remains authoritative.
- Repeated-personal-mistake detection is conservative and only becomes true after sufficient sample/confidence.

## Evidence-driven Profile v2

- `profile_evidence_adapter.*` normalizes already available game/statistics/analysis data only; it never fetches data itself.
- `game_relevance_scorer.*` + `sample_builder.*` provide reusable relevance/diversity scoring; the staged funnel uses a bounded 20-game initial sample for the immediate first impression.
- The interesting-game stage is bootstrap-safe: recency affects priority but MUST NOT hard-gate older unanalysed games. Its bounded candidate budget is age-band balanced so metadata-only history can become eligible for shared-cache analysis even before Accuracy/error signals exist.
- `missing_evidence_detector.*` + `adaptive_profile_analyzer.*` only describe missing-work requests. The service layer is the only layer allowed to execute those requests through the existing `AnalysisService`. Before any profile-triggered Stockfish work, services must reuse `AnalysisService::shared_cached_analysis(...)`; profile code must never create or own a parallel engine-analysis cache.
- `pattern_matcher.*`, `trend_engine.*`, `hypothesis_manager.*` and `coach_priority_engine.*` own interpretation. A single game must never become a stable personal weakness.
- The old whole-profile `profile.context.v2` serializer was removed in cleanup update 72. Do not recreate a direct learned-profile serializer for Gemini. The active Coach path is `profile.context.v3` assembled by the general `native/src/knowledge/KnowledgeRuntime`.
- Example positions are evidence/training candidates from the player's own games; they remain linked by `gameId`/`ply` and are not standalone invented puzzles.
## Staged Background Profile Funnel

`profile_analysis_funnel.*` owns the pure selection/filter logic for the nine-stage profile pipeline. The progression is cheap-to-expensive: small initial sample -> all-game metadata/relevance sweep -> representative historical sample -> interesting-game filter -> move/position candidates -> existing-evidence filter -> fast probe -> verification -> deep analysis. Every stage must justify promotion to the next stage. This folder receives normalized records from services; it never reads SQLite and never starts Stockfish directly.

- Stage 1 selects at most 20 diverse/recent games for a fast first impression; it must not wait for the full history.
- Stage 2 classifies/summarizes and relevance-ranks one cheap metadata row for every game. Metadata consideration is not engine analysis.
- Stage 3 builds a representative historical population before expensive filtering. Its strata use coarse existing metadata (opening family, color, outcome, time control, age, ending-length phase and relative opponent strength) so high-skill/complex libraries cannot send nearly every game downstream merely because most games look interesting. The sampling layer must preserve real population proportions first and may only then apply bounded priority boosts.
- Stage 4 bounds the interesting-game set inside the Stage-3 sample; ordinary sampled games may stop here.
- Stage 5 promotes only a small number of persisted move/position candidates per interesting game.
- Stage 6 reuses decisive existing classification/loss evidence and only forwards unresolved targeted positions to engine work. Missing FEN must never trigger an automatic whole-game fallback.
- Stage 7 produces a cheap quality request used as a promotion/budget signal. Runtime execution is service-owned: once engine work is justified, the service requests the cheapest sufficient full-game depth/MultiPV through the shared Analysis cache. Existing equal-or-better complete analysis short-circuits the request. The profile layer itself never decides where engine payloads are stored.
- Stage 8/9 may raise the requested quality for unresolved tactical/mating/close-alternative evidence, but any actual engine upgrade is still written only to the shared authoritative game-analysis cache. These tiers are reasoning/budget concepts, not separate persistence layers.
- A later stronger normal Analysis run is a new evidence generation for the profile. Services must invalidate/requeue the derived profile work, then rebuild Evidence/Patterns/Confidence/Hypotheses/Graph from the stronger shared-cache rows. The profile layer must never respond to that revision by starting duplicate Stockfish work when the new cache already satisfies its requested quality.

## Progress semantics

Profile progress must distinguish cheap library coverage from expensive evidence work. `indexed` means the metadata sweep has classified the game and is a valid terminal state for ordinary low-relevance history; it must never be presented as engine analysis. `done` is reserved for promoted/relevant games whose current profile work is resolved, whether by reused authoritative KChess evidence or a profile-triggered upgrade of that same shared analysis cache. Persisted profile JSON exposes indexed/relevant/resolved work separately; retired sparse-probe telemetry is not part of the active profile contract. Do not derive UI wording that calls metadata indexing "analyzed games".


## Profile library and Knowledge Graph boundary

The persistent profile library is the source of truth: existing game/statistics/shared-analysis stores and the versioned learned profile contain the real data. This folder owns learning only; it does not own Coach graph retrieval. The general `native/src/knowledge/` runtime projects learned profile/evidence locators into the Knowledge Graph and assembles `profile.context.v3`.

`ProfileQueryScope` remains a bounded routing contract produced by the AI query planner. Profile-learning code must not broaden it, read SQLite, or start Stockfish. Proof/supporting evidence is selected by the general Knowledge Graph from authoritative source locators.

## Update 85 - single authoritative analysis cache

- Normal Analysis and Player Profile Maintenance are clients of the same persisted `analysis_runs` / `move_analysis` cache.
- Profile maintenance may request cheaper settings, but it MUST reuse any equal-or-better saved analysis and MUST upgrade the same cache when stronger work is required.
- A weaker run MUST NOT replace a stronger saved run. Engine analysis is never duplicated into a profile-owned store.
- Profile evidence comes from the single shared analysis cache; the retired sparse probe table is removed by migration 35.
- The profile database stores derived player knowledge only; graph relationships are owned by `native/src/knowledge/`.
- Shared analysis is deleted only through the existing user analysis-deletion flow; derived profile state is refreshed when that authoritative cache changes.

## Update 90 - background progress fields

`ProfileBackgroundProgress` may persist provider-history coverage beside queue/evidence counters so learned-profile snapshots remain self-describing. These fields are telemetry only: they must not alter hypothesis learning, evidence weighting, or graph semantics. Missing fields in older JSON payloads keep compatibility defaults.

## Update 92 - shared-cache terminal semantics

When the service requests the funnel's sufficient full-game quality and `AnalysisService` reports an equal-or-better complete shared run (`cache_ready`), that evidence request is resolved for the current source generation. The service must not loop the same game back through Stage 6 without a newer source generation; later stronger analysis still invalidates/requeues through the existing shared-cache change path.

## Update 93 - calibrated profile confidence

Overall `ChessProfile::confidence` is Coach-readiness confidence, not background-work progress. It must be calibrated from analytically-backed independent games, analyzed move evidence, and resolved relevant-evidence coverage. Metadata-only library rows may enrich openings/results/time-control context but must never by themselves drive overall confidence toward a high value. Provider history that is still incomplete caps overall confidence conservatively. Keep per-pattern/per-signal confidence semantics separate from this overall readiness score.

## Update 94 - nine-stage funnel numbering

`profile_analysis_funnel.*` uses stable persisted Stage 1..9 numbering: Stage 1 initial fast sample; Stage 2 global metadata + relevance sweep; Stage 3 representative/prioritized historical sample; Stage 4 interesting-game filter; Stage 5 move/position candidates; Stage 6 existing-evidence reuse; Stage 7 fast probe; Stage 8 verification; Stage 9 deep analysis. Stage 3 is a hard architectural boundary before expensive per-game inspection; do not bypass it once the sampler is active. `ProfilePipelineStage` is the native contract for these numbers.


## Update 95 - Stage-3 sampling population

`profile_analysis_funnel.*` defines the metadata-only Stage-3 sampling population and stable coarse strata. Strata are derived from already available game/statistics metadata only: opening family, player color, outcome, time-control class, recency bucket, coarse ending-length phase and opponent strength relative to the player. Building this population never parses PGN/moves, queries persistence itself or starts Stockfish. Keep strata coarse enough that large libraries do not fragment into thousands of one-game buckets. Update 96 owns allocation/selection of the hard-bounded historical sample from this population.
## Update 96 - representative historical sample

`select_profile_historical_sample(...)` owns the deterministic Stage-3 allocation. The original fixed 500-target behavior is superseded by Updates 128-129: 500 remains only the default hard ceiling. Allocation is two-phase: the representative majority follows the observed conditional hierarchy (color -> opening family -> outcome -> time control -> termination -> age/opponent/game-length context), then a bounded discretionary tail may be steered by Stage-2 relevance/profile priorities. Priority must never turn complexity into an unbounded sample multiplier. New games and later explicit user-driven analysis are outside this initial-history cap and may add evidence over time.


## Update 97 - Stage 3 is the historical promotion boundary

Stage 4/5/6 may consume historical games only when Stage 3 selected them into the representative sample. A high relevance/complexity score is not permission to bypass the sample. Stage-1 bootstrap games are selected from inside Stage 3; only genuinely new post-bootstrap games may join Stage 4 outside the historical ceiling. Sampled games that fail Stage 4 remain valid metadata/index terminal work at pipeline Stage 3 and must not create engine requests.

## Update 98 - bounded Stage 7-9 escalation

- Stage 7-9 escalation is hard-bounded per game. The default policy allows at most 3 fast candidates, 2 verification candidates and 1 deep candidate per promoted game. General complexity or high player strength must never bypass these ceilings.
- Verification requires a minimum game relevance and Deep Analysis requires a stricter minimum relevance in addition to the Stage 8 decision. A merely inconclusive/complex position is not enough by itself.
- Background probe budgets remain PC-safe by default: node/time ceilings are primary, thread counts stay conservative, and dynamic early-stop remains enabled. Server deployments may supply a stronger policy/budget later without changing the nine-stage selection architecture.
- The Stage 3 historical sample remains the outer hard boundary. Stage 7-9 budgets reduce work inside that sample; they never widen it.

## Update 99 - funnel telemetry semantics

Nine-stage telemetry must describe work that actually happened. The current Stage-3 historical sample is capped at 500; Stage 1 is a subset of it, while genuinely post-bootstrap new-game learning is separate incremental work. `pipeline_stage` remains highest-stage-reached diagnostic history, while current sample/interesting membership is recomputed by orchestration. Do not present Stage 7 promotion as Stage 8 verification or Stage 9 deep evidence unless those later decisions/results are truly persisted.


## Update 100 - profile probe quality contract

Stage 7/8/9 are impression-building tiers, not publication-grade game analysis: hard depth ceilings are 4, 8 and 12 respectively. Fast uses MultiPV 1; Verification/Deep use MultiPV 2. Hash is 1024 MB across profile tiers; Fast/Verification use one thread and Deep may use two. Relevance controls escalation, not a hidden depth increase. Dynamic early stop stays enabled.

## Update 102 - compact profile summary metrics

- `ChessProfile` may persist compact summary metrics derived from already supplied evidence: weighted average Accuracy, estimated playing strength, estimate confidence, and analyzed-game count.
- Estimated playing strength is not an official rating and must remain conservative. It may combine robust observed player ratings with bounded opponent/result performance evidence, but it must never fabricate strength without rating/performance samples.
- These summary metrics are derived only from existing profile evidence; this layer still must not read SQLite or start Stockfish.

## Update 112 - general Knowledge Graph is a read-only consumer

`native/src/knowledge/profile_knowledge_graph_projector.*` may read the persisted `ChessProfile` and evidence locators as authoritative input, but ownership of learning stays in this folder. Do not move pattern matching, trend learning, hypothesis learning or confidence semantics into the Knowledge Graph. The general graph may derive routing-only recovery/error-cascade observations from the shared saved analysis cache, but it must never write those back as a competing `ChessProfile` truth.

## Update 121 - Knowledge-Gap steering

`ProfileRelevanceContext::priority_game_ids` is the narrow bridge from Knowledge-Graph active learning into the nine-stage funnel. It is a relevance/sampling hint only: direct gap targets receive a bounded Stage-2 boost and count as profile-interest candidates, but historical games still must pass the existing Stage-3 representative-sample gate before Stage 4-9 work. Never turn a gap into a second queue, direct Stockfish call or unlimited historical bypass.

## Cleanup Update 124 — Profile-only graph removed

`profile_graph.*`, `profile_graph_builder.*` and `profile_graph_retriever.*` are deleted. This folder now contains only profile learning/funnel logic; all graph/retrieval functionality belongs to `native/src/knowledge/`.

## Update 127 - Stage-3 sampling metadata and eligibility

- Stage 3 consumes only cheap persisted metadata. `ProfileGameMetadata` now carries the coarse `termination_type` produced by the same native classifier used by Statistics plus an explicit `sample_eligible` bit.
- Games shorter than 16 plies (8 full moves) remain fully indexed in the Games DB, Statistics and Knowledge Graph, but are excluded from expensive historical profile sampling. This exclusion is sampling-only and must never delete or hide their metadata.
- `ProfileSamplingPopulation.total_games` describes the whole metadata population, while `eligible_games`/`excluded_games` describe the Stage-3 sampling population. Allocation and later adaptive budgeting must use `eligible_games`, never the full library count.
- Termination is an additional sampling dimension. Update 129 replaces the former joint cross-product allocation with hierarchical conditional sampling while preserving the same authoritative metadata source; PGNs are not reparsed and Flutter never invents categories.

## Update 128 - adaptive Stage-3 sample requirement

- `estimate_profile_sampling_requirement(...)` owns the Stage-3 sample-size decision. Raw library size is not a multiplier: the target is derived from the observed metadata distribution already built from Statistics/Games metadata.
- The default fixed bounds are 40 eligible games minimum and 500 maximum. Accounts with fewer eligible games pass through all eligible games; accounts above the floor receive a dynamic `recommended_games` between those bounds.
- Diversity is measured from the effective category distribution of opening family, player color, result, time control, termination, recency band, coarse game-length phase and relative opponent strength. Only observed primary hierarchy paths influence the target sub-linearly; theoretical cross-product combinations never exist in the sampler and cannot force hundreds of analyses.
- `ProfileSamplingRequirement` carries total/eligible/excluded counts, minimum/recommended/maximum, required representative strata, diversity, covered population share and `capped`. Later service/UI diagnostics must expose these native values rather than reconstructing the target in Flutter.
- `select_profile_historical_sample(...)` consumes `recommended_games`; `maximum_games` remains a hard safety cap, not a completion target.


## Update 129 - hierarchical statistics-driven Stage-3 sampling

- `ProfileSamplingPopulation` keeps one normalized metadata candidate per eligible game instead of materializing the full combination cross-product. Opening family prefers the persisted opening-name family (for example Italian Game/Ruy Lopez) and falls back to ECO only when the name is missing.
- The representative share is allocated recursively through color -> opening family -> result -> time control -> termination -> recency -> relative opponent strength -> coarse ending-length context. Every level preserves the observed conditional population proportion, so meaningful combinations can receive seats without giving every tiny leaf a mandatory slot.
- Stage-2 relevance, hypotheses and Knowledge-Gap hints choose games only inside the representative allocation or the bounded discretionary tail. The tail is capped per observed primary hierarchy path using `maximum_priority_multiplier`; it cannot erase the player's real statistical distribution.
- Short/ineligible games remain metadata/statistics/Knowledge-Graph data and never enter this expensive historical selection. Sampling continues to use only existing Statistics/Games metadata; no PGN parsing or engine work is introduced.

## Update 130 - Stage-1 containment and >500 semantics

- The fast Stage-1 first impression is now a subset of the selected Stage-3 historical sample. It must not add up to 20 extra historical games beyond the adaptive recommendation/hard ceiling.
- `build_initial_profile_sample(...)` skips `sample_eligible=false`; short games remain metadata/statistics/Knowledge-Graph evidence but never gain expensive profile work through Stage 1.
- More than 500 lifetime evidence games is valid only through genuinely post-bootstrap incremental games. Historical archive work remains bounded by `ProfileSamplingPolicy::maximum_games`.


## Update 131 - sampling coverage vs profile confidence

- Stage-3 structural coverage and `ChessProfile::confidence` are separate native concepts. `ProfileHistoricalSample::covered_population_share` measures how much of the eligible metadata population is represented by at least one selected observed hierarchy path; it must never be interpreted as confidence in chess conclusions.
- `ProfileBackgroundProgress` persists adaptive sampling telemetry (`eligible/excluded`, dynamic recommendation, min/max, strata coverage, diversity, cap state) plus `initialPreparationComplete`. The historical sample budget is the native `recommended_games`, never a hard-coded 500 target.
- Initial profile preparation may be complete below 100% Coach-readiness confidence. Completion is work-state semantics (history known + all currently relevant evidence resolved), while confidence remains evidence calibration from analytically backed games/moves.


## Update 133 - Stage-3 contract cleanup

- `kProfileSamplingMinimumPlies` is the single native eligibility threshold for short historical games; callers must not duplicate `16` in service/UI code.
- `ProfileSamplingPolicy` contains only active policy knobs. The unused legacy `minimum_per_stratum` field is removed because hierarchical proportional allocation never consumes it.
- `ProfileHistoricalSample` no longer exposes unused population/seat counters. Downstream code consumes the persisted requirement, selected entries, covered strata and covered-population share only.
- The hard maximum remains `ProfileSamplingPolicy::maximum_games` (default 500). It is a safety limit, not a target.
