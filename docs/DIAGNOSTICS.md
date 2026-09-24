# KChess Diagnose

Der read-only Diagnose-Dialog auf der Profilseite kombiniert native Runtime-Diagnose mit Flutter-Startup-Telemetrie. Die Werte dienen nur zur Ursachenanalyse; sie steuern weder Scheduler noch Engine-, Profil- oder Knowledge-Verhalten.

## Flutter-Startup

`flutterStartup` (`flutter.startup.v1`) zeigt:

- `buildMode` und `platform`, damit Debug/Profile/Release nicht miteinander verwechselt werden.
- `marks`: Zeitpunkte relativ zu `main()` für Binding, FFI-Gateway, `runApp`, App-`initState`, Controller-Start/-Ready, ersten Frame und ersten Ready-Frame.
- `ffiGateway`: Dauer von DLL-Open, App-Support-Verzeichnis, Sync der gebündelten Opening-Dateien und FFI-Symbolbindung.
- `controllerInitialize`: Dauer der bestehenden initialen Gateway-Reads (`initialize`, Profile, Settings, Games, Favorites, Provider-Übersicht) plus Gesamtdauer.
- `frameTiming`: begrenzte Stichprobe der ersten bis zu 120 Flutter-Frames mit Build-/Raster-Dauer und langsamen Frames über 16,67 ms.

Für reale Startzeitvergleiche ist `profile` oder `release` aussagekräftiger als `debug`. Debug bleibt für die tägliche Entwicklung korrekt, enthält aber JIT-/Debug-Overhead.

## Native Prozesslast

`processRuntime` (`process.runtime.v1`) wird nur beim Abruf des Inspectors gemessen und erzeugt keinen Hintergrund-Sampler.

Unter Windows enthält der Snapshot:

- `processCpuTimeMs`: kumulierte CPU-Zeit des KChess-Prozesses.
- `sinceCoreCreate`: durchschnittliche Prozess-CPU seit Core-Erstellung.
- `sinceLastInspectorSample`: durchschnittliche Prozess-CPU seit dem vorherigen Diagnoseabruf.
- `cpuCoreEquivalentPercent`: 100 % entspricht ungefähr einem vollständig belegten logischen CPU-Kern; Werte über 100 % bedeuten parallele CPU-Nutzung.
- `cpuLogicalCapacityPercent`: derselbe Verbrauch auf die gesamte logische CPU-Kapazität des Rechners normiert.
- `logicalProcessors` und, wenn verfügbar, `processHandleCount`.

Für eine Lastdiagnose den Inspector einmal öffnen, einige Sekunden warten und aktualisieren. `sinceLastInspectorSample` beschreibt dann genau dieses Zeitfenster.

## Bestehende native Diagnose

- `coreStartup`: synchrone native Startphasen.
- `profileBackground.workerActivity`: aktueller Profil-Worker-Schritt.
- `runtimeActivity`: KnowledgeRuntime-Arbeit und Ressourcenbesitz.
- `analysisPerformance`, `statisticsPerformance`, `runtimePerformance`: native Service-Messwerte.
- `sqliteWritePriority`: aktive/wartende SQLite-Schreiber und Wartezeiten.

Ein fertiges Profil kann unabhängig davon Hintergrundarbeit ausführen. Deshalb immer `profileBackground.status`, `workerActivity`, `runtimeActivity` und `processRuntime` getrennt betrachten.

## Cleanup-Serie 1 - produktive Diagnosepfade

Diagnosen müssen ausschließlich den tatsächlich produktiven Laufzeitpfad abbilden.
Für Scouting ist das die Scout-Report-Pipeline; der entfernte leichte Scout-Endpunkt
als produktive Basis, solange kein ausdrücklich akzeptierter und integrierter Nachfolger
aktiviert wurde. Verworfene v2-Artefakte oder reine Entwicklungszweige dürfen in der
Runtime-Diagnose nicht als Fallback oder aktive Pipeline erscheinen.

Repository-Hygiene-, Trainings- und Exportartefakte sind Entwicklerzustand und dürfen
keine Runtime-Entscheidungen beeinflussen. Flutter-Startup-Telemetrie bleibt read-only;
Scheduling, Analysepriorität und Ressourcenentscheidungen verbleiben in der nativen
Produktlogik.
