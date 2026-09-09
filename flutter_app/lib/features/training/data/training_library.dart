import '../models/training_exercise.dart';

/// The bundled exercise catalogue.
///
/// Seeded in Dart because these are static content, not domain logic — the
/// positions are inert FEN/SAN strings that the native core validates. Every
/// line alternates solver move / opponent reply starting from the exercise's
/// FEN, so the player can replay it move by move; `core_tests.cpp`
/// (`test_training_endgame_lines`) replays all three against the real move
/// generator, which is what proves the SAN below is legal and unambiguous.
///
/// Titles and hints stay German for now; they move into the ARB files together
/// with the rest of the exercise content.
class TrainingLibrary {
  const TrainingLibrary._();

  /// Theoretical endgames, ordered from easiest to hardest.
  static const endgames = <TrainingExercise>[
    TrainingExercise(
      id: 'endgame_opposition',
      title: 'Bauern-Opposition (Wesentliches Schlagen)',
      category: TrainingCategories.endgame,
      goal: TrainingGoals.win,
      // White king d5, pawn e4 · black king e7, White to move.
      startingFen: '8/4k3/8/3K4/4P3/8/8/8 w - - 0 1',
      // 1.Ke5 Kd7 2.Kf6 Kd6 3.e5+ Kd5 4.e6 Kd6 5.e7 Kd7 6.Kf7 and the pawn
      // queens: take the opposition, outflank, and only then push.
      targetMovesSan: [
        'Ke5',
        'Kd7',
        'Kf6',
        'Kd6',
        'e5+',
        'Kd5',
        'e6',
        'Kd6',
        'e7',
        'Kd7',
        'Kf7',
      ],
      hintText:
          'Nimm zuerst die Opposition, dann umgehe den König. Der Bauer '
          'zieht erst, wenn der König vor ihm steht.',
    ),
    TrainingExercise(
      id: 'endgame_lucena',
      title: 'Lucena-Stellung (Brückenbau mit dem Turm)',
      category: TrainingCategories.endgame,
      goal: TrainingGoals.win,
      // White king d8, pawn d7, rook e1 (cuts the black king off the d-file) ·
      // black king f7, rook a1, White to move.
      startingFen: '3K4/3P1k2/8/8/8/8/8/r3R3 w - - 0 1',
      // 1.Re4! Ra2 2.Kc7 Rc2+ 3.Kb6 Rb2+ 4.Kc6 Rc2+ 5.Kb5 Rb2+ 6.Rb4 — the
      // rook on the fourth rank is the bridge that ends the checks.
      targetMovesSan: [
        'Re4',
        'Ra2',
        'Kc7',
        'Rc2+',
        'Kb6',
        'Rb2+',
        'Kc6',
        'Rc2+',
        'Kb5',
        'Rb2+',
        'Rb4',
      ],
      hintText:
          'Stelle den Turm auf die vierte Reihe, bevor der König herauskommt '
          '— er baut später die Brücke gegen die Schachs.',
    ),
    TrainingExercise(
      id: 'endgame_philidor',
      title: 'Philidor-Verteidigung (Turmendspiel Remis-Technik)',
      category: TrainingCategories.endgame,
      goal: TrainingGoals.draw,
      // White king d5, pawn e5, rook h1 · black king e7, rook a6, Black to
      // move: the defender applies the technique here.
      startingFen: '8/4k3/r7/3KP3/8/8/8/7R b - - 0 1',
      // 1…Rb6! (hold the sixth rank) 2.e6 Rb1! 3.Ke5 Re1+ — once the pawn
      // leaves the sixth rank the rook checks from behind and White is never
      // allowed to shelter.
      targetMovesSan: ['Rb6', 'e6', 'Rb1', 'Ke5', 'Re1+'],
      hintText:
          'Halte den Turm auf der sechsten Reihe, solange der Bauer dort '
          'nicht steht. Zieht er vor, gehe sofort auf die erste Reihe und '
          'schache von hinten.',
    ),
  ];

  /// Everything the progress service knows about. Tactics and opening drills
  /// join this list once their content ships.
  static const all = <TrainingExercise>[...endgames];

  static List<TrainingExercise> byCategory(String category) => [
    for (final exercise in all)
      if (exercise.category == category) exercise,
  ];

  /// The exercise after [id] within its category, or null at the end of the
  /// list — drives the player's "Nächstes Endspiel" action.
  static TrainingExercise? next(TrainingExercise exercise) {
    final siblings = byCategory(exercise.category);
    final index = siblings.indexWhere((entry) => entry.id == exercise.id);
    if (index < 0 || index + 1 >= siblings.length) return null;
    return siblings[index + 1];
  }
}
