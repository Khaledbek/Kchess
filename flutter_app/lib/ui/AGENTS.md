# App Shell / Startup Agent

## Scope

`app_root.dart`, Startup und globale Feature-Komposition.

## Regeln

- App-Shell soll Navigation/Komposition bleiben, keine Feature-Domainlogik aufnehmen.
- Bei Featureproblemen zuerst das Feature selbst bearbeiten; `app_root.dart` nur öffnen, wenn Routing/Composition betroffen ist.
- Profilkopf und Hauptnavigation nicht mit fachlichen Provider-/Library-Regeln koppeln.
- Startup-Fehlerzustände dürfen native Fehler darstellen, aber nicht fachlich neu interpretieren.

## Update 151 - startup timing ownership

- App-shell startup may place monotonic diagnostic marks for `appInitState`, first Flutter frame and first ready frame. These marks are observation-only; AppRoot readiness still follows `AppController.phase` and no timing threshold may alter startup behavior.
