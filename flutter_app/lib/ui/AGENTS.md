# App Shell / Startup Agent

## Scope

`app_root.dart`, Startup und globale Feature-Komposition.

## Regeln

- App-Shell soll Navigation/Komposition bleiben, keine Feature-Domainlogik aufnehmen.
- Bei Featureproblemen zuerst das Feature selbst bearbeiten; `app_root.dart` nur öffnen, wenn Routing/Composition betroffen ist.
- Profilkopf und Hauptnavigation nicht mit fachlichen Provider-/Library-Regeln koppeln.
- Startup-Fehlerzustände dürfen native Fehler darstellen, aber nicht fachlich neu interpretieren.
