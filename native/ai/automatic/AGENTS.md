# Automatic Coach Agent

## Scope

`native/ai/automatic/` owns the deterministic gate that decides whether a completed chess event is important enough to spend a Coach/LLM call.

## Rules

- A completed legal move is conversation context, but an unsolicited provider call needs native teaching-criticality >= 0.62. Verified quiz answers bypass this gate for immediate feedback. No timer/debounce is used.
- Reuse existing KChess classification, WDL-derived expected-score loss, position features and tactical detection. Do not start Stockfish from this folder.
- Current triggers: inaccuracy-equivalent move, mistake, blunder, missed tactic, new high-confidence motif, large WDL shift, game-phase transition and repeated personal mistake.
- KChess currently has no separate `inaccuracy` move category. Automatic coaching may treat an `okay` move with expected-score loss >= 0.065 as the inaccuracy-equivalent trigger without changing the authoritative move classification.
- `repeated_personal_mistake` is an input signal produced natively by `profile/` through the coach profile bridge. Flutter/request JSON must not classify personal repetition itself.
- Trigger reasons are machine IDs, never localized UI strings.
- Criticality combines the strongest native reason with small independent motif, phase and repeated-error bonuses. A phase transition alone remains quiet.

## Update 157 - Automatic Coach is background priority

- Automatic Coach remains latest-position-wins inside a session and is additionally preemptible by manual asks or explicit hints at the `CoachService` execution boundary.
- Automatic trigger criticality is unchanged by scheduling priority. Foreground priority must not lower/raise trigger thresholds or create concurrent provider calls.

## Update 161 - teaching-value gate

- `AutomaticCoachDecision::teaching_value` is the final unsolicited-call gate. Preserve the existing >= 0.62 threshold and the existing objective reasons; do not weaken engine/classification truth to manufacture a teaching event.
- `objective_importance` comes from the existing reasons. `personal_relevance` is limited to already-native repeated-personal-mistake evidence, and `practice_relevance` is bounded scheduling context supplied from the existing spaced-repetition store.
- `interruption_cost` is process-local recency policy for prior successful unsolicited turns. It may defer weaker events but must not suppress verified quiz-answer feedback. No timer/debounce thread is allowed.
