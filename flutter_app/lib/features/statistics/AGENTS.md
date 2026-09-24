# Statistics UI Agent

Die Accuracy-/Trend- und Eröffnungsschwächen-Sections zeigen ausschließlich native Statistik-DTOs. Ihr Fortschrittsindikator beobachtet den bestehenden Profil-Worker; Flutter startet keine eigene Analyse und berechnet keine Schach- oder Trendlogik.

Die Accuracy-Section aktualisiert sich bei Änderungen der nativen `sourceRevision`, damit nachträglich aus dem gemeinsamen Cache vervollständigte Phasenwerte erscheinen. Der Revision-Wert ist nur ein UI-Refresh-Signal.

## Start hier

`presentation/statistics_screen.dart` + genau die betroffene Section (`rating`, `form`, `openings`, `phase`, `termination`, `comparison`).

## Kontext sparen

Nicht alle Sections gleichzeitig laden. Bei einem Wert zuerst DTO-Feld und genau seine Renderstelle suchen.

## Domain-Grenze

Aggregation, Rating-/Eröffnungs-/Phasen-/Termination-/Head-to-Head-Berechnung bleibt in `native/src/services/statistics_*`. Flutter formatiert und visualisiert nur.
