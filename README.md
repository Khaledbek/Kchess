# KChess

KChess ist eine lokale Schach-App für Windows x64 und Android ARM64. Die Oberfläche wird mit Flutter gebaut; die fachliche Laufzeitlogik liegt im nativen C++20-Core. Python wird ausschließlich für Entwicklungs-, Datenbuilder- und Diagnosewerkzeuge verwendet und ist kein Bestandteil der ausgelieferten App.

## Aktueller Funktionsumfang

- Profile für Chess.com, Lichess und lokale PGN/FEN-Nutzung
- lokale Bibliothek mit PGN-/FEN-Import, Favoriten und Sammlungen
- Online-Spielesynchronisierung über öffentliche Provider-APIs
- lokale Analyse mit auswählbarem Stockfish 18 oder Stockfish 19
- MultiPV, Best-Move-Pfeile, Side-Lines, Move-Klassifikation und lokale Accuracy
- lokales Spiel gegen Bots samt Spielprotokoll
- Training mit Opening Lab, Blunder Buster, Endgame Academy und Studien
- Statistikbereich für lokale und synchronisierte Partien
- Engine-, Analyse-, Design-, allgemeine sowie Daten-/Speicher-Einstellungen
- vollständige UI-Lokalisierung über ARB für Deutsch, Englisch und Arabisch

## Architektur

```text
Flutter / Dart
UI, Navigation, View-State, Darstellung
        |
        | dart:ffi / stabile C-ABI
        v
C++20 Core
Schach-, Analyse-, Provider-, Datenbank- und Trainingslogik
        |
        +-- Stockfish 18
        +-- Stockfish 19
        +-- SQLite
        +-- öffentliche Chess.com-/Lichess-APIs

Python
nur Development: Opening-Builder, Datenaufbereitung, Smoke-/Diagnosewerkzeuge
```

Flutter soll keine neue Fachlogik enthalten. Schachregeln, Engine-Entscheidungen, Klassifikation, Accuracy, Persistenz, Providerlogik und Trainingswahrheit gehören in C++.

## Hauptnavigation

Die aktuelle Shell enthält:

1. Spiele
2. Play
3. Training
4. Favoriten
5. Statistik
6. Einstellungen

Das aktive Profil wird über den Profilkopf geöffnet bzw. gewechselt. Downloads sind kein eigener Navigationsbereich mehr: lokal gespeicherte Online-Partien werden als Favoriten in der obersten Sammlung `Downloads` geführt.

## Projektstruktur

```text
Kchess/
├─ flutter_app/                 Flutter UI, FFI-Adapter, ARB, Assets
│  ├─ lib/features/            Feature-spezifische Präsentation
│  ├─ lib/ffi/                 Dart-FFI-Grenze
│  ├─ lib/shared/              gemeinsame Modelle, Theme und Widgets
│  ├─ lib/ui/                  App-Shell und gemeinsame UI-Bausteine
│  └─ l10n/                    app_en.arb, app_de.arb, app_ar.arb
├─ native/                     C++20-Core und C-ABI
│  └─ src/                     analysis, api, chess, engine, persistence,
│                              providers, services, theory, training
├─ tools/                      Python-Entwicklungswerkzeuge
├─ docs/                       Architektur, Produktspezifikation, Compliance
├─ img/                        gemeinsame lokale UI-Assets
└─ third_party/                externe Abhängigkeiten und Enginequellen
```

`third_party/` ist ein externer Abhängigkeitsbaum für unter anderem Stockfish, SQLite und nlohmann/json. Er wird bei normalen UI-/Core-Bereinigungen nicht verändert und kann in Transport-/Arbeits-ZIPs bewusst fehlen.

## Lokalisierung

Alle sichtbaren Flutter-Texte gehören in:

```text
flutter_app/l10n/app_en.arb
flutter_app/l10n/app_de.arb
flutter_app/l10n/app_ar.arb
```

`flutter_app/lib/localization/generated/` wird von Flutter erzeugt und nicht manuell bearbeitet.

## Bauen und testen

Flutter:

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

Python-Tools werden separat ausgeführt; sie sind keine Runtime-Abhängigkeit der App.

## Wichtige Dokumente

- `AGENTS.md` – verbindliche Repository-Regeln
- `flutter_app/AGENTS.md` – Flutter-Regeln
- `native/AGENTS.md` – Native-Regeln
- `tools/AGENTS.md` – Python-/Tooling-Regeln
- `docs/ARCHITECTURE.md` – aktueller technischer Aufbau
- `agent/PRODUCT_SPEC.md` – aktuelle Produktspezifikation
- `docs/LICENSE_COMPLIANCE.md` – Build- und Lizenz-Compliance
- `THIRD_PARTY_NOTICES.md` – Drittanbieterhinweise
