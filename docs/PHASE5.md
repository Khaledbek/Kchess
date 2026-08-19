# Phase 5 – Library and offline management

Phase 5 builds on the Phase-4 provider cache instead of introducing a second
library database.

## Implemented behavior

- Favorites are profile-scoped and now work for local PGN/FEN entries as well
  as provider games.
- Downloads remain a separate profile-scoped state. Removing a download flag
  does not remove favorites or analysis data.
- The Downloads and Favorites library views span every locally known month;
  they are no longer restricted by the month selected on the normal Games
  screen.
- Local PGN/FEN entries can be deleted explicitly. Foreign-key cascades remove
  the moves, analysis rows and local flags belonging to that local entry.
- A provider month cache can be cleared explicitly. Cache validators and
  disposable synced games from that month are removed, while downloaded,
  favorited or analysed games are retained.
- Clearing provider cache never starts Stockfish and never changes classifier,
  accuracy or opening-book versions.

## Native API additions

- `kc_delete_local_game(handle, game_id)`
- `kc_clear_cached_month(handle, profile_id, YYYY-MM)`

No schema migration is required; Phase 5 uses the existing `downloads`,
`favorites`, `provider_month_cache`, `games` and `analysis_runs` tables.

## Validation in this source bundle

The focused persistence test was compiled against the system SQLite library and
passed 18 assertions, including cache cleanup preservation/pruning and local FEN
deletion. Full Flutter/Android/Windows builds cannot be reproduced from the
trimmed source upload because the vendored Flutter/native toolchain and
third-party build trees are intentionally not present in this archive.
