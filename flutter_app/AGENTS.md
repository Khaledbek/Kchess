# Flutter AI Instructions

## Rolle

Flutter ist Präsentations-, Interaktions- und View-State-Schicht. Fachliche Wahrheit kommt aus C++.

## Kontext sparen

- Zuerst die lokale Feature-`AGENTS.md` lesen.
- Nicht pauschal `app_root.dart`, alle Models oder alle FFI-Dateien öffnen.
- Bei einem Widgetproblem: Widget + direkte Callsite(s) lesen.
- Bei einem Datenproblem: DTO-Feld per `rg` verfolgen; Native erst öffnen, wenn der Vertrag betroffen ist.
- Bei großen Screens nur die relevante Section um den Treffer lesen.

## Flutter darf

Screens, Widgets, Navigation, Layout, Theme, Assets, Animationen, lokale UI-Zustände, Dialoge, native Job-Orchestrierung, dünne FFI-Adapter und reine Darstellung besitzen.

## Flutter darf nicht als Domainquelle besitzen

Schachlegalität, PGN/FEN/SAN-Parsing, Ergebnislogik, Analyseheuristiken, Move-Klassifikation, Accuracy, Theory, Persistenz/Migrationen, effektive Engine-Ressourcenregeln oder Trainingslösungen.

## Lokalisierung

Sichtbare Texte nur über ARB (EN/DE/AR gemeinsam). `lib/localization/generated/*` nie manuell ändern.

## Shared / FFI

- gemeinsame Models: `lib/shared/models/`
- Theme/UI-Bausteine: `lib/shared/`
- FFI: `lib/ffi/`
- keine alten Compatibility-Pfade unter `lib/models`, `lib/theme`, `lib/view_models`, `lib/ui/screens` wieder einführen

## Arbeitsweise

Vor DTO-/Widget-Cleanup projektweit nach Aufrufern suchen. Native-JSON nicht nur deshalb ändern, weil Flutter ein Feld nicht konsumiert. Keine automatischen Builds/Tests/Analyzer/Run.


## Update 146 - Windows native build integration

- `windows/CMakeLists.txt` delegates the Windows/MSVC x64 `kchess_core` build to the native persistent CMake cache under `%LOCALAPPDATA%\KChess\build-cache\<checkout-id>` and installs the resulting DLL into the normal Flutter bundle. Flutter remains the packaging/UI owner, not the native dependency tracker.
- Do not restore an in-`flutter_app/build` duplicate `kchess_core` build for Windows while the persistent path is active. Non-Windows/native platform behavior keeps the existing native CMake integration.

## Update 151 - Flutter startup instrumentation

- `lib/diagnostics/app_startup_diagnostics.dart` owns read-only Flutter/bootstrap timing only: `main`, FFI bootstrap phases, controller initialization phases, first frame/first ready frame and a bounded initial frame-timing sample.
- These measurements are diagnostic presentation data. They must not decide navigation, startup gating, background scheduling, caching, profile work or native resource budgets.
## Cleanup Update 2 - dependency ownership

- Keep a package in `pubspec.yaml` only when Flutter production/test code owns a direct import or Flutter requires the SDK dependency directly. `path` was removed as an unused direct dependency; its lock entry is transitive only.
- Current direct non-SDK packages have verified production callsites: `ffi`, `file_picker`, `fl_chart`, `flutter_svg`, `intl`, and `path_provider`.
- Static import reachability from `lib/main.dart` currently covers every Dart file under `lib/`; do not delete screens, shared widgets, models or FFI files merely because a narrow feature search does not show a direct callsite.

### Coach verdict-grounding fallback

Flutter may render `safeFallbackKind=verdict_grounding_unavailable` only through ARB localization. It must not infer a chess verdict, select a move, or turn unavailable native grounding into a UI-side opinion. The verdict decision remains native C++ authority.
- Analysis UI reconciliation: when native main-line refinement transitions from running to complete, the analysis screen must perform the terminal adjacent-position refresh as well. Do not cache a previous after-move snapshot merely because the job is no longer running; the terminal native snapshot may contain the final Stockfish `bestmove` and a deeper reclassification. Flutter only mirrors this native state and must not preserve an older label independently.


## Opening graph integration series — Update 2/7

- Flutter only bundles and installs `assets/opening_lines.kcl` into application support storage, exactly like `opening_book.kcb` and `opening_names.kco`.
- Flutter must not parse KCL, select opening moves, merge transpositions, or implement graph fallback policy. All opening-line semantics remain native C++.
