# Native AI Coach Instructions

## Scope

`native/ai/` owns AI Chess Coach runtime orchestration and chess-grounded reasoning. Flutter may display these results but must not reproduce this logic.

## Architecture

- Keep the coach chess-only; `OFF_TOPIC` is rejected by native routing/policy.
- Prefer deterministic KChess facts, cache, theory/opening data and Stockfish before asking an LLM.
- Provider code must remain replaceable; do not couple contracts to one model/vendor.
- Python is development/training tooling only and must not become an app runtime dependency.
- Keep request/response DTOs transport-oriented. Do not hide engine calls or domain decisions inside DTOs.

## Context discipline

Read this file plus the nearest child `AGENTS.md`. Search existing KChess systems before adding logic so analysis, theory, engine, profile, cache and training behavior are reused rather than duplicated.

## File style

Keep files focused and small. Split components by responsibility as the coach grows, and give non-trivial handwritten files explicit section markers.

## Orchestration

- `coach_orchestrator.*` is the single top-level coach pipeline entry point.
- Keep its flow ordered as route → plan → context → retrieve → analyze → practicality → provider → validate → response.
- Keep domain algorithms in their focused components; the orchestrator only coordinates the single route → plan → context → retrieve → analyze → practicality → teach → provider → validate pipeline.
- The orchestrator coordinates components but does not become the home for domain algorithms.

## Domain routing

- `domain_router.*` is the deterministic baseline for chess-only routing and intent confidence.
- `domain_router_terms.h` contains only compact routing vocabulary; keep domain decisions out of Flutter.
- Treat loaded FEN/PGN as chess context, but do not invent session semantics here; conversation state belongs to the session component.

## Conversation state

- `conversation/coach_session.*` owns compact ephemeral session state and board/topic inheritance for follow-ups.
- A `session_id` alone is not conversation evidence; routing treats a follow-up as contextual only when memory exists for that ID.
- Keep full transcripts out of native session state. Store only the compact fields required to resolve later turns.

## Query planning

- `query_planner.*` converts the routed intent into a small evidence/budget plan; it must not retrieve evidence itself.
- `dto/evidence_plan.h` is the stable deterministic evidence-planning contract: compositional sources + information needs + perspective/scope/depth/priority. Natural-language questions should map to combinations of these needs rather than proliferating hard-coded intent classes.
- Keep evidence requests ordered and provider-neutral. Cache/conversation/position/theory/opening/profile/engine are requested through shared evidence kinds rather than vendor payloads.
- Engine budget in `QueryPlan` is only the requested planning hint; `engine_budget.*` owns the final runtime decision.
- General chess questions must remain valid without a loaded FEN/PGN.

## Context building

- `context_builder.*` prepares the provider-neutral context immediately after query planning; do not assemble prompts in Flutter.
- Keep the user question and FEN authoritative, summarize only compact session fields, and reduce PGN to the recent parsed main line instead of forwarding full games by default.
- Input budgets follow the coach depth targets (concise 800, standard 1500, detailed 2500 estimated tokens). The estimate is a lightweight runtime guard, not a provider tokenizer.
- Reuse the native PGN parser for chess structure; do not add a second PGN parser inside the coach.
- Later provider adapters may tokenize more precisely, but they must respect the already bounded context rather than silently expanding it.
## Position intelligence

- `position/position_features.*` owns deterministic board-derived coach features; keep engine search out of this layer.
- Feature payloads are evidence, not evaluation. Preserve raw/transparent metrics so later weakness, plan and practicality components can interpret them without duplicating board scans.
- Extend the same `position/` subsystem in later position-intelligence updates and follow `position/AGENTS.md`.

## Position Features II

- Strategic board facts stay under `position/`; do not move pawn/king/outpost heuristics into the orchestrator.
- `position_structure_features.*` extends the shared `position.features` evidence payload rather than introducing a competing evidence contract.
- Weak-square, outpost and bad-piece outputs are candidates until the dedicated weakness layer interprets them.

## Weakness analysis

- `position/weakness_analyzer.*` is the single feature-to-weakness interpreter.
- It consumes the already extracted `PositionFeatures`; do not duplicate FEN parsing or board scans inside the analyzer.
- `position/position_weakness_facts.*` may extend the one-pass position extraction with deterministic candidate facts needed by the analyzer.
- Weakness evidence is provider-neutral and remains separate from the later exploitation-planning layer.

## Weakness exploitation

- `position/exploitation_planner.*` is the only weakness-to-attack-method mapper.
- It consumes `PositionWeaknesses`; no FEN parsing, Stockfish search, provider text, or duplicate weakness detection belongs there.
- Its output is structured evidence for later plan/candidate/provider stages, not a concrete move recommendation.

## Tactical detection

- `position/tactical_detector.*` and `position/tactical_line_motifs.*` form the single deterministic tactical-motif layer.
- Query planning requests `tactical_motifs` only for position-bound intents where tactical evidence is useful; the orchestrator only coordinates extraction.
- Tactical motif evidence is board evidence, not engine truth or natural-language coaching. Later engine/provider/validator stages may confirm consequences.

## Tactical detection II

- Advanced tactical detection remains split by responsibility under `position/`: static geometry in `tactical_pattern_motifs.*`, legal-trigger patterns in `tactical_move_motifs.*`.
- `TacticalDetector` stays the single public facade and serializes the combined motif evidence; callers must not invoke advanced motif helpers directly.
- Trigger squares in motif evidence identify a legal tactical candidate only. Candidate Move, Engine Budget and Validator updates remain responsible for proving objective quality and tactical consequences.
## Chess concept catalog

- `concepts/concept_catalog.*` is the shared provider-neutral taxonomy for tactics, strategy, pawn structures, piece play, king safety, openings, endgames, calculation, defence, attack and training.
- Catalog entries use stable machine IDs plus compact aliases; localized explanations and provider prose do not belong in the catalog.
- General concept lookup must remain valid without a loaded board. Update 30 adds retrieval on top of this single catalog instead of creating a second knowledge taxonomy.

## Concept retrieval

- `concepts/concept_retriever.*` supplies provider-neutral concept evidence for general chess questions and concept-oriented intents, including requests without a loaded board.
- Concept evidence belongs to the retrieval stage; do not move free-form lookup into Flutter or the orchestrator's domain-analysis code.

## Strategic plan generation

- `position/plan_generator.*` converts already extracted position/weakness/exploitation evidence into a compact ranked set of strategic candidate plans.
- Query planning requests `strategic_plans` only for position, plan and game-review contexts with board data.
- The orchestrator may compute prerequisite feature/weakness/exploitation DTOs without emitting every intermediate evidence item, but it must not duplicate their algorithms.
- Strategic plans remain move-neutral until the Candidate Move System grounds them in legal/engine-backed moves.

## Engine budget

- `engine_budget.*` is the single runtime policy for deciding whether a fresh engine call is needed and whether it is `none`, `probe` or `deep`.
- Never run Stockfish directly from the budget component. It only returns a decision; the existing engine source executes work.
- Reuse cached/persisted engine grounding before spending a fresh engine budget.
- Deep search is reserved for review-oriented or explicitly detailed comparison work; ordinary tactical/move questions stay probe-level unless a later policy has stronger evidence.

## Evidence retrieval

- `evidence_retriever.*` is the single cache-first evidence ordering layer.
- Preserve retrieval priority as cache → existing analysis → local context/concepts/features → theory/opening → user profile → engine.
- Bind existing KChess cache, persisted-analysis, theory/opening, profile and engine systems through `EvidenceSources`; do not create duplicate storage or engine clients inside `native/ai/`.
- `engine_budget.*` resolves `ENGINE_NONE` / `ENGINE_PROBE` / `ENGINE_DEEP` after cache and persisted analysis retrieval; existing engine grounding suppresses duplicate fresh searches.
- `EvidenceRetriever` passes the resolved budget to the existing engine source through a copied `QueryPlan`; engine clients remain outside `native/ai/`.
- Evidence IDs are stable machine references owned by the retriever/component, never localized display text.
- Reuse the `PositionFeatures` DTO returned during retrieval when later position-intelligence stages need it; do not rescan the same FEN merely to rebuild identical features.

## Candidate moves

- `candidate_move_system.*` is the single engine-neutral selector for Best Move, up to three MultiPV alternatives, the played/user move and the critical reply from the best principal variation.
- Candidate selection consumes structured engine output; it must not start Stockfish itself or duplicate engine evaluation logic.
- `EvidenceSources::existing_candidates` exposes structured cached/persisted MultiPV when available; `engine_candidates` may return fresh engine evidence and its snapshot from one engine call. Never invoke both generic engine analysis and candidate analysis for the same fresh search.
- A candidate PV uses UCI moves and, when present, starts with its root move; the second PV move is therefore the critical opponent reply.
- Candidate moves are objective engine-ranked options only. Player-level practicality, risk and forgiveness belong to the later practicality layer and must not be baked into this component.

## Practicality

- `practicality/practicality_engine.*` turns existing candidate/position/tactical evidence into objective practical metrics; it never starts Stockfish.
- Update 35 remains player-independent and emits raw metrics plus `difficulty`, `risk`, `forgiveness` and `clarity`. Player/Elo adaptation belongs to Update 36.
- Missing inputs remain unknown and reduce confidence; never coerce missing engine/feature evidence into a false zero.
- Practicality does not override candidate ranking or recommend objectively inferior moves.

## Player-aware practicality

- `practicality/player_practicality.*` is a player-fit overlay on top of the objective Update 35 assessment; it never replaces or mutates engine ranking.
- Player-aware choice may select only the engine best move or an alternative inside the fixed objective quality gate. Unknown objective loss is not enough to promote a non-best move.
- `EvidenceSources::practicality_player` is the typed bridge for rating/skill/risk context. The richer persistent chess-profile model remains owned by the later profile layer and should map into this compact view instead of duplicating practicality logic.
- If no trustworthy player context exists, emit only objective practicality and do not invent a default Elo/profile.

## LLM provider layer

- `providers/llm_provider.*` is the single provider-neutral language-model contract used by the orchestrator.
- Provider adapters must contain transport/inference plumbing only; chess reasoning, evidence selection and validation remain in their dedicated native layers.
- The orchestrator receives a replaceable provider instance and must not contain vendor/model branches. OFF_TOPIC requests never reach the provider.
- Provider errors/unavailability must not fabricate a natural-language answer. Structured response/error propagation is extended by Update 38.

## Structured coach response

- `dto/structured_coach_response.h` is the single structured answer contract: natural-language answer, claims, concepts, recommendations and evidence references.
- The orchestrator copies provider content into `CoachResponse`; it derives `CoachAnswerType` deterministically from the requested `CoachMode`.
- Do not treat provider-supplied structure as validated chess truth. `validation/` in Update 39 owns claim, move and evidence-reference validation and at most one repair pass.

## Response validation

- `validation/chess_validator.*` validates machine-typed board claims and concrete recommended moves against authoritative native chess helpers/evidence.
- `validation/response_validator.*` validates evidence references, concept IDs and grounding before provider content reaches `CoachResponse`.
- Provider prose is never parsed as chess truth. Factual claims that need deterministic checking use `CoachClaimKind` plus `subject`/`value`.
- The provider may receive exactly one repair request containing the rejected structured candidate and validation feedback; there is no repair loop.
- If repaired content still fails, invalid provider content is not copied into the public coach answer.

## Gemini request budgeting

- `providers/` owns free-tier RPM/TPM/RPD admission; routing, evidence selection, services and Flutter must not duplicate quota calculations.
- Automatic turns yield at soft limits. Manual/follow-up turns and the single validation-repair pass retain reserved headroom until hard limits.
- Google 429 starts provider cooldown/backoff but never an internal retry loop. Provider error codes propagate to the app bridge so UI can distinguish temporary rate limits from generic provider unavailability.
- Keep quota-priority/error contracts synchronized end-to-end: `CoachRequest::automatic_turn` → `LLMProviderRequest::automatic_turn`, and `LLMProviderResult::error_code` → `CoachResponse::provider_error_code`. A source file must never be shipped without the matching DTO/header contract.

## App integration

- `native/src/services/coach_service.*` ist die einzige Core/C-ABI-Brücke in den Orchestrator. UI-Surface, Context-ID und Profile-ID sind Transportkontext, keine neue Intent-Logik.
- Gespeicherte PGNs werden nativ anhand der Context-ID ergänzt; Flutter soll keine vollständige Partie rekonstruieren oder übertragen, wenn KChess sie bereits gespeichert hat.

## Automatic coach trigger

- `automatic/automatic_coach_trigger.*` is the single native gate before automatic provider calls.
- It consumes existing move classification/WDL-derived score loss and cheap deterministic position/tactical signals; it never starts Stockfish itself.
- A normal move must not reach the provider. Only a positive `AutomaticCoachDecision` may be converted into an automatic `CoachRequest` by the app integration service.
- Repeated-personal-mistake detection is supplied by the profile layer (Update 43); the trigger only consumes that signal.

## User chess profile AI

- `profile/chess_profile.*` is the single versioned learned-profile contract for preferences, abilities, common mistakes, risk, openings, strengths/weaknesses and confidence.
- `profile/profile_updater.*` learns only from aggregated existing KChess observations. It does not read SQLite, call Stockfish or infer unknown preferences.
- `native/src/services/coach_profile_bridge.*` maps persistence into this profile and into the existing `PracticalityPlayerContext`; do not duplicate player-fit scoring in the profile layer.
- Repeated-personal-mistake signals are derived from the learned profile natively and fed into `automatic/`; Flutter must not decide them.

- `ConceptRetriever` keeps lexical retrieval as the zero-model baseline and invokes embeddings only when the lexical match is weak; concept embeddings are cached by model id/version.
- `optimization/provider_input_optimizer.*` trims only the copy of evidence sent to the language model. Full evidence remains available for native validation.
- Offline dataset/training/evaluation/quantization/benchmark tooling belongs under `tools/ai/`; Python remains development-only.

## Query family classification and profile retrieval

- `query_classifier.*` assigns every chess request one coarse family before context retrieval: `personal_chess`, `general_chess`, or `position`. Detailed `CoachIntent` remains the second-level intent. Mixed position+personal requests use the `position` family plus `needs_profile=true`; do not create parallel routing in Flutter.
- `QueryPlan` carries the family and independent `needs_profile`, `needs_position`, `needs_concepts` flags. A fresh engine budget must not by itself imply that personal profile data is sent to the LLM.
- Personal profile retrieval is handled by `native/src/knowledge/KnowledgeRuntime`, which consumes the already-classified `QueryPlan`/`ProfileQueryScope`. General chess questions do not receive profile chunks unless the classifier explicitly marks a personal need.

## Profile context resolution

`KnowledgeRuntime::coach_evidence(...)` is the only personal-profile context path. It performs native routing, hybrid retrieval, ranking and evidence-packet assembly and emits the stable provider contract `profile.context.v3`. `EvidenceRetriever`/`CoachService` consume that evidence; they must not query profile SQLite directly or recreate the removed profile-only graph/resolver stack. The former whole-profile v2 serializer must not be reintroduced.

## Final query classification contract (Update 73)

- Classify each request once into `personal_chess`, `general_chess`, or `position`; downstream stages must consume that `QueryPlan` instead of independently guessing intent.
- `ProfileQueryScope` is the native bounded scope for personal retrieval: topics, time controls, phases, proof requests and comparison requests. It contains IDs/filters only, never profile payloads.
- Generic training questions remain `general_chess`; training becomes personal only when the wording or inherited session scope explicitly refers to the current player.
- Follow-ups inherit query family/profile scope only inside the same `profile_id`. Reusing a session ID after switching profiles must start with empty personal context.
- Position grounding and personal-profile grounding are independent flags. A position/engine request must not implicitly expose profile data.
- Graph retrieval must consume `ProfileQueryScope` directly. Scope filtering happens before lexical/embedding ranking; embeddings can reorder eligible nodes but cannot broaden topics, time controls or phases.
- Comparison questions require balanced graph coverage for each explicitly compared time-control/phase. Missing graph coverage is returned explicitly and later layers must treat it as insufficient evidence rather than silently answering from one side.

## Provider-visible personal grounding

The provider boundary is query-plan aware. `ContextBuilder` omits FEN/PGN unless the plan needs position context, `ProviderInputOptimizer` keeps profile chunks atomic and budgeted, and `ResponseValidator` validates personal claims against the exact evidence actually sent to the provider. `profile_fact` is an exact scalar lookup; `profile_inference` is a separately labelled tentative deduction from multiple exact supplied premises. Incomplete requested profile scope cannot produce a grounded personal answer, and failed personal grounding remains fatal after the single repair pass.

## Compile-safety note (Update 78)

- Native AI code is compiled as modern C++; do not use reserved C++20 keywords such as `concept` as identifiers. Prefer descriptive names such as `concept_item` for concept records.
- Warning-only cleanup must not change validation semantics. If a parameter is intentionally unused to preserve an interface contract, mark that explicitly (for example `(void)param`) rather than removing the contract parameter locally.

## Update 123 — persönlicher Coach-Kontext

Persönlicher Coach-Kontext wird ab Update 123 vom allgemeinen nativen Knowledge Graph geliefert. `profile.context.v3` bleibt der stabile Provider-/Validator-Vertrag, aber seine Chunks stammen aus Query Router, Hybrid Retrieval, Ranking und `EvidencePacketBuilder`. Der frühere `ProfileGraphRetriever` wurde in Cleanup 124 gelöscht.

## Cleanup Update 124 - retired profile retrieval removed

The old `profile/profile_graph*` and service-side `coach_profile_context_resolver` implementation are deleted. `profile.context.v3` remains the provider/validator contract, but all personal retrieval now comes from the general Knowledge Graph runtime.

## Update 135 - personal evidence budget exception

Personal evidence continues exclusively through KnowledgeRuntime and `profile.context.v3`, now combining current native StatisticsService facts with graph evidence. The old depth-based 800/1500/2500 targets remain for ordinary non-personal context; sufficient personal evidence may use up to 4000 estimated tokens with a 4500 prepared-context-plus-evidence ceiling. Fixed provider instructions/schema remain separately counted by provider admission. Provider-visible scalar validation and the single-repair policy are unchanged. Internal required-chunk groups preserve scope/relationship support across final trimming.

## Update 137 - topic-specific quiz pacing

`CoachService` supplies native practice counts by motif without exposing the map to Gemini. After the existing DomainRouter chooses the exercise topic, `CoachOrchestrator` uses that same motif identity for both difficulty selection and later attempt recording. A Beta(1,1) posterior with conservative approximate bounds changes difficulty only after sufficient graded attempts. No global aggregate may make a weak endgame exercise harder because of unrelated opening answers. Quiz shape requires visible native best-candidate evidence, a follow-up question and no recommendation.

## Update 136 - adaptive practice loop

`CoachOrchestrator` resolves a native-verified played answer before retrieval and records its per-motif outcome through the service callback. Session state keeps the original quiz board/candidate/opponent reply only for that attempt; it is never new-position proof. Quizzes with a player scope may retrieve existing player-scoped similar positions. Native teaching target and profile context guide exercise difficulty; Gemini remains responsible for varied language, never for measuring skill or selecting unsupported factual moves.

## Update 153 - Coach pipeline diagnostics baseline

`CoachOrchestrator` emits read-only `CoachPipelineTrace` timing metadata after a turn completes. The trace measures the existing single pipeline (session -> route -> plan -> context -> retrieve -> position analysis -> practicality -> provider -> validation/repair) and must not create a second orchestration path. `CoachService` aggregates only privacy-safe counters/timings for the developer inspector; no question text, board payload, player identity or session token is retained in diagnostics.

## Teaching planner

- `teaching/teaching_planner.*` owns the deterministic lesson objective and delivery policy after practicality and before provider request construction.
- Keep one primary teaching objective per turn. The provider receives the native plan and may phrase it naturally, but native validation enforces recommendation/concept/reveal limits.
- Practice counters and future skill/scheduling signals extend this one planner; do not duplicate teaching policy in Gemini prompts, Flutter or a second scheduler.

## Update 155 - fine-grained teaching skills

- `teaching/skill_taxonomy.*` owns stable namespaced skill IDs for verified Coach practice. Resolve them from the already-available evidence set; never launch Stockfish or a second analysis pass for skill classification.
- A tactical motif becomes a persistent skill label only at the same >=0.85 confidence threshold used by native motif claim validation. Lower-confidence heuristics fall back to the coarse native teaching skill.
- `TeachingPlan::skill_id` / `skill_family` are internal native learning-policy fields. They may drive practice counters/scheduling but are not evidence that the player has or lacks that skill, and they need not be exposed to the LLM.
- Historical coarse practice buckets remain fallback priors only. New scored attempts must be recorded under the resolved fine-grained skill ID so unrelated topics no longer share one difficulty bucket.

## Update 158 - position-intelligence cache ownership

- `position/position_analysis_stage.*` owns deterministic Coach position intelligence and its bounded read-through cache. Features, weaknesses, exploitation, plans and tactics for the same exact FEN must be reused here rather than recomputed independently in retrieval/orchestration.
- Cache entries contain only deterministic native DTOs, are process-local, capped at 128 positions and are never persisted as player knowledge. Engine analysis/candidates remain in their existing analysis caches and are not duplicated here.
- `EvidenceRetriever` stays responsible for source retrieval/engine-budget decisions; it must not reintroduce its own position-feature pass. `CoachOrchestrator` consumes the stage output and keeps practicality/provider/validation ownership unchanged.

## Update 159 - validated response cache

- `optimization/validated_response_cache.*` may reuse only a previously native-validated `StructuredCoachContent` for an exact provider-request + validation-evidence key. Provider-visible and full validation evidence, session summary, teaching plan, profile scope, prompt/schema version and provider id are key material; any relevant evidence change must miss.
- Cache hits still pass through the ordinary Coach turn/session lifecycle. Never cache provider errors, unvalidated candidates, learning-attempt outcomes or player state, and never persist this cache to SQLite.

## Update 160 - expected-score-aware Practicality v2

- Candidate evidence may carry root-side `expected_score` derived from the already-returned engine WDL. This is reuse of existing engine truth, not a new search or model.
- Practicality uses expected-score loss as the primary objective safety/risk signal when available and falls back to centipawn loss otherwise. Engine rank remains authoritative and rank 1 stays eligible.
- `ModelVersionRegistry` reports `Practicality` v2 so exact response-cache keys naturally miss across the scoring-policy change.

## Update 161 - Automatic Coach teaching value

- Automatic coaching now separates objective event importance from final teaching value. Existing classification/WDL/motif/phase reasons remain authoritative; learner-state signals only adjust whether an unsolicited interruption is worth spending.
- Spaced-repetition due state is scheduling context, not skill evidence. Recent successful unsolicited Coach delivery contributes a bounded interruption cost. Verified pending-question answers still bypass the unsolicited gate.
- Keep this policy inside `automatic/` plus the existing `CoachService` integration. Do not duplicate teaching-value calculations in Flutter, Gemini prompts, the profile graph or the spaced-repetition scheduler.



## Update 164 - personal training from own games

- `teaching/personal_training_selector.*` chooses a quiz source only from already-learned `ChessProfile::patterns[].example_positions`; these positions remain references to the player's real persisted games and are never invented puzzles.
- Selection combines weakness severity/confidence, existing `coach_priorities` and spaced-repetition due priority. Due state affects scheduling priority only and must not become a player-skill claim.
- The selector is pure policy: no SQLite access, no engine work and no second training persistence store. `CoachService` resolves the learned profile/practice rows and hydrates the selected game/FEN through existing Database APIs.
- Explicit current-position quizzes keep their old semantics. Own-game selection happens only for an explicit `personalTraining` request with a player profile.

## Update 165 - provider-input hot-path optimization

- Provider evidence optimization may drop only exact duplicate/empty provider-visible items and the already-defined out-of-scope profile evidence; it must not mutate the full native validation evidence.
- Coach diagnostics report provider input/selected item counts, exact duplicate removals and estimated evidence-token reduction. This instrumentation is read-only and must not change routing, engine truth, teaching policy or validator behavior.

## Update 166 - response-cache key reuse optimization

- Build the exact validated-response cache key once per provider stage and reuse that same prepared key for lookup and any later store after native validation/repair. A successful miss must not serialize the full provider request plus validation evidence a second time merely to cache the result.
- The prepared key remains process-local and exact; it does not weaken cache invalidation or replace any provider/evidence field with a lossy fingerprint. Coach diagnostics may report key-build time/bytes, but cache-key construction must remain read-only and must not affect provider, teaching or validation behavior.

## Update 167 - AI Trainer cleanup/final ownership

- The final Coach pipeline is singular: session resolve → deterministic semantic route → query plan → context → cache-first evidence retrieval → cached position intelligence → objective practicality → native teaching plan → optimized provider input → exact validated-response cache/provider → native validation/one repair → session/verified-learning bookkeeping. Do not introduce a parallel Coach pipeline.
- Process-local caches are optimization only: position intelligence and validated provider responses are never persisted as player knowledge. Persisted learner state remains exclusively in the existing `ai_coach_skill_progress` rows.
- Update-167 cleanup removes stale stub/future-model wording only; no compatibility path or authoritative persistence/FFI contract is retired without an explicit migration.

## Fix Update 168 - quiz/hint validation fallback and retrieval scope

- A quiz must not request the full personal Knowledge packet merely because a `profile_id` exists for scoring. Personal evidence is retrieved only when the question actually requires personal claims.
- `personalTraining` may consume the learned profile once in `CoachService` to select a concrete own-game position; after selection, that provider turn must not re-fetch the full personal graph just to ask the exercise. The selected FEN/candidates and existing practice state remain authoritative.
- If an LLM quiz/hint response still fails the one repair pass (or its structured response is invalid), the provider prose is discarded. Native code may return only a stable `safeFallbackKind`; Flutter maps that machine key to localized ARB wording. Never expose rejected provider prose or move recommendations through this fallback.
- Recent Coach diagnostics include the final validation issue codes and any safe fallback kind so trust-boundary failures remain diagnosable.

## Fix Update 169 - quiz feedback and personal-training scope

- Completed native move classification participates in quiz grading: `theory`, `forced`, `brilliant`, `critical`, `best`, `excellent` and `good` confirm a successful learner answer even when it was outside the few candidates saved when the quiz was asked. This extends the existing session scorer; it does not create a second evaluator.
- A legal move without positive or negative native confirmation stays `legal_alternative_not_graded` and must remain neutral in provider wording.
- When `personal_training_position_selected` is set, the classifier clears inherited profile scope as well as `needs_profile`; the learned profile was already consumed to select the exercise and must not be fetched again for that provider turn.
- Coach diagnostics may expose only machine learner-attempt status and native classification, never question text or session identity.

## Fix Update 171 - Automatic question hand-off

Automatic Coach may leave a validated concrete board question in session memory when native candidates exist. That question reuses the existing pending-move/session path for the learner's next move, but remains unscored unless it originated from explicit quiz mode. Feedback to such an open question is interactive foreground work; it is not an unsolicited Automatic-provider request for quota priority.

## Update 172 - focused Coach dialogue

- The provider receives request-scoped instructions: common grounding always applies; board, quiz/hint, verified-attempt and personal-profile rules are included only when relevant. Native teaching, evidence selection and validation remain authoritative.
- Off-topic turns stop after native routing, before retrieval, position intelligence or provider cache work. A legal learner attempt resolved by session state still records its native learning result first.
- Verified-attempt wording is activated by typed native session state, not conversation prose. `CoachPrompt` v3 invalidates old exact response-cache entries.

## Update 173 - feedback-first grounded dialogue

The orchestrator passes the resolved native Coach session into `TeachingPlanner` so a completed learner attempt can shape the next objective before provider generation. Free-prose UCI move mentions receive a narrow validator gate against provider-visible candidates/PV or valid move claims. Provider instructions support this gate; they do not make unstructured prose authoritative. The existing validation/repair boundary remains mandatory.

## Update 174 - linked claims and completed-move evidence

The single orchestrator appends `move.contrast.v1` only when session resolution proved the played move and resulting FEN against the original question board. The position subsystem computes static before/played/candidate feature differences; the provider can describe them but cannot turn them into a fresh engine judgment. The provider output schema and prompt versions advance together so accepted claims quote their exact answer location and the native validator checks both quote and source.

## Update 175 - segmented grounding and cached outcome

The orchestrator validates production Gemini output as ordered factual/general/uncertainty/dialogue segments and joins them into visible prose. One factual segment links exactly one source-checked claim; factual or personal/board assertions may not hide in unlinked prose. This structural contract still needs human dialogue evaluation for semantic entailment. A verified learner move may add objective outcome fields to `move.contrast.v1` only through the same completed shared-analysis cache entry for its persisted game/ply/FEN. The prompt and response-cache versions advance with the wire contract.

## Update 178 - candidate-reference boundary

`CandidateMoveSystem` assigns deterministic IDs (`candidate.best`, bounded alternatives and `candidate.user`) when it materializes engine-neutral candidates. Provider-originated concrete recommendations and move-bound claims must select one of those IDs; provider adapters resolve IDs to UCI before validation. Do not reintroduce a provider-authored `move_uci` recommendation field or let an LLM create move authority outside native candidate evidence.

## Update 179 - explicit position follow-up routing

- A contextual user question with its own detectable chess intent keeps that current intent; only a genuinely elliptical `CoachIntent::follow_up` inherits `context_intent`. The prior trainer skill/topic must never override a new concrete board question.
- Explicit follow-ups may still consume the compact conversation evidence so references to the previous explanation remain understandable, but the previous answer is never board truth.
- Prevention/avoidance questions on a loaded board (for example avoiding a queen trade) are plan/move-choice work and request a probe-level native engine/candidate pass before provider explanation.

## Update 180 - single open trainer exercise

For a session/FEN with an unresolved scored quiz, another quiz request is a native `quiz_question_pending` state response and stops before planning/retrieval/provider work. It must preserve the original candidate/scoring state; Hint or Explain can advance assistance without silently replacing the exercise. Flutter only localizes/presents this machine state and suppresses duplicate pending notices.

## Update 181 - safe fallback and repair policy

`coach_orchestrator.*` may build a degraded position response only from exact `engine.candidates.v1` evidence. `position_explanation` carries the native best candidate and `candidate_comparison` at most the native best plus one alternative; visible prose remains localized in Flutter. Quiz/hint presentation failures and native move-authority violations bypass the repair provider. Keep the one repair pass only for schema/grounding defects that can safely be corrected without changing native chess authority.

## Update 183 - position-bound candidate integrity

- `EvidenceRetriever` is the last native integrity gate before candidate evidence becomes provider-visible or session/fallback state. When a request has an exact FEN, root candidates that are illegal in that FEN are discarded before `engine.candidates.v1` is emitted.
- Candidate PVs are kept only as the legal prefix beginning with the same root move. An invalid/mismatched PV must never supply a trainer critical reply. Duplicate root moves are collapsed before stable candidate IDs are assigned.
- The sanitized `CandidateMoveSet` is the only candidate set passed onward to Gemini, native safe fallback and pending trainer-question scoring. If no legal root candidate survives, downstream code must treat candidate evidence as unavailable rather than exposing a stale/illegal move.

## Update 184 - provider repair prevention

- Encode deterministic teaching-plan recommendation limits directly in the Gemini response schema, including zero recommendations for quiz/hint or reveal-level-0 turns.
- If the provider-visible packet contains no native candidate IDs, do not advertise candidate-bound claim kinds to Gemini.
- This optimization must reduce preventable invalid generations without relaxing the existing native validator or candidate-authority boundary.
## Update 186 - reliability-series invariant

The completed 176-185 series has one authority chain: exact request FEN -> native legal/sanitized candidates -> bounded provider-visible evidence -> structured provider output -> native validation -> session/fallback/UI DTO. No later optimization may bypass a link in that chain, reuse cached prose across model/prompt/schema/validator identities, or move chess truth into Flutter/provider prose.

## Update 187 – Explicit current-question authority

Current user text outranks session/lesson history. Concrete current-turn intent is recorded in `DomainRoute::explicit_current_intent`; elliptical follow-ups may still inherit context, but scored/current positional questions keep their own route. Stale completed-move contrast/feedback is injected only on the actual completed-move turn. Diagnostics expose current intent, context intent and whether the current intent is authoritative. Worst/fastest-loss wording is recognized as move-explanation intent now; dedicated native analysis modes follow in the next series update.
## Update 188 - native position analysis modes

- `QueryPlan` now carries one native `PositionAnalysisMode` resolved from the current turn before retrieval/provider generation. Supported semantics include position overview, best move, worst move, fastest loss, avoid trade, threat, move explanation, what-if, candidate comparison and verified learner-move evaluation.
- Explicit current-turn wording and native completed-move state select the mode; previous lesson/session topics do not override it. A concrete mode forces objective engine grounding but does not itself implement a new engine search strategy.
- Provider requests and validated-response cache identity carry the resolved mode so later engine/evidence/provider work cannot silently reuse an answer produced for a different chess question. Dedicated worst-move/fastest-loss engine execution is the next update.

## Update 189 - complete-root extreme move analysis

- `worst_move` and `fastest_loss` are not ordinary top-N candidate questions. `EngineBudgetSystem` must force their dedicated foreground engine pass even when the same FEN already has ordinary hint/cache evidence; a top-N cache cannot prove which omitted legal move is worst.
- `AnalysisService::coach_extreme_move_json()` evaluates every legal root move with a bounded scout, then refines only the three shallow worst candidates. A proven losing mate outranks centipawn loss; shortest losing mate is the `fastest_loss` focus, otherwise the lowest root-side evaluation is the focus.
- Extreme results are ephemeral and never persisted as normal game analysis. `CandidateMoveSet::focus` / `focus_kind` distinguish the intentionally bad move from `best`; never relabel an extreme focus as a recommendation or as Stockfish's best move.
- Candidate/PV legality is still sanitized by `EvidenceRetriever` before provider exposure. `engine.candidates.v1` may expose `focus` plus bounded extreme alternatives, and validators must recognize that candidate ID as native move authority.

## Update 190 - verified chess fact identity

Candidate evidence now carries `coach.chess_facts.v1` truth-units with stable native Fact IDs. These facts are derived only from already legal, native `CandidateMoveSet` data and are the future authority boundary for provider prose. Keep `engine.candidates.v1` backward compatible while migrating later provider/renderer stages to reference Fact IDs instead of recreating move/evaluation relationships from prose.
## Update 193 - native verified-fact rendering

- Provider-authored move/fact references remain opaque tokens through Gemini parsing and native response validation. Concrete UCI/PV notation is resolved only after validation by `rendering/VerifiedFactRenderer` from exact native `engine.candidates.v1` evidence.
- A fact token may be rendered only when the owning segment selected that exact `fact_id`. A candidate token may be rendered only when the segment is grounded by a fact for that candidate or an exact typed candidate-bound claim. Unknown or mismatched tokens are native authority failures and bypass provider repair.
- Validated-response cache entries contain already rendered native notation; repair serialization may retokenize that native notation for the provider, but no provider output can directly become concrete move notation. Flutter remains presentation-only and does not resolve chess truth.

## Update 195 - fallback / validation / diagnostics integration

- A rejected provider explanation may degrade to native board truth when exact candidate evidence survives. `worst_move` and `fastest_loss` use dedicated fallback kinds and expose their verified `focus` only as a native board overlay; an intentionally harmful focus must never be stored or described as a recommendation.
- Provider unavailability/output failure, validator rejection and renderer rejection remain distinct diagnostics. A safe fallback may keep the user interaction usable, but must preserve the provider/validation error code and a stable `fallbackReason` for inspection.
- Recent Coach diagnostics expose resolved analysis mode, whether it came from the current turn, native candidate/fact counts and focus kind. These fields are observation-only and must never affect routing or engine policy.

## Update 196 - provider repair policy

Invalid grounded board prose must not automatically cause a second LLM request. If native evidence already supports a safe fallback, return that fallback immediately. Automatic Coach turns are one-provider-call maximum. The single repair pass remains available only for manual requests without a deterministic native fallback and without native authority violations.

## Update 197 - engine / evidence / cache optimization

- The complete-root `worst_move` / `fastest_loss` search remains the only authority for extreme-move questions, but its finished result may be reused process-locally for the exact FEN, engine settings and exact extreme mode. Ordinary top-N hint evidence is never accepted as proof of an extreme.
- Coach engine caches publish immutable candidate snapshots after the engine call and never hold their mutex while Stockfish runs.
- Provider-only candidate compaction trims the duplicated grounded fact payload in lockstep with retained candidates/PVs. Full native evidence remains untouched and authoritative.

## Update 198 - Grounded Coach Architecture cleanup

Updates 187-197 form one cumulative grounded-coach contract. Explicit current user intent owns routing; `PositionAnalysisMode` selects native analysis semantics; Stockfish/native code owns legal moves, extreme ranking and structured chess facts; Gemini receives opaque candidate/fact references and owns only pedagogical wording/selection; the verified-fact renderer and native validator are the final authority before UI presentation. `coach_response.v7`, `CoachPrompt v9` and `coach.status.v3` are the synchronized production contracts for this series. Flutter remains UI/localization only. No `third_party`, secrets, build products or generated local credentials belong in update archives.


## Chess expert registry

- `experts/chess_expert_registry.*` is the Update 203 canonical capability registry for planner-visible chess intelligence sources. It describes which `EvidenceNeed` values each source can satisfy and whether it requires board, profile, or conversation context; it never executes an engine, model, database lookup, or provider call itself.
- Keep `EvidenceSource` as the stable learned-planner source enum. Runtime callbacks are named `EvidenceSourceHook`; do not reuse the planner enum name for callable adapters.
- New expert categories must be added to `dto/evidence_plan.h` and the registry together. Prefer extending an existing expert's capabilities over creating narrow one-question expert types.
- Later aggregation may consult registry capabilities to select adapters, but source execution remains in the existing service/analysis/profile/opening systems rather than being reimplemented under `native/ai/experts/`.

- Update 204: `ExistingAnalysisExpert` is the canonical coverage gate for persisted/cached position analysis. Existing MultiPV may suppress fresh engine work only when it actually satisfies every engine-backed `EvidenceNeed` requested by the evidence plan. Top-N analysis must never claim complete legal-root coverage or material consequences merely from centipawn scores.

### Update 205 — Stockfish expert boundary
- Stockfish is the objective engine expert behind the EvidencePlan, not a language/intention router. Engine work is scheduled through the existing position-analysis/engine-budget path.
- Reuse existing analysis only when coverage proves the requested engine-backed needs. Exhaustive root questions and `fresh_required` plans may force fresh analysis; a learned `reuse_only` preference cannot suppress hard native move-evaluation obligations.

### Update 206 — Human/Elo bot expert
- `HumanEloBotExpert` reuses the existing KChess bot difficulty curve as a human-likelihood projection over already verified engine candidates.
- Human-model evidence is explicitly probabilistic/practical evidence, never objective chess truth. Engine/existing-analysis evidence remains authoritative for correctness.
- The expert must never start an engine search itself. It consumes candidate evaluations supplied by the evidence pipeline so human modelling cannot duplicate Stockfish work.
- `EvidenceKind::human_model` is a first-class provider evidence kind. Provider text may describe likely human choices from it, but must not reinterpret those probabilities as best-move evaluations.

### Update 207 — Position/Tactics/Opening/Profile expert adapters
- `experts/context_evidence_experts.*` is the normalization bridge from existing KChess evidence streams into the planner-visible expert pipeline. It must reuse `EvidenceItem` payloads; it does not rescan the board, rerun tactical detection, query databases, or create profile/opening truth.
- `ExpertEvidence` adds source ownership, satisfied `EvidenceNeed` values and an explicit `objective_truth` flag around an existing evidence item. It is aggregation metadata only; the wrapped payload remains authoritative.
- Deterministic board facts may be marked objective only when they are direct board-derived facts. Strategic weakness/plan and tactical-pattern heuristics remain non-objective even when deterministic.
- Opening/profile adapters consume already retrieved `opening`/`theory`/`user_profile` evidence. They must not bypass `CoachService`, `KnowledgeRuntime`, or theory ownership to perform their own persistence lookups.

### Update 208 — Evidence Aggregator
- `experts/evidence_aggregator.*` is the single deterministic merge boundary for already collected `ExpertEvidence`. It never executes experts, starts Stockfish, queries persistence, calls a model/provider, or invents missing facts.
- Update 210 conversation continuity is orthogonal to intent inheritance: every accepted session-backed chess turn may receive the compact previous goal/answer as non-authoritative conversation context, even when the current turn has its own explicit intent. Only a true elliptical `CoachIntent::follow_up` may inherit `context_intent` as its effective intent. Never treat prior assistant prose as board truth.
- Aggregation ranks evidence by requested source/need coverage, confidence and explicit `objective_truth`; objective board/engine truth wins ID conflicts over heuristic/practical evidence. Same-ID duplicates are collapsed before provider context is built.
- Conflicting payloads with the same stable evidence ID are never forwarded together. The aggregator selects one deterministic winner and reports `conflicts_resolved` / `conflicting_ids` for diagnostics so ambiguity cannot silently become contradictory LLM context.
- `satisfied_needs` and `missing_needs` are computed after deduplication/truncation. A planner request is therefore considered covered only by evidence that actually survives into the aggregate packet.
- Optional evidence may supplement requested sources when it satisfies a requested `EvidenceNeed`; it must not be treated as a substitute for absent objective truth merely because its relevance score is high.

### Update 209 - Coach LLM Context v2

Provider requests now carry the compositional `EvidencePlan` alongside the current question, canonical board context, relevant session summary and provider-visible evidence. The plan describes requested information, not chess truth: concrete board claims remain grounded only by supplied native evidence. Provider cache identity includes the evidence plan so semantically different information requests cannot reuse the same validated prose merely because their visible evidence happens to match.

## Update 209 - structured LLM context boundary

The final coach provider receives `coach_llm_context.v2`: current question, request classification, canonical board state, relevant conversation, compositional EvidencePlan, teaching policy and exact provider-visible evidence are serialized as distinct structured sections. Do not flatten these domains back into ambiguous prose. EvidencePlan/request metadata may guide reasoning but cannot support a chess claim; only exact provider-visible native evidence can. Prompt identity advances to `CoachPrompt v10`, so pre-v2 cached prose must not be reused.

- The learned planner predicts compositional `EvidencePlan` sources/needs plus scope/depth/perspective. Predictions below the native confidence gate fail closed to the deterministic QueryPlanner fallback. Missing or invalid model files must never make the Coach unavailable.
- Learned planning may broaden/select evidence retrieval and may replace only planner-overridable analysis guesses. Structural native constraints (`user_move_uci`, explicit compare mode, native hint target, board/profile availability) remain authoritative.
- Planner output is an information request, never chess truth. Engine legality/evaluation and all factual grounding remain native.
- `all_legal_moves` is retained as a distinct evidence scope so exhaustive questions cannot collapse into ordinary top-N candidate analysis.

### Update 212 - simplified grounding boundary

`coach_response.v8` makes native chess facts the sole provider-visible grounding mechanism for current-board truth. Provider-authored typed claims are no longer permitted for moves, checks, mates, evaluations, piece placement or tactical motifs. Keep typed claims only for the temporary opening/profile/move-contrast migration bridge. `CoachPrompt v11` identifies this contract change and invalidates prior exact response-cache entries.

## Update 213 – Evidence/latency optimization

- Evidence aggregation is coverage-first under a hard item budget: reserve the best available item for each requested EvidenceNeed before filling remaining slots by global relevance. Do not regress to plain top-N truncation; it can silently drop the only evidence for a rare need.
- `EvidenceAggregationResult` exposes `coverage_reserved` and `items_dropped_by_budget` for diagnostics only. These counters never alter chess truth.
- Engine depth follows `EvidencePlan.depth` for foreground/critical work. Background/automatic turns must not escalate to a deep engine search solely because richer prose is desired.
- Reuse existing analysis before fresh engine work remains mandatory. Optimization may reduce duplicate/redundant evidence, but must never fabricate missing evidence or weaken grounding.


## Stateful Coach interaction architecture (new series)

- `interaction/semantic_dictionary.*` is the canonical provider-neutral vocabulary for the deterministic interaction planner. Extend this dictionary instead of inventing free-form provider tool names.
- `interaction/action_catalog.*` defines the small set of primitive executable coach actions. Complex user requests are action chains, not new bespoke action implementations.
- `interaction/conversation_state.*` stores compact session references only; never forward or persist the full transcript as runtime state.
- `interaction/follow_up_resolver.*` gets first chance to resolve short follow-ups against explicitly available session actions. Ambiguous turns must defer rather than guess.
### Native include-path hotfix

- `kchess_core` exposes `native/ai` as a private include root because established AI sources include sibling headers as `experts/...`, `interaction/...`, etc.; do not remove this include root unless every AI include is migrated consistently.

### Interaction authority migration
- The coach main path now resolves canonical interaction semantics before `QueryPlanner`. High-confidence interaction actions constrain `QueryPlan::analysis_mode`; legacy routing remains a chess-domain/evidence adapter during migration and is not authoritative for those semantics.

## Interaction answer gate
- Pure board/UI/session action chains (`show_line`, `show_move`, `show_arrows`, `show_candidates`, `play_move`, game start/stop/configuration and native analysis without an explicit language action) must not call the remote Answer LLM.
- `AnswerGate` is authoritative for provider invocation in `CoachOrchestrator`; only explicit language actions such as `answer_with_evidence`, `explain_line`, `explain_move`, or `compare_moves` may pass the gate.
- Deterministic board actions surface their native move payload through `CoachResponse::board_moves`; provider prose is optional and must not be required for action completion.

- Current runtime has no local small-model stack. Routing, query planning and concept retrieval use deterministic native logic; Gemini remains the only Coach LLM provider.


## Cleanup Series 2 - deterministic planner only

The experimental learned Evidence Planner, KCEP artifacts, tiny router/context models, local text encoder and reranker are removed. The native deterministic semantic/evidence pipeline is authoritative. `EvidencePlan` remains a deterministic compositional contract; it is not a model prediction payload. Do not restore model-file fallbacks or parallel planner runtimes.

## LLM Interaction Series 1 - Update 3

Mixed product/language turns no longer share one overloaded intent. `ResolvedInteractionPlan` carries the coarse native `InteractionRequestKind` and a separate `InteractionAnswerIntent`. The product/session action chain is authoritative and its Flutter `clientActions` are emitted even when the same turn requires Gemini prose. Provider input uses `coach_llm_context.v5` with only native-selected session/product actions plus deterministic action parameters; Gemini cannot choose or execute app actions. `CoachPrompt v14` is the exact-cache boundary. For future live-coaching language attached to `start_game`, do not create an immediate current-position/mistake analysis merely because words such as `Fehler`/`mistake` or `erklären`/`explain` are present.

## LLM Interaction Series 1 - Update 4

Action fulfillment is now validated after the final Coach response has assembled native client actions. `ResponseValidator` still owns provider prose/grounding only; `ActionFulfillmentValidator` separately owns product-action transport. A schema-valid Gemini answer cannot make a requested `start_game`/resume/resign action successful. `CoachPipelineTrace` records the native interaction request kind, answer intent, Answer-LLM gate, requested client-bound actions, transported client actions and fulfillment issues from the real orchestration path.


### Automatic move ownership
Automatic feedback must use `CoachRequest::move_attribution` as the native ownership contract. `mover_color` is derived from the authoritative pre-move FEN, `learner_color` comes from game context, and only `mover_role == learner` plus `verified_learner_move` may be treated as the user's completed move. Opponent/unknown moves must never inherit learner-attempt state.


## Coach Answer Quality Series 3 - Chess Verdict Contract

`verdict/chess_verdict_builder.*` is the native authority for coach judgments. It consumes already verified candidate/classification state only; it must not run an engine, parse persuasion from user prose, or let provider/session wording choose a verdict. Existing native move classifications outrank lightweight candidate-delta classification. White-perspective candidate evaluation is converted only for stable position labels and mover loss. Extreme complete-root analyses may bind the selected extreme/result but must not fabricate a normal move-quality category without a best-move comparison.

## Coach Answer Quality Serie 3 - Update 2 native verdict review

`verdict/chess_verdict_review.*` is the only conversational bridge from user disagreement to chess re-evaluation. It may use user prose only to detect that the previous native judgment is being challenged; it must bind the recheck to persisted native verdict/move/FEN state and never derive a chess conclusion from wording. `QueryPlanner` marks the turn fresh-required foreground engine work. A move challenge is comparable only if the fresh verdict actually evaluates the same challenged move; otherwise the result is `inconclusive`.

## Coach Answer Quality Serie 3 - Update 3 Verdict Lock

Provider verdict authority is now fail-closed. `coach_response.v9` requires `verdict_lock`; Gemini's JSON schema fixes its values to the native authoritative verdict/review and `ResponseValidator` compares the returned echo against the same native inputs before the answer is rendered or cached. Never relax this to prompt-only agreement. The provider explains native chess judgments; it does not vote on them. `CoachPrompt v15` is the cache boundary, while `coach_llm_context.v6`, ABI 11 and SQLite schema 44 remain unchanged.
## Coach Answer Quality Serie 3 - Update 4 grounded judgment gate

Evaluative chess questions are fail-closed at the native boundary. `PrimaryInteractionPlanner` marks explicit move/position judgment semantics (`evaluate_move` / `evaluate_position`) across DE/EN/AR, and `CoachOrchestrator` upgrades those plans to foreground engine grounding even when legacy routing would otherwise choose a generic `position_overview`. A move judgment requires an authoritative verdict bound to an evaluated move and non-unknown move verdict; a position judgment requires an authoritative non-unknown position verdict. If that proof is unavailable, Gemini is not called and the response uses the localized `verdict_grounding_unavailable` safe-fallback. Diagnostics expose `groundingRequired`, `groundingAvailable`, and `groundingBlocked`. Never answer "good/bad", "blunder?", or "who stands better?" from provider prose alone.



## Coach Answer Quality Serie 3 / Update 5 — verdict-first answer structure

Authoritative chess-evaluation answers now have a machine-enforced visible order: `verdict_summary` first, then grounded explanation segments, then at most the existing optional follow-up. `coach_response.v10` requires the summary exactly when an authoritative verdict is active; the parser/renderer prepend it to visible text and `ResponseValidator` rejects a missing summary. The provider still cannot choose the judgment: `verdict_lock` remains fail-closed native authority. `CoachPrompt v16` invalidates older cached prose because the presentation contract changed. No build or test command was executed for this update.

## Coach Answer Quality Serie 3 / Update 6 — adversarial verdict regression

`kchess_coach_verdict_regression_tests` is the executable fail-closed boundary for authoritative Coach judgments. Keep adversarial DE/EN/AR move/position evaluation phrases, user-objection rechecks, review outcomes, `verdict_lock`, and mandatory `verdict_summary` assertions here. A user asking whether the Coach now agrees (`gibst du mir jetzt recht`, `agree with me`, `هل توافقني`) is a verdict challenge when a prior authoritative judgment exists; it must reopen the same native move/FEN review rather than invite conversational agreement. A fresh best/worst/extreme-move question remains a new analysis and must not be misclassified as an objection. Provider output that softens/replaces the native verdict, invents a summary without native authority, omits the decisive summary, or claims a different review outcome must fail validation.
