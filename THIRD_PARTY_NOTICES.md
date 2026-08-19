# Drittanbieterhinweise

## nlohmann/json 3.12.0

- Zweck: robustes Parsen und Normalisieren der nicht vertrauenswürdigen
  Chess.com-JSON- und Lichess-JSON/NDJSON-Antworten
- Quelle/Release: `https://github.com/nlohmann/json/releases/tag/v3.12.0`
- vendorte Datei: `third_party/nlohmann/include/nlohmann/json.hpp`
- SHA-256: `aaf127c04cb31c406e5b04a63f1ae89369fccde6d8fa7cdda1ed4f32dfc5de63`
- Lizenz: MIT; Text in `licenses/nlohmann/LICENSE.MIT`

Die HTTP-Laufzeit fügt keine Bibliotheksabhängigkeit hinzu: Windows verwendet
das Betriebssystem-API WinHTTP, Android die Plattformklasse
`HttpsURLConnection` über JNI.

## Stockfish 18

- Zweck: lokaler Schachmotor, MultiPV/WDL/NNUE
- Quelle: `https://github.com/official-stockfish/Stockfish`
- Tag/Commit: `sf_18`, `cb3d4ee9b47d0c5aae855b12379378ea1439675c`
- Lizenz: GNU GPL Version 3
- vendorte Quelle: `third_party/stockfish/source`
- Lizenz/AUTHORS: `licenses/stockfish/`
- Netze: `nn-c288c895ea92.nnue`, `nn-37f18f62d772.nnue`
- Prüfsummen und Build: `docs/LICENSE_COMPLIANCE.md`

## SQLite 3.53.4

- Zweck: eingebettete lokale Datenbank
- Quelle: `https://www.sqlite.org/2026/sqlite-amalgamation-3530400.zip`
- Dateien: `third_party/sqlite/sqlite3.c`, `sqlite3.h`
- Archiv SHA-256: `1e71ddf93849c6a6ecf58b827c0692073d2dd7ee40196158068f7b29f422e87d`
- Lizenzstatus: Public Domain

## Flutter 3.47.0 / Dart 3.13.0

- Zweck: Android-/Windows-Präsentation
- Quelle: `https://github.com/flutter/flutter`
- Lizenz: BSD 3-Clause

## Lichess Open Database

- Zweck: Quelldaten des offline ausgelieferten KCB1-Opening-Books
- Quelle: `https://database.lichess.org/standard/`
- verwendete Standard-Rated-Dumps: Januar 2013, Januar 2015, Februar 2015
- Dateien: `lichess_db_standard_rated_2013-01.pgn.zst`,
  `lichess_db_standard_rated_2015-01.pgn.zst`,
  `lichess_db_standard_rated_2015-02.pgn.zst`
- Lizenz: CC0 1.0
- Verarbeitung und Prüfsummen: `tools/opening_book/BUILD_METADATA.md`

## Nur zur Entwicklung: Opening-Book-Builder

Diese Komponenten erzeugen das Book und werden nicht mit der App ausgeliefert:

- `chess` 1.11.2 / python-chess – PGN- und Zugverarbeitung – GPL-3.0-or-later
- `zstandard` 0.25.0 – Streaming-Dekompression – BSD-3-Clause

Exakte Pins stehen in `tools/opening_book/requirements.txt`.

## Direkte Dart-Pakete

Exakte Auflösungen stehen in `flutter_app/pubspec.lock`.

- `ffi` 2.2.0 – FFI-Speicher/UTF-8 – BSD 3-Clause
- `path_provider` 2.1.6 – privates App-Datenverzeichnis – BSD 3-Clause
- `intl` 0.20.3 – Lokalisierung – BSD 3-Clause
- `file_picker` 12.0.0 – PGN-Dateiauswahl auf Android/Windows – MIT
- `flutter_svg` 2.3.0 – skalierbare SVG-Schachfiguren aus lokalen Assets – MIT

Neue, von `file_picker` aufgelöste Produktionspakete:

- `android_file_picker` 1.0.1 – Android-Implementierung – MIT
- `windows_file_picker` 1.0.1 – Windows-Implementierung – MIT
- `file_picker_darwin` 1.0.1 – federierte Plattformkomponente – MIT
- `file_picker_linux` 1.0.1 – federierte Plattformkomponente – MIT
- `file_picker_web` 3.0.1 – federierte Plattformkomponente – MIT
- `file_picker_platform_interface` 3.0.1 – Plattformvertrag – MIT
- `ffi_leak_tracker` 0.1.2 – FFI-Ressourcenprüfung – BSD 3-Clause
- `cross_file` 0.3.5+4 – plattformneutrales Dateimodell – BSD 3-Clause

Transitive Flutter-/Dart-Pakete werden in der von Flutter generierten
Anwendungslizenzliste mit ihren jeweiligen Lizenztexten ausgewiesen.
