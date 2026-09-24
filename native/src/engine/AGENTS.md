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

- SF18 veröffentlicht während einer laufenden Suche für die Live-Anzeige nur vollständige, exakte MultiPV-Rangfolgen einer Suchtiefe; deren Rang-1-PV ist die vorläufige Empfehlung. Nach Abschluss ist der finale Stockfish-`bestmove` die autoritative Empfehlung für gespeicherte Analyse, Pfeile und Klassifikation.
- SF19 behält seinen eigenen Exact-Snapshot-Pfad. Ein vollständiger Stand benötigt pro Rang einen benutzbaren, eindeutigen Wurzelzug; unvollständige oder doppelte PVs werden nicht als Evidenz veröffentlicht. Die Exact-MultiPV-Linien bleiben Score/WDL/PV-Evidenz; nach abgeschlossener Suche ist auch hier der finale SF19-`bestmove` die autoritative Empfehlung.
- Bei weniger legalen Wurzelzügen als angefordertem MultiPV wird die erwartete Rangzahl auf die tatsächlich verfügbaren Züge begrenzt. Engine-ID und bestehende Cache-Grenzen bleiben getrennt.

## Classification/Arrow Coherence Series - Update 2

- The service-level `analysis.snapshot.v1` contract treats one rank-1 PV as belonging to one exact native snapshot. Engine cache identity includes the NNUE network and participates in the analysis config namespace.
- A later engine iteration, a different MultiPV budget or a different live generation is a different snapshot even when FEN is identical. Callers must never silently combine those values and call them one coherent result.

## Classification/Arrow coherence series — Update 3/9 (SF18)

- SF18 sideline consumers now request the classification-width root snapshot up front. The adapter's complete rank-ordered snapshot is therefore shared by the visible rank-1 arrow and next-ply classification; a later wider search must not be used to reinterpret an already published arrow.
## Best-Move / Reclassification Series — Update 6/6

- Do not reintroduce `coherent_stockfish19_ranked_lines` or any helper that promotes historical PV data to make final `bestmove` look like MultiPV rank 1. SF19 has one exact snapshot accumulator for line evidence; the completed callback independently owns `AnalysisResult.best_move`.
- `current_result()` is a running-search view and therefore uses the newest complete exact rank-1 PV. `analyze()` is the completed-search view and restores the final engine `bestmove` when usable.
- Rapid sideline replacement remains latest-request-wins: cancel/join the previous search, retain only intentionally reusable engine/position cache state, and never publish the superseded root snapshot as current truth.

