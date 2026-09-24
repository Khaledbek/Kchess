# Python / Tooling AI Instructions

## Rolle

`tools/` enthält Builder, Datenaufbereitung und Diagnose. Python ist keine Android-/Windows-Runtime von KChess.

## Kontext sparen

Nur das konkrete Tool und seine Format-/Metadata-Datei lesen. Native oder Flutter erst öffnen, wenn ein Binärformat/API-Vertrag überprüft werden muss. Große Quelldumps niemals vollständig in Kontext laden.

## Aktuell

- `opening_book/` → KCB1
- `opening_names/` → KCO1
- `provider_smoke.py` → manueller Provider-Smoke-Check
- `ai/` → Dataset-Splits, Trainings-Metadaten, Evaluation, Quantisierung und Retrieval-Benchmarks

## Regeln

- Runtime-Domainlogik bleibt in C++.
- Builder reproduzierbar und möglichst deterministisch halten.
- Inputs, Checksummen, Version/Parameter dokumentieren.
- Große Quellen streamen.
- `BUILD_METADATA.md` bei ausgelieferten Datenartefakten pflegen.
- keine einmaligen lokalen Hilfsskripte ohne dauerhaften Zweck committen.
- `third_party/` nur bei expliziter Dependency-Aufgabe ändern.
- keine automatischen Builds/Tests starten.


## Update 146/148 - developer build commands

- `build_dev.ps1` and `rebuild_native.ps1` are developer build orchestration only. They must not contain application/domain logic.
- `build_dev.ps1` is the normal Windows command and must never call `flutter clean`. `rebuild_native.ps1` resets only the current checkout's external KChess-core cache under `%LOCALAPPDATA%` and then rebuilds `kchess_core` directly through CMake/MSBuild. It must never call `flutter run` or launch the app, and it must not delete the stable SF18/SF19 libraries under `native/prebuilt/windows/` or the separate Stockfish cache.
- There is no custom full-clean wrapper. Flutter's own `flutter clean` keeps its normal role when a Flutter reset is required or immediately before creating a compact project ZIP.

## Update 170 - quiet native rebuild diagnostics

- `rebuild_native.ps1` remains a true recovery rebuild: it still deletes only the current checkout's external `kchess-core` build tree and recompiles the whole core. Output suppression must never change that reset/build behavior.
- Normal CMake/MSBuild per-file chatter is hidden. Warnings/errors stay visible, and the script prints only local native source/build inputs changed since the previous successful native build. The exact comparison uses a SHA-256 manifest stored beside (not inside) the resettable core cache; the first run may fall back to the previous `kchess_core` artifact timestamp.
- The source manifest is developer build metadata only. It must never be used as application state, dependency truth, or a replacement for CMake/MSBuild dependency tracking. `third_party/` and `native/prebuilt/` are excluded from this display manifest.

## kchess_update_serie_1 - Update 11 repository hygiene

- `check_repo_hygiene.ps1` is a developer guard only. It scans files visible to Git (tracked plus non-ignored untracked files) and fails on private credential paths, common private-key/API-key signatures, local model binaries, or files above the configured size ceiling.
- The guard must not read or print secret values. Findings report only the file path/category.
- Source manifests/contracts needed to describe model compatibility belong in normal source/docs paths, not inside an ignored binary payload tree.

## Native rebuild UX

- `rebuild_native.ps1` keeps its full native recovery semantics while rendering normal configure/build progress on one carriage-return-updated line. The line shows percent, current compile unit and a compact changed-input summary; compiler/CMake warnings and errors remain separate visible diagnostics.
- Progress percentages are UI/diagnostic estimates only. CMake/MSBuild exit status remains the sole build-success authority.
