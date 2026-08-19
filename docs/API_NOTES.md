# C-ABI und Provider-APIs – Phase 4

Die öffentliche C-ABI liegt in `native/include/kchess/core_api.h`.

Neue Import- und Analyseoperationen:

- `kc_import_pgn_json`, `kc_import_fen_json`
- `kc_games_json`, `kc_game_json`
- `kc_start_analysis_json`, `kc_analysis_status_json`
- `kc_move_analysis_status_json`, `kc_cancel_analysis`

Import und Datenbankabfragen sind synchron und kurz. `kc_start_analysis_json`
startet den langen Stockfish-Job nativ und kehrt sofort zurück. Flutter pollt
Status-DTOs; es führt keine Engine- oder PGN-Logik aus.

Analyse-DTOs enthalten Status/Fortschritt, Engineversion, Konfigurationshash,
aktuellen Halbzug, Best Move sowie pro MultiPV-Linie Rang, Tiefe, CP oder Mate,
WDL, Knoten und UCI-PV. Phase 3 ergänzt Klassifikation, Classifier-Version,
Expected Scores/Loss, empfohlenen Zug, optionale Theory-Statistik und eine
Summary mit Engine-, Accuracy- und Book-Version sowie Counts pro Farbe.

Die öffentliche ABI bleibt binär kompatibel: neue Daten werden innerhalb der
vorhandenen JSON-DTOs übertragen. `kc_move_analysis_status_json` liefert die
persistierte Klassifikation jedes einzelnen Halbzugs. Der Engine-Cache bleibt
von Classifier-, Accuracy- und Book-Version unabhängig.

## Asynchrone Provider-C-ABI

- `kc_start_provider_profile_json(type, username)` validiert ein öffentliches
  Konto, bevor ein Profil gespeichert wird, und liefert sofort eine Job-ID.
- `kc_start_provider_sync_json(profile, year, month)` synchronisiert Profil,
  Stats, Monatsliste und genau einen Monat im Hintergrund.
- `kc_provider_job_status_json` liefert Phase, Abschluss, normalisiertes
  Ergebnis sowie `errorKind`; `kc_cancel_provider_job` setzt den Abbruch.
- `kc_provider_overview_json` liest nur den lokalen Cache.
- `kc_set_game_favorite` und `kc_set_game_downloaded` ändern lokale Zustände.

An der ABI-Grenze werden alle Exceptions abgefangen. Fehlerklassen sind
`offline`, `timeout`, `dns`, `tls`, `notFound`, `gone`, `rateLimited`,
`server`, `invalidResponse`, `cancelled` und `transport`.

## Chess.com PubAPI

Nur die offizielle Published Data API wird verwendet:

- `GET /pub/player/{username}`
- `GET /pub/player/{username}/stats`
- `GET /pub/player/{username}/games/archives`
- `GET /pub/player/{username}/games/{YYYY}/{MM}`

Requests enthalten einen neutralen `KChess/0.1.0 (public-provider-client)`-
User-Agent, gzip sowie vorhandene `If-None-Match`- und
`If-Modified-Since`-Header. 304 behält den normalisierten Cache. Optionale
Avatare und Accuracies bleiben optional; Daily/Rapid/Blitz/Bullet werden in
gemeinsame Typen überführt.

## Lichess API

Verwendet werden nur öffentliche offizielle Endpunkte:

- `GET /api/user/{username}`
- `GET /api/user/{username}/perf/{perf}` für Bullet, Blitz, Rapid,
  Classical und Correspondence
- `GET /api/games/user/{username}` mit `since`, `until`, `pgnInJson=true`,
  `opening=true`, `accuracy=true`, `evals=false`, `ongoing=false` und NDJSON

Die NDJSON-Antwort wird chunk- und zeilenweise geparst. `createdAt`, `seenAt`
und `lastMoveAt` werden von Millisekunden auf die gemeinsame Sekundenbasis
normalisiert. Lichess-Flair bleibt Metadatum; es gibt kein erfundenes
Avatarfeld. Nach 429 sind weitere Lichess-Requests mindestens eine volle
Minute blockiert.

## Sicherheit und Fair Play

Es werden keine Passwörter, Tokens, OAuth-Daten, Chats oder privaten Daten
gespeichert. Usernamen werden validiert und URL-kodiert. JSON/NDJSON,
Weiterleitungen, Body- und Zeilengrößen werden begrenzt. TLS-Verifikation wird
nicht abgeschaltet. Laufende Partien werden nicht importiert oder automatisch
analysiert; KChess sendet keine Züge und keine Vorschläge an Anbieter.
