# Training UI Agent

## Start hier

UI unter `presentation/`, Request-/Practice-DTOs unter `models/`. Für eine konkrete Aufgabe nur den betreffenden Trainingsmodus öffnen.

## Routing

- Dashboard/Navigation → `training_arena_screen.dart`, `training_navigation.dart`
- Opening Lab → `opening_lab_screen.dart`
- Blunder Buster → `blunder_buster_screen.dart`
- Endgame → `endgame_*`
- Practice Player → `practice_player.dart`, `training_player_widgets.dart`

## Domain-Grenze

Lösungsvarianten, Legalität, Fortschritt, Versuche, Kataloge und Defender-Jobs bleiben in `native/src/training/`.

## Token-Regel

Große native `.inc`-Katalogdaten niemals lesen, außer die Aufgabe betrifft genau deren Inhalt/Generator. DTOs nur feldweise verfolgen.
