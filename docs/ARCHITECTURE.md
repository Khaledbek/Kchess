# Architektur – Phase 4

## Laufzeitschichten

```text
Flutter Widgets, ARB, Theme und View-State
                 |
       dart:ffi / stabile C-ABI
                 |
C++20 Core
  |-- GameProvider -> Chess.com PubAPI / Lichess API
  |-- Plattform-HTTP: WinHTTP / Android HttpsURLConnection über JNI
  |-- serieller ProviderRequestScheduler, Cache-Validatoren und CancelToken
  |-- PGN-/SAN-Parser und vollständige FEN-Validierung
  |-- regelkonformes Stellungsmodell pro Halbzug
  |-- asynchroner Analyse-Job / Abbruch / Wiederaufnahme
  |-- ChessEngine -> offizieller Stockfish-18-Enginekern
  |-- KCB1 OpeningTheoryProvider (einmalig geladen, binäre Suche)
  |-- versionierter MoveClassifier und Local Accuracy
  `-- SQLite-Migrationen, getrennte Caches, MultiPV und Profile
```

Flutter zeigt native DTOs an und übernimmt keine Schachregeln. Die C-ABI
verwendet opaque Handles, UTF-8, JSON-DTOs und `kc_string_free`; C++-Exceptions
werden an der ABI-Grenze abgefangen.

## Import und Stellungsmodell

`native/src/chess/pgn.cpp` liest Tags, SAN, Kommentare, NAGs und geklammerte
Varianten. Varianten bleiben im Parse-Ergebnis erhalten; die Analyse verarbeitet
weiterhin nur die Hauptlinie. Jeder Hauptlinienzug wird gegen Stockfish-Move-Generation
legal geprüft und als SAN, UCI, FEN davor und FEN danach gespeichert.

`native/src/chess/fen.cpp` prüft alle sechs FEN-Felder, Brettbelegung, Könige,
Bauernränge, Zugrecht, Rochaderechte, En-passant-Feld und Zähler. Fehler werden
als Importfehler zurückgegeben und verlassen niemals die C-ABI als Exception.

## Engine und Analyse

`StockfishEngine` verwendet den offiziellen Enginekern direkt hinter der
bestehenden `ChessEngine`-Abstraktion. Die semantischen UCI-Operationen werden
auf die offizielle Engine-API abgebildet: Initialisierung/Ready, New Game,
Position, Go, Stop und Shutdown. Optionen: Tiefe 18, MultiPV 3, Ponder aus,
Windows 4 Threads/256 MB Hash, Android 2 Threads/128 MB Hash. Centipawn-,
Mate-, WDL-, Knoten- und PV-Daten werden übernommen.

Der C-API-Start legt nur einen nativen Hintergrund-Thread an und kehrt sofort
zurück. Der Worker analysiert die FEN nach jedem Halbzug nacheinander. Nach
jedem Ergebnis schreibt SQLite den Zug, die primäre Bewertung und alle
MultiPV-Linien. Abbruch behält fertige Zeilen; ein späterer Start setzt beim
ersten fehlenden Halbzug fort.

Der Engine-Cache-Schlüssel enthält Stockfish-Version, Tiefe, MultiPV und das
optionale Zeitlimit pro Stellung.
Threads und Hash beeinflussen nicht die semantische Cache-Kompatibilität.
Klassifikation, Accuracy und Opening Book haben davon getrennte Versionen.
Ändert sich nur eine dieser Versionen, verwendet KChess die vorhandenen
Enginewerte und baut lediglich die Klassifikation neu auf.

Eine vollständige Analyse mit mindestens gleicher Tiefe, mindestens gleicher
Linienzahl und einem nicht kleineren Zeitbudget ist wiederverwendbar; ein
unbegrenzter Lauf gilt als stärker als ein zeitbegrenzter. `Pfeile anzeigen`
ist reine Flutter-Darstellung und gehört weder zum Engine-Hash noch zur
Cache-Kompatibilität.

## Offline Opening Theory

`tools/opening_book/build_book.py` streamt eine oder mehrere `.pgn`-/`.pgn.zst`-
Dateien, baut über einen schlanken Visitor nur die konfigurierten Opening-Plies
auf und aggregiert alle Quellen gemeinsam in temporärem SQLite. `min_games`
wird erst nach Abschluss sämtlicher Inputs angewandt. Wiederholtes `--input`
und deterministisch sortiertes `--input-dir` werden unterstützt; doppelte
aufgelöste Pfade werden abgelehnt. Das Ergebnis bleibt das dokumentierte
Festformat KCB1. Sein `positionKey` entspricht Stockfish 18
`Position::key()` ohne Zugzähler und berücksichtigt Figuren, Zugrecht,
Rochaderechte sowie ein relevantes En-passant-Feld. Python- und C++-Fixtures
sichern identische Keys ab. Die inkrementelle Key-Berechnung des Visitors wird
für normale Züge, Rochade, En passant und Promotion gegen die vollständige
Referenzberechnung getestet.

Flutter kopiert das gebündelte `assets/opening_book.kcb` beim Start atomar in
das private App-Datenverzeichnis, bevor der Core initialisiert. Der native
Provider validiert Header, Version, Dateigröße, Sortierung, Züge und Summen,
lädt die 28-Byte-Einträge einmal und sucht danach binär. Eine fehlende oder
beschädigte Datei deaktiviert Theory kontrolliert; Engine-Analyse und App
bleiben funktionsfähig. Theory wird nur vergeben, wenn Stellung und tatsächlich
gespielter Zug vorhanden sind und der bereits beim Build angewandte
`min_games`-Filter erfüllt wurde.

## Klassifikation und Perspektive

Primär wird der Expected Score aus Stockfish-WDL berechnet:
`(W + 0.5 * D) / (W + D + L)`. Mate wird zuerst behandelt; ohne WDL dient
`1 / (1 + exp(-cp / 400))` als begrenzter Fallback. Da das gespeicherte
Post-Move-Ergebnis aus Sicht der nun ziehenden Gegenseite gilt, wird es zentral
mit `1 - score` in die Sicht des Spielers invertiert. Der Verlust ist
`clamp(bestExpected - playedExpected, 0, 1)`.

`MoveClassifierConfig` Version 1 enthält alle Schwellen zentral. Die Priorität
lautet Theory, Brilliant, Best, Blunder, Miss, Mistake, Excellent, Okay,
Unknown. Die Grenzwerte sind:

- Best: gespielter Stockfish-Bestzug oder Loss `<= 0.005`
- Excellent: Loss `> 0.005` und `<= 0.03`
- Okay: Loss `> 0.03` und `<= 0.08`
- Mistake: Loss `> 0.08` und `<= 0.20`
- Blunder: Loss `> 0.20` oder ein neu erlaubtes erzwungenes Matt
- Miss: Best Expected Score `>= 0.75` und Loss `>= 0.15`, oder ein verpasstes
  eigenes erzwungenes Matt; Blunder hat bei extremem Verlust Vorrang

Brilliant ist absichtlich konservativ: praktisch bester Nicht-Theorie-Zug,
Abstand zum zweiten MultiPV-Zug mindestens `0.10` und zusätzlich entweder ein
Materialopfer von mindestens 250 Centipawns nach gegnerischer Antwort innerhalb
der ersten vier PV-Halbzüge oder ein einziger taktischer Zug mit Abstand
mindestens `0.20`. Im Zweifel bleibt die Kategorie Best.

## Local Accuracy

Accuracy-Algorithmus Version 1 berechnet pro messbarem Zug
`100 * exp(-4.0 * loss)` und mittelt diese Werte pro Farbe. Theory-Züge werden
ausgeschlossen, weil sie Eröffnungswissen statt Engine-Präzision messen. Eine
Partie mit ausschließlich Theory-Zügen erhält als expliziten Fallback 100;
ohne Theory und ohne messbaren Verlust bleibt Accuracy `null`. Alle Ergebnisse
werden auf 0 bis 100 begrenzt.

## Persistenz und Summary

Migration 003 ergänzt getrennte Engine-/Classifier-/Accuracy-/Book-Versionen,
Expected Scores, Loss, empfohlenen Zug, Theory-Statistiken sowie Accuracy und
Counts pro Farbe. Phase-2-Daten bleiben lesbar. Nach vollständiger Analyse
werden die Klassifikationen transaktional gespeichert; die Summary zeigt
Spieler oder beide Farben, Accuracy, alle Kategorien, Zugzahlen, Tiefe und
Versionen.

## UI

Die Analyseansicht öffnet vor dem Engine-Ergebnis, lädt Partie/Brett aus
SQLite, pollt ausschließlich kurze Statusabfragen und kann jeden bereits
analysierten Halbzug separat laden. Die ausgewählte MultiPV-Linie bestimmt den
Brett-Pfeil. Kategorie, bestehendes lokales Symbol oder neutraler Fallback,
Book-Partien, Expected-Score-Verlust und empfohlener Enginezug werden direkt am
Zug angezeigt. `showBoardArrows` ist reine Darstellung und kein Cache-Parameter.

## Öffentliche Online-Provider

`ChessComProvider` und `LichessProvider` implementieren dieselbe
`GameProvider`-Schnittstelle. Provider-JSON bzw. Lichess-NDJSON endet im
C++-Core; Flutter erhält ausschließlich normalisierte KChess-DTOs. Die
HTTP-Abstraktion unterstützt HTTPS mit System-Zertifikatsprüfung, gzip,
HTTPS-only-Weiterleitungen, Zeitlimits, Abbruch, Antwortgrößenlimits und
optionales Chunk-Streaming. Windows verwendet WinHTTP. Android hängt native
Worker über JNI an die VM und verwendet `HttpsURLConnection`; der
Lichess-Export wird in beiden Fällen zeilenweise verarbeitet.

Alle Requests eines Providers werden durch `ProviderRequestScheduler`
serialisiert. Nach Chess.com-429 gilt wachsendes Backoff ab 30 Sekunden. Nach
Lichess-429 blockiert der Scheduler mindestens 60 Sekunden und respektiert ein
längeres numerisches `Retry-After`. Es gibt kein aggressives automatisches
Retry. Die Job-C-ABI kehrt sofort mit einer ID zurück; Flutter pollt kompakte
Statusobjekte. Profilwechsel und Core-Shutdown brechen nicht mehr benötigte
Jobs ab.

Online-Spiele werden nur übernommen, wenn sie abgeschlossen sind und eine
gültige vollständige PGN-Hauptlinie enthalten. Danach liegen sie im selben
`games`-/`game_moves`-Modell wie lokale Importe. Opening Book, Stockfish,
Classifier und Local Accuracy verwenden unverändert diese gemeinsame Strecke.
Es existiert keine Live-Analyse und keine Provider-Engineauswertung.

## Provider-Persistenz und Offline-Modus

Migration 004 ergänzt normalisierte öffentliche Profilfelder, getrennte
Provider-Accuracy pro Farbe, Ergebnis aus Profilsicht, Time-Control-Typ,
Provider-Endzeit sowie diese Caches:

- `provider_profiles_cache`: normalisiertes Profil, ETag, Last-Modified,
  `fetched_at`, Ablauf und Normalisierungsversion
- `provider_stats_cache`: normalisierte Performancewerte und dieselben
  Validatoren
- `provider_month_cache`: Validatoren und Status pro `YYYY-MM`
- `provider_sync_state`: Status- und Cooldown-Vorbereitung

Spiele werden in Transaktionen anhand
`profile_id + provider + provider_game_id` aktualisiert. Favorit und Download
sind getrennte lokale Tabellen. Provider-Accuracy bleibt getrennt von der
eigenen Accuracy; nach lokaler Analyse hat die Local Accuracy in DTO und UI
Vorrang. Beim Öffnen erscheinen gespeicherte Profile, Stats und Spiele sofort;
ein Hintergrundsync ergänzt sie. Netzwerkfehler löschen nie lokale Daten.
Vollständige PGNs sind offline öffnbar, und die lokale Analyse benötigt danach
keinen Provider.

Chess.com-Avatare werden nur von validierten HTTPS-Hosts der Domains
`chess.com`/`chesscomfiles.com` in einen UUID-basierten privaten Dateinamen
geladen. Bei Fehlschlag bleibt das gebündelte Fallback. Lichess-Flair ist
separates Metadatum; ein Avatar wird nicht erfunden.

## Phase 5 library lifecycle

The library remains a projection of the existing `games` table plus independent
`favorites` and `downloads` flags. The normal provider Games screen may be
month-scoped, but Favorites and Downloads deliberately ignore that UI month
selection so saved games remain reachable across months.

Provider cache cleanup is conservative: deleting a `provider_month_cache` row
may prune only provider games from that UTC month that have no download flag,
no favorite flag, and no persisted analysis run. Local PGN/FEN deletion is a
separate explicit operation and is only accepted for the active local profile.
