# KChess

KChess ist eine lokale Flutter/C++20-Schachanalyse für Windows x64 und
Android ARM64. Phase 4 ergänzt öffentliche Chess.com- und Lichess-Profile,
Stats sowie monatlich synchronisierte abgeschlossene Partien über deren
offizielle APIs. ETag/Last-Modified, serielles Rate-Limiting und SQLite-Caches
ermöglichen den Offline-Betrieb; es werden keine Tokens oder Passwörter
benötigt. Online-PGNs münden in dasselbe lokale Game-Modell wie Importe.

Phase 2/3 importiert weiterhin PGN-Dateien, PGN-Text und FEN-Stellungen,
rekonstruiert alle Stellungen nativ und analysiert sie asynchron mit dem
offiziellen Stockfish 18. Ergebnisse und MultiPV-Linien werden pro Halbzug in
SQLite gespeichert und nach einem Neustart wiederverwendet. Ein gebündeltes
CC0-KCB1-Book erkennt Opening Theory vollständig offline; ein eigener
versionierter WDL-Klassifikator erzeugt Theory, Brilliant, Best, Excellent,
Okay, Miss, Mistake und Blunder sowie Local Accuracy und eine Summary pro Farbe.

## Bauen und testen

```powershell
cd flutter_app
flutter pub get
flutter gen-l10n
flutter analyze
flutter test
flutter build windows --debug
flutter build apk --debug --target-platform android-arm64
```

Native Tests:

```powershell
cmake -S native -B build/native -DKCHESS_BUILD_TESTS=ON
cmake --build build/native --config Release
ctest --test-dir build/native -C Release --output-on-failure
```

Opening Book reproduzieren und testen:

```powershell
python -m pip install -r tools/opening_book/requirements.txt
python -m unittest discover -s tools/opening_book/tests -v
python tools/opening_book/build_book.py `
  --input lichess_db_standard_rated_2013-01.pgn.zst `
  --input lichess_db_standard_rated_2015-01.pgn.zst `
  --input lichess_db_standard_rated_2015-02.pgn.zst `
  --output flutter_app/assets/opening_book.kcb `
  --max-ply 20 `
  --min-games 100
```

Der Builder streamt alle Inputs in eine gemeinsame SQLite-Aggregation und
wendet `min_games` erst danach an. Das ausgelieferte KCB1 umfasst 3.114.122
Partien, 13.832 Stellungen und 27.022 Move-Entries bei 756.776 Byte.

Details stehen in `docs/ARCHITECTURE.md` und
`docs/LICENSE_COMPLIANCE.md`; die exakte Book-Provenienz steht in
`tools/opening_book/BUILD_METADATA.md`. Das mit Stockfish kombinierte Werk wird
unter GPLv3 bereitgestellt; dies ist keine Rechtsberatung.
