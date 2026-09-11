# Python / Tooling AI Instructions

## Rolle

`tools/` enthält Builder, Datenaufbereitung und Diagnose. Python ist keine Android-/Windows-Runtime von KChess.

## Kontext sparen

Nur das konkrete Tool und seine Format-/Metadata-Datei lesen. Native oder Flutter erst öffnen, wenn ein Binärformat/API-Vertrag überprüft werden muss. Große Quelldumps niemals vollständig in Kontext laden.

## Aktuell

- `opening_book/` → KCB1
- `opening_names/` → KCO1
- `provider_smoke.py` → manueller Provider-Smoke-Check

## Regeln

- Runtime-Domainlogik bleibt in C++.
- Builder reproduzierbar und möglichst deterministisch halten.
- Inputs, Checksummen, Version/Parameter dokumentieren.
- Große Quellen streamen.
- `BUILD_METADATA.md` bei ausgelieferten Datenartefakten pflegen.
- keine einmaligen lokalen Hilfsskripte ohne dauerhaften Zweck committen.
- `third_party/` nur bei expliziter Dependency-Aufgabe ändern.
- keine automatischen Builds/Tests starten.
