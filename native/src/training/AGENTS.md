# Native Training Agent

## Scope

Training-Kataloge, Practice-Positionen, Sessions, Lösungen, Versuche und Fortschritt.

## Token-Regel

Katalog-`.inc`-Dateien nicht in Kontext laden. Bei Logikaufgaben C++-Klassen/Services lesen; Dateninhalt nur bei ausdrücklich datenbezogener Aufgabe.

## Routing

- Practice → `practice_*`
- Training-Service/Katalog → `training_service.*`, `training_catalog.*`
- große Daten → `data/` (nur bei Bedarf)

Lösung/Legalität/Progress bleibt nativ; Flutter zeigt nur Zustand und Eingaben.


## Opening graph integration series — Update 5/7

- Opening-practice continuation policy is native and composed from the three immutable opening assets: KCL defines the legal training graph, KCB contributes per-edge game/result statistics and sampling weight, and KCO names the reached destination position.
- KCB never expands the KCL graph: a statistically known move that is not a KCL edge is not a training continuation. Conversely, a valid KCL edge missing from a filtered KCB remains trainable with fallback weight 1.
- KCO/KCB data is attached in `PracticeService`; Flutter receives only resulting session state and must not join opening assets itself.
- The legacy `.inc` catalogue still selects the scenario start position in Update 5. Its complete migration/removal belongs to Update 6; do not create a second catalogue system in parallel.


## KCL line-semantics correction series — Update 2/3

- Practice consumes only `OpeningLineGraph::continuations(fen)`. It must not read KCL records or assume persisted record moves are replies from the record's terminal position.
- KCL1 persists standard-start-to-terminal opening lines; `opening_line_graph.*` derives the actual position -> continuation graph in native C++ and merges transpositions before Practice sees it.
- `openings_*.inc` remains the stable catalogue/id/hierarchy source for existing persisted `opening_<id>` progress. Its setup line may position the board, but it never constrains continuation legality after start; derived KCL topology does.
- KCB only sorts/weights derived KCL continuations, and KCO only labels reached canonical positions. Neither source expands or filters graph legality.
