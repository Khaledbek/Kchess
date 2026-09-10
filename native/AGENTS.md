# Native C++ Core Instructions

## 1. Rolle

`native/` enthält die gesamte Fach-, Schach-, Analyse-, Daten- und Persistenzlogik von KChess.
Flutter ist nur Client dieser Logik.

## 2. Zuständigkeiten

C++20 ist die einzige fachliche Wahrheit für:

- PGN, SAN, FEN, Züge und Legalität
- Profile und Provider
- Datenbank und Migrationen
- Settings und effektive Engine-Konfiguration
- Game Library und Statistik
- Stockfish
- Voranalyse und Liveanalyse
- Side-Line-Analyse
- MultiPV / `searchmoves`
- globale Positionscaches und Analyse-Wiederverwendung
- Move-Klassifikation
- Accuracy
- Theory / Opening Book
- Resultat-/Termination-Domainstatus
- Training-Kataloge und Lösungsvarianten
- Trainingsversuche, Erfolgs-/Meisterschaftsregeln und Fortschrittspersistenz

## 3. Analyse-Schichten

Bestehende Trennung respektieren:

```text
engine/                 Stockfish / ChessEngine
services/analysis_*     Orchestrierung und Persistenz der Analyse
analysis/move_*         Klassifikation
analysis/accuracy.*     Accuracy
persistence/            SQLite / Migrationen
api/                    C-ABI für Flutter
```

Keine Klassifikations- oder Accuracy-Formel in Flutter spiegeln.

## 4. Klassifikation und Accuracy

- Klassifikation und Accuracy bleiben voneinander unabhängig.
- Beide dürfen dieselben Engine-Rohdaten verwenden, aber Accuracy wird nicht aus Labels abgeleitet.
- Versionsänderungen explizit versionieren.
- Alte persistierte Werte bei Versionswechsel korrekt invalidieren oder neu berechnen.
- Tests für Grenzfälle, Mate, WDL-/CP-Sättigung und Tiefe ergänzen.

## 5. Analyse-Persistenz

Pro Partie nur einen autoritativen Voranalyse-Stand erhalten.

- höherwertige Analyse ersetzt niedrigere erst nach erfolgreichem Abschluss
- niedrigere Anfrage verwendet vorhandenen höheren Stand
- globale Positionscaches separat halten
- Side Lines niemals in Hauptlinienanalyse überschreiben
- Side-Line-Settings separat persistent speichern

## 6. FFI / C-ABI

- keine C++-Klassen direkt exponieren
- UTF-8, primitive Typen, opaque handles oder JSON/DTOs
- Exceptions an ABI-Grenze abfangen
- Speicherbesitz eindeutig dokumentieren
- Langläufer über Start/Status/Cancel oder gleichwertiges Jobmodell
- Flutter soll keine fachliche Nachberechnung brauchen

## 7. Sections pro Datei

Jede handgeschriebene nicht-triviale C++-Datei besitzt mindestens eine benannte Section.
Beispiel:

```cpp
// -----------------------------------------------------------------------------
// Section: Cache compatibility
// -----------------------------------------------------------------------------
```

Neue Fachservices, DTOs und Adapter nach einer klaren Verantwortung schneiden
und möglichst unter 500 Zeilen halten. Ab 1000 Zeilen ist vor einer Erweiterung
eine Aufteilung erforderlich; nur eine technisch begründete Ausnahme darf
größer bleiben. Sections ersetzen keinen Dateischnitt.
Vendorte Third-Party-Dateien, insbesondere Stockfish, nicht nur für Stilregeln verändern.

## 8. Stockfish

- offizielle Stockfish-Quellen und Lizenzhinweise erhalten
- NNUE-/SIMD-/Build-Konfiguration reproduzierbar halten
- `Threads`, `Hash` und ähnliche Optionen nicht unnötig neu setzen
- Transposition Table und persistente Engine-Lebenszyklen sinnvoll wiederverwenden
- Plattformkompatibilität bei SIMD beachten

## 9. Tests

Bei nativen Änderungen soweit relevant:

- Unit Tests für geänderte Domainlogik
- Analysis-Workflow-Test
- Classifier-/Accuracy-Regressionstests
- Datenbankmigrationen testen
- `kchess_core` vollständig bauen
- Windows/Android-spezifische CMake-Pfade beachten
