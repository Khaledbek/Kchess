# Native Services Agent

## Routing

- Analyse → `analysis_service.*`
- Bot → `bot_service.*`
- Games/Favorites → `game_library_service.*`
- Profile → `profile_service.*`
- Provider-Sync → `provider_service.*`
- Settings → `settings_service.*`
- Statistics → `statistics_service.*`, `statistics_details.cpp`

## Token-Regel

Nur den betroffenen Service lesen; andere Services erst bei konkretem Call. Header zuerst verwenden, um API/Verantwortung zu verstehen, dann nur relevante Implementierungsfunktionen öffnen.

## Regeln

Services orchestrieren Domain-/Persistence-Komponenten. Keine DB-SQL-Duplikate in Services einführen, wenn `Database` bereits eine passende Operation besitzt. Fehler-/Kompatibilitätspfade nicht ohne Aufrufer- und Migrationsprüfung löschen.
