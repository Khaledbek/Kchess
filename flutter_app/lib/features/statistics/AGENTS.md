# Statistics UI Agent

## Start hier

`presentation/statistics_screen.dart` + genau die betroffene Section (`rating`, `form`, `openings`, `phase`, `termination`, `comparison`).

## Kontext sparen

Nicht alle Sections gleichzeitig laden. Bei einem Wert zuerst DTO-Feld und genau seine Renderstelle suchen.

## Domain-Grenze

Aggregation, Rating-/Eröffnungs-/Phasen-/Termination-/Head-to-Head-Berechnung bleibt in `native/src/services/statistics_*`. Flutter formatiert und visualisiert nur.
