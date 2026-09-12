# User Chess Profile AI Agent

## Scope

`native/ai/profile/` owns the compact learned chess profile used by coaching and practicality.

## Rules

- Preferences and measured abilities are separate. Never infer a preference merely because a player is weak/strong at something.
- Unknown fields stay unknown with low/zero confidence; do not fabricate style, Elo, risk tolerance or strengths.
- Learn only from existing KChess chess data supplied as aggregated observations. This folder must not read SQLite or start Stockfish.
- Persisted JSON is versioned through `ChessProfile`; provider/UI code must not define a second profile schema.
- `profile_updater.*` may derive stable machine IDs for strengths, weaknesses and common mistakes, but visible wording belongs to the coach/ARB layer.
- Player-aware practicality consumes a compact mapping from this profile; engine ranking remains authoritative.
- Repeated-personal-mistake detection is conservative and only becomes true after sufficient sample/confidence.
