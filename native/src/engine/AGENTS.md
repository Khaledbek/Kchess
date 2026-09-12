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
