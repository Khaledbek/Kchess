# KChess Product Specification

## 1. Produkt

KChess ist eine lokale Schach-App für Android und Windows. Die Oberfläche läuft in Flutter; Schach-, Analyse-, Provider-, Trainings- und Persistenzlogik läuft im nativen C++20-Core. Python ist nur Development-/Builder-Tooling. Ein eigener Server ist nicht erforderlich.

Unterstützte Profilquellen:

1. Chess.com
2. Lichess
3. lokales PGN/FEN-Profil

## 2. Start und Profile

Beim Start:

1. Flutter-Splash anzeigen.
2. nativen Core initialisieren.
3. SQLite öffnen und Migrationen anwenden.
4. Settings/Profile laden.
5. letztes aktives Profil wiederherstellen.
6. bei fehlendem Profil First-Run-Onboarding öffnen.

Online-Profile verwenden ausschließlich öffentliche Providerdaten; keine Passwort- oder Token-Anmeldung. Lokale Profile funktionieren vollständig offline.

Profile können gewechselt und gelöscht werden. Lokale PGN/FEN-Profile können in ein Online-Profil zusammengeführt werden, soweit die bestehende Merge-Logik dies zulässt.

## 3. Hauptnavigation

Aktuelle Reihenfolge:

1. Spiele
2. Play
3. Training
4. Favoriten
5. Statistik
6. Einstellungen

Auf breitem Windows-Layout erscheint eine NavigationRail/Sidebar; auf schmaleren Layouts ein Drawer. Der Profilkopf öffnet die Profilseite.

Es gibt **keinen separaten Downloads-Navigationspunkt** mehr.

## 4. Spiele und Bibliothek

### Online-Profile

Der Games-Bereich zeigt synchronisierte Provider-Partien mit Suche, Filterung, Sortierung und Monatsauswahl. Providerdaten werden nativ gecacht und können bei aktivierter Einstellung automatisch synchronisiert werden.

### Lokales PGN/FEN

Importmöglichkeiten:

- PGN-Text
- PGN-Datei
- FEN

PGN/FEN-Import wird der lokalen PGN/FEN-Bibliothek zugeordnet, auch wenn ein Online-Profil aktiv ist. Der zuvor aktive Profilkontext bleibt erhalten.

### Spieleinträge

Je nach verfügbarer Information können unter anderem angezeigt werden:

- Spieler und Ratings
- Farbe/Perspektive
- Datum/Zeitkontrolle
- Ergebnis
- Favoritenstatus
- Analyse-/Accuracy-Status
- Opening/ECO

## 5. Favoriten und Downloads

Favoriten sind lokal persistent und können Sammlungen zugeordnet werden.

Downloads sind heute eine Kompatibilitäts-/Sammlungssemantik:

- „lokal speichern“ einer Online-Partie markiert sie als Favorit
- die Partie wird der obersten Sammlung `Downloads` zugeordnet
- `downloaded` wird aus dieser Mitgliedschaft abgeleitet
- Legacy-Downloaddaten werden nativ migriert/kompatibel interpretiert

Favoriten und Downloads sind damit keine getrennten Datenwelten mehr.

## 6. Play

Der Play-Bereich enthält aktuell:

- gegen einen lokalen Bot spielen
- Bot-Spielprotokoll ansehen

Setup, Zugauswahl, Engine-/Bot-Logik, Ergebnisstatus und Speicherung kommen aus dem nativen Core. Flutter übernimmt Darstellung und Interaktion.

## 7. Training

Training umfasst aktuell:

- **Opening Lab** – Eröffnungsbaum und Repertoire-/Katalogtraining
- **Blunder Buster** – taktisch/fehlerorientiertes Training
- **Endgame Academy** – strukturierte Endspiel-Drills
- **Endgame Studies** – eingebettete Studien

Der native Core besitzt Katalog, legale Stellungen, Lösungszüge, Zugvalidierung, Sessionstatus, Fortschritt und Meisterschaftsregeln. Flutter zeigt Dashboard, Baum, Board und Fortschritt.

## 8. Analyse

Eine gespeicherte Partie oder Position kann lokal analysiert werden.

Funktionen:

- Stockfish 18 oder Stockfish 19 auswählbar
- Hauptlinienanalyse
- MultiPV
- Best-Move-Pfeil
- Evaluation/WDL-Darstellung
- Move-Klassifikation
- lokale Accuracy
- Theory-/Opening-Erkennung
- PGN-/Zugnavigation
- temporäre Side-Lines als Variantenbaum
- Rückkehr zur Hauptlinie
- Boardrotation
- Result-/Endgame-Symbole

Side-Line-Analyse ist flüchtig und darf den persistierten Hauptanalyse-Stand nicht überschreiben.

## 9. Klassifikation

Die Klassifikation ist eine eigene native KChess-Logik und kein kopiertes Fremdsystem.

Unterstützte sichtbare Kategorien umfassen unter anderem:

- Theory
- Brilliant
- Great
- Best
- Excellent
- Okay
- Miss
- Mistake
- Blunder

`Best` muss mit einem tatsächlich validierten Engine-Bestmove konsistent sein. Brilliant/Great werden konservativ und engine-/positionsabhängig nativ bestimmt. SF18 und SF19 dürfen getrennte native Kalibrierungen besitzen.

## 10. Accuracy

Provider-Accuracy und lokale KChess-Accuracy sind getrennte Quellen.

- Providerwerte nur als Providerdaten behandeln.
- lokale Accuracy wird aus der nativen Analyse berechnet.
- lokale Accuracy nicht als offizielle Chess.com-/Lichess-Accuracy bezeichnen.
- Accuracy bleibt unabhängig von den Klassifikationslabels.

## 11. Statistik

Der Statistikbereich aggregiert lokale und synchronisierte Partien nativ. Aktuelle UI-Bereiche umfassen:

- Overview
- Form
- Rating
- Termination
- Spielphasen
- Openings
- einzelne Opening-Partien
- Spielervergleich

Flutter stellt diese Daten dar und soll keine parallele Statistik-Domainlogik erzeugen.

## 12. Profilseite

Online-Profile zeigen nur Daten, die von der öffentlichen API oder aus lokal gespeicherten Partien ableitbar sind. Dazu gehören je nach Provider/Performance beispielsweise Rating, Titel und verfügbare Performance-/Partiedaten.

Lokale PGN/FEN-Profile besitzen keine erfundene Providerstatistik.

## 13. Einstellungen

Aktuelle Settings-Kategorien:

1. Engine
2. Analyse
3. Design
4. Allgemein
5. Daten & Speicher

### Engine

- Auswahl Stockfish 18 / Stockfish 19
- Threads/Hash bzw. native Engine-Ressourcenwerte
- persistente Auswahl

### Analyse

Analysebudgets, Tiefe/Lines/MultiPV und Side-Line-bezogene Werte werden über die bestehende Settings-Pipeline verwaltet.

### Design

System-/Hell-/Dunkelmodus und UI-Darstellungsoptionen.

### Allgemein

Sprache sowie Provider-/App-Verhalten wie automatische Online-Synchronisierung.

### Daten & Speicher

Cache-/lokale Datenspeicheraktionen mit klarer Trennung zwischen Cache, Partien, Favoriten und persistierter Analyse.

## 14. Lokalisierung

Unterstützte UI-Sprachen:

- Deutsch
- English
- العربية

Alle sichtbaren Flutter-Texte gehören in:

- `flutter_app/l10n/app_de.arb`
- `flutter_app/l10n/app_en.arb`
- `flutter_app/l10n/app_ar.arb`

Arabisch ändert Text/Sprache, nicht die globale LTR-Geometrie. Board, Navigation und Spielerorientierung werden nicht automatisch gespiegelt.

## 15. Provider und Netzwerk

### Chess.com

Öffentliche Profil-, Statistik- und Monats-/Game-APIs werden nativ angesprochen. Cache-Header und Rate-Limiting respektieren.

### Lichess

Öffentliche User-/Performance-/Games-APIs werden nativ angesprochen. NDJSON/Games-Verarbeitung und Rate-Limiting bleiben im C++-Providerlayer.

Netzwerkfehler dürfen lokale Daten nicht löschen. Online-Synchronisierung ist optional; lokale Analyse/Training/Import funktionieren ohne eigenen Server.

## 16. Persistenz

SQLite speichert unter anderem:

- Profile und aktives Profil
- Settings
- Spiele und Provider-Caches
- Favoriten/Sammlungen
- Analyseergebnisse und Cache-Metadaten
- Trainingsfortschritt

Migrationen müssen ältere lokale Daten erhalten, soweit technisch möglich.

## 17. Assets und Opening-Daten

KChess verwendet lokale Assets für Board/Icons/Provider-Fallbacks sowie zwei offline genutzte Opening-Datenartefakte:

- `opening_book.kcb` – statistische Theory-/Move-Daten
- `opening_names.kco` – ECO-/Eröffnungsnamenindex

Die Builder liegen unter `tools/` und sind Python-Development-Tools; Python wird nicht mit der App ausgeliefert.

## 18. Plattformen

Aktuell unterstützt:

- Windows x64
- Android ARM64

Nicht als aktuelle Runtime-Ziele behandeln:

- iOS
- Web
- macOS
- Linux

## 19. Nicht-Ziele

- Züge an Chess.com/Lichess senden
- Live-Engine-Unterstützung für laufende Online-Partien
- eigener Cloud-Server
- Cloud-Sync als zentrale Voraussetzung
- Social Feed/Chat
- Werbung/Zahlungen
- proprietäre Fremdklassifikationen kopieren

## 20. Technische Priorität

Bei Widersprüchen zwischen älteren Texten und dem aktuellen Quellcode gilt:

1. aktuelle `AGENTS.md`-Regeln
2. `docs/ARCHITECTURE.md`
3. diese Produktspezifikation
4. tatsächliche stabile C-ABI-/Datenbank-Kompatibilität

Veraltete historische Branch-/Update-Bezeichnungen sind keine Architekturquelle mehr.
