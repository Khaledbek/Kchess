# Flutter Layer Instructions

## 1. Rolle

`flutter_app/` ist ausschließlich Präsentations-, Interaktions- und View-State-Schicht.
Die fachliche Wahrheit kommt aus dem nativen C++-Core.

## 2. Erlaubt in Flutter

- Screens und Widgets
- Navigation
- responsive Layouts
- Board-Darstellung
- Theme/Farben/Assets
- Animationen
- lokale UI-Auswahlzustände
- Dialoge, Menüs und Formulare
- Polling nativer Jobs
- dünne FFI-Adapter und DTO-Mapping

## 3. Nicht in Flutter implementieren

Keine neue Domainlogik in Dart:

- keine Schachlegalität
- kein PGN/FEN/SAN-Parsing als fachliche Quelle
- keine Ergebnis-/Matt-/Remis-Berechnung
- keine Analyseheuristiken
- keine Move-Klassifikation
- keine Accuracy-Berechnung
- keine Theory-Entscheidung
- keine Persistenz-/Cache-Regeln
- keine effektiven Engine-Ressourcenregeln
- keine Trainingskataloge, Lösungsvarianten, Erfolgs-/Meisterschaftsregeln oder
  Fortschrittspersistenz

Wenn Legacy-Code so etwas enthält und der Bereich bearbeitet wird, bevorzugt nach C++ migrieren.

## 4. Feature-Struktur

`lib/ui/app_root.dart` bleibt klein und enthält nur Home-/Shell-Verantwortung.

Hauptscreens gehören unter:

```text
lib/features/<feature>/presentation/
```

Aktuelle Features:

- `analysis`
- `favorites`
- `games`
- `play`
- `profile`
- `settings`
- `statistics`
- `training`

Keine neuen großen Screen-Klassen in `app_root.dart` einfügen.

## 5. Lokalisierung ausschließlich über ARB

Sichtbare Texte ausschließlich in:

```text
l10n/app_en.arb
l10n/app_de.arb
l10n/app_ar.arb
```

- keine sichtbaren Strings in Widgets hardcoden
- auch sichtbare Katalogdaten, Übungstitel, Hinweise, Fehlermeldungen und
  Platzhalter ausschließlich über ARB lokalisieren
- immer DE/EN/AR gemeinsam ergänzen
- `lib/localization/generated/*` niemals manuell ändern
- generierte `AppLocalizations` verwenden
- Arabisch ändert nur Text/Sprache; globale UI bleibt LTR
- Board und App-Layout dürfen beim Sprachwechsel nicht gespiegelt werden

## 6. Sections pro Datei

Jede handgeschriebene nicht-triviale Dart-Datei besitzt mindestens eine benannte Section.
Beispiel:

```dart
// -----------------------------------------------------------------------------
// Section: Result presentation
// -----------------------------------------------------------------------------
```

Dateien nach einer klaren UI-Verantwortung schneiden und möglichst unter 500
Zeilen halten. Ab 1000 Zeilen ist vor einer Erweiterung eine Aufteilung in
Screens, Widgets, View-State oder DTO-Mapping erforderlich; nur eine technisch
begründete Ausnahme darf größer bleiben. Sections ersetzen keinen Dateischnitt.
Generierte Dateien sind ausgenommen.

## 7. Analysis UI

Flutter darf native Analyseergebnisse nur darstellen:

- Evaluation
- Engine-Linien
- Klassifikationssymbol
- Klassifikationsfarbe
- Accuracy
- Theory
- Resultatstatus

Die Entscheidung, welcher Wert fachlich gilt, kommt aus C++.
UI-Effekte wie Win/Loss/Draw/Give-up-Animation, Andocken am Spielernamen und „nur einmal zeigen“ bleiben Flutter-Verantwortung.

## 8. FFI

- keine Schachlogik im FFI-Adapter
- DTOs möglichst unverändert mappen
- Fehler aus Native in UI-Zustände übersetzen, nicht fachlich neu interpretieren
- lange Jobs niemals auf dem UI-Thread ausführen

## 9. Qualität

Bei Flutter-Änderungen soweit verfügbar:

1. `dart format`
2. `flutter analyze`
3. relevante Widget-/Unit-Tests
4. `flutter build windows --debug`
5. Android-Build bei plattformspezifischen Änderungen
