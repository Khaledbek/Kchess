# Provider Agent

## Scope

Chess.com, Lichess, Local Provider, Modelle, gemeinsame Requests und Scheduler.

## Kontext sparen

Bei Provider-spezifischem Fehler nur den Provider + `provider_common`/Scheduler lesen, wenn tatsächlich genutzt. Profil-/Sync-Service erst öffnen, wenn Orchestrierung betroffen ist.

## Regeln

HTTP/API-Daten in native Provider-Modelle normalisieren. Flutter soll keine Provider-Sonderregeln nachbauen. Rate-Limit-/Retry-/Cache-Verhalten nicht durch UI-Heuristiken ersetzen.
