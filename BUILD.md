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

Die Ausgabe ist bewusst kompakt: normale CMake/MSBuild-Dateilisten werden
unterdrückt. Warnungen/Fehler bleiben sichtbar; nach einem erfolgreichen Lauf
werden nur lokale native Quell-/Builddateien angezeigt, die sich seit dem letzten
erfolgreichen nativen Build geändert haben. Der Rebuild selbst bleibt trotzdem
ein vollständiger Core-Rebuild.

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
