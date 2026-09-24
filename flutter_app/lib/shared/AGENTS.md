# Shared Flutter Agent

## Scope

Gemeinsame Models, Theme und breit wiederverwendete Widgets wie Board und Evaluation Bar.

## Token-Regel

Shared-Code hat großen Blast Radius. Vor jeder Änderung zuerst alle Call-Sites mit `rg` erfassen; danach nur repräsentative/konkret betroffene Aufrufer öffnen.

## Regeln

- Board-Widget = Darstellung/Interaktion, nicht Schachlegalität.
- Evaluation Bar = Darstellung nativer Werte, keine Analyseberechnung.
- Shared DTO-Feld nur entfernen, wenn **kein** Flutter-Aufrufer es liest.
- Theme-Änderungen nicht mit Feature-Logik vermischen.
- Keine Feature-spezifische Logik in Shared verschieben, nur um Dateien zu verkürzen.
