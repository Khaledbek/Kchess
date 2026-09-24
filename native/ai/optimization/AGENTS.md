# Coach Optimization Instructions

`native/ai/optimization/` may reduce provider input size or avoid unnecessary model work, but it must never remove authoritative validation or change engine truth.

Prefer deterministic cheap gates before local/remote model calls. Provider evidence trimming must keep the original full evidence set available to native validation.

## Profile-aware provider budgets

Provider trimming consumes the native `QueryPlan`. General-chess requests must drop `user_profile`; personal requests prioritize the bounded `profile.context.v3`. Profile JSON is atomic at chunk boundaries—never substring-truncate a chunk. Mixed position+personal questions reserve provider budget for objective position evidence. Comparison scopes must preserve representatives for each requested side or mark the provider-visible profile context `insufficient_evidence`. Native validation retains the full evidence set for chess truth and separately receives the exact trimmed evidence actually shown to the provider.

## Update 135 - sufficient serialized evidence

For personal requests, consume the native packet's bounded `providerTokenBudget` and actual serialized demand instead of silently forcing every packet back to the former 500/900/1400 evidence caps. Evidence remains capped at 4000 estimated tokens and prepared context plus evidence at 4500; fixed provider instructions/response schema are outside that context estimate. Mixed questions reserve space for objective board evidence.

`requiredChunkGroups` is internal selection metadata: every group needs at least one surviving chunk. Preserve relationship endpoints and required topics/comparison cells before optional chunks; recheck every group after trimming. Remove those internal ID groups and budget hints before sending JSON to the provider. Never substring-truncate factual chunks, retain orphan relationships, or copy an original complete verdict after required support was lost.

## Update 159 - exact response reuse

`ValidatedResponseCache` is a bounded process-local optimization after provider-input construction. It stores only content that has passed native response validation. Its exact key includes all provider request fields that affect generation plus the complete native validation evidence, so profile/analysis/teaching/session changes invalidate by key mismatch rather than manual invalidation. It must never cache errors, bypass session bookkeeping, or become persisted knowledge.

## Update 165 - provider-input hot-path optimization

Provider-visible evidence removes only exact duplicate `EvidenceItem` values before priority/budget trimming. The complete native evidence vector remains unchanged for validation, cache-key truth and returned diagnostics. Provider input diagnostics expose item counts plus estimated evidence tokens before/after selection so later tuning can be based on measured reduction rather than hidden trimming.

## Update 166 - prepared exact response-cache key

`ValidatedResponseCache` prepares its full exact key once per provider stage, then reuses it for lookup/store. Do not reintroduce duplicate canonical serialization on a cache miss, and do not replace the exact key with a collision-prone digest just to save memory.

## Update 172 - typed feedback cache identity

The exact provider-response key includes the native verified-learner-feedback flag because it changes the relevant Gemini instruction. Prompt wording changes bump `CoachPrompt` in the existing model-version registry.

## Update 174 - preserve completed-move contrast

`move.contrast.v1` is compact current-turn native evidence and receives high provider-input priority. The full and provider-visible evidence sets remain separate for exact native validation. `CoachPrompt v4` and `coach_response.v3` prevent reuse of answers from before quote-linked claims and contrast instructions.

## Update 175 - segmented-answer cache boundary

`CoachPrompt v5` and `coach_response.v4` exclude earlier unsegmented validated responses from exact cache reuse. Keep `move.contrast.v1` in the bounded provider-visible packet when present, including optional verified shared-analysis scalars; validation must still use only the actual selected packet rather than full retrieved evidence.

## Update 177 - cache identity for response v5

`CoachPrompt v6` + `coach_response.v5` invalidate exact provider-response cache entries created under the former global-claims/quote-link wire contract. Keep both identities in the existing key; no compatibility reuse is allowed across this boundary.

## Update 178 - candidate-reference cache boundary

`CoachPrompt v7` + `coach_response.v6` invalidate exact cached provider responses from before native candidate IDs. `engine.candidates.v1` stays high-priority provider evidence because its candidate IDs now form part of the structured response contract; trimming must not synthesize or rewrite those IDs.

## Update 182 - hardened validated-response identity

`ValidatedResponseCache` keys use the provider cache identity rather than provider id alone and include the native `CoachValidator` contract version in addition to prompt/schema versions. Changing the configured LLM model or validator policy must produce a cache miss; previously validated prose must never cross either boundary. Keep the cache process-local and exact.

## Update 184 - prevent avoidable repair calls at generation time

Provider schemas should encode deterministic native output limits whenever the limit is already known before inference. In particular, recommendation count/reveal constraints and absence of candidate authority should be rejected by the generation schema rather than consuming a second provider repair call. Do not weaken post-provider validation to gain latency.

## Update 185 - provider-visible evidence compaction

- `ProviderInputOptimizer` may compact only the provider-visible copy of evidence after native retrieval and before token-budget selection. Full native evidence remains unchanged and remains authoritative for validation, session scoring, fallbacks and other runtime consumers.
- `engine.candidates.v1` compaction may bound provider-visible PV length and the number of alternative candidates according to requested answer depth/mode/teaching limits. It must preserve native `candidate_id`, root UCI, rank/evaluation fields for retained candidates, the best line, the learner/user line when present and the critical reply metadata. It must never synthesize, reorder or rewrite candidate identity.
- Provider-visible tactical motif arrays may be tail-trimmed to a depth-specific bound. Native tactical evidence remains complete outside the provider packet; claims are still validated only against the exact provider-visible packet actually sent.
- Compaction happens before evidence-budget selection, so saved tokens may allow a second relevant evidence item to fit without raising the total provider budget. Diagnostics expose compacted-item count and estimated token savings; these counters are observation-only and must not change routing or teaching policy.
## Update 186 - optimization cleanup boundary

Provider-call reduction, response caching and provider-visible evidence compaction are latency optimizations only. They may reduce duplicate work or disposable context, but the full native evidence and legality/validation/session contracts remain authoritative. Keep diagnostic counters observational and keep optimized provider packets deterministic for the same request/evidence/version identity.


## Update 189 - focus-preserving compaction

Provider-visible candidate compaction must preserve and PV-trim `engine.candidates.v1.focus` exactly like `best`/`user_move`. It may not rename an extreme focus to `best`, drop its stable candidate ID, or change extreme-candidate ordering.

## Update 191 - move-reference cache boundary

`CoachPrompt v8` invalidates exact cached provider responses that were generated while Gemini could emit raw current-position move notation. Provider-input compaction must preserve candidate/fact IDs used by opaque move references; it must not rewrite those identities.

## Update 192 - response cache boundary v7

`CoachPrompt v9` + `coach_response.v7` define a new exact response-cache namespace. Cached v6 responses use the old provider-authored factual/general classification and must never be reused for v7 grounded fact-reference segments.

## Update 196 - repair budget gate

- A validation failure with a deterministic native safe fallback does not trigger a second provider call. Prefer the native verified fallback immediately; provider repair is not allowed to trade extra latency/quota for presentation prose that native code can safely degrade.
- Automatic Coach turns never use the provider repair pass. Automatic output is opportunistic and must remain one-call maximum; a later board event may request fresh grounded prose.
- Authority-boundary failures remain non-repairable. A single repair pass is reserved only for manual requests that have no deterministic native fallback and did not violate native move/fact authority.
- Diagnostics distinguish `native_fallback_preferred`, `automatic_repair_suppressed`, and `validation_nonrepairable` through the existing fallback-reason field. Do not add retry loops or weaken validation to improve latency.

## Update 197 - grounded evidence compaction

- Provider-visible `engine.candidates.v1` fact arrays are compacted consistently with the retained candidate set: facts for alternatives removed from the disposable provider packet are removed too, and principal-variation fact payloads use the same PV bound as their candidate line.
- Full native candidate/fact evidence remains unchanged for validation, safe fallback and session truth. Compaction may never rewrite a fact ID, candidate ID, root move, evaluation, mate value or analysis focus.
- Repeated explicit extreme questions on the exact same FEN/settings may reuse the dedicated ephemeral native extreme-result cache instead of re-running the complete-root scout. Cache separation preserves `worst_move` versus `fastest_loss` semantics.

## LLM Interaction Series 1 - Update 3 cache boundary

The validated response cache key includes the full provider-visible native interaction contract (`requestKind`, `requestedActions`, `answerIntent`, Elo/color/game-mode/time-control parameters). `CoachPrompt v14` and `coach_llm_context.v5` prevent reuse of prose generated before mixed action/language intent was explicit.


### Attribution-aware cache keys
Validated Coach response-cache keys include the complete native automatic-move attribution contract. `CoachPrompt v14` / `coach_llm_context.v5` invalidate prose cached before mover ownership became explicit.


## Coach Answer Quality Series 3 - Verdict-aware cache identity

Validated response-cache identity includes `context_schema_version` and the complete provider-visible `ChessVerdictContract`. `coach_llm_context.v5` therefore cannot reuse prose generated before native verdict authority existed, and two turns with different native judgments cannot collide even when the natural-language question is identical.
