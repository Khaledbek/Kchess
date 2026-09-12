# Coach Practicality Instructions

## Scope

`native/ai/practicality/` owns objective practical-difficulty assessment for engine-grounded coach candidates.

## Rules

- Keep this layer native and provider-neutral. It may interpret existing candidate, position-feature and tactical evidence but must not run Stockfish itself.
- Update 35 scores are player-independent. Elo, personal strengths, style and risk tolerance are applied only by the later player-aware layer.
- Preserve raw metrics alongside aggregate `difficulty`, `risk`, `forgiveness` and `clarity`; downstream coaching must be able to explain why a score changed.
- Missing source data is unknown, not zero. Prefer optional metrics and lower confidence over invented certainty.
- Candidate ranking remains objective engine truth. Practicality may describe difficulty/risk but must not promote an objectively inferior move over a better one.
- Keep visible language out of this subsystem; it emits machine evidence only.

## Player-aware overlay

- `player_practicality.*` consumes objective `PracticalityAssessment`, candidate lines and a compact typed player context; it does not call engines or read persistence directly.
- Engine rank stays authoritative. A practical alternative is eligible only when its known evaluation loss is at most 80 cp; rank 1 is always eligible.
- Rating/skill/risk preferences influence fit only among objectively eligible choices. Missing profile signals disable the player-adjusted evidence instead of fabricating a generic player.
- Keep the persistent/learned user-profile implementation in `native/ai/profile/`; it maps into this existing scoring boundary through `PracticalityPlayerContext` rather than duplicating player-fit logic.
