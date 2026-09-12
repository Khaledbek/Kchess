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
- seitenbezogene lokale Accuracy kommt aus `analysis_runs.white_local_accuracy` / `black_local_accuracy`; `games.local_accuracy` ist nur der kombinierte Legacy-Wert
- keine UI-Anforderung direkt als SQL-Sonderfall modellieren, wenn Service-/Domainlogik geeigneter ist

## AI Chess Profile Persistence

`ai_chess_profiles` stores one versioned JSON payload per existing KChess profile. SQLite owns only persistence and aggregated source observations; interpretation/learning belongs to `native/ai/profile/`. Preserve cascade deletion through the profile foreign key.

