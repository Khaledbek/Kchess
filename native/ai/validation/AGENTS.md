# AI Coach Validation Instructions

## Scope

`native/ai/validation/` owns the final deterministic trust boundary between provider output and a `CoachResponse`.

## Rules

- Validate structured facts; never infer correctness from natural-language prose.
- Reuse authoritative native chess helpers for legality, check/mate and board state. Do not reimplement move legality here.
- Missing/unknown evidence IDs and missing structured claims do not invalidate ordinary non-profile prose; personal profile assertions are governed by the stricter grounding contract below.
- Engine-value claims require an exact value for the same move in provider-visible native candidates. Tactical motifs require a matching high-confidence native motif; opening facts require an exact native opening/theory field.
- A concrete recommended move must be legal in the supplied position and occur among provider-visible native best/alternative candidates. Unsupported structured board claims and arrows are rejected.
- The validator may request at most one provider repair pass. Invalid provider content is never exposed as a valid coach answer.
- Keep validation provider-neutral and independent from Flutter.

## Repaired metadata fallback

After the single repair pass, any remaining invalid structured claim or recommendation rejects the whole provider answer. Metadata-only sanitization is not a safe fallback because the same unsupported assertion may remain in free prose. Free prose without structured claims is still model-generated and cannot be independently proven by this validator.

The orchestrator additionally enforces the quiz response shape: a native provider-visible best candidate must exist before the provider call; an accepted quiz must provide a follow-up question and no explicit recommendation. This protects the move-choice task without converting provider prose into native chess truth.

## Personal-profile grounding

Personal assertions are a stricter contract than optional visualization metadata. `profile_fact` must resolve an exact scalar from a provider-visible `profile.context.v3` chunk and copy its `epistemicStatus`. `profile_inference` is explicitly non-native: it must cite at least two exact provider-visible profile scalar subjects, carry no invented scalar value, and remain marked `inference`/`hypothesis`. If a profile request has missing/incomplete provider-visible scope, `profile_status=insufficient_evidence` is mandatory and personal claims are forbidden. Chess moves/facts use native board legality plus the exact provider-visible evidence set for structured engine/opening/motif support; personal grounding uses exact provider-visible profile chunks. A repaired answer with failed personal grounding must never survive metadata sanitization.

## Update 135 - post-selection evidence authority

The upstream packet and provider optimizer now preserve required chunk groups (topics, comparison cells, relationships and specialized evidence). These internal IDs are removed before provider delivery. Validation still uses only the exact provider-visible `profile.context.v3` and must never resurrect omitted/stale facts from the full native graph. `supportConfidence` and related routing-quality fields must not be confused with domain confidence or personal skill values.

## Update 173 - coordinate move mentions in prose

When a current board is present, reject UCI coordinate-move tokens in answer/follow-up prose unless that move was supplied in candidate/PV evidence or appears as a successfully validated structured legal/check/mate claim. The lexical gate is intentionally narrow; it cannot prove SAN, ordinary prose or personal inferences and must never replace the existing structured validation/repair boundary.

## Update 174 - quote-linked claims and static contrast facts

The production provider path requires a non-general typed claim to carry a short exact `answer_quote` found in the answer or follow-up question. Validate the quote and source independently; keep the legacy validator API's quote requirement optional for existing local callers. `position_contrast_fact` can assert only an exact scalar under the allowlisted `move.contrast.v1` paths that the provider actually saw. This is still not complete semantic proof of all prose; do not claim it is.

## Update 175 - segmented production validation

Production Gemini responses require joined answer/follow-up segments. Each factual segment must link exactly one non-general claim whose `answer_quote` equals its full text; every non-general claim must be linked. General/uncertainty/dialogue segments cannot carry claim indices, coordinate moves or numbers in a personal/position question. Continue to validate claim semantics against the exact provider-visible evidence; segment shape alone is not a semantic entailment proof. Contrast outcome claims can cite `verifiedAnalysis` scalars only when that object was supplied.

## Update 178 - candidate ID validation

When a structured claim/recommendation carries `candidate_id`, validate the ID/UCI pair against the exact provider-visible `engine.candidates.v1` payload before applying ordinary legality/evaluation checks. The production Gemini adapter always supplies this metadata for move-bound output; move-only validation remains only for compatible native/local callers.

## Update 192 - fact-reference segment validation

Production segment validation no longer rejects prose because the provider labelled it general/nonfactual. A grounded segment is established natively by at least one supplied `fact_id` or one valid typed claim. Every `fact_id` must occur in the exact provider-visible `engine.candidates.v1` facts array. The old `nonfactual_segment_contains_concrete_claim` / `factual_segment_claim_required` classification path is retired for v7; semantic chess authority remains native and the coordinate-move trust boundary remains enforced independently.

## Update 195 - validation failure hand-off

Validation remains the trust boundary. When structured/provider prose fails it, orchestration may discard that prose and hand only already-native candidate/fact state to a safe fallback. This is not validation repair and must not convert an extreme-analysis focus into a recommendation. Renderer authority failures are likewise non-authoritative provider output and may expose only native fallback state.
