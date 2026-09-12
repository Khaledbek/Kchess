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
