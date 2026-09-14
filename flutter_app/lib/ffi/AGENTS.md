# Flutter FFI Agent

## Dateien

- Vertrag: `core_gateway.dart`
- `dart:ffi`-Implementierung: `ffi_core_gateway.dart`

## Token-sparender Trace

Bei einer FFI-Aufgabe immer **nur das konkrete Symbol** verfolgen:

1. Gateway-Methode
2. FFI-Binding/Lookup
3. gleichnamige/zugehörige C-ABI-Funktion in `native/src/api/core_api.cpp`
4. genau den aufgerufenen Native-Service

Nicht `core_api.cpp` oder alle Bindings komplett lesen.

## Regeln

ABI, Speicherbesitz, UTF-8 und Fehlerpfade stabil halten. Keine fachliche Neuberechnung in Dart. Native-JSON-Felder dürfen von Flutter ignoriert werden, ohne den Native-Vertrag automatisch zu löschen.


## Coach

`CoreGateway.coachAsk`/`kc_coach_ask_json` ist der einzige Transport für Coach-Antwortanfragen. `CoreGateway.coachContext`/`kc_coach_context_json` darf ausschließlich FEN/PGN in ein natives Board-DTO auflösen. Dart übergibt nur Kontext und zeigt Ergebnisse; keine Evidenz-, Routing-, PGN/FEN-Parsing- oder Validierungslogik in FFI ergänzen.

## Coach automatic gate

`coachAutomatic` is a thin JSON bridge to `kc_coach_automatic_json`. Flutter passes context only; all trigger decisions and the zero-LLM-call fast path remain native.

## Coach-Langläufer

Lokale LLM-Inferenz darf den Flutter-Isolate nicht blockieren. `coachAsk` und `coachAutomatic` starten native Coach-Jobs und pollen deren Status; direkte synchrone Coach-C-ABI-Funktionen bleiben nur aus Kompatibilitätsgründen bestehen.

## Player Profile

`CoreGateway.playerProfile` / `kc_player_profile_json` is a read/display transport for the versioned native profile snapshot. Dart may select fields for presentation but must not derive or mutate profile-domain facts. ABI version 9 includes this export.

## Update 123 — Graph Inspector FFI

`CoreGateway.knowledgeInspector` / `kc_knowledge_inspector_json` ist ein read-only Diagnosevertrag. Dart übergibt ausschließlich Filter (`profileId`, optional `query`/`nodeId`, `limit`) und rendert das native Ergebnis. Keine Graphsuche, Confidence-/Coverage-Berechnung oder Query-Trace-Auswertung in Dart ergänzen.


## Fix Update 126 - non-blocking Knowledge Inspector

The Graph Inspector is diagnostic and must never wait behind a long KnowledgeRuntime refresh. `KnowledgeRuntime::inspector_json(...)` uses a non-blocking runtime lock and returns `status: busy`/`retryable: true` immediately when refresh/Coach work owns the runtime. Inspector candidate expansion, relations, chunks and recent traces stay tightly bounded. Flutter remains read-only and must not move graph/query logic into Dart.

## Update 132 - enriched inspector payload

`knowledge.inspector.v1` may now contain additive `runtimeActivity`, `profileBackground` and `samplingGuard` objects. Dart renders these native diagnostics as returned; it must not infer background ownership, sampling caps or pipeline state locally.

## Update 149 - startup/background diagnostic additions

`knowledge.inspector.v1` may additionally contain `coreStartup`, richer `runtimeActivity`, and `profileBackground.workerActivity`/`activePhase`/`activeDetail`. Dart continues to render these native diagnostics only and must not derive scheduling or startup policy locally.

## Update 151 - FFI bootstrap diagnostics

- `FfiCoreGateway.create()` may record read-only bootstrap durations for DLL loading, application-support-directory lookup, bundled opening asset synchronization and native symbol binding. These timings must not change asset installation or ABI behavior.
- The Diagnose dialog may append the Flutter-owned `flutterStartup` snapshot beside the native inspector payload. It must not rewrite native fields or infer domain state from Flutter timings.

## Update 152 - initial Games window

`CoreGateway.initialGames` / `kc_initial_games_json` is an additive read-only transport for the ordinary Games UI bootstrap. Native returns `{month,games}` and owns current-month/fallback selection. `games()` keeps its existing full-library semantics for callers that intentionally need the complete active-profile library.

## Update 173 - Coach diagnostics transport

`CoreGateway.coachPerformanceDiagnostics` / `kc_coach_performance_diagnostics_json` is an additive read-only pass-through to the existing Coach service performance report. Dart must not reconstruct trace timings or provider decisions from UI state.
