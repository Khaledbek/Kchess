# Coach LLM Provider Agent

## Scope

`native/ai/providers/` owns only the replaceable language-model boundary for the AI Chess Coach.

## Rules

- `LLMProvider` is the single provider contract used by the coach orchestrator.
- Keep chess facts, routing, evidence selection, practicality and validation outside providers.
- `RemoteProvider` adapts an external transport callback. Do not hard-code API keys or secrets here.
- `coach_provider.*` builds the one remote provider selected in `config/coach_provider.json`. `provider_config.*` loads its settings, reads the key from the ignored `secrets/<provider>_api_key.txt` and owns the local quota guard in `secrets/<provider>_usage.json`.
- `provider_prompt.*` is the single provider-neutral Coach instruction/input text and `provider_transport.*` the single quota-guarded HTTPS path. Wire adapters (`gemini_provider.cpp`, `claude_provider.cpp`, `openai_compatible_provider.cpp`, declared in `provider_adapters.h`) only serialize that prompt and parse the vendor reply.
- `"provider": "auto"` is resolved only in `provider_config.cpp`: the first id in `"autoOrder"` with a real key file wins. There is no runtime fallback to another provider after a failed call.
- Built-in provider ids are `gemini`, `claude`, `deepseek` and `openai`. Another vendor needs a config block with an explicit `api` (`openai_chat_completions` or `anthropic_messages`), https `endpoint` and `model`; do not add vendor branches outside the adapters.
- Adapters report unparseable/truncated output as `<provider>_response_invalid` so the orchestrator's safe fallback stays provider-neutral.
- The Coach language model is remote inference only. Do not add or select a local GGUF fallback for the Coach.
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

- `coach_response_json.h` serializes/parses typed claims, recommendations and the optional trainer question for every provider; never reduce provider output back to answer-only text. Claude receives the same schema with closed objects; JSON-mode providers receive it in the instruction.
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
