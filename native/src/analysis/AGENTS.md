# Native Analysis Agent

## Scope

Move-Klassifikation und Accuracy.

## Startdateien

- `move_classifier.cpp/.h`
- `move_classifier_sf19.cpp/.h`
- `accuracy.cpp/.h`

## Token-Regel

Bei Klassifikationsfehlern nur die betroffene Enginevariante + aufgerufene Helfer lesen. Engine-Service erst öffnen, wenn Eingangsdaten/Score-Perspektive unklar sind.

## Regeln

- Zugklassifikation immer relativ zur Stellung **vor** dem Zug und zur besten gegnerischen Antwort bewerten.
- SF18/SF19 dürfen getrennte Kohärenzpfade haben, aber keine Dart-Korrektur benötigen.
- Accuracy bleibt unabhängig von UI-Labels.
- Keine Klassifikationsentscheidung in Flutter spiegeln.

## Classification/Arrow Coherence Series - Update 4/9 (SF19)

- `move_classifier_sf19.*` must treat `played_is_best=true` as authoritative for the published root snapshot. Do not add another post-move contradiction gate inside severity classification.
- Cross-snapshot contradiction belongs to `AnalysisService` as diagnostic evidence only. It must never leave a completed legal move as `unknown/pending`; the current classifier result remains visible and a later/deeper pass may replace it.
- Brilliant may still require independent confirmation. Failure of that confirmation falls back to non-Brilliant/unstable behavior and must never create a Blunder/Mistake.

## Classification/Arrow Coherence Series - Update 5/9 (shared severity)

- SF18 and SF19 share `classify_move_severity(...)` for negative Mistake/Blunder severity. Normalized WDL/expected-score loss is authoritative whenever present; centipawns are fallback-only for legacy/incomplete samples.
- Material is supporting semantic evidence, never a severity multiplier. The service feeds net PV-end material into the common classifier; transient capture swings must not create Mistake/Blunder labels.
- Forced mate transitions remain categorical exceptions. Positive labels (Best/Critical/Brilliant/Excellent/Good/Okay) keep engine-specific calibration and rank semantics.
- A published rank-1 move is still protected by the coherence invariant from Updates 3/4; severity must never override `played_is_best` inside its authoritative snapshot.

## Classification publication contract — current

- Every completed legal move receives a normal classifier category. `unknown`, `pending`, and stability diagnostics are transient analysis states only; they are never the durable result of a completed move.
- Root/played depth differences, early convergence, budget completion, and boundary proximity may be logged as diagnostic context, but they do not suppress a finished label. More depth is allowed to reclassify the move later.
- Theory/forced categories remain categorical. SF18 v18 and SF19 v1910 are the current classifier generations for this publication contract.
- Flutter must only render the native category and must not manufacture, stabilize, or preserve a previous label on its own.

### Classification/Arrow Coherence Series - final regression boundary

SF18 v18 and SF19 v1910 are the current classifier generations. Regression coverage must preserve WDL/expected-score-first negative severity, CP fallback only when normalized outcome evidence is absent, final-PV net material rather than transient material dips, rank-1 immunity from independent after-search punishment, and the rule that every completed legal move has a normal category.

## Best-Move / Reclassification Series — Update 4/6

- SF18 and SF19 are strict Analysis truth namespaces. The selected engine owns its live worker, final best move, WDL/PV evidence, classification/reclassification and reusable game analysis. No completed run may prune or substitute the other Stockfish generation's persisted analysis for the same game.
- `Database::prune_game_analyses_except(...)` is engine-scoped: a newer run supersedes only rows with the same `engine_version`. Switching SF18 -> SF19 -> SF18 therefore preserves each engine's compatible game cache instead of destructively deleting the inactive engine's truth.
- Live engine changes remain fail-safe: `Core::set_engine_id()` validates the candidate before persisting the setting, then `AnalysisService::prepare_for_engine_change()` cancels/joins main-line, refinement and variation work from the previous engine. The next request is created from the newly selected engine only.
- Position-cache and sideline reuse remain isolated by `ChessEngine::cache_identity()` / `position_cache_engine_identity(...)`; the in-memory sideline cache is cleared when its engine changes. Flutter contains no SF18/SF19 reconciliation policy.
- No ABI or SQLite schema migration is introduced by this update. No builds or tests are executed by the assistant.
## Best-Move / Reclassification Series — Update 6/6

- Final `bestmove` authority is not equivalent to mutating the captured MultiPV ordering. Classifier input may use the final move identity together with the exact root snapshot evidence; do not force the line vector into a synthetic rank order.
- Regression expectation remains: completed legal moves are classified, deeper evidence may change the category, and exchange/material features use final net PV material rather than transient capture swings.


## Unified Move Classification Series — Update 1/7

- `MoveAnalysisFacts` is the single engine-neutral facts record consumed by both SF18 and SF19 classification. Do not add a second per-engine facts model for rank, score, mate, material or root move identity.
- Root analysis publishes the played UCI move, authoritative final bestmove UCI, legal/rank context, WDL/expected-score evidence, CP/mate evidence and final-PV material deltas into that record before classification.
- SF19-specific policy may wrap the common facts only for genuinely SF19-only confirmation state; final PV material is common evidence and must not be duplicated in the SF19 wrapper.
- This update intentionally does not change visible categories or thresholds. Tactical material exposure, recovery/compensation, clustering and mate-aware policy are later updates in this series.

## Unified Move Classification Series — Update 2/7

- `root_move_material_exposure_evidence(...)` is the native board-facts owner for immediate profitable material exposure after a played root move. It inspects legal opponent captures after the move and records the strongest conservative concession, including captures of a different own piece than the one that moved.
- Immediate exposure is not yet a Brilliant decision. The facts record distinguishes exposed piece value, conservative immediate net loss, immediate recapturability, exposure of another piece, and whether that victim already occupied the same square before the root move. Deeper recovery/compensation remains Update 3.
- A legal capture is not automatically treated as a material concession: if the capturing piece can be immediately recaptured, only the favorable exchange margin counts. Flutter must not reproduce or reinterpret this board logic.
- The `f5g6` reference FEN `r4rk1/p4p1p/3p1npQ/1PpPpP2/4P3/2n4P/Pq2BP2/2R3RK w - - 0 22` is a regression fixture for an ignored/other-piece exposure. No visible category or threshold changes in this update.

## Unified Move Classification Series — Update 5/7

- Positive move quality is cluster-based rather than fixed-rank-based. Any analyzed alternative inside the Best-equivalence envelope is `best`, regardless of whether Stockfish ranks it 2, 3, 4 or 5.
- `critical` is the native Great label and is rank-1 strict. It requires the second candidate to belong to a meaningfully worse cluster: forced-mate uniqueness, a practical outcome-band drop, or a numeric separation confirmed by both expected-score and CP when both signals exist. The CP fallback boundary is 50cp (0.5 pawn).
- Alternatives outside Best but still inside the Excellent envelope are `excellent` independent of exact top-five rank; Good/Okay are progressively wider positive clusters. Rank is evidence, not the label itself.
- SF18/SF19 classifier generations are v17/v1909. Persisted engine evidence remains reusable; only derived classifications need regeneration.
- The obsolete `only_move_tactical` classifier fact was removed because Great no longer has a separate tactical-only shortcut. Do not reintroduce a second uniqueness path outside the cluster policy.

## Unified Move Classification Series — Update 6/7

- Mate quality is categorical. When best and played moves preserve the same forced-mate result, `mate_distance_loss(...)` compares mate distance directly; saturated WDL/CP values must not flatten M1/M3/M10 into the same label. Up to one mate unit is Best-equivalent, then progressively Excellent/Good/Okay. Losing a previously available forced mate remains `miss`; newly allowing forced mate remains `blunder`.
- Native move classification now has a distinct visible `inaccuracy` category between Okay and Mistake. `miss` is reserved for a concrete missed opportunity (forced mate, material opportunity or meaningful available advantage without self-damage) and is no longer the generic Inaccuracy surrogate.
- Shared negative severity is now four-tiered: none / Inaccuracy / Mistake / Blunder. WDL/expected-score remains authoritative when available; CP remains fallback-only. Current shared boundaries are 0.08 / 0.15 / 0.25 expected-score loss and 180 / 260 / 400cp fallback.
- `inaccuracy` is persisted and transported as a normal native category, included in analysis/profile summaries, Coach/knowledge semantics and Flutter presentation. Flutter does not derive it. The reserved icon asset is `assets/analysis_img/move_inaccuracy.png`; until custom artwork replaces it the repository contains a temporary copy of the existing Miss icon so the asset path is valid.
- Current classifier generations are SF18 v18 and SF19 v1910. Old engine evidence remains reusable; derived labels must be regenerated under the current classifier contract.

## Unified Move Classification Series — Update 7/7 final contract

- Regression coverage must preserve the combined classifier contract: engine-rank/equivalence clusters, Great uniqueness, Brilliant material-concession/compensation, mate-distance precision, distinct Inaccuracy, Miss as missed opportunity, and WDL/expected-score-first negative severity.
- The `f5g6` FEN `r4rk1/p4p1p/3p1npQ/1PpPpP2/4P3/2n4P/Pq2BP2/2R3RK w - - 0 22` remains the canonical ignored-threat Brilliant regression fixture. Its purpose is to ensure a Brilliant move may consciously leave a different own non-pawn piece profitably capturable.
- Mate-distance regressions explicitly cover preserving mate while lengthening it (for example M3 -> M5 and M3 -> M10); these cases must not be flattened by saturated WDL/CP.
- `inaccuracy` is a durable visible category distinct from `miss`. Current classifier generations remain SF18 v18 and SF19 v1910.
- This series ended with static audit only. Do not infer that tests/builds were executed by the assistant.
