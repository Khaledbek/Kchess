# KChess Architecture

## 1. Architekturprinzip

KChess trennt UI, Runtime-Domainlogik und Development-Tooling klar:

```text
Flutter / Dart UI
        |
        | dart:ffi / stabile C-ABI
        v
C++20 Core
        |
        +-- Stockfish 18
        +-- Stockfish 19
        +-- SQLite
        +-- Chess.com / Lichess HTTP

Python tools
        +-- KCB1 Opening-Book Builder
        +-- KCO1 Opening-Name Builder
        +-- Provider Smoke Tool
```

Flutter rendert und orchestriert UI-Zustände. C++ ist die fachliche Runtime-Wahrheit. Python wird nur zur Entwicklung/Datenaufbereitung verwendet.

## 2. Flutter-Schicht

Flutter besitzt:

- App-Shell und Navigation
- Feature-Screens
- Board-Darstellung
- Result-/Klassifikationssymbole und Animationen
- Theme und Assets
- View-State
- dünne FFI-Gateways und DTO-Mapping

Aktuelle Hauptbereiche:

```text
Games | Play | Training | Favorites | Statistics | Settings
```

Die Profilseite wird über den Profilkopf geöffnet. `lib/ui/app_root.dart` komponiert die Shell; Feature-Code liegt unter `lib/features/`.

## 3. Native Schicht

Der C++20-Core besitzt:

- PGN/FEN/SAN/Zugmodell und Legalität
- Profile und Provider
- Game Library, Favoriten und Download-Kompatibilität
- Statistik
- Settings und SQLite-Persistenz
- Stockfish-Engine-Lifecycle
- Analyse, Side-Lines, MultiPV und Cache
- Move-Klassifikation und Accuracy
- Opening Theory und Opening Names
- lokale Bot-Spiele
- Training und Fortschritt

## 4. Engine-Layer

KChess unterstützt zwei auswählbare Engines:

- `stockfish18`
- `stockfish19`

Stockfish 18 und 19 werden getrennt gebaut und gemeinsam in den Core integriert. Stockfish 19 nutzt einen separaten Namespace-/Buildpfad, damit Symbolkollisionen vermieden werden. Die Engineauswahl wird persistent gespeichert.

Engine-spezifische Besonderheiten wie SF19-MultiPV-Kohärenz und die SF19-Klassifikationskalibrierung bleiben im nativen Layer. Flutter erhält fertige Engine-/Analyse-DTOs.

## 5. Analyse

Die Analysepipeline umfasst:

- Hauptlinien-/Voranalyse einer Partie
- Live-/Positionanalyse
- MultiPV und Best Move
- Side-Line-Analyse als flüchtiger Variantenbaum
- native Evaluation-Bar-Projektion
- native Move-Klassifikation
- native Accuracy
- Positionscache und persistierte Wiederverwendung

Side-Lines dürfen den autoritativen Hauptanalyse-Stand nicht überschreiben. Höherwertige Hauptanalyse ersetzt niedrigere erst nach erfolgreichem Abschluss.

## 6. Klassifikation und Accuracy

Move-Klassifikation und Accuracy sind getrennte native Systeme.

- `Best` muss mit dem tatsächlich validierten Engine-Bestmove konsistent sein.
- Engine-spezifische Modelle dürfen getrennt kalibriert werden.
- Brilliant/Great/Fehlerklassen werden ausschließlich im nativen Classifier bestimmt.
- Accuracy wird nicht aus den Labels abgeleitet.

## 7. Bibliothek, Favoriten und Downloads

Partien stammen aus:

- Chess.com-Synchronisierung
- Lichess-Synchronisierung
- lokalem PGN-Import
- lokalem FEN-Import
- lokal gespeicherten Bot-Partien

Favoriten sind lokal persistent und können Sammlungen zugeordnet werden. Downloads sind kein eigener Navigationsbereich mehr: das lokale Speichern einer Online-Partie setzt den Favoritenstatus und ordnet sie der obersten Sammlung `Downloads` zu. Legacy-Downloaddaten werden nativ kompatibel behandelt.

PGN/FEN-Import gehört zur lokalen PGN/FEN-Bibliothek, auch wenn gerade ein Online-Profil aktiv ist.

## 8. Provider

Chess.com und Lichess werden über öffentliche APIs ohne Login-Tokens oder Passwörter verwendet. Providerantworten werden nativ normalisiert und gecacht. Netzwerkfehler dürfen lokale Daten nicht zerstören.

## 9. Play

Lokale Bot-Partien werden über den nativen `BotService` und die Engine-/Move-Selection-Pipeline ausgeführt. Flutter zeigt Setup, Boardzustand, Resultat und Spielprotokoll. Bot-Partien dürfen nicht auf Dart-Schachlogik angewiesen sein.

## 10. Training

Training besteht aktuell aus:

- Opening Lab
- Blunder Buster
- Endgame Academy
- Endgame Studies

Der C++-Core besitzt Kataloge, Positions-/Zugvalidierung, Sessionstatus, Fortschritt und Verteidiger-Jobs. Flutter besitzt Navigation, Baumdarstellung, Boardinteraktion und Polling.

Die Opening-Trainingsdaten stammen aus den eingebetteten Lichess-`chess-openings`-Snapshots. Die Studien tragen ihre Quellenmetadaten direkt im eingebetteten Katalog; der aktuelle Studienbestand basiert auf gemeinfreien Kling/Horwitz-Studien.

## 11. Statistik

Statistik wird nativ aggregiert. Flutter stellt Overview, Form, Rating, Termination, Phasen, Openings und Spielervergleich dar. Die UI soll keine Statistikwerte fachlich neu berechnen, wenn sie bereits nativ geliefert werden.

## 12. Persistenz

SQLite speichert unter anderem:

- Profile und aktives Profil
- Settings
- Games und Provider-Caches
- Favoriten/Sammlungen
- Analysen und Cache-Metadaten
- Training/Progress

Migrationen und alte Compatibility-Felder können bewusst weiter bestehen und dürfen nicht nur wegen geringer aktueller Nutzung entfernt werden.

## 13. FFI

Die Grenze ist eine stabile C-ABI:

- keine C++-Klassen direkt
- UTF-8 und serialisierbare DTOs
- explizite Speicherfreigabe
- keine Exceptions über die ABI-Grenze
- native Jobs für Langläufer

## 14. Lokalisierung

Alle sichtbaren Flutter-Texte stammen aus:

```text
flutter_app/l10n/app_en.arb
flutter_app/l10n/app_de.arb
flutter_app/l10n/app_ar.arb
```

Generierte Dateien unter `lib/localization/generated/` werden nicht manuell bearbeitet. Arabisch ändert Sprache/Text, nicht die globale Layout-Richtung.

## 15. Python-Tooling

Python wird nicht in der App ausgeliefert. Es erzeugt reproduzierbare Entwicklungsartefakte:

- `tools/opening_book/build_book.py` -> `opening_book.kcb`
- `tools/opening_names/build_names.py` -> `opening_names.kco`
- `tools/provider_smoke.py` -> manuelle Providerdiagnose

## 16. Third Party

`third_party/` enthält externe Quellen und wird bei normalen KChess-Refactors nicht verändert. Dazu gehören insbesondere:

- Stockfish 18
- Stockfish 19
- SQLite
- nlohmann/json

Details stehen in `docs/LICENSE_COMPLIANCE.md` und `THIRD_PARTY_NOTICES.md`.
