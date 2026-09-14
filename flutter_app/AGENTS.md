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
