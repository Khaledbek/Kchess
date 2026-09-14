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
- Engine rank stays authoritative. Rank 1 is always eligible. When engine WDL is available, practical alternatives use root-side expected-score loss (<= 0.08) as the primary objective safety gate; the historical <= 80 cp gate remains the fallback when WDL is unavailable.
- Rating/skill/risk preferences influence fit only among objectively eligible choices. Missing profile signals disable the player-adjusted evidence instead of fabricating a generic player.
- Keep the persistent/learned user-profile implementation in `native/ai/profile/`; it maps into this existing scoring boundary through `PracticalityPlayerContext` rather than duplicating player-fit logic.

## Update 160 - expected-score-aware practicality v2

- Candidate DTOs carry root-side expected score derived from existing engine WDL; no extra engine search is allowed.
- Objective risk/forgiveness and player-aware alternative eligibility prefer expected-score loss when available, because equal centipawn gaps can have very different practical result impact. CP remains the compatibility fallback for engines/results without WDL.
- Practicality v2 does not change engine ranking or allow player preference to promote a result-significantly inferior move.
