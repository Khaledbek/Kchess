# Coach Interaction Layer Instructions

## Scope

`native/ai/interaction/` owns the compact semantic vocabulary, primitive coach actions, compact conversation state, and deterministic follow-up resolution used by the new stateful coach architecture.

## Rules

- Flutter renders actions and follow-up labels but does not decide their chess/domain meaning.
- Keep semantic IDs stable, compact, lowercase, and provider-neutral. They are machine contracts, not localized UI strings.
- Do not store full chat transcripts in `ConversationState`; store only compact IDs/parameters/references needed for future turns.
- Prefer composition of a small primitive action set over one bespoke function per natural-language intent.
- Deterministic follow-up resolution must be conservative: resolve only when the current session exposes a matching valid follow-up action; otherwise defer to the planner layer.
- Structured follow-up button actions are authoritative and must not be reparsed as natural language.
- No provider calls, engine work, profile queries, or Flutter logic belong in this directory. Those are orchestrated later from resolved actions.

## Planner response whitelist

- Extract only complete identifier-shaped tokens that exactly match `semantic_dictionary.*`; matching may be case-insensitive, but stored IDs must be canonical lowercase.
- Ignore unknown words, invented tool names, explanations, punctuation and duplicate IDs. Never infer a near match or substring match.
- Preserve first-seen canonical token order so later deterministic resolution can remain stable and diagnosable.
- An empty extracted selection is valid input for later fallback policy; do not silently invent a default action in the extractor.
## Deterministic parameter parsing

- `parameter_parser.*` extracts only explicit, low-risk values from the current utterance (for example Elo/rating numbers, requested color, move reference, move number, time control and explicit game mode cues).
- Parameter parsing must not infer chess intent, fabricate defaults, inspect profile data or depend on provider output. Semantic interpretation belongs to the later deterministic resolver.
- Prefer conservative recognition. When a value is ambiguous, leave it unset and let the planner/resolver handle the ambiguity.


## Update series 2 — interaction requirement resolver

- `InteractionRequirementResolver` is the deterministic boundary between untrusted planner tokens and executable KChess behavior.
- It may add mandatory dependencies (for example engine for PV/evaluations, human model for human-move prediction, profile/history for tendencies), but it must never execute arbitrary provider text or unknown IDs.
- Semantic IDs remain compositional; do not create one bespoke runtime function for every wording. Resolve them into the primitive action catalog plus evidence sources/needs.
- Explicit parameters parsed deterministically from the user utterance (Elo, color, move reference, time control, game mode) take precedence over guessed free-form values.
- Planner output is advisory. Technical validity, dependency completion, and session-reference checks are authoritative here.

## Update series 2 — action chain engine

- `action_chain_engine.*` orders already-resolved primitive actions into deterministic execution chains; it does not call providers, engines, UI, or models itself.
- Session configuration (for example rating) must precede game start, game start must precede live coaching, evidence-producing work must precede board/language presentation, and language rendering should remain last.
- Board/game actions must not be forced to wait for an answer LLM when no language response is required.
- Invalid chains must fail closed with diagnostics instead of silently inventing missing session state or actions.

## Update series 2 - follow-up suggestions

- Follow-up suggestions are structured actions, not free-form chat strings.
- A suggestion must carry a canonical `action_id` plus an ARB `label_key`; UI text is localized in Flutter, never hard-coded in native logic.
- Reuse active evidence/line/move references whenever possible so clicking a suggestion can bypass the planner LLM.
- Keep the suggestion list capped (default: 4) and deterministic.

### Active coach turn UI projection

`coach_turn_view.*` is a presentation DTO boundary only. It may expose the latest user/coach turn and at most four structured follow-up actions. It must not contain planner, resolver, evidence-selection, or action-selection logic. Flutter should render the supplied follow-up action IDs/label keys and send the structured action identity back; it must not convert a chip tap into a fresh natural-language planner request.

## Diagnostics and regression contract

- Interaction diagnostics must remain compact and transcript-free. Log identifiers, selected semantic IDs, native actions, evidence requirements, answer-gate result, structured follow-ups and action-chain order; do not dump the full conversation merely for debugging.
- `plannerUsed=false` is expected for deterministic short follow-ups and structured follow-up-chip actions when active session references make the intent unambiguous.
- The answer LLM is gated independently from the planner. Pure board/game/session actions should not pay for prose generation.
- `interaction_regression_cases.*` is the stable provider-neutral corpus for routing regressions. Add previously broken real user flows there before expanding model-specific tests.

## Primary coach semantic path

- `primary_interaction_planner.*` is the high-confidence deterministic front door for current-turn semantics. It emits canonical semantic IDs plus the coarse `InteractionRequestKind` contract (`question`, `analysis`, `app_action`, `conversation_control`).
- `CoachOrchestrator` must resolve `PrimaryInteractionPlanner -> parameter parser -> InteractionRequirementResolver -> ActionChainEngine -> AnswerGate` before legacy evidence planning.
- Legacy `QueryPlanner` is migration-only evidence-budget/retrieval adaptation. It must not overwrite explicit interaction semantics such as best move, worst move, fastest loss, comparison, move evaluation, or direct app/session actions. Direct `app_action`/`conversation_control` turns must suppress positional evidence/engine planning even when a board is present.
- Start/resume/stop-game language is recognized deterministically for the supported app languages (German, English, Arabic). Do not route those requests through a learned/local planner or infer them from a board-state default.

## LLM Interaction Series 1 - Update 2

`ActionChainEngine` now feeds an executable native client-action transport through `CoachResponse`. Game start/resume/resign actions are session mutations and do not require board evidence or an Answer-LLM call. Keep action IDs canonical and bounded; add new product actions to `PrimitiveAction`/`action_catalog` and map them natively rather than encoding commands in prose.

## LLM Interaction Series 1 - Update 3

Mixed turns are represented as orthogonal native contracts: `InteractionRequestKind` owns the product/session class, while `InteractionAnswerIntent` owns the language responsibility. A turn may therefore start/resume/configure a game and still request an acknowledgement/explanation. Never force product actions back through prose merely because a language response is also required. `clientActions` must be produced from the native `ActionChain` before provider execution so provider success/failure cannot suppress an already resolved product action.

`live_coaching` combined with `start_game` is a mixed action request. The deterministic planner adds the live-coaching session mutation plus a bounded language acknowledgement; generic `explain` matching must not reinterpret future in-game coaching as an immediate current-position explanation.

## LLM Interaction Series 1 - Update 4

`action_fulfillment_validator.*` is the single read-only contract for client-bound product-action fulfillment. It validates the final native `CoachResponse::client_actions` against the already resolved deterministic `ResolvedInteractionPlan`/`ActionChain`; provider prose can never satisfy a product action. Start/resume/resign/rating transport is checked including typed Elo where applicable. Provider-response grounding validation remains a separate concern.

The former standalone `interaction_diagnostics.*` path was removed because it was not wired to the production Coach pipeline and labelled planned chain steps as executed work. Runtime interaction/action diagnostics must come from `CoachOrchestrator` traces recorded by `CoachService`, so the inspector reflects the actual provider gate and final client-action transport.

## LLM Interaction Series 1 - Update 5

`kchess_interaction_action_routing_tests` is the executable regression boundary for direct product/session language. Keep German, English and Arabic positive cases plus negative non-action cases here when changing `PrimaryInteractionPlanner`, `ParameterParser`, the resolver, action chain, answer gate or fulfillment validator. The static `interaction_regression_cases.*` corpus remains useful provider-neutral fixture data, but it is not a substitute for executable assertions.

For `start_game`, explicit Elo/rating text is configuration, not a `predict` task. Explicit color is a typed native parameter. `ActionFulfillmentValidator` must reject a transported `open_bot_game` whose color differs from the resolved native plan. Provider prose cannot repair or override either parameter.
