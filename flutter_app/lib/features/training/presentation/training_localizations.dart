// -----------------------------------------------------------------------------
// Section: Stable exercise id to ARB mapping
// -----------------------------------------------------------------------------

import '../../../localization/generated/app_localizations.dart';

String trainingExerciseTitle(AppLocalizations strings, String exerciseId) =>
    switch (exerciseId) {
      'endgame_opposition' => strings.trainingOppositionTitle,
      'endgame_lucena' => strings.trainingLucenaTitle,
      'endgame_philidor' => strings.trainingPhilidorTitle,
      _ => strings.trainingEndgameTitle,
    };

String trainingExerciseHint(AppLocalizations strings, String exerciseId) =>
    switch (exerciseId) {
      'endgame_opposition' => strings.trainingOppositionHint,
      'endgame_lucena' => strings.trainingLucenaHint,
      'endgame_philidor' => strings.trainingPhilidorHint,
      _ => strings.trainingEndgameTitle,
    };
