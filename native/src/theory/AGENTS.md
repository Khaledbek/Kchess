# Theory / Opening Agent

## Scope

Opening Book, Opening Name Index und Positionsschlüssel.

## Routing

- Theory Lookup → `opening_theory_provider.*`
- Opening Name → `opening_name_index.*`
- Positionsidentität → `position_key.*`
- Builder/Format → `tools/opening_book/` bzw. `tools/opening_names/`

## Regeln

Binärformat-Kompatibilität und Positionsschlüssel stabil halten. Generated Daten nicht manuell patchen, wenn der Builder die Quelle der Wahrheit ist.

## Update 109 Knowledge-Graph position identity

`native/src/knowledge/opening_graph_projector.*` reuses `stockfish_position_key(...)` as the canonical opening-position identity. Do not introduce a second Zobrist/canonical-FEN implementation in Knowledge Graph code; transposition convergence depends on this exact shared identity contract.

## Opening graph integration series — Update 2/7

- `opening_line_graph.*` owns strict native parsing/validation of the immutable `KCL1` training-line graph. KCL is a third opening asset beside KCB statistics and KCO names; do not merge these responsibilities or change KCB/KCO formats to carry graph data.
- KCL1 layout currently accepted by runtime: 96-byte header, 20-byte sorted terminal-position records, and a packed 2-byte move table using move-encoding version 1. A record's persisted offset is a **byte offset** into that move table and its count is the number of plies in the complete standard-start-to-terminal line; the range is not an outgoing-edge list.
- `KclOpeningLineGraph` replays every stored root-to-terminal line once at load time, validates that its final `stockfish_position_key(...)` equals the record key, and derives an in-memory position adjacency graph by merging shared prefixes and transpositions. Illegal stored moves or terminal-key mismatches fail the KCL load.
- `OpeningLineGraph::node_index(...)` and `continuations(fen)` are the single native graph-navigation API. They query the **derived** graph through the canonical Stockfish key; Training/Flutter must never reinterpret persisted KCL record ranges as outgoing moves.
- Transpositions are position identity, never path identity: distinct legal move orders that produce the same canonical position key converge to the same derived runtime node.

## Opening graph integration series — Update 3/7

- KCL and KCO use the same canonical `stockfish_position_key(...)` contract and both readers compute an ordered 64-bit fingerprint for diagnostics. KCL topology must remain available when optional KCO coverage/fingerprint differs; only naming metadata may degrade.
- KCL move-encoding versioning now reuses `kBookMoveEncodingVersion` from `position_key.*`. Every KCL edge must round-trip through the same `decode_book_move(...)` / `encode_book_move(...)` codec already used by KCB; do not add a graph-specific move codec.
- The fingerprint is a compatibility proof between independently built immutable assets, not a replacement position identity and not a persisted gameplay key.


## Opening graph integration series — Update 5/7

- KCL, KCB and KCO remain separate immutable responsibilities. Training composes them at query time: KCL topology first, KCB statistics second, KCO destination naming third. Do not change KCB/KCO binary formats to embed graph metadata.
- A broader/rebuilt KCB is automatically consumed through the existing `OpeningTheoryProvider::lookup(fen, uci)` contract; no month-window or source-filter assumption belongs in runtime code.
