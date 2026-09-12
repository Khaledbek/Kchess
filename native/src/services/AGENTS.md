# Native Services Agent

## Routing

- Analyse → `analysis_service.*`
- Bot → `bot_service.*`
- Games/Favorites → `game_library_service.*`
- Profile → `profile_service.*`
- Provider-Sync → `provider_service.*`
- Settings → `settings_service.*`
- Statistics → `statistics_service.*`, `statistics_details.cpp`
- AI Chess Coach App-Bridge → `coach_service.*`
- Coach User-Profile Bridge → `coach_profile_bridge.*`

## Token-Regel

Nur den betroffenen Service lesen; andere Services erst bei konkretem Call. Header zuerst verwenden, um API/Verantwortung zu verstehen, dann nur relevante Implementierungsfunktionen öffnen.

## Regeln

Services orchestrieren Domain-/Persistence-Komponenten. Keine DB-SQL-Duplikate in Services einführen, wenn `Database` bereits eine passende Operation besitzt. Fehler-/Kompatibilitätspfade nicht ohne Aufrufer- und Migrationsprüfung löschen.


## Coach Service

`coach_service.*` ist nur die App-/Persistenzbrücke zum `native/ai/CoachOrchestrator`: Transport-JSON parsen, vorhandenen PGN-/Profilkontext anbinden und strukturierte Antwort serialisieren. Keine zweite Coach-Pipeline, Prompt-Logik oder Schachalgorithmen im Service anlegen.

## Automatic Coach

`CoachService::automatic_json` maps stored game/analysis data into `AutomaticCoachEvent`, while event classification stays in `native/ai/automatic/`. Every valid just-played move may start one automatic coach turn. Automatic jobs use latest-position-wins semantics: starting a newer automatic job marks all older automatic jobs cancelled so rapid move sequences never build an LLM backlog.

## Coach User Profile

`coach_profile_bridge.*` is the only service-side mapping from persisted KChess profile/game/analysis data into `native/ai/profile/`. It may refresh the versioned learned JSON and map it into coach evidence/practicality, but learning rules stay in `native/ai/profile/`.


## Coach Context Preview

`CoachService::context_json` may only validate/parse a pasted FEN or PGN through the authoritative chess helpers and return presentation-ready board DTOs. For PGN it returns the initial position plus main-line positions so Flutter can navigate without reparsing chess notation. It must not persist pasted context or invoke the LLM.

## Coach Jobs

Provider-Inferenz läuft über `CoachService`-Jobs (`start/status/cancel`), damit C-ABI/Flutter niemals während eines LLM-/API-Laufs blockieren. Der Orchestrator wird innerhalb des Services serialisiert, damit Session-State nicht parallel mutiert wird. Für Automatic-Coach-Jobs gilt latest-position-wins: ein neuer Zug cancelt ältere noch laufende/queued Automatic-Jobs; gecancelte Jobs dürfen keine Antwort veröffentlichen. Der Service wählt den konfigurierten nativen Provider-Factory-Pfad; API-Schlüssel bleiben außerhalb des Services und außerhalb von Flutter.

## Coach Hint Job

- The shared single-position hint cache supplies the existing candidate-retrieval hook. Key it by FEN and relevant engine/hint settings; never trust UI-provided hint moves as evidence.
- Manual coach questions may reuse the bounded AnalysisService hint search when native engine budgeting requires fresh candidates. Automatic turns must consume existing analysis and never start this search.
- Automatic checks may refer to a native variation job. Verify its played move against previous/current FEN before coaching; classification stays native.

Der Engine-Hint läuft als nicht blockierender Coach-Job. `CoachService` delegiert die Schachsuche an den vorhandenen `AnalysisService`; bis zu zwei nahezu gleichwertige Kandidaten dürfen als strukturierte UCI-/Eval-Daten an Flutter gehen. Das LLM bestimmt niemals den Hint-Zug.
