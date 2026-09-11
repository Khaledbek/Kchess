# Games / Import UI Agent

## Start hier

- Liste: `presentation/games_screen.dart`
- Filter: `presentation/games_filters.dart`
- Import: `presentation/import_dialogs.dart`
- Empty State: `presentation/games_empty_state.dart`

## Domain-Grenze

PGN/FEN/SAN-Parsing, Game-Library-Abfragen, Profilzuordnung und Persistenz bleiben nativ. Flutter sammelt Eingaben und stellt Resultate dar.

## Native bei Bedarf

`native/src/services/game_library_service.*`, Schachparser unter `native/src/chess/`, exaktes FFI-Symbol unter `native/src/api/`.

Keine eigene PGN-/FEN-Validierung als zweite Domainquelle in Dart hinzufügen.
