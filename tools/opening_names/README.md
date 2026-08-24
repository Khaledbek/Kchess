# KChess opening-name index builder

This development-only Python tool turns the
[Lichess `chess-openings`](https://github.com/lichess-org/chess-openings)
catalog (CC0) into `KCO1`, a compact binary that maps a canonical position to
the named opening/variation ending at it. The C++ runtime memory-loads it to
label each stored game with an ECO code and an opening name (for example
`D06 Queen's Gambit Declined: Marshall Defense`). Python is not shipped in the
app.

This is separate from `tools/opening_book`, which builds the statistical
move-popularity book (`KCB`). The two share only the Stockfish position key, and
this builder imports that exact key function from `../opening_book/build_book.py`
so the identities are guaranteed to match. See `name_format.md` for the portable
format and `BUILD_METADATA.md` for the exact CC0 source of the shipped index.

## Input

The catalog ships as five tab-separated files `a.tsv`..`e.tsv`, each with an
`eco  name  pgn` header and one named line per row. Download the current files
from the upstream `master` branch, for example:

```powershell
$dir = "chess-openings"
New-Item -ItemType Directory -Force $dir | Out-Null
foreach ($f in "a","b","c","d","e") {
  Invoke-WebRequest "https://raw.githubusercontent.com/lichess-org/chess-openings/master/$f.tsv" -OutFile "$dir/$f.tsv"
}
```

## Build

```powershell
python tools/opening_names/build_names.py `
  --input chess-openings/a.tsv `
  --input chess-openings/b.tsv `
  --input chess-openings/c.tsv `
  --input chess-openings/d.tsv `
  --input chess-openings/e.tsv `
  --output flutter_app/assets/opening_names.kco `
  --timestamp 1785809821
```

`--input` is repeatable and `--input-dir` adds every `.tsv` in a directory in
deterministic filename order; the same resolved path is rejected twice. Each
row's movetext is replayed to its final position and keyed. When two named lines
transpose to the same final position the deeper line wins, breaking ties by
smaller ECO then smaller name, so the output is deterministic. Supplying
`--timestamp` (and the default `--source`/`--license`) makes the whole file
byte-for-byte reproducible.

The builder prints input files, named lines read, unique position entries,
merged transposition collisions, the deepest named-line ply and the output size.

## Test

```powershell
python -m unittest discover -s tools/opening_names/tests -v
```

## Development dependencies

- `chess` 1.11.2 / python-chess, GPL-3.0-or-later: legal SAN replay. Used only at
  build time; not shipped.
