# PRODUCT_SPEC.md

## 1. Produktidee

Lokale Schach-Analyse-App für Android und Windows mit einer gemeinsamen Flutter-Oberfläche und einem nativen C++-Core. Stockfish analysiert vollständig lokal auf dem Gerät. Die App benötigt keinen eigenen Server.

Hauptquellen:
1. Chess.com-Benutzerprofil
2. Lichess-Benutzerprofil
3. lokales PGN/FEN-Profil

---

## 2. App-Start / Splash

Bei jedem Start:
1. Splash-/Ladeseite anzeigen.
2. native C++-Core initialisieren.
3. lokale Datenbank öffnen/migrieren.
4. Einstellungen und Profile laden.
5. letztes aktives Profil bestimmen.

Wenn noch kein Profil existiert:
- First-Run-Onboarding öffnen.

Wenn Profile existieren:
- zuletzt verwendetes Profil öffnen.

Der Splash darf nicht künstlich verlängert werden.

---

## 3. First-Run-Onboarding

Nur beim ersten Start ohne vorhandenes Profil.

Elemente:
- App-Titel/Logo
- Eingabefeld
- Auswahl des Profiltyps:
  - Chess.com
  - Lichess
  - PGN/FEN lokal
- Button zum Fortfahren

### Verhalten Chess.com
- Feldlabel: Benutzername
- öffentliche Profil-API prüfen
- bei Erfolg Profil anlegen
- Anzeigename = Benutzername
- Avatar-URL speichern, falls API `avatar` liefert
- sonst lokales `provider_chesscom_fallback` Asset
- keine Anmeldung/Passworteingabe

### Verhalten Lichess
- Feldlabel: Benutzername
- öffentliche User-API prüfen
- bei Erfolg Profil anlegen
- Anzeigename = Benutzername
- kein erfundenes Avatar-Feld
- lokales `provider_lichess_fallback` Asset verwenden
- optional Lichess-Flair als separates kleines Metadatum

### Verhalten PGN/FEN
- Feldlabel: Profilname
- kein Netzwerkzugriff
- lokales Schach-Profil anlegen
- `profile_local_chess` Asset verwenden

---

## 4. Profil-/Navigationsleiste

### Windows
Linke permanente Sidebar.

Oben:
- runder Avatar
- daneben Profilname
- Klick auf Profilkopf öffnet Profilseite

Darunter:
- `+ Konto anlegen`
- `Konto wechseln`

Navigation:
- Spiele
- Downloads
- Favoriten
- Profil
- Einstellungen

### Android
Gleiche Informationsarchitektur als Drawer/kompakte Navigation.

Profilwechsel:
- Liste aller lokalen Profile
- Avatar
- Name
- Providerkennzeichnung
- Auswahl setzt Profil als aktiv
- aktive UUID persistent speichern

---

## 5. Profilseite – Chess.com / Lichess

Nur Daten anzeigen, die über offizielle API oder lokal berechenbar sind.

Oben:
- Avatar/Fallback
- Benutzername
- Provider
- optional Titel
- optionale öffentliche Metadaten

Zeitformat/Variante auswählbar, z. B.:
- Bullet
- Blitz
- Rapid
- Classical/Daily, sofern Provider unterstützt
- Varianten nur, wenn tatsächlich vorhanden

Für ausgewählte Performance:
- aktuelle Elo/Rating
- Bestwert, falls vorhanden
- gespielte Partien
- Gewinne grün
- Niederlagen rot
- Remis neutral
- Winrate
- Lossrate
- Drawrate
- ggf. Rating-Verlauf/Progress, wenn API verfügbar
- weitere sinnvolle öffentliche Statistiken nur bei tatsächlicher API-Verfügbarkeit

Keine Statistik erfinden.

### Local PGN/FEN
Keine Provider-Statistikseite.
Optional nur lokale Zusammenfassung:
- gespeicherte Partien
- analysierte Partien
- importierte FENs

---

## 6. Hauptseite Online-Profile: Spiele

Standard:
- Spiele des laufenden Kalendermonats
- neueste zuerst

Header:
- Suche
- Filter
- Sortierung
- Favoriten-Shortcut
- Downloads-Shortcut
- Monat wechseln

Filter:
- gewonnen
- verloren
- remis
- als Weiß
- als Schwarz
- Zeitformat
- analysiert / nicht analysiert
- heruntergeladen / nicht heruntergeladen

Sortierung:
- Datum neu → alt
- Datum alt → neu
- eigene Accuracy hoch → niedrig
- eigene Accuracy niedrig → hoch
- Rating optional

### Spielelisten-Zeile

Links:
- eigenes, neutrales Symbol für Zeitformat

Mitte:
- Weißer Spieler oben
- Schwarzer Spieler unten
- jeweiliges Rating in Klammern
- eigenes Profil visuell dezent hervorheben

Zusatz:
- Accuracy des Profilspielers
  - Providerwert, wenn vorhanden und noch keine lokale Analyse existiert
  - lokale App-Accuracy bevorzugen, sobald analysiert
  - `—`, wenn keine Accuracy vorhanden

Rechts:
- Ergebnisindikator:
  - Sieg: grün
  - Niederlage: rot
  - Remis: neutral/grau
- Download-Button
- Favoriten-Herz

Nicht die exakten Chess.com-Glyphen/Farbsysteme kopieren.

---

## 7. Favoriten

Ein Herz markiert/unmarkiert ein Spiel als Favorit.

Favoriten:
- sind profilbezogen
- bleiben lokal gespeichert
- können online oder heruntergeladen sein
- eigener Bereich `Favoriten`

Favorisieren lädt ein Spiel nicht automatisch herunter.

---

## 8. Downloads

Download-Button speichert das komplette Spiel für Offline-Nutzung.

Bereich `Downloads`:
- nur lokal verfügbare Spiele
- gleiche Karten-/Listenansicht
- Filter/Sortierung
- Analyse ohne Internet

Löschen eines Downloads:
- Favoritenstatus nicht automatisch löschen
- lokale Analyse nur nach klarer Benutzeraktion löschen oder beibehalten gemäß implementierter Datenstrategie

---

## 9. Hauptseite PGN/FEN-Profil

Keine Online-Spielesynchronisierung.

Oben:
- `+` Import/Aktion

`+` öffnet:
- PGN aus Zwischenablage einfügen
- PGN-Datei laden
- FEN einfügen

Liste zeigt:
- alle importierten/gespeicherten Partien
- analysiert / nicht analysiert
- Datum/Metadaten, soweit PGN vorhanden
- Favorit
- lokale Accuracy nach Analyse

FEN-Einträge können als gespeicherte Positionen behandelt werden.

---

## 10. Einstellungen

Unterbereiche:
1. Engine
2. Sprache
3. Design
4. Lizenzen/Über

### Engine
Basis:
- Preset Niedrig / Mittel / Hoch / Benutzerdefiniert
- Depth
- Lines / MultiPV
- Threads
- Hash
- optional Zeit pro Zug
- Reset

Standard = Mittel.

### Sprache
- Deutsch
- English
- العربية

Flutter ARB:
- `app_de.arb`
- `app_en.arb`
- `app_ar.arb`

Arabisch = RTL.

### Design
V1:
- System
- Hell
- Dunkel
- Akzent/Betonung aus kleiner eigener Palette

Nicht V1:
- alternative Figuren-Sets
- alternative Brett-Sets

### Lizenzen/Über
Anzeigen:
- App-Version
- Stockfish-Version/Commit
- GPLv3-Hinweis
- Stockfish-Source-Link/Bereitstellung
- Third-Party Notices

---

## 11. Analyseansicht

Öffnen durch Klick auf eine Partie.

### 11.1 Brett
- Schachbrett oben
- Standard-Figurenset aus `img/`
- Spielerorientierung:
  - wenn Profilspieler bekannt und Weiß: Weiß unten
  - wenn Profilspieler bekannt und Schwarz: Schwarz unten
  - bei lokalem PGN: falls ein "eigener Spieler" eindeutig bekannt ist entsprechend orientieren, sonst Weiß unten
- Gegner oben
- Spieler unten

Spielername + Rating können ober-/unterhalb des Bretts angezeigt werden.

### 11.2 Unterer Analysebereich
Brett bleibt fix sichtbar.
Bottom-Control-Bar bleibt fix sichtbar.
Nur der Inhaltsbereich dazwischen scrollt.

Auf breitem Windows-Layout:
- zwei Spalten

Auf Android:
- ebenfalls zwei logische Hälften; responsive Darstellung darf intern kompakter werden, ohne Informationen zu entfernen.

#### Linke Hälfte
Aktueller Zug:
- SAN
- Move-Kategorie
- eigenes Farbsystem
- eigenes Symbol

Kategorien:
- Brilliant
- Best
- Excellent
- Okay
- Miss
- Mistake
- Blunder

Darunter:
- mögliche Engine-Lines als Antworten/Fortsetzungen
- MultiPV
- Eval
- Line-Nummer
- anklickbare Variante

#### Rechte Hälfte
- bester Zug
- Evaluation
- beste Hauptvariante
- ggf. Differenz zum tatsächlich gespielten Zug

### 11.3 PGN / Varianten
Im scrollbaren Analysebereich:
- komplette Zugliste/PGN
- aktueller Zug hervorgehoben
- Klick auf Zug springt zur Stellung
- ausprobierte Nebenvarianten in Klammern darstellen
- Varianten dürfen das Originalspiel nicht überschreiben
- Rückkehr zur Hauptlinie ermöglichen

---

## 12. Steuerleiste Analyse

Fix am unteren Rand der Analyseansicht.

Genau fünf Hauptbuttons:
1. `|<` erster Zug / Startstellung
2. `<` vorheriger Zug
3. `▶/⏸` ab aktuellem Zug automatisch abspielen
4. `>` nächster Zug
5. `>|` letzter Zug / Endstellung

Auto-Play:
- ein Zug pro Sekunde
- startet ab aktueller Position
- stoppt am Partieende
- Button wechselt auf Pause während Wiedergabe

Optional Tastatur auf Windows:
- Home
- Pfeil links
- Space
- Pfeil rechts
- End

---

## 13. Analyseablauf

Für eine komplette Partie:
1. PGN parsen
2. jede Stellung vor und nach einem Zug erzeugen
3. Stockfish lokal analysieren
4. Best Move und MultiPV speichern
5. Score normalisieren
6. Verlust/Verbesserung aus Sicht des ziehenden Spielers bestimmen
7. Move-Kategorie berechnen
8. lokale Accuracy berechnen
9. Resultate persistieren
10. UI inkrementell aktualisieren

Analyse muss abbrechbar sein.

Bei App-Schließen:
- abgeschlossene Ergebnisse speichern
- laufende Analyse sauber abbrechen
- späterer Resume kann als Erweiterung vorbereitet werden

---

## 14. Move-Klassifikation – unabhängiges System

Kein Chess.com-Klassifikationssystem kopieren.

V1 soll ein eigenes, dokumentiertes System verwenden.

Empfohlen:
- Bewertungsänderung nicht blind in Centipawns interpretieren
- Mate-Scores gesondert
- wenn möglich Stockfish-WDL/Win-Probability-basierte Verlustmetrik
- Schwellenwerte zentral in `MoveClassifierConfig`
- Unit Tests mit festen Stellungen

Grundlogik:
- `Best`: tatsächlich gespielter Zug entspricht Engine-Bestmove oder ist innerhalb sehr kleiner Bewertungsdifferenz
- `Excellent`: sehr geringer Verlust
- `Okay`: kleiner akzeptabler Verlust
- `Mistake`: deutlicher Verlust
- `Blunder`: sehr großer Verlust oder Verlust eines klaren Gewinns
- `Miss`: verpasste klare Chance, ohne zwingend der größte Fehler zu sein
- `Brilliant`: nur konservativ bei sehr starkem/nahezu einzigem Zug mit nachvollziehbarer taktischer Eigenschaft; niemals allein wegen hoher Eval

Wenn zuverlässige Brilliant-Heuristik in V1 nicht möglich ist, Kategorie unterstützen, aber selten/gar nicht vergeben, statt falsche Ergebnisse zu erzeugen.

---

## 15. Accuracy

Zwei Quellen unterscheiden:

### Provider Accuracy
Von Chess.com/Lichess, wenn tatsächlich vorhanden.
Feld:
`provider_accuracy`

### App Accuracy
Von lokaler Stockfish-Analyse.
Feld:
`local_accuracy`

UI-Regel:
- nach lokaler Analyse standardmäßig `local_accuracy`
- Providerwert optional in Details separat anzeigen
- niemals lokale Accuracy als offizielle Chess.com-/Lichess-Accuracy bezeichnen

Formel in eigenem Modul und mit Version speichern:
`accuracy_algorithm_version`

So bleiben spätere Verbesserungen nachvollziehbar.

---

## 16. API- und Cache-Verhalten

### Chess.com
Laufender Monat:
`/pub/player/{username}/games/{YYYY}/{MM}`

Profil:
`/pub/player/{username}`

Stats:
`/pub/player/{username}/stats`

- seriell
- Cache-Header nutzen
- optionales `avatar`
- optionales `accuracies`

### Lichess
Profil:
`/api/user/{username}`

Performance:
`/api/user/{username}/perf/{perf}`

Games:
`/api/games/user/{username}`

Für laufenden Monat:
- `since` = Monatsbeginn
- `until` = Monatsende/jetzt
- NDJSON streamen
- `accuracy=true`, wenn sinnvoll
- nur einen Request gleichzeitig

---

## 17. Fehlerfälle

Beispiele:
- Benutzer existiert nicht
- Internet fehlt
- API 429
- API 5xx
- ungültiges PGN
- ungültiges FEN
- Stockfish startet nicht
- Analyse abgebrochen
- Speicher voll
- DB-Migration fehlgeschlagen

UI:
- verständliche Meldung
- Retry nur bei sinnvoller Fehlerklasse
- keine Endlosschleifen
- lokale Daten nicht wegen Netzwerkfehler löschen

---

## 18. Nicht-Ziele V1

- Online spielen
- Züge an Chess.com/Lichess senden
- Live-Engine-Unterstützung während einer Online-Partie
- Cloud-Sync
- eigener Server
- Social Features
- Chat
- Zahlungen
- Werbung
- Schachlektionen
- alternative Figuren-/Board-Themes
- iOS/Web/macOS/Linux
