# Shipped Opening Book – Build Metadata

The bundled `flutter_app/assets/opening_book.kcb` was generated exclusively
from official Lichess Open Database standard-rated dumps. Lichess database
exports are made available under CC0 1.0.

## Source selection

The machine had 257.17 GiB free before download. Current individual monthly
dumps on the official index were roughly 28–33 GB each, so this bounded build
uses the existing January 2013 baseline plus January and February 2015. It
raises the corpus from 121,332 to 3,114,122 games without an uncontrolled
multi-dump download. The three compressed inputs total 586,630,352 bytes.

The Zstandard frames do not declare an expanded content size. They were
decompressed directly into the streaming parser and no unpacked PGN copy was
created. The builder is prepared for later months through repeatable `--input`
and deterministic `--input-dir` processing.

## Sources

| Period | Dump | Compressed bytes | SHA-256 |
|---|---|---:|---|
| January 2013 | `lichess_db_standard_rated_2013-01.pgn.zst` | 17,761,302 | `aa40b3671fa3cf1072eb182892cd90b0e1e003a4a5943492f64b77e7f3fd1635` |
| January 2015 | `lichess_db_standard_rated_2015-01.pgn.zst` | 285,559,460 | `9061f9dc0ab0d8710886d1745b5ca83a27b61fe5e504f9dcd549218fa3598f57` |
| February 2015 | `lichess_db_standard_rated_2015-02.pgn.zst` | 283,309,590 | `b69ebf6419fe284adbfcB973bfa6f823c4a59723e794eaabe71de03bdced5c70` |

Each dump URL is the official
`https://database.lichess.org/standard/<dump-name>` location. Source and
license embedded in KCB1 are `lichess` and `CC0-1.0`.

## Reproducible parameters

```powershell
python tools/opening_book/build_book.py `
  --input lichess_db_standard_rated_2013-01.pgn.zst `
  --input lichess_db_standard_rated_2015-01.pgn.zst `
  --input lichess_db_standard_rated_2015-02.pgn.zst `
  --output flutter_app/assets/opening_book.kcb `
  --max-ply 20 `
  --min-games 100 `
  --timestamp 1786950941 `
  --source-dump lichess-standard-2013-01_2015-01_2015-02 `
  --sqlite-cache-mb 128
```

- retrieved/built: 2026-08-17
- builder: `kcb-builder-2`
- format: KCB1 / version 1
- build timestamp: `1786950941` (`2026-08-17T07:15:41Z`)
- production filters: `max_ply=20`, `min_games=100`

## Result

- input files / files processed: 3 / 3
- games processed / accepted: 3,114,122 / 3,114,122
- invalid games: 0
- positions seen: 60,047,547
- unique positions: 21,645,194
- unique position/move pairs before filtering: 25,773,024
- accepted positions: 13,832
- accepted moves / KCB entries: 27,022
- moves discarded by `min_games`: 25,746,002
- output size: 756,776 bytes
- output SHA-256: `0a1a6849bef494fc267a3bc9e67dcaf37f8f7e8acb47def0e7bc5940365e2163`
- observed peak process RSS: 194.2 MiB
- observed build time: 4,514.06 seconds (75 minutes 14.06 seconds)

The compact result is 730,772 bytes larger than the previous 26,004-byte
book. The runtime ships neither the PGN dumps, Python, the temporary SQLite
aggregation database nor builder dependencies.

## Packaged artifact sizes

| Artifact | Before Phase 3.1 | After Phase 3.1 | Difference |
|---|---:|---:|---:|
| `opening_book.kcb` | 26,004 bytes | 756,776 bytes | +730,772 bytes |
| Android ARM64 debug APK | 320,124,586 bytes | 320,124,586 bytes | 0 bytes |
| Windows x64 debug directory | 254,691,585 bytes | 255,422,357 bytes | +730,772 bytes |

The freshly rebuilt APK entry is 756,776 bytes uncompressed and 407,782 bytes
compressed. Its SHA-256 and the Windows asset SHA-256 both match the source
book. APK and Windows debug builds completed successfully on 2026-08-17.
