# Coach LLM Provider Agent

## Scope

`native/ai/providers/` owns only the replaceable language-model boundary for the AI Chess Coach.

## Rules

- `LLMProvider` is the single provider contract used by the coach orchestrator.
- Keep chess facts, routing, evidence selection, practicality and validation outside providers.
- `RemoteProvider` adapts an external transport callback. Do not hard-code API keys or secrets here.
- `gemini_provider.*` is the vendor-specific Gemini transport adapter; it reads the key through `gemini_config.*` from the ignored `secrets/` file and uses the shared native HTTPS client.
- The Coach language-model provider is Gemini remote inference. Do not add or select a local GGUF fallback for the Coach.
- Provider requests consume already bounded `CoachContext` plus structured evidence. Providers must not silently expand the PGN/context budget.
- Provider failures return structured status/error codes; do not fabricate a coach answer on failure.
- Structured provider output remains provider-neutral; native validation owns factual acceptance and repair policy.

## File style

Keep vendor/model-specific adapters in separate files and keep the common interface small. Do not introduce provider-specific conditionals in `coach_orchestrator.cpp`.

## Structured output

- Provider results return `StructuredCoachContent`, never an untyped text blob.
- The current common schema version is `coach_response.v3` (`answer_quote` on typed claims); vendor-specific JSON parsing belongs inside the concrete runtime/transport adapter.
- Providers may structure claims/recommendations but must reference supplied `EvidenceItem.id` values rather than inventing factual authority.

## Validation repair

- `gemini_response_json.h` serializes/parses typed claims, recommendations and the optional trainer question; never reduce Gemini output back to answer-only text.
- The trainer question is conversation content, not factual authority. Prompt instructions require its factual premises to be included in checked claims too.

- `LLMProviderRequest::repair_candidate` and `validation_feedback` are provider-neutral repair inputs. An adapter may serialize them however its runtime requires.
- A normal call has no repair candidate. The orchestrator may issue one additional call after validation failure; providers must not start their own repair loops.
- The v2 contract introduced machine-typed claim metadata; v3 additionally links those claims to exact answer excerpts.

## Gemini free-tier guard

- Use `gemini-3.5-flash-lite` in `freeTierOnly` mode unless the user deliberately changes the architecture.
- Do not enable paid Gemini tools, search grounding, batch or caching.
- Enforce separate RPM, TPM and RPD budgets before every call. RPD follows the current Pacific quota day so the local counter matches Google AI Studio instead of mixing two quota days in a rolling 24-hour window.
- Automatic Coach yields at the configured soft limits. Explicit user questions/follow-ups and the one validation-repair pass may use the reserve up to the hard limits.
- HTTP 429 is terminal for the current call and starts a persisted exponential cooldown, honoring `Retry-After` when present. Do not create an internal retry loop that consumes additional quota.
- `LLMProviderRequest::automatic_turn` exists only for quota priority; provider code must not acquire chess-domain responsibilities.
- Never log or return the API key.

## Personal profile wire contract

Provider requests carry the native query family, `needs_profile`, and classified `ProfileQueryScope`. Gemini may use only provider-visible `profile.context.v3` for measured personal history. Exact personal data is emitted as `profile_fact`; qualitative deductions from multiple supplied facts use `profile_inference`. `profile_status` must declare `not_used`, `grounded`, or `insufficient_evidence`. Provider adapters must not widen native scope, invent graph nodes/source IDs, or treat session prose as profile evidence.

## Update 135 - interpreting Knowledge packets

Use compact lists/tables when several requested time-control/account/color cells are supplied. `latestRecordedGameRating` is a historical game rating with its source account, never a live/FIDE claim. `scorePercent`/error-rate values are fractions; retain exact source scalars in claims even when prose formats percentages. `periodStart` defines the actual recent window. Graph supportConfidence/supportCoverage/sourceFreshness are retrieval metadata, not measured playing ability or proof of improvement. Relationship chunks establish only the recorded connection; uncertainty text is not a substitute for a validated personal claim.

## Update 136 - natural teaching with native anchors

Gemini adapts explanation length and tone to the actual question and evidence, avoiding fixed sentence templates and ungrounded maxims. Quiz prompts use the native teaching target and candidates without revealing the move. A past-question candidate/opponent reply is labelled with its original board and only explains a verified attempt. Typed motif, opening, engine-value and recommendation output must match the provider-visible native evidence exactly; if absent, explain uncertainty instead.

Practice counters in profile packets are graded candidate matches or native-confirmed weak attempts only. They never establish unassisted mastery; legal alternatives outside saved MultiPV candidates can remain ungraded. A quiz must ask its move-choice question in `follow_up_question` without supplying a recommendation, otherwise its response is repaired once or rejected.

## Fix Update 169 - learner-attempt wording

Gemini must interpret the native learner-attempt status literally. `verified_strong_move_played` is positive native confirmation; `legal_alternative_not_graded` is neutral and may not be phrased as an implicit criticism such as "legal, but". Negative move feedback is allowed only for `verified_weak_move_played` and still requires supplied chess evidence.

## Update 172 - request-scoped Gemini instructions

- Build the Gemini instruction from a common chess/grounding contract and only the relevant board, quiz/hint, verified-attempt and personal-profile clauses. Shorter prompts never relax native claim, scope or candidate validation.
- Answer the actual question first, adapt to native response depth and avoid generic maxims or repeated scripts. A follow-up question belongs in `follow_up_question` only when useful.
- Verified learner-attempt wording comes from typed native `has_verified_learner_feedback`, never a search through session prose. The field participates in the exact response-cache key.

## Update 173 - coordinate prose contract

Gemini may mention coordinate moves in prose only when supplied candidate/PV context supports them or it provides a matching structured move claim for native validation. This prompt rule complements, but never replaces, `ResponseValidator`.

## Update 174 - response schema v3 and evidence-based teaching

Every non-general claim now carries `answer_quote`, copied verbatim from a short exact answer/question span. Gemini may use `move.contrast.v1` for specific before/played/candidate differences, but static king-zone/activity counts are proxies and native attempt classification alone decides praise/criticism. Prefer the newly verified learner decision over repeating a prior answer. Keep a natural explanatory voice without fixed stock phrases or invented engine reasons.

## Update 175 - current Gemini response schema v4

Gemini emits ordered answer/follow-up segments; native code assembles the visible strings. Each factual segment has one typed claim whose `answer_quote` equals the complete segment. General/uncertainty/dialogue segments carry no personal metric, concrete board fact or move. `move.contrast.v1.verifiedAnalysis` may justify classification, expected-score loss or a saved reply only when present; static feature deltas alone never justify engine language. The current exact response-cache schema is `coach_response.v4`.

## Update 176 - response contract identity

`kCoachResponseSchemaVersion` in `llm_provider.h` is the single provider-neutral identity for the current structured Coach response contract. Request defaults, diagnostics and later cache/version reporting must reuse that constant instead of duplicating the schema string. Provider adapters still return stable error codes; user-facing classification remains a native service responsibility.

## Update 177 - provider response contract v5

`coach_response.v5` removes provider-authored global claim indices and duplicated `answer_quote` text. Gemini now places typed grounding metadata directly on each factual segment (`claim_kind`, `subject`, `value`, evidence and optional epistemic/support fields). Native parsing derives `CoachClaim.text`, `answer_quote` and the internal `claim_indices`, preserving the downstream validator/DTO boundary while eliminating fragile cross-object bookkeeping from the model output. `profile_status` is request-shaped in the Gemini schema: non-profile requests permit only `not_used`; profile requests permit only `grounded` or `insufficient_evidence`. Prompt identity is `CoachPrompt v6` so exact response-cache entries from the old wire contract cannot be reused.

## Update 178 - Gemini candidate selection contract

`coach_response.v6` never accepts provider-authored recommendation UCI. Recommendations use a schema-enumerated provider-visible `candidate_id`; move-bound claim kinds (`legal_move`, `gives_check`, `gives_mate`, `engine_evaluation`) also select a candidate ID and the adapter derives their UCI subject from exact `engine.candidates.v1` evidence. Repair serialization preserves the same ID contract. `CoachPrompt v7` is the matching prompt identity.

## Update 182 - provider failure classes and cache namespace

- `LLMProviderResult::failure_kind` is provider-neutral control metadata. Orchestration may distinguish unusable structured output from transport/quota failures without parsing Gemini-specific error strings.
- Gemini parsing returns stable detailed malformed-output codes (`gemini_response_*`) for diagnostics, while all of them remain one `output_invalid` failure class at the provider-neutral boundary. No provider parser may fabricate fallback prose.
- `LLMProvider::cache_identity()` must include the configured model identity when model selection can change independently of the provider id. `RemoteProvider` therefore namespaces exact response-cache entries by provider plus model.

## Update 184 - generation-time teaching limits

- Gemini's response schema mirrors the native teaching-plan recommendation limit. Quiz, hint and reveal-level-0 turns expose `maxItems: 0`; other turns expose only the native `max_recommendations` allowance.
- When provider-visible candidate evidence is absent, move-bound claim kinds are removed from the schema instead of letting Gemini invent a candidate reference that will be rejected later.
- These are generation-time optimization guards only. Native validation remains mandatory and authoritative after parsing.

## Update 189 - inverted candidate semantics

Provider input includes the resolved native position-analysis mode. For `worst_move` and `fastest_loss`, `engine.candidates.v1.focus` is intentionally the harmful move selected by native complete-root analysis; it is not `best` and must not be emitted as a recommendation. A forced loss may be stated only when native engine evidence explicitly marks `forcedLossFound=true`; otherwise describe the focus only as the worst evaluated move found.

## Update 191 - provider-authored move notation removed

- Provider-visible `engine.candidates.v1` now replaces native `move_uci`, `critical_reply_uci` and PV notation with opaque `move_ref` / `pv_ref` tokens. The full evidence object remains native-only for validation and rendering.
- Gemini must never author SAN/UCI/coordinate notation for current-position moves. When prose needs a concrete move or native PV, it copies an exact opaque token; native parsing resolves that token after the provider trust boundary.
- Raw coordinate notation in provider-authored segment/recommendation text is rejected as malformed output even when the same move exists in evidence. Repair serialization converts already resolved native moves back to opaque tokens before they are shown to Gemini again.
- `CoachPrompt v8` identifies this trust-boundary change. `coach_response.v6` JSON shape remains unchanged until the dedicated grounded-response-schema update.

## Update 192 - coach_response.v7 grounding contract

- `coach_response.v7` replaces provider-authored factual/general segment kinds with rhetorical `purpose` (`explanation`, `question`, `uncertainty`, `transition`) plus a `fact_ids` array.
- Current-position candidate/rank/evaluation/mate/PV/reply/focus assertions reference exact schema-enumerated IDs from `coach.chess_facts.v1`; Gemini never authors the underlying move/evaluation payload.
- Native parsing derives the internal grounded segment classification from fact IDs or typed legacy grounding metadata. Profile/opening/contrast typed claims remain temporarily supported until their dedicated native-fact migration.
- `CoachPrompt v9` is the matching prompt identity. Exact response-cache entries from v6/v8 must not cross this response-contract boundary.
## Update 193 - deferred move-token resolution

Gemini response parsing validates raw coordinate-move prohibition and token membership but keeps `<<move:...>>` / `<<fact:...>>` opaque through `ResponseValidator`. `VerifiedFactRenderer` is the only production stage allowed to replace those tokens with exact native move/PV notation after validation succeeds. Do not reintroduce provider-adapter token resolution before validation.



## Update 209 - Coach LLM Context v2

Provider input uses one explicit `coach_llm_context.v2` object per turn. Keep the current user question, canonical board fields, relevant conversation/session summary, evidence plan, teaching policy and exact provider-visible evidence in separate typed sections. The evidence plan describes information demand only and is never chess proof. Evidence payloads keep stable IDs/kinds/confidence so the model can reason across sources without treating pipeline prose as authority. `CoachPrompt v10` identifies this provider-context boundary for validated-response cache invalidation.


## Update 212 - coach_response.v8 simplified grounding

- `coach_response.v8` removes provider-authored current-board typed claims from the wire contract. Current-board moves, checks, mates, evaluations, piece placement and tactical motifs must use exact native `coach.chess_facts.v1` `fact_ids`.
- Typed `claim_kind` remains only as a migration bridge for `opening_fact`, `profile_fact`, `profile_inference` and `position_contrast_fact` until those evidence domains have equivalent native fact rendering.
- Do not reintroduce parallel move/evaluation claim metadata. If a required current-board fact is absent, the provider must express uncertainty or omit that concrete assertion.
- `CoachPrompt v11` and `coach_response.v8` form one cache boundary; v7 output must not be reused as v8.


## Update 214 - EvidencePlan v2 provider semantics
- `CoachPrompt v12` exposes deterministic evidence-plan dimensions to the provider context: sources, needs, scope, depth, perspective, freshness, interaction and Elo target. These are planning metadata only, never grounding.
- `freshness` describes reuse/fresh-analysis policy, `interaction` describes the requested reasoning/teaching operation, and `eloTarget` applies only to supplied `human_model` likelihood evidence. The provider must not turn any of them into an unsupported board claim.
- `coach_response.v8` remains the response wire schema; only the input context/prompt semantics changed.

## LLM Interaction Series 1 - Update 3 interaction contract

Provider context is `coach_llm_context.v5`. It adds an `interaction` object containing native `requestKind`, native session/product `requestedActions`, `answerIntent`, and explicit deterministic action parameters when present. These fields describe decisions already made by C++; they are not provider tools and the model must never invent, cancel, reinterpret or execute product actions. Mixed turns may carry both native actions and a language request. `CoachPrompt v14` is the matching exact-cache boundary; the response schema remains `coach_response.v8`.


### Automatic move attribution
`coach_llm_context.v5` adds `conversation.moveAttribution` for automatic feedback. This object identifies mover role/color and learner color and may contain an opaque `playedMoveRef` when provider-visible evidence already owns that move. It is role metadata only: concrete move quality/evaluation language still requires normal evidence grounding. The provider must never call an `opponent` move the learner's move, and must use neutral ownership wording for `unknown`.


## Coach Answer Quality Series 3 - Authoritative chess verdict

`coach_llm_context.v5` adds `board.chessVerdict`. When `authoritative=true`, `positionVerdict` and `moveVerdict` are native judgments, not suggestions. The provider may explain the judgment from supplied fact IDs/evidence but may not soften, reverse, hedge away, or replace it because the user argues for another interpretation. A user objection remains a hypothesis until a later native turn supplies a changed verdict. Unknown verdict fields must remain unknown.

## Coach Answer Quality Serie 3 - Update 2 provider review contract

`coach_llm_context.v6` adds `board.verdictReview`. Its `outcome` (`unchanged`, `changed`, `inconclusive`) is native authority. `unchanged` requires the response to keep the prior judgment and explain why the objection does not overturn it; `changed` requires an explicit correction to the new native verdict; `inconclusive` keeps the previous authoritative judgment as the standing verdict and may only state that the native recheck did not prove a change. Provider prose must never infer a fourth outcome, convert user confidence into chess evidence, or turn an inconclusive review into a neutral compromise.

## Coach Answer Quality Series 3 - Update 3 verdict lock

`coach_response.v9` adds a mandatory `verdict_lock` machine echo. Its schema is generated from the native `ChessVerdictContract` / `ChessVerdictReview`, so the provider can emit only the exact native `positionVerdict`, `moveVerdict` and `reviewOutcome` values selected before the provider call. `ResponseValidator` verifies the echo again before rendering or caching. Provider prose remains explanatory only; a different verdict cannot cross the trust boundary as a technically valid response. `CoachPrompt v15` is the matching exact-cache boundary. `coach_llm_context.v6`, ABI 11 and SQLite schema 44 are unchanged.


## Coach Answer Quality Serie 3 / Update 5 — decisive verdict lead

`coach_response.v10` adds required `verdict_summary`. When an authoritative native `ChessVerdictContract` is active, Gemini must provide one short localized decisive lead sentence before any explanatory segments; when no authoritative verdict exists the field must be empty. Native parsing assembles this summary before `answer_segments`, and `ResponseValidator` rejects missing/oversized summaries or summaries emitted without a native verdict. `verdict_lock` remains the semantic authority, so the summary cannot change the native judgment. `CoachPrompt v16` is the new exact-cache boundary; `coach_llm_context.v6`, ABI 11 and SQLite schema 44 are unchanged.
