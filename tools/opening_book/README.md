# KChess opening-book builder

This development-only Python tool streams standard PGN games from the
[Lichess Open Database](https://database.lichess.org/) into `KCB1`. Lichess
database exports are CC0. Python and its packages are not shipped in the app.

The input stream keeps only one PGN game in memory. Aggregation is performed in
a temporary on-disk SQLite database, so the full dump and all raw positions are
never loaded into RAM. The output is a compact sorted binary file used by the
C++ runtime.

```powershell
python -m pip install -r tools/opening_book/requirements.txt
python tools/opening_book/build_book.py `
  --input lichess_db_standard_rated_2013-01.pgn.zst `
  --input lichess_db_standard_rated_2015-01.pgn.zst `
  --input lichess_db_standard_rated_2015-02.pgn.zst `
  --output flutter_app/assets/opening_book.kcb `
  --max-ply 20 `
  --min-games 100
```

`--input` is repeatable. Alternatively, `--input-dir` adds every direct child
ending in `.pgn` or `.pgn.zst` in deterministic filename order; both forms can
be combined. The same resolved path is rejected when supplied twice instead of
being silently double-counted. Different files are intentionally treated as
independent sources and their optional SHA-256 values belong in build metadata.

All inputs share one disk-backed aggregation. Counts for the same
`positionKey + move` are summed across every file and `min_games` is applied
only once, after the final input. Plain `.pgn` and streaming `.pgn.zst` inputs
are supported. `--min-rating`,
`--min-speed`, `--include-speeds`, `--exclude-speeds` and `--max-games` can
reduce a build. `--max-rating` is accepted as an explicitly reserved V1 option
and prints a warning. Use `--source-dump` when the source filename alone is not
enough provenance. `--timestamp` makes fixture builds reproducible.

The builder uses a lightweight PGN visitor: it constructs only the configured
opening plies, skips variations and scans the remainder to the next game. A
small row batch is written to temporary SQLite; neither a compressed nor an
expanded dump is loaded into RAM. The temporary database uses a bounded,
configurable `--sqlite-cache-mb` page cache (default 128 MiB). It prints files,
games, positions, unique
positions and moves, accepted/discarded moves, peak process RSS, output size
and elapsed time. `--progress-games 0` disables periodic progress.

See `book_format.md` for the portable format and key contract.
`BUILD_METADATA.md` records the exact CC0 dumps, checksums, parameters and
output statistics for the book shipped by KChess.

Run tests from the repository root:

```powershell
python -m unittest discover -s tools/opening_book/tests -v
```

Development dependencies:

- `chess` 1.11.2 / python-chess, GPL-3.0-or-later: legal PGN streaming and moves.
- `zstandard` 0.25.0, BSD-3-Clause: streaming `.zst` decompression.
