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
      ply = json['ply'] as int?,
      maxMoves = json['maxMoves']! as int,
      clean = json['clean']! as bool,
      accepted = json['accepted'] as bool?,
      promotions = (json['promotions'] as List<Object?>? ?? []).cast<String>(),
      evaluation = json['evaluation'] as Map<String, Object?>?,
      drill = json.containsKey('targetDepth') ? OpeningDrillState.fromJson(json) : null;

  final String id, status, solverColor;
  final BoardPosition position;
  final int played, maxMoves;

  /// Half-moves replayed in the current line; null on older native builds.
  final int? ply;
  final bool clean;
  final bool? accepted;
  final List<String> promotions;
  final Map<String, Object?>? evaluation;

  /// Opening drill state; null for every other kind of practice.
  final OpeningDrillState? drill;
}

/// Where an opening drill stands, exactly as native judged it.
///
/// The drill has no line to follow: native answers for the opponent with a
/// weighted book reply and accepts the user's move only when the book ranks it.
/// Nothing here is derived in Dart.
class OpeningDrillState {
  OpeningDrillState.fromJson(Map<String, Object?> json)
    : depth = json['depth']! as int,
      targetDepth = json['targetDepth']! as int,
      attempts = json['attempts'] as int? ?? 0,
      bookExhausted = json['bookExhausted'] as bool? ?? false,
      bookMoves = json['bookMoves'] as int? ?? 0,
      openingMoves = (json['openingMoves'] as List<Object?>? ?? const [])
          .cast<String>(),
      opponentMove = switch (json['opponentMove']) {
        final Map<String, Object?> move => OpeningDrillReply.fromJson(move),
        _ => null,
      },
      answer = switch (json['answer']) {
        final Map<String, Object?> answer => OpeningDrillAnswer.fromJson(answer),
        _ => null,
      },
      hint = json['hint'] as String?,
      hintSan = json['hintSan'] as String?;

  /// Book moves the user has answered in this run.
  final int depth;

  /// Answers that complete the run.
  final int targetDepth;

  /// Wrong tries at the position on the board.
  final int attempts;

  /// The run stopped because the book had nothing left to ask.
  final bool bookExhausted;

  /// Legal book replies for the position on the board.
  final int bookMoves;

  /// The catalogue line that sets the scenario up, in notation.
  final List<String> openingMoves;

  /// The book reply native just played for the opponent, if any.
  final OpeningDrillReply? opponentMove;

  /// The user's last accepted answer, if it has not been missed since.
  final OpeningDrillAnswer? answer;

  /// The move the book wanted, set by native only after the user missed it.
  final String? hint;
  final String? hintSan;
}

/// One opponent reply the drill drew from the book.
class OpeningDrillReply {
  OpeningDrillReply.fromJson(Map<String, Object?> json)
    : uci = json['uci']! as String,
      san = json['san']! as String,
      side = json['side']! as String,
      moveNumber = json['moveNumber']! as int,
      alternatives = json['alternatives'] as int? ?? 1;

  final String uci, san, side;
  final int moveNumber;

  /// How many book replies the opponent was drawing from.
  final int alternatives;

  /// Move-number notation: `1. e4` for White, `1... c5` for Black.
  String get notation => side == 'white' ? '$moveNumber. $san' : '$moveNumber... $san';
}

/// The user's accepted answer and where the book ranked it (1 = best).
class OpeningDrillAnswer {
  OpeningDrillAnswer.fromJson(Map<String, Object?> json)
    : uci = json['uci']! as String,
      san = json['san']! as String,
      rank = json['rank']! as int;

  final String uci, san;
  final int rank;
}

class OpeningTreeNode {
  OpeningTreeNode.fromJson(Map<String, Object?> json)
    : id = json['id']! as int,
      openingId = json['openingId']! as int,
      name = json['name']! as String,
      eco = json['eco'] as String?,
      childCount = json['childCount']! as int,
      masteryThreshold = json['masteryThreshold']! as int,
      targetDepth = json['targetDepth'] as int? ?? 10,
      moves = (json['moves'] as List<Object?>? ?? const []).cast<String>(),
      position = BoardPosition.fromJson(
        json['position']! as Map<String, Object?>,
      ),
      progress = ExerciseProgress.fromJson(
        json['progress']! as Map<String, Object?>,
      );
  final int id, openingId, childCount;
  final int masteryThreshold;

  /// Answers a full drill of this scenario asks for.
  final int targetDepth;

  /// The setup line in notation, e.g. `['e4', 'c5']`.
  final List<String> moves;
  final String name;
  final String? eco;
  final BoardPosition position;
  final ExerciseProgress progress;
}

/// Writes a line of SAN with move numbers: `1. e4 c5 2. Nf3`.
String formatOpeningLine(List<String> moves) {
  final buffer = StringBuffer();
  for (var index = 0; index < moves.length; index++) {
    if (index.isEven) {
      if (index > 0) buffer.write(' ');
      buffer.write('${index ~/ 2 + 1}. ');
    } else {
      buffer.write(' ');
    }
    buffer.write(moves[index]);
  }
  return buffer.toString();
}
