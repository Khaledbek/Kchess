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
