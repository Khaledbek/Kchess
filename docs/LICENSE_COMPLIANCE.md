# Release- und Lizenz-Compliance

## Phase-4-JSON und HTTP

nlohmann/json 3.12.0 wird unverändert als offizieller Single Header unter der
permissiven MIT-Lizenz eingebettet. Der lokale SHA-256 stimmt mit dem
Upstream-Release-Asset `json.hpp` überein. Lizenztext und Herkunft liegen in
`licenses/nlohmann/` und `third_party/nlohmann/README.md`.

WinHTTP ist Bestandteil von Windows. Android `HttpsURLConnection`/JNI ist
Bestandteil der Android-Plattform. Daher entsteht aus der HTTP-Schicht keine
weitere zu verteilende C/C++-Runtime oder Lizenzdatei. Zertifikatsprüfung bleibt
jeweils bei der Plattform-Voreinstellung.

## Stockfish 18

- Upstream: `https://github.com/official-stockfish/Stockfish`
- Release/Tag: `Stockfish 18` / `sf_18`
- exakter Commit: `cb3d4ee9b47d0c5aae855b12379378ea1439675c`
- vendorte Quelle: `third_party/stockfish/source`
- lokale Änderungen am Upstream-Quellbaum: eine auf Windows begrenzte
  Host-Integrationsänderung in `src/nnue/network.cpp`: NNUE-Dateien werden über
  einen UTF-8-fähigen `std::filesystem::path` geöffnet; ein Ladefehler wirft im
  eingebetteten DLL-Betrieb einen kontrollierbaren Fehler, statt den gesamten
  Flutter-Prozess mit `exit()` zu beenden. Such-, Bewertungs- und Spiellogik
  bleiben unverändert.
- Lizenz: GNU GPL Version 3

Die App linkt Stockfish direkt in `kchess_core`. Sie behauptet daher keine
lizenzrechtliche Trennung durch UCI oder FFI. Für diese kombinierte Distribution
liegt die GPLv3 in `LICENSE` und `licenses/stockfish/Copying.txt`; AUTHORS und
Upstream-Hinweise bleiben erhalten. Eine proprietäre Veröffentlichung ist ein
gesondertes Rechtsprüfungs-Gate. Dies ist keine Rechtsberatung.

## NNUE

Stockfish 18 referenziert in `src/evaluate.h` diese offiziellen Netze:

| Datei | Größe | SHA-256 |
|---|---:|---|
| `nn-c288c895ea92.nnue` | 108919594 | `c288c895ea924429ea9092e3f36b2b3c1f00f2a3a4c759ff7e57e79e3b43e4a7` |
| `nn-37f18f62d772.nnue` | 3519630 | `37f18f62d772f3107e1d6aaca3898c130c3c86f2ab63e6555fbbca20635a899d` |

Quelle: `https://tests.stockfishchess.org/api/nn/<dateiname>`.

Unter Windows werden beide unveränderten Dateien neben `kchess_core.dll`
installiert. Beim Android/ELF-Build bindet Stockfish sie über seine offizielle
Assembler-`incbin`-Strecke in `libkchess_core.so` ein.

## Reproduzierbare Build-Methode

Windows x64, Release:

```powershell
cmake -S native -B build/native -A x64 -DKCHESS_WITH_STOCKFISH=ON
cmake --build build/native --config Release
```

MSVC nutzt C++20 sowie die Release-Flags des Generators (`/O2 /Ob2 /DNDEBUG`)
und für Stockfish zusätzlich `/W3 /EHsc /bigobj`.

Android ARM64, Debug:

```powershell
cd flutter_app
flutter build apk --debug --target-platform android-arm64
```

Gradle/NDK ruft `native/CMakeLists.txt` mit C++20 auf. Der Stockfish-Teil nutzt
`-O3 -fno-exceptions -fno-rtti` und den Assembler-Include-Pfad für NNUE. Die
Gradle-Konfiguration filtert auf `arm64-v8a`.

## Corresponding Source

Der vollständige, zur Binary passende Quellstand besteht aus diesem gesamten
KChess-Arbeitsbaum einschließlich:

- `third_party/stockfish/source` am oben genannten Commit,
- beiden NNUE-Dateien,
- `native/CMakeLists.txt` und allen KChess-C++/Flutter-Quellen,
- Gradle-/Windows-Builddateien,
- GPLv3, AUTHORS und diesen reproduzierbaren Schritten.

Eine veröffentlichte Binary muss genau diesen Stand oder ein gleichwertiges
GPLv3-konformes Angebot des vollständigen entsprechenden Quellcodes begleiten.

## Opening Theory

KChess liefert ein vollständig offline genutztes KCB1-Opening-Book aus:

- Quelle: Lichess Open Database, `https://database.lichess.org/`
- Lizenz der Datenbank-Dumps: CC0 1.0
- verwendete Standard-Rated-Dumps:
  `lichess_db_standard_rated_2013-01.pgn.zst`,
  `lichess_db_standard_rated_2015-01.pgn.zst` und
  `lichess_db_standard_rated_2015-02.pgn.zst`
- Dump-Zeiträume: Januar 2013, Januar 2015 und Februar 2015
- gesamte komprimierte Quellgröße: 586.630.352 Byte; Einzelgrößen und
  SHA-256-Prüfsummen stehen in `tools/opening_book/BUILD_METADATA.md`
- Builder/Format: `kcb-builder-2` / KCB1 Version 1
- Buildzeitpunkt: `2026-08-17T07:15:41Z`
- Parameter: `max_ply=20`, `min_games=100`
- Ergebnis: 3.114.122 Partien, 27.022 Entries, 756.776 Byte, SHA-256
  `0a1a6849bef494fc267a3bc9e67dcaf37f8f7e8acb47def0e7bc5940365e2163`

Der KCB-Header enthält `source=lichess`, `license=CC0-1.0`, Dump-Kennung,
Builder-Version und Zeitstempel. Vollständige Buildstatistiken stehen in
`tools/opening_book/BUILD_METADATA.md`. Es findet keine Online-Abfrage zur
Laufzeit statt und es werden keine Lichess-Marken in der UI verwendet.

Zusätzlich liefert KChess einen vollständig offline genutzten
KCO1-Eröffnungsnamen-Index aus, der Partien einen ECO-Code und einen
Eröffnungs-/Variantennamen zuordnet:

- Quelle: `lichess-org/chess-openings`, Upstream-Commit
  `4b8622759e7ae6f93f011cc6c83a3823401ab45e` (2026-08-04)
- Lizenz des Katalogs: CC0 1.0
- verwendete Dateien: `a.tsv`, `b.tsv`, `c.tsv`, `d.tsv`, `e.tsv` (SHA-256 in
  `tools/opening_names/BUILD_METADATA.md`)
- Builder/Format: `kco-builder-1` / KCO1 Version 1
- Ergebnis: 3.810 Einträge, 228.888 Byte, SHA-256
  `b4207c778ce0e37d34a3242e1936935c9242b9d6712c5f0d6f1d23704114c1bc`

Der KCO-Header enthält `source=lichess`, `license=CC0-1.0`, Builder-Version und
Zeitstempel. Der Positionsschlüssel ist identisch zu dem des KCB-Books; es
findet keine Online-Abfrage zur Laufzeit statt und es werden keine
Lichess-Marken in der UI verwendet.

Nur für das Development-Tool werden `chess` 1.11.2 (GPL-3.0-or-later) und
`zstandard` 0.25.0 (BSD-3-Clause) benötigt. Weder Python noch diese Pakete
werden in Android- oder Windows-Binaries eingebettet.
