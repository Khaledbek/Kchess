# KChess Repository Instructions

## 1. Zweck und Geltungsbereich

Diese Datei ist die verbindliche Root-Anweisung für das gesamte KChess-Repository.
Zusätzliche schichtspezifische Regeln stehen in:

- `flutter_app/AGENTS.md` für Flutter/Dart
- `native/AGENTS.md` für C++20

Produktverhalten wird in `docs/PRODUCT_SPEC.md` bzw. dessen kanonischer Spezifikation beschrieben.
Architekturdetails stehen in `docs/ARCHITECTURE.md`.

## 2. Verbindliche Architektur

KChess verwendet eine klare Schichtentrennung:

```text
Flutter / Dart
= ausschließlich UI, Darstellung, Navigation und View-State
        |
        | dart:ffi / stabile C-ABI / DTOs
        v
C++20 Core
= gesamte Fach-, Schach-, Analyse-, Daten- und Persistenzlogik
```

### Flutter darf besitzen

- Widgets und Screens
- Navigation
- responsive Layouts
- Theme, Farben, Assets und Animationen
- Board-Darstellung und reine UI-Interaktion
- View-State / Screen-State / Auswahlzustände
- dünne FFI-Adapter
- DTO-zu-UI-Mapping
- Polling bzw. Orchestrierung nativer Jobs
- reine Darstellungsentscheidungen

### Flutter darf nicht besitzen

Keine neue Fachlogik in Dart implementieren. Insbesondere nicht:

- Schachregeln oder Legalitätslogik
- PGN-/FEN-Parsing
- Ergebnis-/Matt-/Remis-Entscheidungen als fachliche Wahrheit
- Stockfish- oder Analyseentscheidungen
- Move-Klassifikation
- Accuracy-Berechnung
- Theory-/Opening-Entscheidungen
- Cache-Kompatibilität
- Profil-/Provider-Domainlogik
- Datenbank-/Persistenzlogik
- Thread-/Engine-Ressourcenregeln als fachliche Wahrheit

Falls Legacy-Dart-Code noch solche Entscheidungen enthält: nicht erweitern. Wenn der Bereich ohnehin geändert wird, die fachliche Entscheidung nach C++ verschieben und Flutter nur das Ergebnis anzeigen lassen.

### C++ besitzt die fachliche Wahrheit

C++20 ist zuständig für:

- Schachmodell, Züge, Legalität, PGN, SAN und FEN
- Profile und Provider
- lokale Datenbank und Migrationen
- Settings-Persistenz und effektive Engine-Werte
- Game Library, Filter, Statistik und lokale Datenmodelle
- Stockfish-Integration
- Voranalyse, Liveanalyse und Side-Line-Analyse
- MultiPV, `searchmoves`, Cache und Wiederverwendung
- Move-Klassifikation
- Accuracy
- Theory / Opening Book
- Ergebnis-/Termination-Domainstatus
- Import/Export und Offline-Verhalten

## 3. Aktuelle Flutter-Struktur

Seit dem UI-Refactor ist `flutter_app/lib/ui/app_root.dart` nur noch Home-/Shell-Einstieg.
Hauptbereiche liegen getrennt unter `features/`:

```text
flutter_app/lib/
├─ app/
├─ features/
│  ├─ analysis/presentation/
│  ├─ app/application/
│  ├─ favorites/presentation/
│  ├─ games/presentation/
│  ├─ play/presentation/
│  ├─ profile/presentation/
│  ├─ settings/presentation/
│  └─ statistics/presentation/
├─ ffi/
├─ localization/generated/
├─ shared/
├─ ui/
└─ main.dart
```

Neue große Screens niemals wieder in `app_root.dart` sammeln. Jeder eigenständige Screen oder größere UI-Unterbereich bekommt eine passende Datei im zugehörigen Feature.

## 4. Aktuelle native Struktur

```text
native/src/
├─ analysis/       # Accuracy und Move-Klassifikation
├─ api/            # C-ABI
├─ chess/          # PGN/FEN/Züge/Stellungsmodell
├─ core/
├─ diagnostics/
├─ engine/         # ChessEngine / Stockfish
├─ http/
├─ persistence/
├─ providers/
├─ services/       # Analysis, Library, Profiles, Settings, Statistics
└─ theory/
```

Neue Fachlogik in den passendsten nativen Bereich einordnen. Keine parallele zweite Implementierung in Flutter erzeugen.

## 5. Lokalisierung – ausschließlich ARB

Alle sichtbaren UI-Texte werden über Flutter-ARB gepflegt:

```text
flutter_app/l10n/app_en.arb
flutter_app/l10n/app_de.arb
flutter_app/l10n/app_ar.arb
```

Verbindliche Regeln:

- Keine sichtbaren Texte hart in Dart codieren.
- Bei neuen Texten immer alle drei ARB-Dateien aktualisieren.
- `lib/localization/generated/*` niemals manuell bearbeiten; diese Dateien werden generiert.
- Lokalisierungsschlüssel semantisch und stabil benennen.
- Schachnotation, FEN, SAN, UCI und Koordinaten nicht übersetzen.
- Arabisch ändert nur Sprache/Text. Die globale App-Geometrie bleibt bewusst LTR.
- Ein Sprachwechsel auf Arabisch darf Board, Navigation, Spielerpositionen oder Analyse-Layout nicht spiegeln.

## 6. Datei- und Section-Konvention

Jede handgeschriebene Datei, die neu erstellt oder wesentlich bearbeitet wird, muss klar strukturierte Sections besitzen.

### Dart / C++

Mindestens eine benannte Section pro nicht-trivialer Datei, zum Beispiel:

```text
// -----------------------------------------------------------------------------
// Section: Board presentation
// -----------------------------------------------------------------------------
```

oder für C++:

```text
// -----------------------------------------------------------------------------
// Section: Accuracy aggregation
// -----------------------------------------------------------------------------
```

Regeln:

- zusammengehörige Klassen/Funktionen unter derselben Section halten
- große Dateien in mehrere sinnvolle Sections gliedern
- keine Section nur zum Selbstzweck; Namen müssen Inhalt beschreiben
- kleine Forwarder/Exports dürfen eine einzige Section besitzen
- generierte Dateien und vendorte Third-Party-Dateien nicht für diese Konvention verändern

### Markdown

Handgeschriebene Markdown-Dateien mit klaren `##`-/`###`-Abschnitten strukturieren.

## 7. FFI-Grenze

Flutter kommuniziert mit C++ nur über eine stabile C-ABI via `dart:ffi`.

- keine C++-Klassen direkt exportieren
- primitive Typen, UTF-8, opaque handles und serialisierbare DTOs verwenden
- Speicherfreigabe explizit definieren
- keine C++-Exception darf die ABI-Grenze verlassen
- native Langläufer als Job/Status/Cancel-Workflow ausführen
- Flutter darf native Resultate darstellen, aber deren fachliche Bedeutung nicht neu berechnen

## 8. Analyse, Klassifikation und Accuracy

Diese drei Systeme sind getrennte native Verantwortlichkeiten:

- Engine-/Analysewerte kommen aus C++/Stockfish.
- Move-Klassifikation wird ausschließlich nativ berechnet.
- Accuracy wird ausschließlich nativ und unabhängig von den Klassifikationslabels berechnet.
- Flutter zeigt Resultate, Farben, Symbole und Animationen an.
- Eine UI-Änderung darf niemals stillschweigend Analyse-, Classifier- oder Accuracy-Regeln verändern.
- Versionsänderungen von Analyse/Classifier/Accuracy müssen Cache/Persistenz korrekt berücksichtigen.

## 9. Analyse-Persistenz

Pro Partie gilt ein autoritativer gespeicherter Voranalyse-Stand.

- tiefere/höherwertige Analyse darf niedrigere ersetzen
- niedrigere Analyse darf höhere nicht überschreiben
- vorhandene höhere Analyse wird bei niedrigeren Anforderungen wiederverwendet
- bei Upgrade darf vorhandener Stand zur Beschleunigung genutzt werden
- alter autoritativer Stand erst nach erfolgreichem neuen Lauf ersetzen
- globaler Positionscache bleibt davon getrennt

Side-Line-Analyse ist flüchtige Variantenanalyse und darf Hauptlinien-Persistenz nicht überschreiben. Side-Line-Engine-Einstellungen sind separat persistent und verwenden die zuletzt angewendeten Werte.

## 10. UI-Verantwortung

Flutter darf insbesondere folgende Dinge entscheiden:

- wie ein Ergebnis animiert wird
- wo Symbole erscheinen
- welche Boardfarbe dargestellt wird
- wann ein bereits gezeigter UI-Effekt nicht erneut abgespielt wird
- Board-Rotation als Benutzerinteraktion
- Screen-Navigation und Dialoge

C++ entscheidet dagegen, welches fachliche Ergebnis vorliegt.

## 11. Plattformen

Unterstützt:

- Windows
- Android

Keine serverseitige Runtime voraussetzen. Analyse und Kernfunktionen bleiben lokal.

## 12. Stockfish und Lizenzen

Stockfish steht unter GPLv3.

- Lizenz- und Copyright-Hinweise erhalten
- verwendete Version/Commit und NNUE-Netze dokumentieren
- Änderungen und reproduzierbare Buildinformationen erhalten
- Third-Party-Lizenzen in `THIRD_PARTY_NOTICES.md` pflegen
- keine Aussage treffen, eine technische Trennung garantiere automatisch proprietäre Lizenzierbarkeit

## 13. Arbeitsweise bei Änderungen

Vor einer Änderung:

1. passende `AGENTS.md` lesen
2. bestehende Implementierung und Datenflüsse prüfen
3. Verantwortlichkeit bestimmen: UI oder Domain
4. bestehende Schnittstelle erweitern statt Parallelcode bauen

Nach einer Änderung:

1. geänderte Dateien auf Section-Struktur prüfen
2. C++ formatieren/testen, wenn native Logik betroffen ist
3. Flutter formatieren/analyzieren/builden, wenn UI betroffen ist
4. ARB-Dateien validieren, wenn Text geändert wurde
5. Windows-/Android-Build soweit Toolchain verfügbar prüfen
6. bekannte Warnungen von echten Fehlern unterscheiden

## 14. Nicht tun

- keine Fachlogik aus Bequemlichkeit in Flutter duplizieren
- keine Übersetzungen direkt in Dart schreiben
- keine generierten Localization-Dateien manuell bearbeiten
- keine bestehenden Caches/Persistenzregeln umgehen
- keine Stockfish-Parameter im UI hart überschreiben, wenn Settings existieren
- keine fremden Markenassets oder proprietären Klassifikationssysteme kopieren
- keine Engine-Hilfe für laufende Online-Partien anbieten
