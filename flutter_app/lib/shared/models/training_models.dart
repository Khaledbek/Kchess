part of 'models.dart';

// -----------------------------------------------------------------------------
// Section: Native training DTOs
// -----------------------------------------------------------------------------

class ExerciseProgress {
  const ExerciseProgress({
    required this.isMastered,
    required this.successStreak,
    required this.attemptCount,
    this.bestDepth = 0,
  });

  factory ExerciseProgress.fromJson(Map<String, Object?> json) =>
      ExerciseProgress(
        isMastered: json['isMastered'] as bool? ?? false,
        successStreak: json['successStreak'] as int? ?? 0,
        attemptCount: json['attemptCount'] as int? ?? 0,
        bestDepth: json['bestDepth'] as int? ?? 0,
      );

  final bool isMastered;
  final int successStreak;
  final int attemptCount;

  /// Deepest an opening drill has run for this exercise; zero elsewhere.
  final int bestDepth;
}

class TrainingExercise {
  const TrainingExercise({
    required this.id,
    required this.category,
    required this.goal,
    required this.solverColor,
    required this.solverMoveCount,
    required this.progress,
    this.nextExerciseId,
  });

  factory TrainingExercise.fromJson(Map<String, Object?> json) =>
      TrainingExercise(
        id: json['id']! as String,
        category: json['category']! as String,
        goal: json['goal']! as String,
        solverColor: json['solverColor']! as String,
        solverMoveCount: json['solverMoveCount']! as int,
        nextExerciseId: json['nextExerciseId'] as String?,
        progress: ExerciseProgress.fromJson(
          json['progress']! as Map<String, Object?>,
        ),
      );

  final String id;
  final String category;
  final String goal;
  final String solverColor;
  final int solverMoveCount;
  final String? nextExerciseId;
  final ExerciseProgress progress;
}

class TrainingCategorySummary {
  const TrainingCategorySummary({
    required this.mastered,
    required this.total,
    required this.solved,
  });

  factory TrainingCategorySummary.fromJson(Map<String, Object?> json) =>
      TrainingCategorySummary(
        mastered: json['mastered'] as int? ?? 0,
        total: json['total'] as int? ?? 0,
        solved: json['solved'] as int? ?? 0,
      );

  final int mastered;
  final int total;
  final int solved;
}

class TrainingOverview {
  const TrainingOverview({
    required this.masteryThreshold,
    required this.exercises,
    required this.categories,
  });

  factory TrainingOverview.fromJson(Map<String, Object?> json) =>
      TrainingOverview(
        masteryThreshold: json['masteryThreshold']! as int,
        exercises: (json['exercises'] as List<Object?>? ?? const [])
            .cast<Map<String, Object?>>()
            .map(TrainingExercise.fromJson)
            .toList(growable: false),
        categories:
            (json['categories'] as Map<String, Object?>? ?? const {}).map(
              (key, value) => MapEntry(
                key,
                TrainingCategorySummary.fromJson(
                  value! as Map<String, Object?>,
                ),
              ),
            ),
      );

  final int masteryThreshold;
  final List<TrainingExercise> exercises;
  final Map<String, TrainingCategorySummary> categories;

  TrainingCategorySummary category(String id) =>
      categories[id] ??
      const TrainingCategorySummary(mastered: 0, total: 0, solved: 0);

  TrainingExercise? exercise(String id) {
    for (final exercise in exercises) {
      if (exercise.id == id) return exercise;
    }
    return null;
  }
}

class TrainingAttempt {
  const TrainingAttempt({
    required this.attemptId,
    required this.position,
    required this.solverMovesPlayed,
  });

  factory TrainingAttempt.fromJson(Map<String, Object?> json) =>
      TrainingAttempt(
        attemptId: json['attemptId']! as String,
        position: BoardPosition.fromJson(
          json['position']! as Map<String, Object?>,
        ),
        solverMovesPlayed: json['solverMovesPlayed'] as int? ?? 0,
      );

  final String attemptId;
  final BoardPosition position;
  final int solverMovesPlayed;
}

class TrainingMoveResult {
  const TrainingMoveResult({
    required this.accepted,
    required this.status,
    required this.position,
    required this.solverMovesPlayed,
    this.clean,
  });

  factory TrainingMoveResult.fromJson(Map<String, Object?> json) =>
      TrainingMoveResult(
        accepted: json['accepted'] as bool? ?? true,
        status: json['status'] as String? ?? 'active',
        position: BoardPosition.fromJson(
          json['position']! as Map<String, Object?>,
        ),
        solverMovesPlayed: json['solverMovesPlayed'] as int? ?? 0,
        clean: json['clean'] as bool?,
      );

  final bool accepted;
  final String status;
  final BoardPosition position;
  final int solverMovesPlayed;
  final bool? clean;

  bool get completed => status == 'completed';
}
