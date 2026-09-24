# Native Engine / Bot Agent

## Scope

Stockfish 18/19 Runtime, Factory, Kohärenz und Bot-Move-Selection.

## Kontext sparen

- Botproblem → zuerst `bot_move_selector.*` + `bot_service.*`
- SF18 Runtime → `stockfish_engine.cpp` / Runtime-Helfer
- SF19 Runtime → `stockfish19_engine.cpp` + nur nötige Kohärenzdatei
- Factory/Enginewahl → `stockfish_factory.*`

`third_party/` und CMake/Fetched Sources nicht öffnen, außer Build/Enginequelle ist ausdrücklich Teil der Aufgabe.

## Regeln

Beide Engines bleiben parallel aktiv. Namespace-/Link-Trennung, Engine-ID und Cache-Identität nicht vermischen. Performanceverbesserungen dürfen Spielstärke/Entscheidungslogik nicht unbeabsichtigt ändern.

## Analyse-Snapshots für SF18 und SF19

- SF18 veröffentlicht für Live-Anzeige und gespeicherte Analyse nur vollständige, exakte MultiPV-Rangfolgen einer Suchtiefe. Der abschließende `bestmove` bleibt für die native Schwierigkeitsprüfung verfügbar; die Rang-1-PV ist die Empfehlung für Klassifikation und Pfeile.
- SF19 behält seinen eigenen Exact-Snapshot-Pfad. Ein vollständiger Stand benötigt pro Rang einen benutzbaren, eindeutigen Wurzelzug; unvollständige oder doppelte PVs werden nicht als Evidenz veröffentlicht.
- Bei weniger legalen Wurzelzügen als angefordertem MultiPV wird die erwartete Rangzahl auf die tatsächlich verfügbaren Züge begrenzt. Engine-ID und bestehende Cache-Grenzen bleiben getrennt.
