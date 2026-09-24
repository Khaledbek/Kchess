# AI Coach Teaching Policy Instructions

## Scope

`native/ai/teaching/` owns the deterministic provider-neutral lesson policy between evidence/practicality and the language-model provider.

## Rules

- KChess chooses the lesson objective, delivery mode, reveal level and output limits natively; the provider only formulates the lesson.
- Teaching policy may consume existing request/session/practice metadata and query-plan intent, but must not start engine work, query SQLite, or invent measured player skill.
- Keep one primary lesson objective per turn. Detailed responses may expose at most two concept references, while ordinary turns focus on one.
- Quiz and early-hint policy must not reveal a concrete move through recommendations.
- Fine-grained skills and spaced repetition extend this same planner/scheduler path; do not create a second teaching scheduler.

## Update 156 - native Coach spaced repetition

- `ai_coach_skill_progress` remains the single learner-practice persistence store; schema migration 40 extends that same row with verified-weak counters plus streak/interval/due scheduling metadata. No parallel training table or Flutter-owned scheduler is allowed.
- `native/ai/teaching/spaced_repetition_scheduler.*` owns interval and due-policy. Scheduling metadata expresses when to revisit a verified exercise, never a measured player rating or proof of mastery.
- Only natively verified quiz attempts update the schedule. An independent success advances the interval; a natively verified weak move resets the streak and schedules a near-term revisit. Ungraded legal alternatives still do not become attempts.
- Historical coarse motif rows remain compatible cold-start priors. New namespaced skill IDs share the same table and scheduling contract.


## Update 164 - own-game practice selector

`personal_training_selector.*` owns deterministic selection among persisted profile example positions. It may use profile weakness/priority fields plus `PracticeProgress` due metadata, but it never reads persistence, starts analysis or measures skill. The selected position is still analyzed/retrieved through the ordinary Coach pipeline and scored through the existing verified quiz path.

## Update 167 - final teaching ownership

`TeachingPlanner` chooses the lesson objective/delivery/reveal contract, `skill_taxonomy` resolves the stable practice skill, `SpacedRepetitionScheduler` owns due policy, and `personal_training_selector` chooses a real persisted example position. These are complementary stages, not interchangeable sources of player truth. Only natively verified attempts mutate persisted practice progress.

## Update 173 - feedback and bounded exploration

When a native session carries a completed learner-attempt status, `TeachingPlanner` prioritizes specific feedback before another lesson for ordinary/automatic turns. `personal_training_selector` may add a bounded UCB-style exercise-priority adjustment only with at least two eligible skills each having three graded attempts and at least 24 such attempts in total. This uses existing verified practice rows and is not a learned strength or outcome-gain model; the original profile weakness, confidence, priority and due terms remain primary.

## Update 174 - evidence-driven feedback focus

After a verified learner attempt, `TeachingPlanner` selects `evidence_contrast` only when the native move-contrast packet survived into this turn; otherwise it keeps ordinary attempt feedback. This is a content focus, not a fixed sentence template or new objective score.

## Update 187 – Explicit current-question authority

A completed learner attempt may shape teaching feedback only on the completed-move turn itself (`user_move_uci` or native success/error confirmation). A later explicit user question must not inherit stale `last_attempt_status` as its teaching objective, even when the same Coach session remains active.

## Update 194 - grounded teaching / hint ladder integration

- An explicit native `PositionAnalysisMode` owns the teaching objective for ordinary user questions; a stale motif/lesson may not relabel `worst_move`, `fastest_loss`, `avoid_trade`, threat, what-if or candidate-comparison requests.
- Hint progression is deterministic and board-local: level 0 orientation, level 1 source piece, level 2 target square, level 3 idea/motif, level 4 verified move reveal. The provider receives the native level but does not decide progression.
- The final reveal may expose only a native verified candidate. Earlier levels keep `max_recommendations == 0`.
