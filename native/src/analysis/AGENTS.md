# Native Analysis Agent

## Scope

Move-Klassifikation und Accuracy.

## Startdateien

- `move_classifier.cpp/.h`
- `move_classifier_sf19.cpp/.h`
- `accuracy.cpp/.h`

## Token-Regel

Bei Klassifikationsfehlern nur die betroffene Enginevariante + aufgerufene Helfer lesen. Engine-Service erst öffnen, wenn Eingangsdaten/Score-Perspektive unklar sind.

## Regeln

- Zugklassifikation immer relativ zur Stellung **vor** dem Zug und zur besten gegnerischen Antwort bewerten.
- SF18/SF19 dürfen getrennte Kohärenzpfade haben, aber keine Dart-Korrektur benötigen.
- Accuracy bleibt unabhängig von UI-Labels.
- Keine Klassifikationsentscheidung in Flutter spiegeln.
