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

## Structured coach response

- `structured_coach_response.h` owns provider-neutral claims, concept references, recommendations and evidence references.
- Keep natural-language text separate from machine references. `evidence_ids` must point at `EvidenceItem.id`; Update 39 validates those references and chess claims.
- `CoachAnswerType` is derived from `CoachMode` by native code, not guessed by Flutter or a provider.

## Validation metadata

- `CoachClaimKind` makes factual claims machine-checkable. For move/check/mate claims `subject` is UCI; for `material_cp`, `subject` is `white`, `black` or `balance` and `value` is integer centipawns; for `piece_on_square`, `subject` is an algebraic square and `value` is a FEN piece token or `empty`.
- `tactical_motif`, `engine_evaluation` and `opening_fact` claims must cite matching evidence kinds.
- `CoachResponse::validation_*` fields report trust-boundary outcome only; Flutter must not reproduce validation logic.
