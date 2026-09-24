# Automatic Coach Agent

## Scope

`native/ai/automatic/` owns the deterministic gate that decides whether a completed chess event is important enough to spend a Coach/LLM call.

## Rules

- A completed legal move is conversation context, but an unsolicited provider call needs a primary native teaching reason plus the recency-aware minimum teaching value. Verified quiz answers bypass this gate for immediate feedback. No timer/debounce thread is used.
- Reuse existing KChess classification, WDL-derived expected-score loss, position features and tactical detection. Do not start Stockfish from this folder.
- Current triggers: native inaccuracy, mistake, blunder, missed tactic, new high-confidence motif, large WDL shift, game-phase transition and repeated personal mistake.
- `inaccuracy` is now a separate authoritative native move category. It maps directly to the low-priority automatic-coach inaccuracy reason; `okay` is no longer reinterpreted as an inaccuracy surrogate. The inaccuracy reason alone remains below the unsolicited-call threshold.
- `repeated_personal_mistake` is an input signal produced natively by `profile/` through the coach profile bridge. Flutter/request JSON must not classify personal repetition itself.
- Trigger reasons are machine IDs, never localized UI strings.
- Criticality combines the strongest native reason with small independent motif, phase and repeated-error bonuses. `new_motif` and `phase_transition` are supporting-only signals and must never trigger an unsolicited provider call by themselves.

## Update 157 - Automatic Coach is background priority

- Automatic Coach remains latest-position-wins inside a session and is additionally preemptible by manual asks or explicit hints at the `CoachService` execution boundary.
- Automatic trigger criticality is unchanged by scheduling priority. Foreground priority must not lower/raise trigger thresholds or create concurrent provider calls.

## Update 161 - teaching-value gate

- `AutomaticCoachDecision::teaching_value` is the final score, but Update Coach-Serie 2/2 adds a primary-reason requirement and a recency-aware minimum: baseline 0.72, stricter immediately after a delivered interruption. Do not weaken engine/classification truth to manufacture a teaching event.
- `objective_importance` comes from the existing reasons. `personal_relevance` is limited to already-native repeated-personal-mistake evidence, and `practice_relevance` is bounded scheduling context supplied from the existing spaced-repetition store.
- `interruption_cost` is process-local recency policy for prior successful unsolicited turns. It may defer weaker events but must not suppress verified quiz-answer feedback. No timer/debounce thread is allowed.

## Coach-Serie 2 / Update 2 - Automatic Coach anti-spam gate

- Successful unsolicited deliveries create a quiet window keyed by Coach session/profile. During the first 30 seconds, ordinary events are hard-blocked; only a blunder, missed tactic, or exceptionally large WDL loss may break the quiet window.
- From 30-75 seconds, only very high-value events clear the elevated gate. From 75-180 seconds a smaller interruption cost remains; after that the normal 0.72 baseline applies.
- Motif/phase signals are supporting context only. An `okay` move with modest loss, a new motif alone, or a phase transition alone must not spend a provider call.
- `triggerGate` diagnostics expose `primaryReasonPresent`, `criticalOverride`, `recencyBlocked`, and `minimumTeachingValue` so skipped automatic turns can be explained without logging prompts or positions.
