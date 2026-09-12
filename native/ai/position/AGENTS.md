# Position Intelligence Agent

## Scope

`native/ai/position/` owns deterministic board-derived coach features and feature-based weakness interpretation. It may use the native chess core and Stockfish board representation, but it must not run engine search or generate natural-language coaching.

## Rules

- Keep features factual or explicitly heuristic; never present a proxy as engine evaluation.
- Reuse native FEN/chess validation and Stockfish position state instead of adding another parser.
- Position Features I covers material, development proxy, mobility/control, space, activity, open/semi-open files and coordination.
- Position Features II covers pawn structure, weak squares, outposts, king safety, passed pawns, bad pieces and color complexes.
- Keep square outputs as native 0..63 indices and do not convert them into localized/UI strings here.
- Flutter may display resulting evidence but must not recalculate position intelligence.

## Weakness Analyzer

- `position_weakness_facts.*` extracts compact board facts that are needed to interpret weaknesses and are not already present in Position Features I/II. These remain candidates/facts, not final coaching claims.
- `weakness_analyzer.*` consumes the aggregate `PositionFeatures`; it must not parse FEN or perform a second board scan.
- The analyzer may classify weak king, backward/isolated/doubled pawns, weak squares/color complexes, loose pieces, bad bishops/knights, unprotected pawns, weak back rank, overloaded defenders, lack of space and development deficit.
- Heuristic weaknesses must carry confidence/severity instead of pretending to be engine truth.
- Keep exploitation advice out of this component; Update 26 owns how detected weaknesses can be attacked.

## Exploitation Planner

- `exploitation_planner.*` converts detected opponent weaknesses into structured attack methods; it consumes `PositionWeaknesses` and must not rescan the board or run engine search.
- Plans are move-neutral strategy evidence. Do not invent concrete legal moves here; candidate moves belong to the later candidate-move/engine layer.
- `by_white` means plans White can use against Black weaknesses; `by_black` means plans Black can use against White weaknesses.
- Keep weakness severity/confidence visible in plan priority instead of presenting heuristics as forced chess truth.
- General strategic plan generation remains a separate later component; this file only answers how an already detected weakness can be exploited.

## Tactical Detector I

- `tactical_detector.*` owns deterministic current-position detection for fork, royal fork, double attack and double check.
- `tactical_line_motifs.*` owns only sliding-ray geometry for pin, skewer and discovered-attack setups; keep this geometry out of the orchestrator.
- Motifs describe the current board, not an engine recommendation and not proof that a tactic wins material. Preserve confidence on heuristic setups.
- Use native square indices and Stockfish position state. Do not convert motifs into localized prose here.
- Update 28 extends the same tactical evidence contract; do not create a second detector or evidence stream for later motifs.

## Tactical Detector II

- `tactical_pattern_motifs.*` owns static advanced motifs: X-Ray, Battery, Overloading and Back-Rank pressure.
- `tactical_move_motifs.*` owns legal-move candidates for Deflection, Decoy, Clearance, Clearance Sacrifice, Removal of Defender, Interference, Zwischenzug, Desperado, Greek Gift and Smothered Mate.
- Advanced move motifs may expose `trigger_from` / `trigger_to`; these are deterministic candidate moves, not engine recommendations.
- Motifs that require game-history semantics or a deeper forced line must use conservative confidence instead of claiming certainty. In particular, Zwischenzug and heuristic Deflection remain candidates until later engine/validator stages confirm consequences.
- Keep all tactical motifs in the existing `TacticalAnalysis` / `tactical_motifs` evidence stream. Do not introduce a parallel tactical DTO or provider-specific payload.

## Strategic Plan Generator

- `plan_generator.*` is the single position-feature-to-strategic-plan layer.
- It consumes existing `PositionFeatures`, `PositionWeaknesses` and `PositionExploitationPlans`; it must not parse FEN, rescan the board, run Stockfish search or generate prose.
- Plans are candidate strategic directions such as development, activity, space, king safety, outposts, passed pawns and pressure on detected weaknesses.
- Keep output move-neutral. Concrete legal move candidates are added only by the later Candidate Move System.
- Limit evidence to a small ranked set per side and preserve confidence so heuristic plans are not presented as engine truth.
