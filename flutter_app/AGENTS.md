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
