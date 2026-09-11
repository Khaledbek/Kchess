# KChess AI Router

## Scope

Diese Datei enthält nur globale Regeln. Für jede Aufgabe zusätzlich die **nächstgelegene** `AGENTS.md` im betroffenen Ordner lesen. Lokale `AGENTS.md`-Dateien enthalten nur bereichsspezifische Hinweise und sollen unnötigen Repository-Kontext vermeiden.

## Architektur

```text
Flutter/Dart = UI, Navigation, View-State, Darstellung, dünne FFI-Adapter
C++20        = Runtime-Domainlogik, Schach, Engine, Persistenz, Provider, Training
Python       = Development-/Builder-/Diagnosewerkzeuge, keine App-Runtime
ARB          = alle sichtbaren Übersetzungen
```

Keine fachliche Runtime-Logik neu in Flutter oder Python duplizieren.

## AI-Kontextbudget

Bei jeder Aufgabe:

1. Diese Datei + nächstgelegene lokale `AGENTS.md` lesen.
2. Mit Symbol-/Textsuche (`rg`) die betroffenen Stellen finden.
3. Zuerst nur Zieldatei + maximal 2–4 direkte Abhängigkeiten öffnen.
4. Große Dateien nicht komplett lesen, wenn ein Abschnitt per Symbolsuche reicht.
5. Kontext nur erweitern, wenn ein konkreter Aufrufer, Vertrag oder Datenfluss es verlangt.
6. Bereits gelesene unveränderte Dateien im selben Task nicht erneut vollständig laden.

Standardmäßig **nicht** durchsuchen/lesen:

- `third_party/`
- `build/`, `.dart_tool/`, CMake-Buildverzeichnisse
- `native/prebuilt/`, `native/.stockfish_build_cache/`
- `flutter_app/lib/localization/generated/`
- große generierte Trainingsdaten unter `native/src/training/data/*.inc`

Ausnahme nur, wenn die Aufgabe genau diesen Bereich betrifft.

## Task-Routing

- Analyse UI → `flutter_app/lib/features/analysis/AGENTS.md`
- Play/Bots UI → `flutter_app/lib/features/play/AGENTS.md`
- Training UI → `flutter_app/lib/features/training/AGENTS.md`
- Games/Import → `flutter_app/lib/features/games/AGENTS.md`
- Favorites → `flutter_app/lib/features/favorites/AGENTS.md`
- Settings → `flutter_app/lib/features/settings/AGENTS.md`
- Statistics → `flutter_app/lib/features/statistics/AGENTS.md`
- Profile → `flutter_app/lib/features/profile/AGENTS.md`
- Shared Board/UI → `flutter_app/lib/shared/AGENTS.md`
- FFI → `flutter_app/lib/ffi/AGENTS.md`
- App-Shell/Startup → `flutter_app/lib/ui/AGENTS.md`
- Native Analyse → `native/src/analysis/AGENTS.md`
- Stockfish/Bot Engine → `native/src/engine/AGENTS.md`
- Native Services → `native/src/services/AGENTS.md`
- Datenbank → `native/src/persistence/AGENTS.md`
- C-ABI → `native/src/api/AGENTS.md`
- Schachmodell/PGN/FEN → `native/src/chess/AGENTS.md`
- Native Training → `native/src/training/AGENTS.md`
- Theory/Openings → `native/src/theory/AGENTS.md`
- Provider → `native/src/providers/AGENTS.md`
- Python/Builder → `tools/AGENTS.md`

## Globale Regeln

- `third_party/` bei normalen KChess-Aufgaben nicht ändern.
- Sichtbare Flutter-Texte ausschließlich in `flutter_app/l10n/app_en.arb`, `app_de.arb`, `app_ar.arb` pflegen; alle drei gemeinsam ändern.
- Generierte Lokalisierungsdateien nie manuell editieren.
- Bestehende ABI-/JSON-/DB-Kompatibilität nicht als „toten Code“ entfernen, ohne alte Daten/Clients auszuschließen.
- Stockfish 18 und 19 sind beide aktive Engines; Engine-spezifische Logik bleibt nativ.
- Side-Line-Analyse ist flüchtig und darf Hauptlinienpersistenz nicht überschreiben.
- Handgeschriebene nicht-triviale Dateien mit klaren Sections strukturieren.
- Keine Builds, Tests, `flutter analyze` oder App-Runs automatisch starten, solange der Benutzer sie selbst ausführen möchte.

## Änderungsstil

Bevor Code geändert wird: Aufrufer suchen, Verantwortung bestimmen, kleinste sichere Änderung wählen. Bei Cleanup nur nachweislich tote/redundante Pfade entfernen. Keine Parallelimplementierung erstellen, wenn eine bestehende Schnittstelle erweitert werden kann.
