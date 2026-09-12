// -----------------------------------------------------------------------------
// Section: Native training DTO contract
// -----------------------------------------------------------------------------

import 'package:flutter_test/flutter_test.dart';
import 'package:kchess/shared/models/models.dart';

void main() {
  test('training overview maps native progress without recomputing it', () {
    final overview = TrainingOverview.fromJson({
      'masteryThreshold': 3,
      'categories': {
        'endgame': {'mastered': 1, 'total': 3, 'solved': 4},
      },
      'exercises': [
        {
          'id': 'endgame_lucena',
          'category': 'endgame',
          'goal': 'win',
          'startingFen': '3K4/3P1k2/8/8/8/8/8/r3R3 w - - 0 1',
          'solverColor': 'white',
          'solverMoveCount': 6,
          'nextExerciseId': 'endgame_philidor',
          'progress': {
            'exerciseId': 'endgame_lucena',
            'isMastered': true,
            'successStreak': 3,
            'successCount': 4,
            'attemptCount': 5,
            'lastAttemptAt': 1788998400,
          },
        },
      ],
    });

    expect(overview.category('endgame').mastered, 1);
    expect(overview.category('endgame').solved, 4);
    expect(overview.exercises.single.progress.isMastered, isTrue);
    expect(
      overview.exercises.single.nextExerciseId,
      'endgame_philidor',
    );
  });
}
