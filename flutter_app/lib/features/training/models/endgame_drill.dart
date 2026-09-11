/// What counts as solving a drill.
enum EndgameGoal {
  /// Mate the defender inside the level's move budget.
  checkmate,

  /// Win the defender's material (queen against rook, say).
  winMaterial,

  /// Survive as the defender without losing.
  holdDraw,
}

/// One difficulty tier of a drill.
///
/// Positions are generated rather than curated, so a level is a *shape* the
/// generator has to hit: how far the defending king starts from the edge, and
/// how far the attacker's king starts from it. Both correlate closely with how
/// long the mate takes, and — unlike asking the engine to score every candidate
/// — they cost nothing to evaluate while shuffling pieces.
class DrillLevel {
  const DrillLevel({
    required this.index,
    required this.difficulty,
    required this.maxMoves,
    required this.minEdgeDistance,
    required this.maxEdgeDistance,
    required this.minKingDistance,
  });

  /// 1-based; level 1 is always unlocked.
  final int index;

  /// One of [DrillDifficulties].
  final String difficulty;

  /// Moves the attacker gets before the drill is failed.
  final int maxMoves;

  /// Chebyshev distance from the defending king to the nearest board edge.
  /// A king already on the edge (0) is nearly mated; one in the centre (3) is
  /// the full technique.
  final int minEdgeDistance;
  final int maxEdgeDistance;

  /// Chebyshev distance between the two kings at the start.
  final int minKingDistance;
}

class DrillDifficulties {
  const DrillDifficulties._();

  static const beginner = 'beginner';
  static const intermediate = 'intermediate';
  static const master = 'master';
}

/// A material configuration the user drills repeatedly, in rising difficulty.
class EndgameDrill {
  const EndgameDrill({
    required this.id,
    required this.title,
    required this.description,
    required this.tip,
    required this.goal,
    required this.attackerPieces,
    required this.defenderPieces,
    required this.levels,
  });

  /// e.g. `endgame_kq_vs_k`. Progress is stored per level, see [progressId].
  final String id;
  final String title;
  final String description;

  /// The one sentence that stops the most common mistake.
  final String tip;
  final EndgameGoal goal;

  /// FEN letters for the side the user plays, uppercase, king first.
  final List<String> attackerPieces;

  /// FEN letters for the side the engine defends with, lowercase.
  final List<String> defenderPieces;

  final List<DrillLevel> levels;

  /// Progress is tracked per level, so mastering "Anfänger" does not claim the
  /// harder tiers.
  String progressId(int levelIndex) => '${id}_l$levelIndex';

  DrillLevel? levelAt(int index) {
    for (final level in levels) {
      if (level.index == index) return level;
    }
    return null;
  }
}

/// A Listudy-style grouping of drills ("Checkmating", "King and Pawn …").
class EndgameCategory {
  const EndgameCategory({
    required this.id,
    required this.title,
    required this.description,
    required this.drills,
  });

  final String id;
  final String title;
  final String description;
  final List<EndgameDrill> drills;

  /// Every progress key in the category, for the mastery counter.
  List<String> get progressIds => [
    for (final drill in drills)
      for (final level in drill.levels) drill.progressId(level.index),
  ];
}
