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
