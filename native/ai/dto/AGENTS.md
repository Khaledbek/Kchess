# AI Coach DTO Instructions

## Scope

`native/ai/dto/` contains lightweight data contracts only.

## Rules

- No engine calls, routing, validation, retrieval or chess calculations here.
- Prefer standard-library value types and explicit defaults.
- Keep DTOs provider-neutral and suitable for later C-ABI/JSON adaptation without exposing provider-specific payload types.
- Extend existing contracts when possible instead of creating parallel versions.

## Evidence identity

- `EvidenceItem.id` is a stable machine reference for grounding/validation. Do not use translated labels, timestamps or provider-generated prose as IDs.

## Candidate move contracts

- `candidate_moves.h` contains only engine-neutral input/output value types; Stockfish classes and provider payloads must not leak into DTOs.
- `CoachRequest::user_move_uci` is optional transport context for comparing the user's played/proposed move with engine candidates; legality/evaluation remains native domain work outside the DTO.
- `CoachRequest::user_move_error_confirmed` is a native service-owned flag after completed-move classification/WDL verification. Do not accept this flag directly from client JSON or derive it in Flutter.
- `CoachRequest::practice_progress` is an internal native transport map for same-skill quiz pacing and spaced-repetition scheduling. The LLM adapter must expose only the chosen teaching target, not raw counters/schedule fields as unsourced profile truth.

## Structured coach response

- `structured_coach_response.h` owns provider-neutral claims, concept references, recommendations and evidence references.
- Keep natural-language text separate from machine references. `evidence_ids` must point at `EvidenceItem.id`; Update 39 validates those references and chess claims.
- `CoachAnswerType` is derived from `CoachMode` by native code, not guessed by Flutter or a provider.

## Validation metadata

- `CoachClaimKind` makes factual claims machine-checkable. For move/check/mate claims `subject` is UCI; for `material_cp`, `subject` is `white`, `black` or `balance` and `value` is integer centipawns; for `piece_on_square`, `subject` is an algebraic square and `value` is a FEN piece token or `empty`.
- `tactical_motif`, `engine_evaluation` and `opening_fact` claims must cite matching evidence kinds.
- `CoachResponse::validation_*` fields report trust-boundary outcome only; Flutter must not reproduce validation logic.
- `CoachResponse::provider_error_code` is the provider-neutral transport field for quota/network/unavailable failures. Services may map it to UI status, but DTOs must not interpret vendor policy.
- `CoachRequest::automatic_turn` and `LLMProviderRequest::automatic_turn` are transport priority flags only; they must not change chess reasoning or profile retrieval semantics.


## Fix Update 168 - safe fallback DTO

`CoachResponse::safe_fallback_kind` is machine-only transport metadata for a native-approved quiz/hint fallback after rejected provider output. It is not visible prose and must never carry a move, player claim or provider text. `CoachRequest::personal_training_position_selected` is internal native state only and is never parsed from client JSON.

## Update 174 - quote and contrast transport

`CoachClaim.answer_quote` carries a short exact answer/question excerpt, not a second natural-language answer. `position_contrast_fact` references one scalar in provider-visible `move.contrast.v1`; append the enum value for wire-order compatibility. `EvidenceKind::move_contrast` is provider-neutral native evidence, not a new engine cache or score.

## Update 175 - answer segment DTO

`StructuredCoachContent` carries ordered `answer_segments` and `follow_up_segments` with machine-only `claim_indices`. Production native validation joins these into visible text and requires each factual segment to link one matching non-general claim. Keep the existing flat answer fields for downstream UI/ABI compatibility; do not create a second response-rendering policy in Flutter.
