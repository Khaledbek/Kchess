# Shipped Opening-Name Index – Build Metadata

The bundled `flutter_app/assets/opening_names.kco` was generated exclusively
from the official [Lichess `chess-openings`](https://github.com/lichess-org/chess-openings)
catalog, which is published under CC0 1.0.

## Source

- Repository: `lichess-org/chess-openings`
- Upstream commit: `4b8622759e7ae6f93f011cc6c83a3823401ab45e`
- Commit date: `2026-08-04T02:17:01Z`
- Retrieved: 2026-08-19 from `https://raw.githubusercontent.com/lichess-org/chess-openings/master/<file>`
- License embedded in `KCO1`: source `lichess`, license `CC0-1.0`

| File | SHA-256 |
|---|---|
| `a.tsv` | `41722fa3d44f294357326fe2ca1b956d9e56490b30efcfa68db61114c9df7e10` |
| `b.tsv` | `310f0997d5a26ac6c9abfabac028e47e78f24356a6ba322cfffbf8f5a3f88d25` |
| `c.tsv` | `b2e64f32e42e6418b327d03a55af65f3a18e762f7cbc0efffc7e9d1ed3aa7343` |
| `d.tsv` | `58cad40b886bd499717eabcce281d4bfcf00eeadbdc00552f42042cf4aac50d2` |
| `e.tsv` | `f1f8494f488f660e284f23527d5acfbeccdbbc3acc76e74f05d125f39d2f8a74` |

## Reproducible parameters

```powershell
python tools/opening_names/build_names.py `
  --input a.tsv --input b.tsv --input c.tsv --input d.tsv --input e.tsv `
  --output flutter_app/assets/opening_names.kco `
  --source lichess --license CC0-1.0 `
  --timestamp 1785809821
```

- format: `KCO1` / version 1
- builder: `kco-builder-1`
- build timestamp: `1785809821` (`2026-08-04T02:17:01Z`, matching the source commit)

## Result

- input files: 5
- named lines read: 3,810
- unique position entries: 3,810
- transposition collisions merged: 0
- maximum named-line ply: 36
- output size: 228,888 bytes
