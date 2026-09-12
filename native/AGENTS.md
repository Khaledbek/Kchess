# Native C++ AI Instructions

## Rolle

`native/` ist die fachliche Runtime-Wahrheit: Schach, Analyse, Stockfish, Persistenz, Provider, Statistik, Bots und Training.

## Kontext sparen

- Nächstgelegene `native/src/**/AGENTS.md` lesen.
- Mit `rg` zuerst Deklaration, Definition und Aufrufer eines Symbols finden.
- Große Dateien (`core_api.cpp`, `database.cpp`, große Services) nur an relevanten Funktionen öffnen.
- Nicht automatisch alle Services oder Engine-Dateien lesen.
- `third_party/`, Prebuilt- und Build-Caches nur bei expliziten Engine-/Dependency-Aufgaben öffnen.

## Kritische Regeln

- C-ABI stabil halten; Exceptions nicht über ABI-Grenze lassen.
- JSON/DTO-Verträge und Speicherbesitz explizit behandeln.
- Migrationen, Legacy-Spalten, Settings-Aliase und alte ABI-Exports können absichtliche Kompatibilität sein.
- Stockfish 18 und 19 bleiben getrennte aktive Runtime-Optionen.
- Side-Lines nicht in autoritative Hauptanalyse persistieren.
- Keine zweite fachliche Implementierung in Dart/Python erzeugen.

## Cleanup

Vor Löschen mindestens Deklaration, Definition, C-ABI/JSON-Vertrag und alle internen Aufrufer prüfen. Geringe Nutzung allein ist kein Beweis für toten Code.

Keine automatischen Builds/Tests starten, wenn der Benutzer sie selbst ausführt.
