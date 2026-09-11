import 'dart:math';

import '../../../ffi/core_gateway.dart';
import '../../../shared/models/models.dart';
import '../models/endgame_drill.dart';

/// Builds a fresh, legal drill position for a level.
///
/// Dart only shuffles coordinates and assembles a FEN string; every question
/// that needs chess rules — is this position legal, is it already over, can the
/// defender simply take the piece — is answered by the native core. That keeps
/// the runtime split intact while still giving endless practice material.
class EndgamePositionGenerator {
  EndgamePositionGenerator(this._gateway, {Random? random})
    : _random = random ?? Random();

  final CoreGateway _gateway;
  final Random _random;

  /// Placements are cheap to propose and most are legal, so a generous cap
  /// still returns in microseconds while guaranteeing termination.
  static const _maxAttempts = 200;

  static int fileOf(int square) => square % 8;
  static int rankOf(int square) => square ~/ 8;

  /// Distance to the nearest edge: 0 on the rim, 3 in the centre.
  static int edgeDistance(int square) {
    final file = fileOf(square);
    final rank = rankOf(square);
    return [file, 7 - file, rank, 7 - rank].reduce(min);
  }

  /// King-move distance between two squares.
  static int chebyshev(int a, int b) =>
      max((fileOf(a) - fileOf(b)).abs(), (rankOf(a) - rankOf(b)).abs());

  /// The FEN for a placement, with [sideToMove] to move.
  static String fenFor(Map<int, String> placement, String sideToMove) {
    final rows = <String>[];
    for (var rank = 7; rank >= 0; rank--) {
      final row = StringBuffer();
      var empty = 0;
      for (var file = 0; file < 8; file++) {
        final piece = placement[rank * 8 + file];
        if (piece == null) {
          empty++;
          continue;
        }
        if (empty > 0) {
          row.write(empty);
          empty = 0;
        }
        row.write(piece);
      }
      if (empty > 0) row.write(empty);
      rows.add(row.toString());
    }
    return '${rows.join('/')} $sideToMove - - 0 1';
  }

  /// A legal, playable start position for [level], or null if none could be
  /// assembled (never expected for the shipped drills — the caller shows an
  /// error rather than a broken board).
  Future<String?> generate(EndgameDrill drill, DrillLevel level) async {
    for (var attempt = 0; attempt < _maxAttempts; attempt++) {
      final placement = _proposePlacement(drill, level);
      if (placement == null) continue;
      final fen = fenFor(placement, 'w');
      if (await _isUsable(placement, fen)) return fen;
    }
    return null;
  }

  /// Random squares that satisfy the level's shape.
  ///
  /// Draws from the squares that already match each constraint instead of
  /// rejecting random guesses: the hardest level allows only the innermost
  /// squares, and blind sampling failed to find one often enough to matter.
  Map<int, String>? _proposePlacement(EndgameDrill drill, DrillLevel level) {
    final defenderSquares = [
      for (var square = 0; square < 64; square++)
        if (edgeDistance(square) >= level.minEdgeDistance &&
            edgeDistance(square) <= level.maxEdgeDistance)
          square,
    ];
    if (defenderSquares.isEmpty) return null;
    final defenderKing =
        defenderSquares[_random.nextInt(defenderSquares.length)];

    // Two squares apart at minimum, or the kings would be illegally adjacent.
    final minimumGap = max(2, level.minKingDistance);
    final attackerSquares = [
      for (var square = 0; square < 64; square++)
        if (square != defenderKing &&
            chebyshev(square, defenderKing) >= minimumGap)
          square,
    ];
    if (attackerSquares.isEmpty) return null;

    final placement = <int, String>{
      defenderKing: drill.defenderPieces.first,
      attackerSquares[_random.nextInt(attackerSquares.length)]:
          drill.attackerPieces.first,
    };
    for (final piece in [
      ...drill.attackerPieces.skip(1),
      ...drill.defenderPieces.skip(1),
    ]) {
      final free = [
        for (var square = 0; square < 64; square++)
          if (!placement.containsKey(square)) square,
      ];
      if (free.isEmpty) return null;
      placement[free[_random.nextInt(free.length)]] = piece;
    }
    return placement;
  }

  /// Asks the core the three things that make a generated position usable.
  Future<bool> _isUsable(Map<int, String> placement, String fen) async {
    try {
      // 1. The defender must not already be in check, or the attacker could
      //    capture the king. validate_fen catches adjacent kings but not this,
      //    so ask about the same board with the defender to move.
      final mirrored = await _gateway.boardPosition(fenFor(placement, 'b'));
      if (mirrored.inCheck) return false;

      // 2. A drill that starts with the piece hanging is not the drill — the
      //    defender would simply trade into a dead draw. Core-generated SAN
      //    marks captures, so no rules reasoning is needed here.
      final defenderMoves = await _gateway.boardLegalMoves(
        fenFor(placement, 'b'),
      );
      if (defenderMoves.any((move) => move.san.contains('x'))) return false;

      // 3. Neither side should start with a hanging piece, and the position
      //    must be playable.
      final attackerMoves = await _gateway.boardLegalMoves(fen);
      if (attackerMoves.any((move) => move.san.contains('x'))) return false;

      final position = await _gateway.boardPosition(fen);
      return position.status == BoardStatus.playable;
    } on CoreGatewayException {
      // An illegal placement (adjacent kings, malformed FEN) is a rejected
      // candidate, not an error worth surfacing.
      return false;
    }
  }
}
