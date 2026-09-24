# Profile UI Agent

## Start hier

- Screen: `presentation/profile_screen.dart`
- UI-Helfer: `presentation/profile_support.dart`

## Domain-Grenze

Provider-Sync, Merge, lokale Profile, Löschung und persistente Zuordnung bleiben nativ.

## Native bei Bedarf

- Profile → `native/src/services/profile_service.*`
- Sync → `native/src/services/provider_service.*`
- Provider → `native/src/providers/`

Provider-DTO-Felder nur nach projektweiter Nutzersuche entfernen; Native-JSON nicht unnötig brechen.

## Player Profile v2 UI

`presentation/profile_screen.dart` may render the native learned profile, confidence, background progress, coach priorities and links to training positions. It must not calculate patterns, confidence, relevance, trends, sample membership or queue priority in Dart. Training buttons may resolve an existing `gameId`/`ply` to an already parsed `BoardPosition` and open the existing Coach; chess interpretation remains native.

The Profile screen complements Statistics rather than recreating its charts/metrics. Fixed labels belong in all three ARB files.

## Progress presentation

The profile UI must not label metadata indexing as completed analysis. Show library indexing separately from relevant-game resolution, reused existing analyses, and targeted fast/verification/deep evidence. The main progress bar follows relevant-game resolution when relevant work exists; otherwise it may show indexing coverage. While profile maintenance owns a shared AnalysisService job, use native `overallProgress` / `currentGameProgress` from the snapshot so the bar advances inside the current game instead of waiting for a whole-game state transition. Polling may be more frequent while this lightweight native snapshot is displayed, but Dart must never derive engine progress or profile-domain state itself. Fixed labels remain ARB-only.

## Update 91 - native-only progress presentation

The Profile screen renders `syncing_history`, provider-history coverage, persistent queue counts, and live shared-analysis progress only from `CoreGateway.playerProfile`. Dart must not start history sync, advance the queue, infer missing months, or make profile preparation depend on this widget being mounted. The periodic refresh is presentation polling only; native `PlayerProfileService` owns all background continuation when the user is on any app section.

## Update 93 - confidence vs work progress

Profile confidence and background work progress are separate concepts. Render the native learned-profile confidence as Coach-readiness confidence, and render the native background `overallProgress` numerically beside the existing progress bar. Use one decimal place so large relevant queues still show visible forward movement. Dart may format the native fraction for display but must not recalculate profile confidence or pipeline semantics.

## Update 99 - nine-stage funnel diagnostics

Render the native funnel counters beside the existing profile work telemetry: scanned library size, current historical sample size/budget, Stage-4 interesting games and engine-promoted games. These are diagnostics, not Coach confidence. Keep the numerical overall work percentage and profile confidence visually/semantically separate. All fixed wording remains in ARB; Dart must not derive Stage-3 membership or infer Stage 8/9 completion from an engine-promoted game.

## Update 102 - live profile presentation

- The Profile screen presents only compact, useful learned metrics (estimated playing strength, measured Accuracy, profile confidence, analyzed evidence) before coach priorities/strengths/weaknesses.
- Profile preparation is shown as a live activity card with smooth UI interpolation between native snapshots rather than a domain-owning progress bar. Native background fields remain authoritative.
- Flutter may animate/interpolate displayed progress and poll while the screen is visible, but it must never calculate pipeline membership, confidence, estimated strength, or trigger preparation work.

## Update 103 - compact profile hierarchy and live funnel

- Keep the learned-profile summary visually compact: estimated playing strength is the primary value; measured Accuracy, Coach-readiness confidence and analyzed evidence are supporting metrics rather than four equally weighted cards.
- The live preparation card may present native funnel counters as a readable library -> sample -> interesting -> engine/evidence flow. `enginePromotedGames` is native telemetry and may be rendered directly; Dart must not infer promotion or stage state from other counters.
- Live presentation remains observation-only. Smooth animation is cosmetic and must never alter native progress, queue state or sampling decisions.

## Update 123 — Knowledge Graph Inspector

Die Profilseite darf den nativen Knowledge Graph ausschließlich über `CoreGateway.knowledgeInspector` diagnostisch anzeigen. Der Inspector ist read-only; Suche, Relationen, Quality, Provenance, Chunks und Query Traces kommen vollständig aus C++. Sichtbare feste UI-Texte werden weiterhin ausschließlich aus ARB-Lokalisierungen bezogen.


## Cleanup Update 124 - retired probe telemetry

The Profile UI reads `indexedGames`, relevant/resolved work and reused-analysis telemetry only. Retired `knownGames`, `processedGames` fallback and fast/verification/deep probe-position chips are removed from active UI code. Flutter still does not derive profile pipeline semantics.


## Fix Update 126 - non-blocking Knowledge Inspector

The Graph Inspector is diagnostic and must never wait behind a long KnowledgeRuntime refresh. `KnowledgeRuntime::inspector_json(...)` uses a non-blocking runtime lock and returns `status: busy`/`retryable: true` immediately when refresh/Coach work owns the runtime. Inspector candidate expansion, relations, chunks and recent traces stay tightly bounded. Flutter remains read-only and must not move graph/query logic into Dart.

## Update 131 - coverage/readiness presentation

- Render native `samplingCoverage` as profile/sample coverage and keep it visually distinct from `confidence` (Coach-readiness confidence) and background work progress.
- `historicalSampleBudget` is the dynamic native recommendation; Flutter must never fall back to or display 500 as a target merely because 500 is the native hard cap.
- Profile completion is native `status` / `initialPreparationComplete` semantics. UI must not infer completion from confidence or coverage percentages.

## Update 132 - visible background cause

The read-only Diagnose dialog may display the additive native `runtimeActivity`, `profileBackground` and `samplingGuard` sections. In particular, a `status=busy` response is no longer opaque: the payload identifies the KnowledgeRuntime phase and current profile/sampling/engine state. No polling decision, retry policy or sampling interpretation moves into Flutter.

## Update 133 - adaptive sampling presentation cleanup

- Profile coverage, sample recommendation and sample progress remain separate native values. Flutter renders them only; it must not substitute the 500 hard cap for the adaptive recommendation.
- The `playerProfileCoverageShort` label is ARB-owned (EN/DE/AR). Generated localization sources are produced by Flutter from `l10n.yaml`/ARB and must not be hand-maintained in update patches.

## Update 149 - precise Diagnose payload

The Diagnose dialog may render native `coreStartup`, detailed KnowledgeRuntime activity, and profile `workerActivity` fields verbatim. A completed profile may legitimately have derived background maintenance active; Flutter must not collapse those two states into one inferred status.

## Update 151 - combined developer diagnostics

- The existing read-only Diagnose dialog may append `flutterStartup` from the Flutter startup instrumentation to the native Knowledge Inspector result. The section is clearly UI/bootstrap telemetry; all profile/graph/runtime truth remains native.
