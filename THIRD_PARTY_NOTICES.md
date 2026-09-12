# Drittanbieterhinweise

## nlohmann/json 3.12.0

- Zweck: JSON-/NDJSON-Verarbeitung für Providerdaten
- Quelle: `https://github.com/nlohmann/json/releases/tag/v3.12.0`
- lokale Quelle: `third_party/nlohmann/include/nlohmann/json.hpp`
- SHA-256: `aaf127c04cb31c406e5b04a63f1ae89369fccde6d8fa7cdda1ed4f32dfc5de63`
- Lizenz: MIT

## Stockfish 18

- Zweck: lokale Analyse-/Bot-Engine
- Tag/Commit: `sf_18`, `cb3d4ee9b47d0c5aae855b12379378ea1439675c`
- lokale Quelle: `third_party/stockfish/source`
- Lizenz: GNU GPL Version 3
- Netze: `nn-c288c895ea92.nnue`, `nn-37f18f62d772.nnue`

## Stockfish 19

- Zweck: alternativ auswählbare lokale Analyse-Engine
- Tag: `sf_19`
- Source-Ziel: `third_party/stockfish19/source`
- Release-Archiv SHA-256: `519b653d0d1ffb96531d982ccbe5c6a19425e8388e0e3c2f70f34b424ab32d76`
- Netz: `nn-1a298aa575a0.nnue`
- Netz SHA-256: `1a298aa575a085434d29027978dc36867fe9c5bcea9376654b7a8eba1e52dfc2`
- Lizenz: GNU GPL Version 3

Weitere Build-/Compliance-Details für beide Engines stehen in `docs/LICENSE_COMPLIANCE.md`.

## SQLite 3.53.4

- Zweck: eingebettete lokale Datenbank
- Quelle: `https://www.sqlite.org/2026/sqlite-amalgamation-3530400.zip`
- lokale Dateien: `third_party/sqlite/sqlite3.c`, `sqlite3.h`
- Archiv SHA-256: `1e71ddf93849c6a6ecf58b827c0692073d2dd7ee40196158068f7b29f422e87d`
- Lizenzstatus: Public Domain

## Flutter / Dart

- Zweck: Android-/Windows-Präsentation und Dart-FFI
- Flutter-/Dart-Lizenz: BSD 3-Clause
- exakte SDK-/Paketauflösungen ergeben sich aus der lokalen Flutter-Toolchain und `flutter_app/pubspec.lock`

## Lichess Open Database

- Zweck: Quelldaten des offline ausgelieferten KCB1-Opening-Books
- Quelle: `https://database.lichess.org/standard/`
- verwendete Dumps: Januar 2013, Januar 2015, Februar 2015
- Lizenz: CC0 1.0
- Verarbeitung/Prüfsummen: `tools/opening_book/BUILD_METADATA.md`

## Lichess chess-openings

- Zweck: Quelldaten des offline ausgelieferten KCO1-Eröffnungsnamenindex und des eingebetteten Opening-Trainingskatalogs
- Quelle: `https://github.com/lichess-org/chess-openings`
- Upstream-Commit: `4b8622759e7ae6f93f011cc6c83a3823401ab45e`
- Lizenz: CC0 1.0
- Verarbeitung/Prüfsummen: `tools/opening_names/BUILD_METADATA.md`

## Gemeinfreie Endspielstudien

Der eingebettete Studienkatalog weist als Quelle `Chess Studies, Or, Endings of Games` von Josef Kling und Bernhard Horwitz (1851) aus. Die eingebetteten Quelldaten kennzeichnen die Rechte als Public Domain.

## Python nur zur Entwicklung

Diese Komponenten werden nicht mit der App-Runtime ausgeliefert:

- `chess` / python-chess – PGN/SAN/Zugverarbeitung für Builder
- `zstandard` – Streaming-Dekompression für `.zst`-Opening-Dumps

Exakte Pins des Opening-Book-Builders stehen in `tools/opening_book/requirements.txt`.

## Direkte Dart-Pakete

Exakte aufgelöste Versionen stehen in `flutter_app/pubspec.lock`. Aktuell direkt verwendet werden unter anderem:

- `ffi`
- `file_picker`
- `fl_chart`
- `flutter_svg`
- `intl`
- `path`
- `path_provider`

Die jeweiligen Lizenztexte werden zusätzlich über Flutters generierte Anwendungslizenzliste ausgewiesen.
