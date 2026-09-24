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

## Update 152 - explicit analysis cancellation

Starting analysis remains a native AnalysisService job. The preparation modal and a running AnalysisScreen expose an upper-right close (`X`) action that calls the existing `CoreGateway.cancelAnalysis` contract; Flutter must not simulate cancellation by merely closing the route.

## Classification/Arrow Coherence Series - Update 8/9

Best-move/threat-arrow eligibility and classification visibility are native policy. `AnalysisService` transports `analysis.arrow.v1` plus `analysis.classification.v1`. `analysis_arrow_resolver.dart` may only enforce the arrow presentation boundary (`renderable` + exact displayed FEN) and return the transported move; board/cards may show a classification only when its native classification contract is renderable. Flutter must not infer rank 1 from `lines`, `bestMove`, engine depth, classifier labels or SF18/SF19-specific conditions, and it must not repair an invariant violation locally. A non-renderable or stale contract means no corresponding visual.

### Classification/Arrow Coherence Series - final presentation boundary

Presentation is fail-closed. `analysis_arrow_resolver.dart` requires the exact `analysis.arrow.v1` schema, a non-empty snapshot ID, matching displayed FEN and `renderable=true`. Classification widgets require `AnalysisClassificationContract.presentationRenderable`, which accepts only `analysis.classification.v1` with non-empty snapshot provenance and native `renderable=true`. Do not use null contracts as implicit permission and do not fall back to `bestMove`, PV rank or local category inference.

## Unified Move Classification Series — Update 7/7 presentation contract

Flutter renders the native classification verbatim. `inaccuracy` is a first-class transported category with label text from ARB and icon path `assets/analysis_img/move_inaccuracy.png`. Flutter must not recompute Best/Excellent clusters, Great uniqueness, Brilliant sacrifice evidence, mate-distance quality, Miss, or negative severity. Those decisions remain native C++.
