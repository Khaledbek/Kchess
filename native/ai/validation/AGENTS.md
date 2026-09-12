# AI Coach Validation Instructions

## Scope

`native/ai/validation/` owns the final deterministic trust boundary between provider output and a `CoachResponse`.

## Rules

- Validate structured facts; never infer correctness from natural-language prose.
- Reuse authoritative native chess helpers for legality, check/mate and board state. Do not reimplement move legality here.
- Missing/unknown evidence IDs and missing structured claims do not invalidate natural prose.
- Reject engine claims when they contradict an available native value for the same move. Missing evidence alone is not a contradiction.
- A concrete recommended move must be legal in the supplied position; missing candidate references alone do not reject it.
- The validator may request at most one provider repair pass. Invalid provider content is never exposed as a valid coach answer.
- Keep validation provider-neutral and independent from Flutter.

## Repaired metadata fallback

After the single repair pass, invalid structured claims/recommendations may be stripped while retaining the repaired natural-language answer. Only metadata that passes native validation may drive board arrows/highlights; do not fail the whole repaired answer solely because optional visualization metadata is still invalid.
