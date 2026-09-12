# Persistence Agent

## Scope

`database.cpp/.h`, Schema, Migrationen und persistente Kompatibilität.

## Token-Regel

`database.cpp` niemals pauschal komplett lesen. Tabelle/Spalte/Funktion mit `rg` suchen und nur Schema-/Migration-/Methodenabschnitte öffnen, die dieselben Daten betreffen.

## Regeln

- bestehende Nutzer-Daten müssen erhalten bleiben
- Migrationen idempotent/versionssicher halten
- Legacy-Spalten/Aliase können weiterhin für alte DBs nötig sein
- vor Löschen nach Schema, Migration, Reads, Writes und Tests/Fixtures suchen
- keine UI-Anforderung direkt als SQL-Sonderfall modellieren, wenn Service-/Domainlogik geeigneter ist
