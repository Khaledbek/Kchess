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

## Update 152 - month-scoped Games UI

The normal online Games UI must not bootstrap through `CoreGateway.games()` / the full library. `CoreGateway.initialGames()` receives the native-selected display month plus only that month's summaries: current UTC month when it has games, otherwise the newest earlier month with games, or the current month with an empty list when the library has no games. Month selection/fallback and SQLite scoping stay native. Saved library/favorite surfaces may still use their existing broader scopes deliberately.
