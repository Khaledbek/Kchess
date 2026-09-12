# Native AI Coach Instructions

## Scope

`native/ai/` owns AI Chess Coach runtime orchestration and chess-grounded reasoning. Flutter may display these results but must not reproduce this logic.

## Architecture

- Keep the coach chess-only; `OFF_TOPIC` is rejected by native routing/policy.
- Prefer deterministic KChess facts, cache, theory/opening data and Stockfish before asking an LLM.
- Provider code must remain replaceable; do not couple contracts to one model/vendor.
- Optional small-model assets belong under `third_party/model/`, never inside Flutter; the Coach LLM itself uses Gemini.
- Python is development/training tooling only and must not become an app runtime dependency.
- Keep request/response DTOs transport-oriented. Do not hide engine calls or domain decisions inside DTOs.

## Context discipline

Read this file plus the nearest child `AGENTS.md`. Search existing KChess systems before adding logic so analysis, theory, engine, profile, cache and training behavior are reused rather than duplicated.

## File style

Keep files focused and small. Split components by responsibility as the coach grows, and give non-trivial handwritten files explicit section markers.

## Orchestration

- `coach_orchestrator.*` is the single top-level coach pipeline entry point.
- Keep its flow ordered as route → plan → context → retrieve → analyze → practicality → provider → validate → response.
- Temporary stubs are placeholders only; later updates replace them with focused components instead of adding parallel pipelines.
- The orchestrator coordinates components but does not become the home for domain algorithms.

## Domain routing

- `domain_router.*` is the deterministic baseline for chess-only routing and intent confidence.
- `domain_router_terms.h` contains only compact routing vocabulary; keep domain decisions out of Flutter.
- Treat loaded FEN/PGN as chess context, but do not invent session semantics here; conversation state belongs to the session component.
- Future tiny intent models may augment this baseline, not create a second routing contract.

## Conversation state

- `conversation/coach_session.*` owns compact ephemeral session state and board/topic inheritance for follow-ups.
- A `session_id` alone is not conversation evidence; routing treats a follow-up as contextual only when memory exists for that ID.
- Keep full transcripts out of native session state. Store only the compact fields required to resolve later turns.

## Query planning

- `query_planner.*` converts the routed intent into a small evidence/budget plan; it must not retrieve evidence itself.
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
- The current lexical retriever is a deterministic baseline. Future embedding/model retrieval may augment it, but the catalog IDs and evidence contract remain authoritative.
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
- The Coach LLM is remote Gemini. `third_party/model/` is reserved only for optional small router/embedding/tokenizer assets, not a local Coach GGUF.
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


## Small models, embeddings and final optimization

- `models/small_models.*` defines optional provider-neutral Tiny Intent, Context Planner and Embedding interfaces. The coach must work unchanged when all are absent.
- Tiny intent inference only refines ambiguous routing; strong deterministic routes remain authoritative.
- Tiny context planning may reduce concept count or add safe evidence requests, but it may not create an engine call or remove required grounding.
- `ConceptRetriever` keeps lexical retrieval as the zero-model baseline and invokes embeddings only when the lexical match is weak; concept embeddings are cached by model id/version.
- `optimization/provider_input_optimizer.*` trims only the copy of evidence sent to the language model. Full evidence remains available for native validation.
- `models/model_versions.*` is the central stable version registry for PositionFeatures, ConceptCatalog, Practicality, CoachPrompt, RouterModel, ContextPlannerModel, EmbeddingModel and ChessProfile.
- Offline dataset/training/evaluation/quantization/benchmark tooling belongs under `tools/ai/`; Python remains development-only.
