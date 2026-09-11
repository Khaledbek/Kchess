import '../models/endgame_drill.dart';

/// The drill catalogue, grouped the way Listudy groups its endgames.
///
/// Only "Mattsetzen" ships as generated levels so far; the theoretical
/// positions stay in [TrainingLibrary] as fixed lines, because Lucena and
/// Philidor are specific positions rather than a material configuration you can
/// shuffle.
class EndgameCatalog {
  const EndgameCatalog._();

  /// Three tiers, from a defender already pinned to the edge to one sitting in
  /// the centre with the attacking king far away.
  static const _checkmateLevels = <DrillLevel>[
    DrillLevel(
      index: 1,
      difficulty: DrillDifficulties.beginner,
      maxMoves: 10,
      minEdgeDistance: 0,
      maxEdgeDistance: 0,
      minKingDistance: 2,
    ),
    DrillLevel(
      index: 2,
      difficulty: DrillDifficulties.intermediate,
      maxMoves: 20,
      minEdgeDistance: 1,
      maxEdgeDistance: 2,
      minKingDistance: 2,
    ),
    DrillLevel(
      index: 3,
      difficulty: DrillDifficulties.master,
      maxMoves: 30,
      minEdgeDistance: 2,
      maxEdgeDistance: 3,
      minKingDistance: 4,
    ),
  ];

  static const _queenVsRookLevels = <DrillLevel>[
    DrillLevel(
      index: 1,
      difficulty: DrillDifficulties.beginner,
      maxMoves: 20,
      minEdgeDistance: 0,
      maxEdgeDistance: 0,
      minKingDistance: 2,
    ),
    DrillLevel(
      index: 2,
      difficulty: DrillDifficulties.intermediate,
      maxMoves: 30,
      minEdgeDistance: 1,
      maxEdgeDistance: 2,
      minKingDistance: 2,
    ),
    DrillLevel(
      index: 3,
      difficulty: DrillDifficulties.master,
      maxMoves: 45,
      minEdgeDistance: 2,
      maxEdgeDistance: 3,
      minKingDistance: 3,
    ),
  ];

  static const checkmating = EndgameCategory(
    id: 'checkmating',
    title: 'Mattsetzen',
    description:
        'Die Grundtechniken: den gegnerischen König an den Rand drängen '
        'und sicher matt setzen.',
    drills: [
      EndgameDrill(
        id: 'endgame_kq_vs_k',
        title: 'König & Dame gegen König',
        description:
            'Treib den König an den Rand und setze matt. Vorsicht vor Patt!',
        tip:
            'Lass dem König immer mindestens ein Fluchtfeld, bis dein eigener '
            'König zur Unterstützung da ist — sonst wird es Patt.',
        goal: EndgameGoal.checkmate,
        attackerPieces: ['K', 'Q'],
        defenderPieces: ['k'],
        levels: _checkmateLevels,
      ),
      EndgameDrill(
        id: 'endgame_kr_vs_k',
        title: 'König & Turm gegen König',
        description:
            'Baue mit Turm und König ein immer kleineres Gefängnis und setze '
            'matt.',
        tip:
            'Der Turm schneidet ab, der König drängt. Ohne deinen König '
            'gelingt das Matt nicht.',
        goal: EndgameGoal.checkmate,
        attackerPieces: ['K', 'R'],
        defenderPieces: ['k'],
        levels: _checkmateLevels,
      ),
      EndgameDrill(
        id: 'endgame_q_vs_r',
        title: 'Dame gegen Turm',
        description:
            'Treib König und Turm in Zugzwang, gewinne den Turm oder setze '
            'matt.',
        tip:
            'Suche nach Doppelangriffen auf König und ungedeckten Turm. '
            'Vorsicht vor Turmopfern für Patt!',
        goal: EndgameGoal.checkmate,
        attackerPieces: ['K', 'Q'],
        defenderPieces: ['k', 'r'],
        levels: _queenVsRookLevels,
      ),
    ],
  );

  static const categories = <EndgameCategory>[checkmating];

  static EndgameDrill? drillById(String id) {
    for (final category in categories) {
      for (final drill in category.drills) {
        if (drill.id == id) return drill;
      }
    }
    return null;
  }
}
