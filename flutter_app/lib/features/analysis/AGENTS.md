# Analysis UI Agent

## Start hier

- Hauptscreen: `presentation/analysis_screen.dart`
- Pfeile: `presentation/analysis_arrow_resolver.dart`
- Side-Line-Graph: `presentation/analysis_variation_graph.dart`
- temporäres Bot-Spiel: `presentation/analysis_temporary_bot_game_screen.dart`

## Token-Regel

`analysis_screen.dart` ist groß: **nie standardmäßig komplett lesen**. Zuerst Symbol/Section mit `rg`, dann nur den relevanten Bereich plus direkte Helper öffnen.

## Verantwortung

Flutter zeigt native Evaluation, WDL, MultiPV, Best-Move-Pfeile, Klassifikation, Accuracy, Theory, Result-Symbole und Side-Line-Navigation. Fachliche Bewertung/Klassifikation bleibt nativ.

## Native nur bei Bedarf

- Analysejob/Resultat → `native/src/services/analysis_service.*`
- Klassifikation/Accuracy → `native/src/analysis/`
- Engineunterschiede → `native/src/engine/`
- FFI-Vertrag → `flutter_app/lib/ffi/` + exaktes Symbol in `native/src/api/core_api.cpp`

Keine SF18/SF19-Korrekturlogik in Dart hinzufügen. Side-Lines bleiben flüchtig.
