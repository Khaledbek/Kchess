import 'training_exercise.dart';

/// Per-exercise progress as persisted by `TrainingProgressService`.
///
/// [isMastered] is the stored flag rather than a getter over [successStreak]:
/// once an exercise has been solved three times in a row it stays mastered, so
/// a later slip lowers the streak without taking the badge away again.
class ExerciseProgress {
  const ExerciseProgress({
    required this.exerciseId,
    this.isMastered = false,
    this.successStreak = 0,
    this.successCount = 0,
    this.attemptCount = 0,
    this.lastAttemptDate,
  });

  factory ExerciseProgress.fromJson(Map<String, Object?> json) {
    final timestamp = json['lastAttemptDate'] as String?;
    return ExerciseProgress(
      exerciseId: json['exerciseId'] as String? ?? '',
      isMastered: json['isMastered'] as bool? ?? false,
      successStreak: json['successStreak'] as int? ?? 0,
      successCount: json['successCount'] as int? ?? 0,
      attemptCount: json['attemptCount'] as int? ?? 0,
      lastAttemptDate: timestamp == null ? null : DateTime.tryParse(timestamp),
    );
  }

  /// Clean solves in a row that mark an exercise as mastered.
  static const masteryThreshold = 3;

  final String exerciseId;
  final bool isMastered;

  /// Clean solves since the last failed attempt, capped at [masteryThreshold].
  final int successStreak;

  /// Total clean solves — what the tactics counter on the hub reports.
  final int successCount;
  final int attemptCount;
  final DateTime? lastAttemptDate;

  /// The progress after one attempt, without mutating this instance.
  ExerciseProgress afterAttempt({required bool success, required DateTime at}) {
    final streak = success ? successStreak + 1 : 0;
    return ExerciseProgress(
      exerciseId: exerciseId,
      isMastered: isMastered || streak >= masteryThreshold,
      successStreak: streak > masteryThreshold ? masteryThreshold : streak,
      successCount: success ? successCount + 1 : successCount,
      attemptCount: attemptCount + 1,
      lastAttemptDate: at,
    );
  }

  Map<String, Object?> toJson() => {
    'exerciseId': exerciseId,
    'isMastered': isMastered,
    'successStreak': successStreak,
    'successCount': successCount,
    'attemptCount': attemptCount,
    'lastAttemptDate': lastAttemptDate?.toIso8601String(),
  };
}

/// Immutable view of all stored progress, handed out by the service's stream so
/// widgets never read a map that is about to be mutated underneath them.
class TrainingProgressSnapshot {
  const TrainingProgressSnapshot(this._byExerciseId);

  const TrainingProgressSnapshot.empty() : _byExerciseId = const {};

  final Map<String, ExerciseProgress> _byExerciseId;

  ExerciseProgress progressFor(String exerciseId) =>
      _byExerciseId[exerciseId] ?? ExerciseProgress(exerciseId: exerciseId);

  bool isMastered(String exerciseId) => progressFor(exerciseId).isMastered;

  /// Exercises of [category] that are mastered, out of [catalogue].
  int masteryCount(String category, List<TrainingExercise> catalogue) {
    var count = 0;
    for (final exercise in catalogue) {
      if (exercise.category == category && isMastered(exercise.id)) count++;
    }
    return count;
  }

  /// Total clean solves recorded for [category] — repeats included, because the
  /// hub counter reads "gelöste Taktiken", not "gemeisterte Aufgaben".
  int solvedCount(String category, List<TrainingExercise> catalogue) {
    var count = 0;
    for (final exercise in catalogue) {
      if (exercise.category == category) {
        count += progressFor(exercise.id).successCount;
      }
    }
    return count;
  }
}
