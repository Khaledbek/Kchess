# KChess Architecture

## 1. Architekturprinzip

KChess trennt Darstellung und Fachlogik strikt:

```text
Flutter / Dart UI
        |
        | dart:ffi / C-ABI
        v
C++20 Core
        |
        +-- Stockfish
        +-- SQLite
        +-- Provider APIs
```

Flutter rendert und orchestriert UI-Zustände. C++ ist die einzige fachliche Wahrheit.

## 2. Flutter-Schicht

Flutter besitzt:

- Home-/Navigation-Shell
- Feature-Screens
- Board-Darstellung
- Animationen
- Theme und Assets
- View-State
- dünne FFI-Gateways

`lib/ui/app_root.dart` enthält nur noch Home-/Shell-Code. Die Hauptbereiche liegen getrennt unter `lib/features/*/presentation/`.

## 3. Native Schicht

Der C++20-Core besitzt:

- PGN/FEN/SAN/Zugmodell und Legalität
- Profile, Provider, Game Library und Statistik
- Settings und Persistenz
- Stockfish und Engine-Lifecycle
- Voranalyse, Liveanalyse und Side-Line-Analyse
- Cache/Wiederverwendung
- Theory
- Move-Klassifikation
- Accuracy
- Training-Kataloge und validierte Lösungsvarianten
- Trainingsversuche, Fortschritt, Erfolgsserien und Meisterschaft

## 4. Analyse

Die Analyse verwendet den offiziellen Stockfish-Core hinter `ChessEngine`/`StockfishEngine`.
Threads und Hash werden nur bei tatsächlicher Änderung neu konfiguriert, damit Threadpool und Transposition Table wiederverwendet werden können.
Windows und Android verwenden plattformspezifische SIMD-Pfade.

Voranalyse und Liveanalyse sind getrennte Qualitätsstufen. Ein vorhandener höherwertiger Analyse-Stand darf für niedrigere Anforderungen wiederverwendet werden. Pro Partie bleibt nur ein autoritativer persistierter Voranalyse-Stand; ein höherer erfolgreicher Lauf ersetzt den niedrigeren.

Side-Line-Analyse verwendet einen separaten flüchtigen Variantenstatus und darf Hauptliniendaten nicht überschreiben. Ihre zuletzt angewendeten Engine-Einstellungen werden separat gespeichert.

## 5. Klassifikation

Die Move-Klassifikation wird ausschließlich nativ berechnet.
`Best` ist an den tatsächlichen Stockfish-Rang-1-Zug gebunden. Weitere Kategorien verwenden native Engine-Rohdaten und konservative CP-/Mate-/Positionsregeln.
Flutter erhält nur das fertige Klassifikationsresultat und rendert Symbol/Farbe.

## 6. Accuracy

Accuracy wird ausschließlich nativ und unabhängig von Klassifikationslabels berechnet.
Die aktuelle Logik verwendet CP-/Mate-Regret, Entscheidungsgewichtung und Stabilitätsinformationen statt einer reinen gesättigten WDL-Differenz.
Triviale Entscheidungen in bereits klaren Stellungen erhalten geringes Gewicht; kritische/unique Entscheidungen höheres Gewicht.

## 7. Persistenz und Versionierung

SQLite speichert Profile, Games, Analyse, Settings, Provider-Caches und Statistiken.
Analyse-, Classifier-, Accuracy- und Theory-Versionen werden getrennt behandelt, damit vorhandene Engine-Rohdaten nach Möglichkeit wiederverwendet werden können.

## 8. FFI

Die Grenze ist eine stabile C-ABI:

- keine C++-Klassen direkt
- UTF-8 und serialisierbare DTOs
- explizite Speicherfreigabe
- keine Exceptions über die ABI-Grenze
- asynchrone native Jobs für Langläufer

## 9. Lokalisierung

Alle sichtbaren Flutter-Texte stammen ausschließlich aus:

```text
flutter_app/l10n/app_en.arb
flutter_app/l10n/app_de.arb
flutter_app/l10n/app_ar.arb
```

Die generierten Dateien unter `lib/localization/generated/` werden nicht manuell bearbeitet.
Das gilt auch für sichtbare Dateninhalte wie Übungstitel, Hinweise, leere
Zustände und Fehlermeldungen: C++ liefert stabile semantische IDs und fachliche
Werte, Flutter löst die zugehörigen Texte über ARB auf.
Arabisch ändert die Sprache, nicht die globale Layout-Richtung: KChess bleibt layoutseitig LTR, damit Board, Navigation und Spielerorientierung nicht gespiegelt werden.

## 10. Datei-Struktur und Sections

Handgeschriebene Quell- und Dokumentationsdateien werden in klar benannte
Sections gegliedert und nach einer Verantwortung geschnitten. Zielgröße sind
höchstens ungefähr 500 Zeilen. Handgeschriebene Dateien ab 1000 Zeilen werden
vor einer Erweiterung aufgeteilt; nur technisch begründete Ausnahmen dürfen
größer bleiben. Große Screens werden in Screen, View-State und wiederverwendbare
Widgets getrennt, native Fachbereiche in kleine Services, Modelle und Adapter.
Generierte und vendorte Dateien sind von Stil-Refactors ausgenommen.

## 11. Training

Flutter zeigt Trainingsbereiche, Übungsfortschritt und Board-Interaktionen an.
Der C++20-Core besitzt den Trainingskatalog, die Lösungszüge, Zugvalidierung,
Versuchsstatus, Erfolgs-/Meisterschaftsregeln und SQLite-Persistenz. Die C-ABI
liefert hierfür DTOs mit stabilen Übungs- und Textschlüsseln; Flutter darf daraus
keine fachlichen Ergebnisse neu berechnen.
