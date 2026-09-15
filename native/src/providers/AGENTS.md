# Provider Agent

## Scope

Chess.com, Lichess, Local Provider, Modelle, gemeinsame Requests und Scheduler.

## Kontext sparen

Bei Provider-spezifischem Fehler nur den Provider + `provider_common`/Scheduler lesen, wenn tatsächlich genutzt. Profil-/Sync-Service erst öffnen, wenn Orchestrierung betroffen ist.

## Regeln

HTTP/API-Daten in native Provider-Modelle normalisieren. Flutter soll keine Provider-Sonderregeln nachbauen. Rate-Limit-/Retry-/Cache-Verhalten nicht durch UI-Heuristiken ersetzen.

## Update 87 - background archive traversal

Player-profile history backfill is orchestrated by `ProviderService`, which reuses the normal `GameProvider` methods and `ProviderRequestScheduler`. Provider implementations remain transport/normalization only. Background history requests must honor the same scheduler cooldowns as explicit UI syncs; no provider-specific archive loop belongs in Flutter.

## Update 90 - provider history progress

`ProviderService::player_profile_history_progress(...)` is the native read-only history-coverage contract for the shared player profile. It derives account discovery and available/synced month counts from the existing profile/archive/month caches; it must not issue network requests or create a second sync state store. The background backfill remains the only writer/orchestrator for missing archive months.
