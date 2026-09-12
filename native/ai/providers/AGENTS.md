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
- The common schema version is `coach_response.v2`; vendor-specific JSON parsing belongs inside the concrete runtime/transport adapter.
- Providers may structure claims/recommendations but must reference supplied `EvidenceItem.id` values rather than inventing factual authority.

## Validation repair

- `gemini_response_json.h` serializes/parses typed claims, recommendations and the optional trainer question; never reduce Gemini output back to answer-only text.
- The trainer question is conversation content, not factual authority. Prompt instructions require its factual premises to be included in checked claims too.

- `LLMProviderRequest::repair_candidate` and `validation_feedback` are provider-neutral repair inputs. An adapter may serialize them however its runtime requires.
- A normal call has no repair candidate. The orchestrator may issue one additional call after validation failure; providers must not start their own repair loops.
- `coach_response.v2` includes machine-typed claim metadata required by native validation.

## Gemini free-tier guard

- Use `gemini-3.5-flash-lite` in `freeTierOnly` mode unless the user deliberately changes the architecture.
- Do not enable paid Gemini tools, search grounding, batch, caching or automatic retry loops.
- Enforce the rolling request guard before each call and treat HTTP 429 as a terminal quota response for that call.
- Never log or return the API key.
