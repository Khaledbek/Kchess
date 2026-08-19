# AGENTS.md

## 1. Zweck dieses Projekts

Dieses Repository enthält eine lokale, plattformübergreifende Schach-Analyse-App für **Android und Windows**.

Die App soll:
- öffentliche Partien eines gespeicherten Chess.com- oder Lichess-Benutzernamens laden,
- PGN-Dateien und eingefügten PGN-Text importieren,
- FEN-Stellungen importieren,
- Partien lokal speichern und für Offline-Nutzung herunterladen,
- Partien vollständig lokal mit Stockfish analysieren,
- mehrere lokale Profile/Konten verwalten,
- Deutsch, Englisch und Arabisch unterstützen,
- ohne eigenen Backend-Server funktionieren.

Die Anwendung ist **keine Spielhilfe für laufende Online-Partien**. Engine-Analyse ist nur für abgeschlossene/importierte Partien und frei eingegebene Stellungen vorgesehen.

---

## 2. Verbindliche Technologie- und Architekturregeln

### Flutter / Dart
Dart ist ausschließlich für die Präsentationsschicht vorgesehen:
- Flutter Widgets
- Navigation
- Responsive Layout
- Theme/Design
- Lokalisierung
- UI-State/View-State
- dünne FFI-Adapter und Mapping zwischen nativen DTOs und UI-Modellen

Dart darf **keine wesentliche Domain-, Engine-, Netzwerk-, Parsing- oder Persistenzlogik** enthalten.

### C++ Runtime Core
Die eigentliche Laufzeitlogik wird in **C++20** implementiert:
- Profilverwaltung
- lokale Datenbank
- Chess.com-Provider
- Lichess-Provider
- Download-/Cache-Logik
- PGN-/FEN-Verarbeitung
- Spiel-/Stellungsmodell
- Stockfish-Ansteuerung
- Analysepipeline
- MultiPV/Lines
- Move-Klassifikation
- Accuracy-Berechnung der App
- Favoriten
- Filter/Sortierung
- Engine-Einstellungen
- App-Einstellungen
- Import/Export
- Offline-Verhalten

### Python
Python ist nur für Entwicklungswerkzeuge zulässig:
- Build-Skripte
- Testdatengeneratoren
- Fixtures
- lokale Datenmigrationstests
- Hilfs- und Validierungsskripte

**Keine Python-Runtime in der ausgelieferten Android- oder Windows-App einbetten.**

### Native Schnittstelle
Flutter kommuniziert mit C++ über eine stabile **C-ABI via `dart:ffi`**.

Keine C++-Klassen direkt über FFI exponieren. Stattdessen:
- opaque handles
- primitive Typen
- UTF-8 Strings
- serialisierbare DTOs
- explizite Speicherfreigabe
- asynchrone native Jobs mit Callback/Event-Bridge

Alle FFI-Funktionen müssen fehlertolerant sein und dürfen keine C++-Exceptions über die ABI-Grenze werfen.

---

## 3. Zielplattformen

Verbindlich:
- Android
- Windows

Nicht Teil von V1:
- Web
- iOS
- macOS
- Linux

Die Architektur darf spätere Plattformen nicht absichtlich blockieren.

---

## 4. Empfohlene Repository-Struktur

```text
/
├─ AGENTS.md
├─ CODEX_PROMPT.md
├─ IMG_REQUIRED.md
├─ README.md
├─ LICENSE
├─ THIRD_PARTY_NOTICES.md
├─ img/
│  └─ ... vom Benutzer bereitgestellte Assets
├─ docs/
│  ├─ PRODUCT_SPEC.md
│  ├─ ARCHITECTURE.md
│  ├─ LICENSE_COMPLIANCE.md
│  └─ API_NOTES.md
├─ flutter_app/
│  ├─ lib/
│  │  ├─ app/
│  │  ├─ ui/
│  │  ├─ localization/
│  │  ├─ theme/
│  │  ├─ ffi/
│  │  └─ view_models/
│  ├─ l10n/
│  ├─ android/
│  └─ windows/
├─ native/
│  ├─ include/
│  ├─ src/
│  │  ├─ api/
│  │  ├─ core/
│  │  ├─ chess/
│  │  ├─ engine/
│  │  ├─ providers/
│  │  ├─ persistence/
│  │  ├─ analysis/
│  │  └─ settings/
│  ├─ tests/
│  └─ CMakeLists.txt
├─ third_party/
│  └─ stockfish/
├─ licenses/
│  ├─ stockfish/
│  └─ dependencies/
└─ tools/
   └─ python/
```

Die genaue Struktur darf verbessert werden, solange die Schichtentrennung erhalten bleibt.

---

## 5. Stockfish und GPLv3 – verbindliche Regeln

Stockfish steht unter **GNU GPLv3**.

Offizielle Referenzen:
- https://stockfishchess.org/
- https://stockfishchess.org/about/
- https://github.com/official-stockfish/Stockfish
- https://github.com/official-stockfish/Stockfish/blob/master/Copying.txt

### Bei Entwicklung
- Stockfish-Code niemals als eigenen Code ausgeben.
- `AUTHORS`, GPL-Lizenztext und Copyright-Hinweise erhalten.
- verwendete Stockfish-Version bzw. exakten Git-Commit dokumentieren.
- verwendete NNUE-Datei bzw. eingebettetes Netz dokumentieren.
- lokale Änderungen an Stockfish separat und nachvollziehbar halten.
- Build-Flags, Toolchain und Schritte dokumentieren, mit denen die ausgelieferte Binary reproduziert werden kann.

### Bei Distribution
Wenn eine Stockfish-Binary oder ein abgeleitetes Stockfish-Werk mit der App verteilt wird:
- GPLv3-Lizenz muss mitgeliefert werden.
- der zur **exakt ausgelieferten Binary passende vollständige Quellcode** muss verfügbar sein oder es muss ein GPL-konformer Verweis/Mechanismus zur Bereitstellung existieren.
- eigene Änderungen an Stockfish müssen ebenfalls unter GPLv3 verfügbar gemacht werden.
- alle zum Erzeugen der ausgelieferten Stockfish-Binary notwendigen Änderungen und Build-Informationen müssen erhalten bleiben.
- keine zusätzlichen technischen oder vertraglichen Beschränkungen einführen, die den GPL-Rechten widersprechen.

### Architektur und Lizenzgrenze
Bevorzugt wird eine klare UCI-basierte Trennung zwischen App-Core und Stockfish.

WICHTIG:
- Eine Prozess-/UCI-Grenze ist **keine automatische rechtliche Garantie**, dass beliebiger proprietärer App-Code außerhalb der GPL bleibt.
- Wenn Stockfish statisch/dynamisch in ein gemeinsames Programm gelinkt oder Quellcode direkt integriert wird, ist vor einer proprietären Veröffentlichung eine gesonderte Lizenzprüfung erforderlich.
- Die einfachste rechtliche Veröffentlichungsstrategie ist, das gesamte kombinierte Werk GPLv3-kompatibel/Open Source zu veröffentlichen.
- Codex darf **nicht** behaupten, eine bestimmte technische Trennung mache die UI sicher Closed Source.
- Vor einer öffentlichen Closed-Source-Veröffentlichung ist eine echte Lizenz-/Rechtsprüfung ein Release-Gate.

Diese Regeln sind technische Compliance-Vorgaben, keine Rechtsberatung.

---

## 6. Drittanbieter-Abhängigkeiten

Neue Produktionsabhängigkeiten nur hinzufügen, wenn sie:
1. notwendig sind,
2. aktiv gepflegt sind,
3. plattformübergreifend funktionieren,
4. eine mit dem Projekt kompatible Lizenz haben.

Bevorzugt: MIT, BSD, Apache-2.0, zlib, public domain oder andere permissive Lizenzen.

Jede Produktionsabhängigkeit muss in `THIRD_PARTY_NOTICES.md` dokumentiert werden:
- Name
- Version/Commit
- Lizenz
- Quelle
- Verwendungszweck

Vor dem Vendoring Lizenz erneut prüfen.

---

## 7. Provider-Abstraktion

Es muss eine gemeinsame Provider-Schnittstelle geben.

Beispiel:

```cpp
class GameProvider {
public:
    virtual ~GameProvider() = default;
    virtual ProviderProfile fetch_profile(...) = 0;
    virtual ProviderStats fetch_stats(...) = 0;
    virtual GamePage fetch_games(...) = 0;
};
```

Implementierungen:
- `ChessComProvider`
- `LichessProvider`
- `LocalPgnFenProvider`

Provider-spezifische JSON-Strukturen dürfen nicht in die UI durchsickern.

---

## 8. Chess.com-Regeln

Nur die offizielle PubAPI verwenden.

Relevante offizielle Dokumentation:
https://www.chess.com/news/view/published-data-api

V1 nutzt insbesondere:
- Spielerprofil
- Spielerstatistiken
- Monatsarchive
- PGN/Game-Daten
- vorhandene Accuracy-Werte, falls die API sie liefert

Regeln:
- keine Website scrapen
- Requests seriell/vernünftig begrenzen
- `429` sauber behandeln
- aussagekräftigen User-Agent verwenden
- HTTP-Caching mit ETag/Last-Modified unterstützen
- `avatar` nur verwenden, wenn die API tatsächlich eine Avatar-URL liefert
- fehlende Felder immer als optional behandeln
- Provider-Accuracy darf `null` sein

### Chess.com Marken-/Designschutz
Nicht kopieren:
- Chess.com Brett-Farbpaletten
- Chess.com Figuren-Designs
- Chess.com Sounds
- Chess.com Move-Classification-Glyphen
- proprietäre Standard-Avatare oder Logos ohne passende Nutzungsrechte

Eigene neutrale Fallback-Grafiken aus `img/` verwenden.

---

## 9. Lichess-Regeln

Nur offizielle Lichess-API verwenden.

Offizielle Referenzen:
- https://lichess.org/api
- https://lichess.org/page/api-tips
- https://github.com/lichess-org/api

Relevante V1-Endpunkte:
- `/api/user/{username}`
- `/api/user/{username}/perf/{perf}`
- `/api/games/user/{username}`

Regeln:
- nur einen API-Request gleichzeitig
- bei `429` mindestens entsprechend offizieller Vorgaben warten und Frequenz reduzieren
- Game-Export streamend verarbeiten
- `since`/`until` für Monatsabfragen verwenden
- `accuracy=true` nur als optionalen Providerwert behandeln
- keine Engine-Unterstützung für laufende Online-Partien anbieten

Lichess' öffentliche User-Struktur hat kein klassisches Avatar-Feld.
Wenn kein vom Benutzer bereitgestelltes lokales Bild existiert:
- `img/provider_lichess_fallback.*` verwenden.
- optional Lichess `flair` als separates Metadatum anzeigen, aber nicht als erfundenes Profilfoto behandeln.

---

## 10. Profile / Konten

Profiltypen:
```text
CHESS_COM
LICHESS
LOCAL_PGN_FEN
```

Jedes Profil hat mindestens:
- interne UUID
- Typ
- Anzeigename
- Provider-Benutzername, falls online
- Avatar/Fallback-Asset
- Erstellungszeit
- zuletzt geöffnet
- Sync-Metadaten
- lokale Einstellungen, falls später erforderlich

Regeln:
- Bei Chess.com/Lichess wird der eingegebene Benutzername standardmäßig als Anzeigename übernommen.
- Bei LOCAL_PGN_FEN ist die Eingabe ein frei wählbarer Profilname.
- Mehrere Profile desselben Providers sind erlaubt.
- Das zuletzt geöffnete Profil wird beim nächsten App-Start automatisch geladen.
- Keine Passwörter speichern.
- V1 benötigt keinen Chess.com-/Lichess-Login und keine OAuth-Tokens für die beschriebenen öffentlichen Funktionen.

---

## 11. Lokale Persistenz

Die App muss ohne Backend-Server funktionieren.

Bevorzugt:
- SQLite für strukturierte Daten
- App-private Dateien für große PGN/Analyseartefakte, falls sinnvoll

Mindestens folgende Entitäten vorsehen:
- profiles
- games
- game_sources
- downloads
- favorites
- analysis_runs
- move_analysis
- provider_stats_cache
- provider_sync_state
- engine_settings
- app_settings

Datenbankmigrationen versionieren.

Keine Daten eines Profils versehentlich einem anderen Profil zuordnen.

---

## 12. Offline-Verhalten

Ein Spiel ist offline verfügbar, wenn es explizit heruntergeladen/importiert wurde.

Download muss mindestens speichern:
- vollständige PGN
- Metadaten
- Spieler
- Ratings
- Ergebnis
- Zeitkontrolle
- Provider-ID/URL, wenn vorhanden
- vorhandene Provider-Accuracy
- lokale Analyse, sobald durchgeführt

Favorit und Download sind getrennte Zustände.

Fehlt Internet:
- lokale Profile vollständig nutzbar
- heruntergeladene Spiele vollständig nutzbar
- Analyse vollständig lokal nutzbar
- Online-Sync zeigt einen nicht störenden Offline-Zustand statt Fehlerloops

---

## 13. PGN/FEN

Unterstützen:
- PGN aus Zwischenablage
- `.pgn` Datei
- FEN aus Zwischenablage/Textfeld
- später optional weitere Formate

PGN-Parser muss robust mit:
- Tags
- SAN-Zügen
- Kommentaren
- NAGs
- Varianten
- Ergebnissen
- SetUp/FEN-PGNs
umgehen können.

Importierte PGN/FEN-Inhalte werden dem ausgewählten LOCAL_PGN_FEN-Profil zugeordnet.

Jede analysierte importierte Partie bleibt im lokalen Profil gespeichert.

---

## 14. Engine-Abstraktion

Kein UI-Code darf Stockfish direkt ansprechen.

```cpp
struct AnalysisRequest {
    std::string fen;
    int depth;
    int multi_pv;
    int threads;
    int hash_mb;
};

struct EngineLine {
    int multipv;
    std::string score;
    std::vector<std::string> moves;
};

struct AnalysisResult {
    std::vector<EngineLine> lines;
    std::string best_move;
};
```

Engine-Layer:
- `ChessEngine` Interface
- `StockfishEngine` Implementierung

Unterstützen:
- Start/Stop
- `uci`
- `isready`
- Positionssetzen
- Analyse
- MultiPV
- Abbruch
- Fortschritt
- Mate-Scores
- Centipawn-Scores
- wenn verfügbar WDL-Daten

Analyse darf UI-Thread niemals blockieren.


### Automatische Vollanalyse beim Öffnen einer Partie
Beim Öffnen einer gespeicherten oder online geladenen Partie:
1. Analyseansicht sofort öffnen.
2. Partie und Brett sofort anzeigen; nicht auf die Engine warten.
3. Falls noch keine vollständige lokale Analyse in der aktuell relevanten Analyseversion vorhanden ist, automatisch eine Vollanalyse starten.
4. Jeden Halbzug der Partie analysieren.
5. Analysefortschritt in der UI anzeigen.
6. Bereits berechnete Züge sofort interaktiv nutzbar machen.
7. Benutzer darf während der Analyse vor/zurück navigieren.
8. Analyse muss abbrechbar sein und den UI-Thread niemals blockieren.
9. Ergebnisse pro Zug inkrementell persistieren.
10. Nach erfolgreichem Abschluss automatisch eine kompakte Analyse-Zusammenfassung als Dialog/Bottom-Sheet öffnen.

Wenn eine vollständige kompatible Analyse bereits lokal gespeichert ist:
- keine unnötige Neuberechnung starten,
- Analyseansicht sofort aus dem Cache aufbauen,
- Zusammenfassung weiterhin über einen UI-Button erneut aufrufbar machen.

### Analyse-Zusammenfassung nach Abschluss
Nach vollständiger Partieanalyse erscheint ein kleines, nicht-blockierendes Zusammenfassungsfenster.

Mindestens anzeigen:
- lokale Accuracy des Profilspielers, falls berechnet
- Anzahl Theory
- Anzahl Brilliant
- Anzahl Best
- Anzahl Excellent
- Anzahl Okay
- Anzahl Miss
- Anzahl Mistake
- Anzahl Blunder
- optional getrennte Zahlen für beide Spieler
- Gesamtzahl analysierter Halbzüge
- Analyseparameter/Engine-Tiefe kompakt in Details

Das Fenster muss schließbar sein und später erneut aus der Analyseansicht geöffnet werden können.

---

## 15. Standard-Engine-Einstellungen

V1 soll einen "Mittel"-Preset haben.

Ausgangspunkt:
- Depth: 18
- MultiPV / Lines: 3
- Threads Android: 2
- Threads Windows: 4
- Hash Android: 128 MB
- Hash Windows: 256 MB
- Ponder: aus

Diese Werte sind Startwerte, keine unveränderlichen Konstanten.
Grenzen anhand realer Geräte validieren.

Engine-Einstellungsseite:
- Preset: Niedrig / Mittel / Hoch / Benutzerdefiniert
- Depth
- Lines / MultiPV
- Threads
- Hash
- optional Analysezeit pro Zug
- Brett-Pfeile anzeigen: Ein/Aus
- Reset auf Standard

`Brett-Pfeile anzeigen` ist eine reine Darstellungseinstellung:
- Standard: Ein
- zeigt bei aktivierter Option den besten Zug bzw. die aktuell ausgewählte Engine-Line als Pfeil(e) auf dem Brett
- bei Deaktivierung keinerlei Analyse ändern oder neu starten
- Einstellung persistent speichern

Ungültige oder gerätegefährdende Werte begrenzen.

---

## 16. Eigene Accuracy und Move-Klassifikation

Die App darf Provider-Accuracy anzeigen, wenn vorhanden.

Zusätzlich soll sie eine **eigene lokale Analysebewertung** erzeugen, damit:
- Offline-Partien funktionieren
- Chess.com/Lichess konsistent behandelt werden
- fehlende Provider-Accuracy ersetzt werden kann

Eigene Accuracy und Klassifikation dürfen **nicht als Chess.com-Accuracy** bezeichnet werden.

Move-Kategorien für V1:
- Theory
- Brilliant
- Best
- Excellent
- Okay
- Miss
- Mistake
- Blunder

Wichtig:
- eigene Farben
- eigene Symbole
- eigene Heuristiken
- keine Chess.com-Formeln/Glyphen kopieren

Die Klassifikationslogik gehört in ein separates, testbares C++-Modul.
Schwellenwerte zentral konfigurierbar halten.

### Theory / Eröffnungstheorie
`Theory` ist eine eigene Zugklasse und hat Vorrang vor der normalen Engine-Klassifikation, wenn der gespielte Zug eindeutig in einer lokal verfügbaren Eröffnungstheorie-/Opening-Book-Datenquelle enthalten ist.

Regeln:
- Theory-Erkennung muss vollständig lokal funktionieren.
- Keine laufende Online-Abfrage ist für die Klassifikation erforderlich.
- Die verwendete Opening-Book-/Theorie-Datenquelle muss lizenzrechtlich mit dem Projekt kompatibel sein.
- Quelle, Version und Lizenz der Theorie-Daten müssen in `THIRD_PARTY_NOTICES.md` dokumentiert werden.
- Keine proprietären Chess.com-/Lichess-Eröffnungsklassifikationen kopieren.
- Falls keine valide Theory-Datenquelle vorhanden ist, darf Codex keine Theory-Züge erfinden; die Schnittstelle bleibt vorbereitet und die Funktion wird klar als noch nicht verfügbar markiert.
- Nach Verlassen bekannter Theorie wird normal mit Stockfish klassifiziert.

Wenn eine Kategorie nicht zuverlässig bestimmt werden kann, lieber konservativ klassifizieren statt eine künstliche "Brilliant"-Markierung zu erzeugen.

---

## 17. Lokalisierung

Sprachen:
- Deutsch
- Englisch
- Arabisch

Flutter ARB verwenden:
```text
app_de.arb
app_en.arb
app_ar.arb
```

Keine sichtbaren UI-Texte hart codieren.

Arabisch:
- RTL vollständig unterstützen
- Sidebar/Drawer, Icons und Textausrichtung prüfen
- Zahlen/Schachnotation sinnvoll lesbar halten
- Brettkoordinaten bleiben Schachnotation und werden nicht übersetzt

---

## 18. Bilder / Assets

Der Benutzer legt alle benötigten eigenen Assets im Root-Unterordner:

```text
img/
```

ab.

Codex soll Assets **nicht visuell analysieren** und keine Bilderkennung/OCR verwenden.

Assets werden ausschließlich anhand:
- Dateiname
- Extension
- expliziter Zuordnung in `IMG_REQUIRED.md`

verwendet.

Wenn ein Asset fehlt:
- keine Datei erfinden
- keine Online-Grafik ungefragt herunterladen
- einen neutralen programmatischen Flutter-Platzhalter anzeigen
- fehlendes Asset in der Abschlussmeldung nennen

Keine fremden Markenassets kopieren.

---

## 19. UI-Grundregeln

- Material 3 als technische Basis ist erlaubt.
- Design muss eigenständig sein.
- Android und Windows responsive.
- Desktop: dauerhafte Sidebar ist bevorzugt.
- Android: Drawer/kompakte Navigation mit denselben Funktionen.
- Touch-Ziele auf Android ausreichend groß.
- Maus/Scrollrad/Tastatur auf Windows berücksichtigen.
- keine UI-Logik duplizieren.
- Brett bleibt bei Analyse sichtbar, wenn unterer Analysebereich scrollt.
- Steuerleiste bleibt beim Scrollen sichtbar.

---

## 20. Qualität und Tests

C++:
- Unit Tests für PGN/FEN
- Provider-Parser mit Fixtures
- Move-Klassifikation
- Datenbankmigrationen
- Filter/Sortierung
- Analyse-DTOs
- Fehlerfälle

Flutter:
- Widget Tests für First Run
- Profilwechsel
- Spieleliste
- Filter
- Analyseansicht
- RTL
- Light/Dark

Integration:
- Mock-Provider verwenden
- Tests dürfen nicht dauerhaft von echten API-Servern abhängen

Vor Abschluss einer Aufgabe:
1. formatieren
2. statische Analyse ausführen
3. native Tests ausführen
4. Flutter Tests ausführen
5. Android-Build prüfen, soweit Toolchain vorhanden
6. Windows-Build prüfen, soweit Toolchain vorhanden
7. neue Dependencies/Lizenzen dokumentieren

---

## 21. Sicherheits- und Fair-Play-Regeln

- Keine Engine-Hilfe für laufende Chess.com-/Lichess-Partien.
- Keine Funktion zum automatischen Spielen oder Senden von Zügen.
- Keine Zugangsdaten abfragen, wenn nicht zwingend erforderlich.
- Keine API-Tokens hardcoden.
- Keine Secrets ins Repository.
- Netzwerkdaten als untrusted input behandeln.
- PGN/FEN validieren.
- Dateipfade gegen Traversal absichern.

---

## 22. Arbeitsweise von Codex

Bei jeder größeren Aufgabe:
1. zuerst vorhandene Struktur und `docs/PRODUCT_SPEC.md` lesen,
2. betroffene Schichten identifizieren,
3. bestehende Schnittstellen erweitern statt parallele Implementierungen erzeugen,
4. Tests zuerst oder zusammen mit der Funktion ergänzen,
5. keine unnötigen Produktionsabhängigkeiten hinzufügen,
6. Lizenzfolgen jeder neuen Abhängigkeit prüfen,
7. nach Änderungen kurz dokumentieren:
   - was geändert wurde,
   - welche Tests liefen,
   - welche bekannten offenen Punkte bestehen.

Nicht eigenmächtig Produktanforderungen streichen.

Wenn eine Anforderung technisch nicht exakt umsetzbar ist:
- robuste Fallback-Lösung implementieren,
- Abweichung dokumentieren,
- keine erfundenen Providerdaten verwenden.

---

## 23. Definition of Done für V1

V1 ist erst fertig, wenn:
- First-Run-Profilanlage funktioniert
- mehrere Profile gespeichert/wechselbar sind
- letztes Profil beim Neustart geladen wird
- Chess.com aktuelle Monatsgames lädt
- Lichess aktuelle Monatsgames lädt
- lokaler PGN-Import funktioniert
- FEN-Import funktioniert
- Downloads offline funktionieren
- Favoriten funktionieren
- Spielefilter funktionieren
- Stockfish lokal jeden Halbzug einer geöffneten Partie vollständig analysiert
- Analyseansicht inkl. Board, Lines, Theory-/Move-Klassifikation, Best Move und 5 Steuerbuttons funktioniert
- automatische Analyse-Zusammenfassung nach vollständiger Partieanalyse funktioniert
- Engine-Einstellungen inklusive Brett-Pfeile Ein/Aus gespeichert werden
- DE/EN/AR inkl. RTL funktionieren
- Light/Dark/System Theme funktioniert
- Android-Build funktioniert
- Windows-Build funktioniert
- Stockfish/GPL-Dokumentation vorhanden ist
- Drittanbieter-Lizenzen dokumentiert sind
