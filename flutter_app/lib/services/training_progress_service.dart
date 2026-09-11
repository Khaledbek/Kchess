import 'dart:async';
import 'dart:convert';

import 'package:shared_preferences/shared_preferences.dart';

import '../features/training/data/training_library.dart';
import '../features/training/models/models.dart';

/// Where the progress payload is kept.
///
/// The indirection exists so the storage can move behind the C ABI later
/// without touching the service or the widgets: the core owns persistence per
/// the runtime split, and this Dart-side store is the interim home for training
/// progress until a native key/value export exists.
abstract class TrainingProgressStore {
  Future<String?> read();
  Future<void> write(String payload);
}

/// Default store: a single JSON blob in [SharedPreferences].
class SharedPreferencesProgressStore implements TrainingProgressStore {
  const SharedPreferencesProgressStore();

  static const storageKey = 'training_progress_v1';

  @override
  Future<String?> read() async =>
      (await SharedPreferences.getInstance()).getString(storageKey);

  @override
  Future<void> write(String payload) async =>
      (await SharedPreferences.getInstance()).setString(storageKey, payload);
}

/// Store that keeps everything in memory — used by tests and as the fallback
/// when the platform channel is unavailable.
class InMemoryProgressStore implements TrainingProgressStore {
  InMemoryProgressStore([this._payload]);

  String? _payload;

  @override
  Future<String?> read() async => _payload;

  @override
  Future<void> write(String payload) async => _payload = payload;
}

/// Reads and writes per-exercise training progress.
///
/// [snapshot] is synchronous so widgets can size a progress bar during the
/// first frame; [changes] pushes a new snapshot after every recorded attempt.
class TrainingProgressService {
  TrainingProgressService({
    TrainingProgressStore? store,
    List<TrainingExercise>? catalogue,
  }) : _store = store ?? const SharedPreferencesProgressStore(),
       catalogue = catalogue ?? TrainingLibrary.all;

  final TrainingProgressStore _store;

  /// Exercises the counts are taken over. Progress for an id outside the
  /// catalogue is still stored, so removing an exercise never loses history.
  final List<TrainingExercise> catalogue;

  final _controller = StreamController<TrainingProgressSnapshot>.broadcast();
  final _progress = <String, ExerciseProgress>{};

  Future<TrainingProgressSnapshot>? _loading;
  bool _loaded = false;

  /// Whether the stored payload has been read yet. Counts read 0 until then.
  bool get isLoaded => _loaded;

  /// Latest known progress. Empty until [load] completes.
  TrainingProgressSnapshot get snapshot =>
      TrainingProgressSnapshot(Map.unmodifiable(_progress));

  /// Emits after every successful write.
  Stream<TrainingProgressSnapshot> get changes => _controller.stream;

  /// Reads the stored payload once; concurrent callers share the same future.
  Future<TrainingProgressSnapshot> load() {
    return _loading ??= _load();
  }

  Future<TrainingProgressSnapshot> _load() async {
    try {
      final payload = await _store.read();
      if (payload != null && payload.isNotEmpty) {
        final decoded = jsonDecode(payload);
        if (decoded is Map<String, Object?>) {
          for (final entry in decoded.entries) {
            final value = entry.value;
            if (value is! Map<String, Object?>) continue;
            _progress[entry.key] = ExerciseProgress.fromJson({
              ...value,
              'exerciseId': entry.key,
            });
          }
        }
      }
    } catch (_) {
      // A corrupt or unreadable payload must never keep the tab from opening;
      // the user simply starts from zero.
      _progress.clear();
    }
    _loaded = true;
    _emit();
    return snapshot;
  }

  ExerciseProgress progressFor(String exerciseId) =>
      _progress[exerciseId] ?? ExerciseProgress(exerciseId: exerciseId);

  /// Exercises of [category] that are mastered.
  int getMasteryCount(String category) =>
      snapshot.masteryCount(category, catalogue);

  /// How many exercises [category] holds in total — the denominator of the
  /// mastery bar.
  int getExerciseCount(String category) {
    var count = 0;
    for (final exercise in catalogue) {
      if (exercise.category == category) count++;
    }
    return count;
  }

  /// Mastered count over an explicit set of ids — used by the endgame drills,
  /// whose levels are progress keys rather than catalogue exercises.
  int getMasteryCountOf(Iterable<String> exerciseIds) {
    var count = 0;
    for (final id in exerciseIds) {
      if (progressFor(id).isMastered) count++;
    }
    return count;
  }

  /// Total clean solves recorded for [category], repeats included.
  int getSolvedCount(String category) =>
      snapshot.solvedCount(category, catalogue);

  /// Mastered share of [category] in 0…1; 0 when the category is empty.
  double getMasteryRatio(String category) {
    final total = getExerciseCount(category);
    return total == 0 ? 0 : getMasteryCount(category) / total;
  }

  /// Records one attempt. Three clean solves in a row mark the exercise as
  /// mastered; a failure only resets the streak, never the badge.
  Future<ExerciseProgress> markExerciseCompleted(
    String exerciseId,
    bool success, {
    DateTime? at,
  }) async {
    await load();
    final updated = progressFor(exerciseId)
        .afterAttempt(success: success, at: at ?? DateTime.now());
    _progress[exerciseId] = updated;
    await _persist();
    _emit();
    return updated;
  }

  /// Clears all stored progress (settings → data reset, and tests).
  Future<void> reset() async {
    await load();
    _progress.clear();
    await _persist();
    _emit();
  }

  Future<void> _persist() async {
    final payload = <String, Object?>{
      for (final entry in _progress.entries) entry.key: entry.value.toJson(),
    };
    try {
      await _store.write(jsonEncode(payload));
    } catch (_) {
      // Losing a write is preferable to breaking the session; the in-memory
      // state stays correct for the rest of the run.
    }
  }

  void _emit() {
    if (!_controller.isClosed) _controller.add(snapshot);
  }

  void dispose() {
    _controller.close();
  }
}
