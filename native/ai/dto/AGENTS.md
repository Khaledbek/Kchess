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

## Update 177 - native-only segment linkage

`CoachAnswerSegment::claim_indices` and `CoachClaim::answer_quote` remain in the provider-neutral DTO for deterministic validation and downstream compatibility, but Gemini no longer authors either field. The provider adapter derives both from an inline factual segment in `coach_response.v5`. Do not re-expose provider-authored numeric claim cross-references in a vendor schema.

## Update 178 - candidate reference metadata

`CandidateMove::candidate_id`, `CoachClaim::candidate_id` and `CoachRecommendation::candidate_id` are native/provider grounding metadata. `move_uci` remains the resolved native chess value consumed by existing response/UI paths. DTO fields do not resolve IDs themselves and must not start chess work.

## Update 190 - structured native chess facts

- `chess_facts.h` defines provider-neutral verified chess truth-units. Every `ChessFact` has a stable `fact_id`; providers may reference that ID but never author or mutate its chess payload.
- Candidate facts use deterministic IDs such as `fact.candidate.best.move`, `fact.candidate.worst.evaluation_cp` and `fact.candidate.best.critical_reply`.
- Concrete move notation, PVs, evaluation values, mate distance, expected score and analysis focus remain native data attached to the fact ID. The DTO performs no chess calculation.
- `engine.candidates.v1` remains the compatibility evidence item; its payload now advertises `coach.chess_facts.v1` and includes a `facts` array. Do not create a parallel engine cache for these facts.

## Update 192 - grounded response segments v7

`CoachAnswerSegment::fact_ids` carries provider-selected references to exact native `coach.chess_facts.v1` truth-units. The provider no longer classifies a segment as factual/general; rhetorical purpose and grounding are separate concerns. Internal `CoachSegmentKind` remains a native compatibility/rendering detail and is derived by the provider adapter from fact/claim references plus rhetorical purpose. Fact IDs never carry authored chess payload and must resolve against exact supplied candidate evidence.

## LLM Interaction Series 1 - Update 2 client actions

`CoachResponse::client_actions` carries bounded machine-only product commands selected by native C++. Current game actions use canonical IDs (`open_bot_game_setup`, `open_bot_game`, `resume_bot_game`, `resign_active_bot_game`) plus optional typed parameters such as Elo. This transport is not evidence and not provider-authored content; provider output must never populate it.

## LLM Interaction Series 1 - Update 3 provider interaction metadata

`LLMProviderRequest::interaction` is provider-visible metadata copied only from the resolved native interaction plan. It is not accepted from Flutter/provider input. Keep product/session action IDs separate from `answerIntent`; a provider language call must not become authority for action execution.

### Deterministic native answer presentation

`CoachResponse::native_answer_kind` is a machine-only presentation hint for successful deterministic answers that intentionally skip the remote provider. Supported deterministic values include `best_move` and `worst_move`. The chess fact remains native in `board_moves`; Flutter may only localize the visible sentence using ARB and must not recompute or infer the move. An empty provider `answer` plus a valid native answer kind is therefore a successful response, not `provider_unavailable`.


## Coach Answer Quality Series 3 - verdict DTO

`chess_verdict.h` is a transport-only native judgment DTO. It contains stable position/move verdict enums, optional native evaluation/loss metadata and provenance. DTO code must not perform engine work or infer verdicts from prose; construction belongs to `ai/verdict/`.
