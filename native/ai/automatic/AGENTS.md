# Automatic Coach Agent

## Scope

`native/ai/automatic/` owns the deterministic gate that decides whether a completed chess event is important enough to spend a Coach/LLM call.

## Rules

- Every verified played move may trigger coaching immediately. Reasons classify special events; a normal move does not need a special reason. No three-second delay.
- Reuse existing KChess classification, WDL-derived expected-score loss, position features and tactical detection. Do not start Stockfish from this folder.
- Current triggers: inaccuracy-equivalent move, mistake, blunder, missed tactic, new high-confidence motif, large WDL shift, game-phase transition and repeated personal mistake.
- KChess currently has no separate `inaccuracy` move category. Automatic coaching may treat an `okay` move with expected-score loss >= 0.065 as the inaccuracy-equivalent trigger without changing the authoritative move classification.
- `repeated_personal_mistake` is an input signal produced natively by `profile/` through the coach profile bridge. Flutter/request JSON must not classify personal repetition itself.
- Trigger reasons are machine IDs, never localized UI strings.
