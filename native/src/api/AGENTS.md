# C-ABI Agent

## Scope

`core_api.cpp` ist die Flutter/Native-Grenze.

## Token-Regel

Datei nicht komplett lesen. Exportsymbol mit `rg` suchen und nur Funktion + direkte Helper öffnen.

## Regeln

- C++-Exceptions abfangen
- C-kompatible Typen/UTF-8/opaque handles verwenden
- Speicherfreigabe eindeutig halten
- Langläufer über Job/Status/Cancel
- bestehende Exports nicht ohne Migrationsgrund entfernen
- C-ABI möglichst dünn halten; Domainlogik gehört in Services/Core

## Update 152 - additive initial Games export

`kc_initial_games_json` is an additive read-only C-ABI export for the native-selected Games UI bootstrap window. It must remain a thin Core/GameLibraryService transport; the existing `kc_games_json` full-library export keeps its semantics.

## Update 173 - Coach diagnostics export

`kc_coach_performance_diagnostics_json` is a read-only string export through Core to `CoachService::performance_diagnostics_json()`. Keep the C-ABI thin and use the ordinary caller-owned string-free contract.
