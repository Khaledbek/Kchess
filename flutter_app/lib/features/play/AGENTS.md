# Play / Bot UI Agent

## Start hier

- Übersicht: `presentation/play_screen.dart`
- Setup: `presentation/bot_game_setup_screen.dart`
- Partie: `presentation/bot_game_screen.dart`
- Log: `presentation/bot_game_log_screen.dart`

## Kontext sparen

Bei UI-Problemen nur die betroffene Präsentationsdatei lesen. Native Bot-/Engine-Code nur öffnen, wenn Zugwahl, Zeit, Elo oder Jobstatus betroffen ist.

## Native Routing

- Bot-Job → `native/src/services/bot_service.*`
- Zugauswahl → `native/src/engine/bot_move_selector.*`
- Stockfish Runtime → `native/src/engine/stockfish*_engine*`, `stockfish_runtime.*`

Flutter darf Botjobs starten/pollen/canceln und darstellen, aber keine eigene Zugauswahl-/Elo-/Abbruchheuristik als fachliche Wahrheit implementieren.
