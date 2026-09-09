/// Category keys a [TrainingExercise] can belong to.
///
/// Kept as plain strings (not an enum) so a persisted progress payload written
/// by an older build keeps parsing after the catalogue grows, and so the native
/// core can hand the same keys across the C ABI later on.
class TrainingCategories {
  const TrainingCategories._();

  static const opening = 'opening';
  static const tactics = 'tactics';
  static const endgame = 'endgame';

  static const all = <String>[opening, tactics, endgame];
}

/// What "solved" means for an exercise — a won endgame is not the same task as
/// a held draw, and the player states the goal before the first move.
class TrainingGoals {
  const TrainingGoals._();

  static const win = 'win';
  static const draw = 'draw';
}

/// One trainable position: a start FEN plus the move sequence that solves it.
///
/// The exercise itself is inert data — validating a played move against
/// [targetMovesSan] is chess logic and therefore belongs in the native core,
/// not here.
class TrainingExercise {
  const TrainingExercise({
    required this.id,
    required this.title,
    required this.category,
    required this.startingFen,
    required this.targetMovesSan,
    required this.hintText,
    this.goal = TrainingGoals.win,
  });

  factory TrainingExercise.fromJson(Map<String, Object?> json) =>
      TrainingExercise(
        id: json['id'] as String? ?? '',
        title: json['title'] as String? ?? '',
        category: json['category'] as String? ?? TrainingCategories.endgame,
        startingFen: json['startingFen'] as String? ?? '',
        targetMovesSan: (json['targetMovesSan'] as List<Object?>? ?? const [])
            .cast<String>()
            .toList(growable: false),
        hintText: json['hintText'] as String? ?? '',
        goal: json['goal'] as String? ?? TrainingGoals.win,
      );

  /// Stable key, e.g. `endgame_lucena`. Also the progress-storage key.
  final String id;

  /// Display name, e.g. "Lucena-Stellung".
  final String title;

  /// One of [TrainingCategories].
  final String category;

  /// Position the exercise starts from.
  final String startingFen;

  /// The full line in SAN, alternating solver move and opponent reply starting
  /// from [startingFen]. Even indices are the solver's; odd indices are played
  /// automatically. Validated against the real move generator by
  /// `native/tests/training_board_tests.cpp`.
  final List<String> targetMovesSan;

  /// Short nudge shown before the solution is revealed.
  final String hintText;

  /// One of [TrainingGoals].
  final String goal;

  /// Moves the solver plays — every second entry of [targetMovesSan].
  int get solverMoveCount => (targetMovesSan.length + 1) ~/ 2;

  Map<String, Object?> toJson() => {
    'id': id,
    'title': title,
    'category': category,
    'startingFen': startingFen,
    'targetMovesSan': targetMovesSan,
    'hintText': hintText,
    'goal': goal,
  };
}
