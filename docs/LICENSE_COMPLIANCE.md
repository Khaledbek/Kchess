# Release- und Lizenz-Compliance

## JSON und HTTP

KChess verwendet `nlohmann/json` 3.12.0 unverändert als Single Header unter der MIT-Lizenz. Herkunft und Lizenztext liegen im lokalen Third-Party-/Lizenzbaum.

Die HTTP-Laufzeit verwendet Plattform-APIs:

- Windows: WinHTTP
- Android: `HttpsURLConnection` über JNI

Dadurch wird für die HTTP-Schicht keine zusätzliche native Netzwerk-Runtime ausgeliefert. Die Zertifikatsprüfung bleibt bei den Plattform-Voreinstellungen.

## Stockfish 18

- Upstream: `https://github.com/official-stockfish/Stockfish`
- Release/Tag: `Stockfish 18` / `sf_18`
- exakter Commit des vendorten Stands: `cb3d4ee9b47d0c5aae855b12379378ea1439675c`
- Quelle: `third_party/stockfish/source`
- Lizenz: GNU GPL Version 3

Die lokale Windows-Host-Integration in `src/nnue/network.cpp` betrifft ausschließlich robustes UTF-8-Dateiöffnen und kontrollierbare Ladefehler im eingebetteten DLL-Betrieb; Such-, Bewertungs- und Spiellogik bleiben unverändert.

### Stockfish-18-NNUE

| Datei | Größe | SHA-256 |
|---|---:|---|
| `nn-c288c895ea92.nnue` | 108919594 | `c288c895ea924429ea9092e3f36b2b3c1f00f2a3a4c759ff7e57e79e3b43e4a7` |
| `nn-37f18f62d772.nnue` | 3519630 | `37f18f62d772f3107e1d6aaca3898c130c3c86f2ab63e6555fbbca20635a899d` |

## Stockfish 19

KChess unterstützt zusätzlich Stockfish 19 als auswählbare Runtime-Engine.

- Upstream: `https://github.com/official-stockfish/Stockfish`
- Release/Tag: `sf_19`
- Source-Ziel: `third_party/stockfish19/source`
- offizielles Release-Archiv SHA-256: `519b653d0d1ffb96531d982ccbe5c6a19425e8388e0e3c2f70f34b424ab32d76`
- offizielles NNUE: `nn-1a298aa575a0.nnue`
- NNUE SHA-256: `1a298aa575a085434d29027978dc36867fe9c5bcea9376654b7a8eba1e52dfc2`
- Lizenz: GNU GPL Version 3

`native/cmake/fetch_stockfish19.cmake` lädt Source und NNUE reproduzierbar, wenn sie lokal fehlen, und prüft die hinterlegten SHA-256-Werte. Stockfish 19 wird in einem umbenannten C++-Namespace gebaut, damit Stockfish 18 und 19 gleichzeitig mit `kchess_core` gelinkt werden können.

## GPL-Einordnung

KChess linkt Stockfish direkt in `kchess_core` und behauptet keine lizenzrechtliche Trennung durch UCI oder FFI. Für eine verteilte kombinierte Binary müssen die GPLv3-Anforderungen und der vollständige entsprechende Quellcode berücksichtigt werden. Dies ist keine Rechtsberatung.

## SQLite

SQLite wird als eingebettete lokale Datenbank aus `third_party/sqlite/` verwendet. Der in `THIRD_PARTY_NOTICES.md` dokumentierte Stand ist Public Domain.

## Reproduzierbare Build-Methode

Windows x64, Release:

```powershell
cmake -S native -B build/native -A x64 -DKCHESS_WITH_STOCKFISH=ON
cmake --build build/native --config Release
```

Android ARM64, Debug:

```powershell
cd flutter_app
flutter build apk --debug --target-platform android-arm64
```

Unter Windows verwendet KChess einen persistenten Source-adjacent Cache für die kompilierten Stockfish-18-/19-Libraries, damit `flutter clean` die teuren Engine-Artefakte nicht unnötig entfernt. Android baut beide Engines über den nativen CMake-Pfad für `arm64-v8a`.

## Corresponding Source

Der zu einer Binary passende Quellstand umfasst insbesondere:

- KChess Flutter- und C++-Quellen
- `third_party/stockfish/source` für Stockfish 18
- `third_party/stockfish19/source` für Stockfish 19 bzw. die reproduzierbare Fetch-Konfiguration
- alle verwendeten NNUE-Netze
- SQLite und nlohmann/json
- CMake-/Gradle-/Windows-Builddateien
- GPLv3-/Third-Party-Lizenzinformationen

Ein Transport-ZIP kann `third_party/` bewusst auslassen; ein Release-/Corresponding-Source-Paket darf daraus nicht automatisch abgeleitet werden.

## Opening Theory

KChess liefert ein vollständig offline genutztes KCB1-Opening-Book aus:

- Quelle: Lichess Open Database
- Lizenz: CC0 1.0
- verwendete Standard-Rated-Dumps: Januar 2013, Januar 2015, Februar 2015
- Builder/Format: `kcb-builder-2` / KCB1 Version 1
- Ergebnis: 3.114.122 Partien, 27.022 Entries, 756.776 Byte
- SHA-256: `0a1a6849bef494fc267a3bc9e67dcaf37f8f7e8acb47def0e7bc5940365e2163`

Vollständige Quell-, Parameter- und Prüfsummeninformationen stehen in `tools/opening_book/BUILD_METADATA.md`.

## Opening Names

KChess liefert zusätzlich den offline genutzten KCO1-Eröffnungsnamenindex aus:

- Quelle: `lichess-org/chess-openings`
- Lizenz: CC0 1.0
- Upstream-Commit: `4b8622759e7ae6f93f011cc6c83a3823401ab45e`
- Builder/Format: `kco-builder-1` / KCO1 Version 1
- Ergebnis: 3.810 Einträge, 228.888 Byte
- SHA-256: `b4207c778ce0e37d34a3242e1936935c9242b9d6712c5f0d6f1d23704114c1bc`

Details stehen in `tools/opening_names/BUILD_METADATA.md`.

## Python-Development-Abhängigkeiten

Python und seine Builder-Abhängigkeiten werden nicht in Android- oder Windows-Binaries eingebettet. Der Opening-Book-Builder verwendet unter anderem `python-chess` und `zstandard`; der Opening-Name-Builder verwendet `python-chess`.
