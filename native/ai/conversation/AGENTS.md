# Coach Conversation Instructions

## Scope

`native/ai/conversation/` owns compact runtime conversation state for the AI Chess Coach.

## Rules

- Keep session state small and chess-specific; never store full chat transcripts here.
- Resolve follow-up board/topic context here before routing or planning.
- Session IDs identify ephemeral in-memory state; persistence belongs to a later explicit storage integration.
- Do not perform engine analysis, provider calls or UI work in this folder.
- Preserve the last known board only when the current request does not provide a newer FEN.
- Bound fields and the number of retained sessions; remember only accepted, validated, non-empty answers.
- On a board change discard old board claims/recommendations. Preserve an open trainer question with its original FEN as conversation context, never as current-board evidence.
- A validated quiz may retain its original engine candidate, acceptable near-equal alternatives and opponent reply. Only a legal UCI move applied to that original FEN and matching the new FEN becomes a scored learner attempt. Clear pending scoring after one attempt; taps and illegal moves never count.
- The original best candidate and its opponent reply may explain only an exact best-candidate attempt. A near-equal alternative is a success but must not inherit the best line's opponent reply. Neither establishes a current-board evaluation. Persisted per-motif counters belong to native Database, not this ephemeral session.
- If the player makes a legal move outside the few saved candidates, provide feedback without grading it. Count a negative attempt only when the native completed-move analysis explicitly confirms a mistake/blunder/miss or a large WDL loss. The request's confirmation flag is service-owned, never parsed from external JSON.

## Personal follow-up scope

- Remember the accepted turn's `QueryPlan` family/profile scope so short follow-ups such as "woher weißt du das?" can retrieve evidence for the same personal topic.
- Never inherit session scope across different `profile_id` values.
- Session memory stores only routing/scope metadata and accepted conversational summaries; it is not profile evidence.

## Authority boundary

Previous accepted answers may guide conversational continuity, but they are never factual evidence for the current board or player profile. Provider context labels prior answer text as `previous_answer_not_authoritative_evidence`; only newly retrieved native evidence may ground a later factual/personal claim.

## Update 155 - session skill identity

A pending validated quiz retains the native `TeachingPlan::skill_id` for exactly that attempt. When the played move is natively verified, the learning callback records that same skill ID; the session must not recompute a different skill after the board changes. Historical coarse motif keys remain compatibility priors only.


## Fix Update 168 - fallback quiz session state

A native-safe `quiz_question` fallback may establish the same pending candidate-scoring state as a validated provider quiz, but it stores no synthetic localized prose in native session memory. The expected move/opponent reply still come only from the native candidate set.

## Fix Update 169 - authoritative quiz-attempt grading

A pending quiz answer may be accepted as a successful attempt when completed native move analysis classifies the played move as `theory`, `forced`, `brilliant`, `critical`, `best` or `excellent`, even if that move was outside the small candidate set retained when the question was asked. `legal_alternative_not_graded` remains strictly neutral and must not be treated as hidden negative feedback. Native classification is service-owned and is never parsed from external Coach JSON.

## Fix Update 171 - open Automatic board questions

A validated Automatic-Coach `followUpQuestion` with native candidates may retain the original board and candidate set as an ephemeral pending move question. The next legal move from that FEN can be compared for conversational feedback, but this question is not a scored learner exercise unless the originating request was explicit `CoachMode::quiz`. Keep scored-vs-unscored state in this existing session object; do not create another training store.

## Update 180 - pending trainer-question state

A scored quiz question remains the single authoritative open exercise for its original FEN until a legal learner move resolves it. Reissuing quiz mode on that same board must not replace the expected move/candidate set or create another provider-generated question. Hint/explain requests may continue the lesson, but only the original pending quiz owns scoring.


## Update 194 - hint continuity

A hint continues the existing scored quiz rather than replacing it. Preserve the original question FEN, expected move, acceptable alternatives, skill ID and scoring flag across hint turns. Hint level is ephemeral board-local session state and resets when the board changes or a new quiz is established.

## Update 210: explicit-question continuity

A session-backed explicit chess question may consume the immediately preceding compact goal/answer even when the router assigns a new concrete current intent. Conversation continuity and intent inheritance are separate concepts: only elliptical `follow_up` turns inherit the prior intent; explicit turns keep their own intent. Previous answers remain labelled non-authoritative and can never ground current-board claims.


## Update 214 - bounded conversation continuity

Keep at most eight compact recent dialogue entries per Coach session. Store manual user turns and accepted manual assistant replies, but mark prior assistant prose as non-authoritative and never use it as chess evidence. Do not append automatic trainer narration to this buffer; automatic turns would otherwise drown out the user's actual conversational thread. Explicit current questions keep their own intent while still receiving relevant session context; only genuinely elliptical follow-ups may inherit prior intent.

## Coach Series 2 Update 4 - restart continuity

`CoachSessionMemory` remains compact and in-memory, but it can now be restored from and snapshotted to a persistence-neutral `CoachSessionState`. The AI layer still does not own SQLite or the durable transcript. `CoachService` restores the compact snapshot before a resumed turn and persists the new snapshot after orchestration. Full user-visible messages live in the persistence layer and must never be injected wholesale into routing, planning, evidence, or provider context.
