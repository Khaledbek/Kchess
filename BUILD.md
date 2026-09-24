# KChess Windows Build

Die großen Windows-Build-Zwischenstände für den KChess-C++-Core und für
Stockfish liegen außerhalb des Projekts unter:

```text
%LOCALAPPDATA%\KChess\build-cache\<checkout-id>\
```

Dadurch bleibt der native Build inkrementell, auch wenn Flutter bereinigt wird.
Geänderte C/C++-Dateien und Header werden weiterhin automatisch von
CMake/MSBuild neu gebaut; unveränderte Object-Dateien werden wiederverwendet.
Die fertigen SF18/SF19-Libraries bleiben dauerhaft unter
`native/prebuilt/windows/` und werden von `rebuild_native.ps1` nicht gelöscht.

## Befehle

### Normal entwickeln

```powershell
.\tools\build_dev.ps1
```

**Immer verwenden** nach normalen Änderungen an Dart, C++, Headern oder ARB.
Das Skript baut inkrementell und führt kein `flutter clean` aus.

Optional:

```powershell
.\tools\build_dev.ps1 -Configuration Release
.\tools\build_dev.ps1 -Configuration Profile
```

### KChess-C++-Core komplett neu bauen

```powershell
.\tools\rebuild_native.ps1
```

**Nur bei Bedarf verwenden**, wenn der native CMake/MSVC-Zwischenstand Probleme
macht oder grundlegende native Buildoptionen geändert wurden. Es löscht nur den
externen KChess-Core-Cache des aktuellen Projektpfads und baut anschließend **nur
`kchess_core` direkt über CMake/MSBuild** neu. Es startet Flutter und die App nicht.
SF18/SF19 und deren stabile `.lib`-Dateien bleiben erhalten.

Die Ausgabe ist bewusst kompakt: Konfiguration und normaler Buildfortschritt
bleiben in **einer einzigen aktualisierten Statuszeile**. Sie zeigt Prozentwert,
aktuell erkannte C/C++-Compile-Unit und eine kompakte Liste der seit dem letzten
erfolgreichen nativen Build geänderten lokalen Quell-/Builddateien. Warnungen und
Fehler werden weiterhin separat vollständig sichtbar ausgegeben. Die Prozentanzeige
ist nur Build-UI; Erfolg oder Fehler wird ausschließlich vom CMake/MSBuild-Exitcode
bestimmt. Der Rebuild selbst bleibt ein vollständiger Core-Rebuild.

### Flutter bereinigen / Projekt vor ZIP aufräumen

Aus `flutter_app`:

```powershell
flutter clean
```

Das ist der normale, unveränderte Flutter-Befehl. Er entfernt Flutter-generierte
Builddateien im Projekt. Die großen nativen CMake/MSBuild-Caches liegen bereits
außerhalb des Projekts und landen deshalb nicht im Projekt-ZIP. Die SF18/SF19-
Libraries unter `native/prebuilt/windows/` bleiben im Projekt erhalten.

Danach bei Bedarf wieder normal bauen:

```powershell
..\tools\build_dev.ps1
```

## Merksatz

```text
Normal arbeiten       -> .\tools\build_dev.ps1
Native Core Problem   -> .\tools\rebuild_native.ps1
Vor Projekt-ZIP       -> cd flutter_app; flutter clean
```
## Update Series 2 - Update 7

Added the deterministic interaction parameter parser under `native/ai/interaction/`. Explicit Elo/rating values, requested color, move references, move numbers, common time controls and explicit play/analysis/training mode cues can now be extracted from the current user utterance without an LLM or chat-history payload. The parser intentionally does not infer chess intent; semantic meaning remains the responsibility of later resolver stages. No build or tests were run by the assistant.

### Update series 2 / Update 8 — Action & Requirement Resolver

Added `native/ai/interaction/interaction_requirement_resolver.{h,cpp}`. The resolver converts whitelisted semantic planner selections plus deterministic parameters into primitive actions and evidence requirements, completes mandatory dependencies, and rejects empty or invalid active-reference plans. No build or tests were run by the assistant.

### Update series 2 / Update 9 — Action Chain Engine

Added `native/ai/interaction/action_chain_engine.{h,cpp}`. It converts a valid resolved interaction plan into an ordered deterministic chain with explicit evidence/session gates. Rating setup precedes game start, game start precedes live coaching, evidence work precedes board presentation, and language actions are ordered last. The engine only defines execution order; it does not execute providers, chess engines, UI work, or LLM calls. No build or tests were run by the assistant.

### Update series 2 / Update 11

Added deterministic follow-up suggestion generation. Suggestions are canonical actions with localization keys and active evidence/line/move references so future UI chips can execute without another planner LLM call. No build or tests were run by the assistant.

## Update series 2 / Update 12A - Coach turn UI contract

- Added `native/ai/interaction/coach_turn_view.*` as the UI-facing projection for the active coach turn.
- The projection carries only the latest user text, latest coach text, up to four structured follow-up actions, and presentation flags.
- Older history remains a scrollable presentation concern; no history is deleted and no planner logic moves into Flutter.
- Follow-up actions remain structured action IDs with optional evidence/line/move references, so tapping them must not be reinterpreted as free text.
- This is the native contract half of the chat UI redesign; Flutter integration is intentionally separated because UI work is a larger update.

### Update series 2 / Update 10 completion — Answer Gate

Added `native/ai/interaction/answer_gate.{h,cpp}`. The gate deterministically decides whether the second answer LLM is required. Pure board/game/session actions complete without prose generation; explicit explanation/comparison/answer actions request the answer LLM. This component was finalized together with Update 14 after the series audit found the standalone Update 10 source file missing. No build or tests were run by the assistant.

### Update series 2 / Update 14 — Regression corpus

`native/ai/interaction/interaction_regression_cases.{h,cpp}` keeps provider-neutral routing fixtures for previously broken flows. The former standalone `interaction_diagnostics.*` implementation was later removed because it was not wired to the productive Coach pipeline; current interaction diagnostics are emitted directly from `CoachOrchestrator`/`CoachService`.

### Cleanup-Serie 1 / Update 6 - Finaler statischer Audit

Der Abschlussaudit wurde ausschließlich statisch durchgeführt; Builds, Tests, `flutter analyze` und `flutter run` wurden nicht ausgeführt. Der entfernte leichte Scout-ABI-/Servicepfad besitzt keine produktiven Code-Referenzen mehr. Die verworfene Evidence-Planner-/Local-Model-Idee wurde in Cleanup-Serie 2 vollständig aus produktiver Runtime, Tooling und aktiver Architekturdokumentation entfernt. Das kumulative Paket enthält ein Cleanup-PowerShell-Skript für physische Löschungen und generierte Cache-/Buildreste.

### LLM-Interaktionsserie 1 / Update 2 — Native Action Fulfillment

Der Coach transportiert direkte Spiel-/Sessionaktionen jetzt als native `clientActions` statt als LLM-Prosa. Flutter führt nur die von C++ gelieferte Navigation bzw. bestehende Bot-FFI-Aktion aus. Action-only Antworten gelten auch ohne `answer` als erfolgreich. In diesem Update wurden entsprechend der Projektvorgabe keine Builds, Tests, `flutter analyze` oder `flutter run` ausgeführt.

## LLM Interaction Series 1 - Update 3

No new runtime dependency or build step is introduced. Mixed app-action/language turns remain entirely within the existing C++ coach pipeline and Gemini provider adapter. The provider context contract is now `coach_llm_context.v4` / `CoachPrompt v14`; `coach_response.v8` is unchanged. Native session/product actions are transported independently of the provider call. No build or test command was executed for this update.

## LLM-Interaktionsserie 1 / Update 4 - Action Fulfillment + produktive Diagnostics

`native/ai/interaction/action_fulfillment_validator.*` wird als normaler Bestandteil von `kchess_core` gebaut. Es gibt keine neue Runtime-Dependency und keinen zusätzlichen Buildschritt. Provider-/Grounding-Validation und Produktaktions-Fulfillment sind getrennte native Verträge. Die Coach-Performance-Diagnostics stammen direkt aus dem produktiven Orchestrator/Service-Trace; der alte unverdrahtete `interaction_diagnostics.*`-Pfad wurde entfernt. Für dieses Update wurden durch den Assistenten keine Builds, Tests, `flutter analyze` oder `flutter run` ausgeführt.

## LLM-Interaktionsserie 1 / Update 5 - Multilingual Regression + Action Parameters

Added `native/tests/interaction_action_routing_tests.cpp` and the `kchess_interaction_action_routing_tests` CTest target. It covers direct start-game requests in German/English/Arabic, negative non-action phrases, mixed start+coaching, explicit Elo and explicit black color. The assistant did not execute this test target or any build/analyze/run command.

Explicit game Elo no longer creates a rating-prediction semantic task. Explicit player color now travels through the native action contract into `CoachClientAction`, through the thin Flutter FFI, and into persisted bot-session creation. `kc_create_bot_game_json` now accepts `player_color_utf8`; the native/Flutter ABI version is 10. Existing setup/default behavior remains white when no explicit color is supplied.

## Coach-Serie 2 / Update 1 - Best-Move Response Fulfillment

Provider-freie `best_move`-Antworten verwenden jetzt den nativen `nativeAnswerKind`-Transport und die vorhandene ARB-Lokalisierung. Es gibt keine neue Runtime-Dependency und keinen neuen Buildschritt. Der Assistent hat für dieses Update entsprechend der Projektvorgabe keine Builds, Tests, `flutter analyze` oder `flutter run` ausgeführt.


## Coach-Serie 2 / Update 2 - Automatic Coach Trigger Gate

This update changes only native Coach policy/diagnostics plus native regression coverage. It adds no new runtime dependency and requires no Flutter asset or ABI change. `native/ai/automatic/automatic_coach_trigger.*` owns the anti-spam gate; `CoachService` supplies successful-delivery recency and exposes `triggerGate` diagnostics. The new `kchess_automatic_coach_trigger_tests` target is registered under the existing native test option but is not run automatically by packaging or cleanup scripts.


### Coach Series 2 Update 3 — move attribution contract
Automatic Coach requests now carry native mover/learner attribution from the verified pre/post position transition. Provider context is `coach_llm_context.v4` and `CoachPrompt v14`; response-cache keys include the attribution contract. Diagnostics expose mover color, learner color, role, played UCI and whether the learner move was verified. No build or test command was executed for this update.

## Coach Series 2 - Update 4/7

Added durable AI Coach session persistence to the existing `kchess.sqlite3` through schema migration 43. New profile-owned session/message tables retain creation/update/open timestamps, monotonic `Sitzung N` numbering, user-visible transcript rows and a separate compact native continuation snapshot. `CoachService` now restores compact session state before resumed turns and persists transcript/state after manual or visible automatic Coach turns; `CoachSessionMemory` remains persistence-neutral. Database APIs for listing, renaming, deleting and reading session history are in place for the next Flutter/FFI update. No build, test, `flutter analyze` or `flutter run` command was executed.

## Coach-Serie 2 / Update 5 - Coach session management

The durable Coach-session store from Update 4 is exposed additively through Core/C-ABI/Flutter FFI for list/create/read-transcript/rename/delete operations. `KCHESS_CORE_ABI_VERSION` and Flutter's supported ABI are now 11. The ordinary Coach tab resumes the most recently updated session for the active profile, while context-specific Coach launches create a fresh session. Session transcript restoration may also restore the persisted native `currentBoard` FEN through the existing `coachContext` board DTO path. Flutter only renders and forwards user commands; C++/SQLite owns IDs, monotonic `Sitzung N` allocation, timestamps and mutation. No build, test, `flutter analyze` or `flutter run` command was executed for this update.

## Coach-Serie 2 / Update 6 - Background CPU / Knowledge Maintenance

Kein neuer Build-Schritt. Die native SQLite-Schema-Version steigt auf 44. Beim ersten Start nach dem Update wird `ai_knowledge_maintenance_state` automatisch angelegt. Ein unveränderter Profil-/Kontowechsel kann danach die persistierte Game-Graph-Projektion wiederverwenden; echte Änderungen führen weiterhin die vollständige Projektion aus, nun mit kooperativen CPU-Yields. Diagnostics zeigen Skip-/Yield-Zähler. Für dieses Update wurden entsprechend der Serienvorgabe keine Builds oder Tests ausgeführt.

## Coach-Serie 2 / Update 7 - Legacy Logo Restore + Final Audit

Der ursprüngliche KChess-App-Icon-Satz wird unverändert wieder ausgeliefert: `assets/analysis_img/app_logo.png`, Android `mipmap-*/ic_launcher.png` und Windows `runner/resources/app_icon.ico` sind byte-identisch zum alten Projektstand. Es gibt keinen zusätzlichen Generator- oder Build-Schritt. Der statische Abschlussaudit hält ABI 11, SQLite-Schema 44 und die vorhandene C++/SQLite -> C-ABI -> Flutter-FFI-Grenze fest. `secrets/` bleibt unberührt. Für dieses Update wurden keine Builds, Tests, `flutter analyze` oder `flutter run` ausgeführt.

## Coach Functional Hotfix 2 - direct best/worst answers

No ABI or SQLite schema change. The hotfix extends the existing provider-free native answer transport with `worst_move`, adds ARB/generated localization accessors for German/English/Arabic, retires stale quiz/hint state when an explicit direct analysis request takes control, and aligns response/trace success with actual renderable output. No build, test, `flutter analyze` or `flutter run` command was executed by the assistant.



## Coach Answer Quality Serie 3 - Update 1

Der native Coach besitzt jetzt einen verbindlichen `ChessVerdictContract` und uebergibt ihn ueber `coach_llm_context.v5` an den Provider. Neue Runtime-Abhaengigkeiten entstehen nicht; `native/ai/verdict/chess_verdict_builder.cpp` ist lediglich eine weitere C++20-Quelle im bestehenden `kchess_core`-Target. `coach_response.v8`, ABI 11 und SQLite-Schema 44 bleiben unveraendert. Der Validated-Response-Cache nimmt Context-Version und Verdict in die Identitaet auf. Entsprechend der Projektvorgabe wurden fuer dieses Update keine Builds, Tests, `flutter analyze` oder `flutter run` ausgefuehrt.

## Coach Answer Quality Serie 3 - Update 2

Kein neuer Build-Schritt und keine neue Runtime-Abhaengigkeit. Der bestehende `kchess_core`-Target enthaelt bereits `ai/verdict/chess_verdict_review.cpp`; die Re-Evaluation nutzt die vorhandene Stockfish-Engine ueber `AnalysisService`. Provider-Kontext steigt auf `coach_llm_context.v6`; ABI 11, `coach_response.v8` und SQLite-Schema 44 bleiben unveraendert. Fuer dieses Update wurden keine Builds, Tests, `flutter analyze` oder `flutter run` ausgefuehrt.

## Coach Answer Quality Serie 3 - Update 3/6

No new dependency or build step is introduced. The existing Gemini structured-output schema advances to `coach_response.v9` with a mandatory native-constrained `verdict_lock`, and the response validator checks the returned lock against `ChessVerdictContract` / `ChessVerdictReview`. Prompt identity advances to `CoachPrompt v15`; provider context stays `coach_llm_context.v6`. ABI 11 and SQLite schema 44 are unchanged. Per project workflow, no build, test, `flutter analyze` or `flutter run` command was executed.
### Coach Answer Quality Serie 3 - Update 4

Evaluative move/position questions now require native verdict grounding before provider generation. Missing move/position verdict proof returns the localized `verdict_grounding_unavailable` safe fallback with zero provider calls. Diagnostics add `chessVerdict.groundingRequired`, `groundingAvailable`, and `groundingBlocked`. No ABI or SQLite schema change.



### Coach Answer Quality Serie 3 / Update 5 — verdict-first response structure
No new dependency or build step is introduced. The existing Gemini structured-output contract advances to `coach_response.v10` with a required `verdict_summary` for authoritative native chess verdicts. Native parsing/rendering places that summary before the explanatory segments; `ResponseValidator` enforces presence/absence against the native verdict lock. Prompt identity advances to `CoachPrompt v16`; `coach_llm_context.v6`, ABI 11 and SQLite schema 44 are unchanged. Per project workflow, no build, test, `flutter analyze` or `flutter run` command was executed.

## Coach Answer Quality Serie 3 / Update 6 — adversarial regression + final audit

Added `native/tests/coach_verdict_regression_tests.cpp` and the `kchess_coach_verdict_regression_tests` CTest target under the existing native test option. The suite exercises German/English/Arabic move and position verdict requests, user objections including explicit "do you agree with me" wording, same-move/same-FEN challenge binding, changed/unchanged/inconclusive review semantics, exact `verdict_lock` enforcement, and required verdict-first summaries. The challenge detector was extended for agreement-seeking objections in DE/EN/AR; new best/worst/extreme questions remain separate analyses. No runtime dependency, ABI change or SQLite migration is introduced: `coach_response.v10`, `CoachPrompt v16`, `coach_llm_context.v6`, ABI 11 and SQLite schema 44 remain the current contracts. Per project workflow, no build, test, `flutter analyze` or `flutter run` command was executed for this update.

## Classification/Arrow Coherence Series - Update 2/9

No new runtime dependency, ABI change or SQLite migration. `PersistedAnalysis` reads the already-existing `analysis_runs.depth` and `analysis_runs.multi_pv` columns so the service can emit the new `analysis.snapshot.v1` provenance contract. Analysis config hashing now uses the existing engine `cache_identity()` (binary/adaptor identity plus NNUE identity) instead of version text alone; incompatible evaluator snapshots therefore naturally receive a new config hash. Flutter only parses the provenance DTO. Per project workflow, no build, test, `flutter analyze` or `flutter run` command was executed for this update.

## Classification/Arrow coherence series — Update 3/9

- SF18 sideline analysis now searches the classification MultiPV width up front and hides surplus lines from UI output. This removes the previous MultiPV-1 arrow -> separate MultiPV-4 classifier split.
- Added an unexecuted native workflow regression asserting that following the published SF18 rank-1 arrow is never classified as miss/mistake/blunder due to MultiPV widening.
- No build, test, `flutter analyze` or `flutter run` was executed for this update.

## Classification/Arrow Coherence Series - Update 4/9 (SF19)

Stockfish 19 classification now fails closed when the exact published rank-1 root move conflicts materially with an independent resulting-position search. The move is not downgraded; classification is withheld and a bounded root-restricted `searchmoves` probe is used only to diagnose stability. The SF19 classifier no longer contains a second severity path that can punish `played_is_best=true`. Variation JSON additionally exposes `classificationStatus=stable|pending|unstable`. ABI and SQLite schema are unchanged. No build/test command was executed for this update.

## Classification/Arrow Coherence Series - Update 5/9

No new dependency, ABI change or SQLite migration. SF18 classifier version advances to 13 and SF19 to 1905. Both engines now use the shared native WDL/expected-score-first negative-severity contract, with centipawns only as fallback when normalized outcome evidence is absent. `AnalysisService` now supplies final-PV net material to the common classifier instead of transient maximum material loss. Existing classifier regression sources were updated but, per project workflow, no build, test, `flutter analyze` or `flutter run` command was executed for this update.

## Classification/Arrow Coherence Series - Update 6/9

No new dependency, ABI change or SQLite migration. Existing `analysis_runs.time_limit_seconds` and `adaptive_early_stop` columns are now loaded into `PersistedAnalysis` so the native stability gate can distinguish a completed time budget from an incomplete depth-bounded search. SF18 classifier version advances to 14 and SF19 to 1906 to invalidate classifications that predate the stability/hysteresis policy. Main/sideline responses expose `classificationStatus`; sideline responses additionally expose `classificationStabilityReason`. Per project workflow, no build, test, `flutter analyze` or `flutter run` command was executed for this update.

## Classification/Arrow Coherence Series - Update 7/9

No new dependency, ABI change or SQLite schema migration. A header-only `analysis/classifier_contract.h` centralizes current SF18/SF19 classifier generations. Existing completed `analysis_runs` and `engine_lines` are reused for reclassification; background/statistics maintenance selects stale classifier generations for derived-only rebuilds without Stockfish work. Statistics/Profile/Game read paths publish only SF18 v14 or SF19 v1906 derived labels. Analysis diagnostics expose both current classifier versions. Per project workflow, no build, test, `flutter analyze` or `flutter run` command was executed.

## Classification/Arrow Coherence Series - Update 8/9

- Added native `analysis.arrow.v1` and `analysis.classification.v1` presentation provenance for main-line and variation responses.
- Main-line arrows are suppressed while live engine/PV provenance and the published classification snapshot differ. Classifications are rendered only when the native classification contract allows them.
- Variation arrows are exposed only after the complete after-position engine search. The widened internal result is now stored under the same widened settings key that the next ply uses for its BEFORE-position lookup, so following a visible arrow reuses that exact root snapshot instead of silently rerunning the root.
- Native presentation fails closed if a published rank-1 move ever arrives with a negative category: the label is withheld and marked unstable. Flutter does not repair or reinterpret the chess result.
- Flutter renders only native presentation contracts with matching board provenance; it no longer derives arrows from PV rank or result-level `bestMove` fallbacks.
- No ABI or SQLite migration is required.
- Per project workflow, no build/test/`flutter analyze`/`flutter run` command was run for this update.

## Classification/Arrow Coherence Series - Update 9/9 Final Audit

The final regression coverage extends the existing `kchess_analysis_workflow_tests`, `kchess_classifier_quality_tests`, `kchess_classifier_sf19_tests`, `kchess_sf19_result_coherence_tests`, and Flutter `analysis_arrow_resolver_test.dart` contracts. SF18/SF19 follow-arrow behavior, presentation provenance, WDL-first severity, stability withholding and fail-closed Flutter rendering are covered in source. No new runtime dependency, ABI change or SQLite migration is introduced. Current classifier generations are SF18 v14 / SF19 v1906; presentation schemas are `analysis.arrow.v1`, `analysis.classification.v1`, and `analysis.snapshot.v1`. Per project workflow, no build, CTest, Flutter test, `flutter analyze` or `flutter run` command was executed by the assistant for this update.

## Opening graph asset (`opening_lines.kcl`)

The Training Arena opening-line graph is shipped as `flutter_app/assets/opening_lines.kcl`. Flutter startup copies the bundled asset to the application-support data directory; native `KclOpeningLineGraph` then performs strict KCL1 validation during `Core::initialize()`.

KCL is independent of the opening book rebuild. `opening_book.kcb` may be regenerated with broader historical coverage while retaining its existing KCB schema; the line graph remains a separate topology asset. A malformed/missing KCL disables only opening-line graph availability and does not change KCB/KCO loading.

### Opening graph position/move contract — Update 3/7

`opening_lines.kcl` and `opening_names.kco` expose the same canonical Stockfish position-key/fingerprint contract for compatibility diagnostics. KCL remains the topology authority even if optional KCO coverage differs; naming may degrade without disabling the graph. KCL move encoding version 1 is the same 16-bit UCI codec exported by `theory/position_key.*` and used by KCB; every loaded move is decode/encode round-trip validated. The KCB schema and its broader-history rebuild remain unchanged. No build, test, `flutter analyze` or `flutter run` command was executed for this update.


### Opening graph integration — final contract

The completed Training-Arena runtime treats every legal `KCL1` edge as an accepted opening continuation. `opening_book.kcb` does not filter graph legality: its game counts only order hints and weight opponent sampling, so replacing the bundled KCB with the broader 14-month rebuild requires no KCL or ABI/schema change. `opening_names.kco` supplies destination ECO/name data where available.

The legacy `openings_*.inc` files remain compile-time catalogue/setup content for stable opening IDs, hierarchy and persisted progress compatibility. KCL1 itself stores terminal-keyed root-to-terminal move lines, from which native runtime derives the continuation graph. Productive continuation play after setup is KCL-only; the old PolyGlot fallback is not used by the Training Arena. Native opening-drill regression setup copies `opening_lines.kcl`, `opening_names.kco`, and `opening_book.kcb` into the test data directory before core initialization to mirror production installation. No build or test command was executed by the assistant for this series.

### Opening graph diagnostic/runtime fix (2026-09-19)
The KCL reader is loaded independently from KCO coverage. A valid `opening_lines.kcl` remains active even when `opening_names.kco` has different coverage/fingerprint; KCO is labeling metadata, not a topology gate. `practiceCommand({"op":"openingStatus"})` reports graph availability and metadata for diagnostics. No build/test command was executed as part of this update series.


## Opening line semantics correction series — Update 3/3

Corrected the KCL1 runtime contract after the Center Game regression exposed that persisted node move ranges had been interpreted as outgoing edges. KCL1 records are terminal-position records with a byte offset plus ply count into a packed root-to-terminal move-line table. `KclOpeningLineGraph` now normalizes byte offsets, replays every line from the standard start position, verifies the replayed terminal Stockfish key, deduplicates shared prefix edges, and merges transpositions into a derived in-memory graph. `continuations(fen)` queries only that derived graph.

Static regression coverage now includes Center Game after `3.Qxd4` (must expose `...Nc6` and never replay `e2e4/e7e5` as replies), Bishop's Opening prefix merging, legal destination-key checks, and move-order transposition convergence. KCO/KCB formats remain unchanged. No build, CTest, Flutter analysis, or app run was executed by the assistant.
