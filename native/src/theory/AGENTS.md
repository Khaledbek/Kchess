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
