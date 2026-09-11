// -----------------------------------------------------------------------------
// Section: Native practice DTOs (no rules or persistence)
// -----------------------------------------------------------------------------
import '../../../shared/models/models.dart';

class PracticeSnapshot {
  PracticeSnapshot.fromJson(Map<String, Object?> json)
    : id = json['session']! as String,
      status = json['status']! as String,
      solverColor = json['solverColor']! as String,
      position = BoardPosition.fromJson(
        json['position']! as Map<String, Object?>,
      ),
      played = json['played']! as int,
      maxMoves = json['maxMoves']! as int,
      clean = json['clean']! as bool,
      accepted = json['accepted'] as bool?,
      promotions = (json['promotions'] as List<Object?>? ?? []).cast<String>(),
      evaluation = json['evaluation'] as Map<String, Object?>?;
  final String id, status, solverColor;
  final BoardPosition position;
  final int played, maxMoves;
  final bool clean;
  final bool? accepted;
  final List<String> promotions;
  final Map<String, Object?>? evaluation;
}

class OpeningTreeNode {
  OpeningTreeNode.fromJson(Map<String, Object?> json)
    : id = json['id']! as int,
      openingId = json['openingId']! as int,
      name = json['name']! as String,
      eco = json['eco'] as String?,
      childCount = json['childCount']! as int,
      masteryThreshold = json['masteryThreshold']! as int,
      position = BoardPosition.fromJson(
        json['position']! as Map<String, Object?>,
      ),
      progress = ExerciseProgress.fromJson(
        json['progress']! as Map<String, Object?>,
      );
  final int id, openingId, childCount;
  final int masteryThreshold;
  final String name;
  final String? eco;
  final BoardPosition position;
  final ExerciseProgress progress;
}
