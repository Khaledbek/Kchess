# Favorites UI Agent

## Start hier

- Collections: `presentation/favorites_screen.dart`
- Collection-Inhalt: `presentation/favorite_collection_screen.dart`
- Dialoge: `presentation/favorite_dialogs.dart`

## Regeln

Favorites und die oberste Sammlung `Downloads` verwenden native Persistenz. Collection-/Game-Zuordnung nicht in Flutter nachmodellieren.

Bei Datenproblemen zuerst die genaue FFI-Methode verfolgen, danach `native/src/services/game_library_service.*` und nur bei Persistenzbedarf `native/src/persistence/` öffnen.
