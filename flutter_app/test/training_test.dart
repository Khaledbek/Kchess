import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:kchess/app/kchess_app.dart';
import 'package:kchess/features/training/data/training_library.dart';
import 'package:kchess/features/training/models/models.dart';
import 'package:kchess/features/training/presentation/endgame_academy_screen.dart';
import 'package:kchess/features/training/presentation/training_arena_screen.dart';
import 'package:kchess/localization/generated/app_localizations.dart';
import 'package:kchess/models/models.dart';
import 'package:kchess/services/training_progress_service.dart';
import 'package:kchess/view_models/app_controller.dart';

import 'support/fake_core_gateway.dart';

const profile = AppProfile(
  id: 'profile-1',
  type: ProfileType.localPgnFen,
  displayName: 'Local',
  avatarAsset: 'profile_unknown.png',
);

/// The seeded endgame the mastery tests drive.
const lucena = 'endgame_lucena';

Widget _localized(Widget child) => MaterialApp(
  locale: const Locale('de'),
  localizationsDelegates: AppLocalizations.localizationsDelegates,
  supportedLocales: AppLocalizations.supportedLocales,
  home: child,
);

/// A three-ply stand-in for a seeded exercise: White rook and king against a
/// lone black king, solver to move.
///
/// The real lines are validated against the actual move generator by
/// `native/tests/training_board_tests.cpp`; these widget tests are about the
/// player's own loop, so they use a line short enough to script by hand.
const _drillExercise = TrainingExercise(
  id: 'drill_rook_check',
  title: 'Turm-Drill',
  category: TrainingCategories.endgame,
  startingFen: '7k/8/8/8/8/8/6R1/6K1 w - - 0 1',
  targetMovesSan: ['Ra2', 'Kh7', 'Ra7+'],
  hintText: 'Bringe den Turm auf die a-Linie und gib dann Schach.',
);

const _afterRa2 = '7k/8/8/8/8/8/R7/6K1 b - - 1 1';
const _afterKh7 = '8/7k/8/8/8/8/R7/6K1 w - - 2 2';
const _afterRa7 = '8/R6k/8/8/8/8/8/6K1 b - - 3 2';

/// Teaches the fake core the drill's positions, plus legal-but-wrong decoys so
/// a rejected move can be told apart from an illegal one.
void _scriptDrill(FakeCoreGateway gateway) {
  gateway.boardScript[_drillExercise.startingFen] = const [
    BoardMoveOption(uci: 'g2a2', san: 'Ra2', fenAfter: _afterRa2),
    BoardMoveOption(
      uci: 'g2g3',
      san: 'Rg3',
      fenAfter: '7k/8/8/8/8/6R1/8/6K1 b - - 1 1',
    ),
  ];
  gateway.boardScript[_afterRa2] = const [
    BoardMoveOption(uci: 'h8h7', san: 'Kh7', fenAfter: _afterKh7),
    BoardMoveOption(
      uci: 'h8g8',
      san: 'Kg8',
      fenAfter: '6k1/8/8/8/8/8/R7/6K1 w - - 2 2',
    ),
  ];
  gateway.boardScript[_afterKh7] = const [
    BoardMoveOption(uci: 'a2a7', san: 'Ra7+', fenAfter: _afterRa7),
    BoardMoveOption(
      uci: 'a2a3',
      san: 'Ra3',
      fenAfter: '8/7k/8/8/8/R7/8/6K1 b - - 3 2',
    ),
  ];
}

/// Drags a piece between two board squares the way a user would.
Future<void> _dragSquare(WidgetTester tester, String from, String to) async {
  final start = tester.getCenter(find.byKey(Key('board-square-$from')));
  final end = tester.getCenter(find.byKey(Key('board-square-$to')));
  await tester.dragFrom(start, end - start);
  await tester.pump();
}

void main() {
  group('TrainingProgressService', () {
    test('three clean solves master an exercise', () async {
      final service = TrainingProgressService(store: InMemoryProgressStore());
      addTearDown(service.dispose);

      for (var i = 0; i < 2; i++) {
        await service.markExerciseCompleted(lucena, true);
      }
      expect(service.getMasteryCount(TrainingCategories.endgame), 0);

      await service.markExerciseCompleted(lucena, true);
      expect(service.progressFor(lucena).isMastered, isTrue);
      expect(service.getMasteryCount(TrainingCategories.endgame), 1);
      expect(
        service.getMasteryRatio(TrainingCategories.endgame),
        closeTo(1 / TrainingLibrary.endgames.length, 1e-9),
      );
    });

    test('a failed attempt resets the streak but keeps the date', () async {
      final service = TrainingProgressService(store: InMemoryProgressStore());
      addTearDown(service.dispose);

      await service.markExerciseCompleted(lucena, true);
      await service.markExerciseCompleted(lucena, true);
      await service.markExerciseCompleted(lucena, false);
      await service.markExerciseCompleted(lucena, true);

      final progress = service.progressFor(lucena);
      expect(progress.isMastered, isFalse);
      expect(progress.successStreak, 1);
      expect(progress.attemptCount, 4);
      expect(progress.successCount, 3);
      expect(progress.lastAttemptDate, isNotNull);
    });

    test('mastery survives a later failure', () async {
      final service = TrainingProgressService(store: InMemoryProgressStore());
      addTearDown(service.dispose);

      for (var i = 0; i < 3; i++) {
        await service.markExerciseCompleted(lucena, true);
      }
      await service.markExerciseCompleted(lucena, false);

      expect(service.progressFor(lucena).isMastered, isTrue);
      expect(service.progressFor(lucena).successStreak, 0);
    });

    test('progress round-trips through the store', () async {
      final store = InMemoryProgressStore();
      final writer = TrainingProgressService(store: store);
      for (var i = 0; i < 3; i++) {
        await writer.markExerciseCompleted(lucena, true);
      }
      await writer.markExerciseCompleted('endgame_philidor', true);
      writer.dispose();

      final reader = TrainingProgressService(store: store);
      addTearDown(reader.dispose);
      await reader.load();

      expect(reader.getMasteryCount(TrainingCategories.endgame), 1);
      expect(reader.progressFor(lucena).successCount, 3);
      expect(reader.progressFor('endgame_philidor').successStreak, 1);
      expect(reader.progressFor('endgame_opposition').attemptCount, 0);
    });

    test('a corrupt payload starts from zero instead of throwing', () async {
      final service = TrainingProgressService(
        store: InMemoryProgressStore('not json at all'),
      );
      addTearDown(service.dispose);

      await service.load();
      expect(service.isLoaded, isTrue);
      expect(service.getMasteryCount(TrainingCategories.endgame), 0);
    });

    test('the stream emits a snapshot after every attempt', () async {
      final service = TrainingProgressService(store: InMemoryProgressStore());
      addTearDown(service.dispose);
      // Broadcast events arrive in a microtask, so collect them through the
      // stream itself rather than cancelling a listener mid-flight.
      final emitted = service.changes
          .map((snapshot) => snapshot.progressFor(lucena).successCount)
          .take(3)
          .toList();

      await service.markExerciseCompleted(lucena, true);
      await service.markExerciseCompleted(lucena, true);

      // load() emits once before the two attempts.
      expect(await emitted, [0, 1, 2]);
    });

    test('solved counts include repeats, mastery counts do not', () async {
      final service = TrainingProgressService(store: InMemoryProgressStore());
      addTearDown(service.dispose);

      for (var i = 0; i < 5; i++) {
        await service.markExerciseCompleted(lucena, true);
      }

      expect(service.getSolvedCount(TrainingCategories.endgame), 5);
      expect(service.getMasteryCount(TrainingCategories.endgame), 1);
      expect(service.getExerciseCount(TrainingCategories.endgame), 3);
    });
  });

  group('nemesisOpening', () {
    OpeningFamily family(String name, int games, int wins, int losses) =>
        OpeningFamily(
          familyName: name,
          baseEco: 'A00',
          color: 'white',
          tally: StatTally(games: games, wins: wins, losses: losses),
          variations: const [],
        );

    test('picks the worst line that clears the sample floor', () {
      final stats = OpeningsStats(
        hasProfile: true,
        families: [
          family('Solid Line', 10, 6, 4),
          family('Bad Line', 10, 2, 8),
          family('Worse But Tiny', 3, 0, 3),
          family('Worst Line', 8, 1, 7),
        ],
      );

      expect(nemesisOpening(stats)?.familyName, 'Worst Line');
    });

    test('returns null when nothing scores badly on a usable sample', () {
      final stats = OpeningsStats(
        hasProfile: true,
        families: [family('Solid Line', 10, 6, 4), family('Tiny', 2, 0, 2)],
      );

      expect(nemesisOpening(stats), isNull);
    });
  });

  group('training tab', () {
    Future<void> pumpShell(WidgetTester tester) async {
      tester.view.physicalSize = const Size(1400, 2400);
      tester.view.devicePixelRatio = 1.0;
      addTearDown(tester.view.reset);

      final gateway = FakeCoreGateway(initialProfiles: const [profile]);
      await tester.pumpWidget(KChessApp(controller: AppController(gateway)));
      await tester.pumpAndSettle();
    }

    testWidgets('the rail lists Training between Spielen and Favoriten', (
      tester,
    ) async {
      await pumpShell(tester);

      final rail = tester.widget<NavigationRail>(find.byType(NavigationRail));
      final labels = rail.destinations
          .map((destination) => (destination.label as Text).data)
          .toList();

      expect(labels.indexOf('Training'), labels.indexOf('Spielen') + 1);
      expect(labels.indexOf('Favoriten'), labels.indexOf('Training') + 1);
    });

    testWidgets('the hub renders its three cards with real size', (
      tester,
    ) async {
      await pumpShell(tester);
      await tester.tap(find.text('Training'));
      await tester.pumpAndSettle();

      expect(find.byType(TrainingArenaScreen), findsOneWidget);
      expect(find.text('Eröffnungs-Labor'), findsOneWidget);
      expect(find.text('Blunder-Buster'), findsOneWidget);
      expect(find.text('Endspiel-Akademie'), findsOneWidget);
      expect(find.text('0 gelöste Taktiken'), findsOneWidget);
      expect(find.text('0 / 3 Stellungen gemeistert (0%)'), findsOneWidget);

      // The hub is a row of cards inside an IntrinsicHeight; a card that
      // collapses still reserves its slot, so assert the rendered geometry
      // rather than the presence of the widgets alone.
      final cards = tester
          .renderObjectList<RenderBox>(
            find.descendant(
              of: find.byType(TrainingArenaScreen),
              matching: find.byType(Card),
            ),
          )
          .toList();
      expect(cards.length, 3);
      for (final card in cards) {
        expect(card.size.width, greaterThan(100));
        expect(card.size.height, greaterThan(100));
      }

      // The mastery bar is the one element sized from data, so it is the one
      // that can silently vanish at 0%.
      final bar = tester.renderObject<RenderBox>(
        find.descendant(
          of: find.byType(MasteryProgressBar),
          matching: find.byType(LinearProgressIndicator),
        ),
      );
      expect(bar.size.height, greaterThan(0));
      expect(bar.size.width, greaterThan(0));
    });

    testWidgets('the hub stacks its cards on a narrow window', (tester) async {
      // The stacked layout puts each card's Column under an unbounded-height
      // ListView, where a flexible spacer would throw instead of rendering.
      tester.view.physicalSize = const Size(700, 1600);
      tester.view.devicePixelRatio = 1.0;
      addTearDown(tester.view.reset);

      final gateway = FakeCoreGateway(initialProfiles: const [profile]);
      await tester.pumpWidget(KChessApp(controller: AppController(gateway)));
      await tester.pumpAndSettle();

      await tester.tap(find.byIcon(Icons.menu));
      await tester.pumpAndSettle();
      await tester.tap(find.text('Training'));
      await tester.pumpAndSettle();

      expect(tester.takeException(), isNull);
      final cards = tester
          .renderObjectList<RenderBox>(
            find.descendant(
              of: find.byType(TrainingArenaScreen),
              matching: find.byType(Card),
            ),
          )
          .toList();
      expect(cards.length, 3);
      for (final card in cards) {
        expect(card.size.height, greaterThan(100));
      }
    });

    testWidgets('the endgame academy lists the seeded positions', (
      tester,
    ) async {
      await pumpShell(tester);
      await tester.tap(find.text('Training'));
      await tester.pumpAndSettle();
      await tester.tap(find.text('Endspiele öffnen'));
      await tester.pumpAndSettle();

      for (final exercise in TrainingLibrary.endgames) {
        expect(find.text(exercise.title), findsOneWidget);
      }
      expect(find.text('Noch nicht geübt'), findsNWidgets(3));

      // Every card carries its description and an un-mastered streak badge.
      expect(
        find.text(TrainingLibrary.endgames.first.hintText),
        findsOneWidget,
      );
      expect(find.text('0/3 fehlerfreie Wiederholungen'), findsNWidgets(3));

      // Tapping a card opens the interactive player on that exercise.
      await tester.tap(find.text(TrainingLibrary.endgames.first.title));
      await tester.pumpAndSettle();
      expect(find.byType(EndgameExercisePlayer), findsOneWidget);
      expect(find.text('Weiß am Zug — Gewinne die Stellung'), findsOneWidget);
    });

    testWidgets('a mastered exercise moves the hub progress bar', (
      tester,
    ) async {
      tester.view.physicalSize = const Size(1400, 2400);
      tester.view.devicePixelRatio = 1.0;
      addTearDown(tester.view.reset);

      final gateway = FakeCoreGateway(initialProfiles: const [profile]);
      final controller = AppController(gateway);
      await controller.initialize();
      final service = TrainingProgressService(store: InMemoryProgressStore());
      addTearDown(service.dispose);

      await tester.pumpWidget(
        _localized(
          TrainingArenaScreen(controller: controller, progress: service),
        ),
      );
      await tester.pumpAndSettle();
      expect(find.text('0 / 3 Stellungen gemeistert (0%)'), findsOneWidget);

      // Three clean runs are what mastery means; the hub reads the same
      // service the player writes to, so the bar has to follow.
      for (var i = 0; i < 3; i++) {
        await service.markExerciseCompleted('endgame_philidor', true);
      }
      await tester.pumpAndSettle();

      expect(find.text('1 / 3 Stellungen gemeistert (33%)'), findsOneWidget);
    });

    testWidgets('statistics deep-links an opening into the training tab', (
      tester,
    ) async {
      await pumpShell(tester);
      await tester.tap(find.text('Statistiken'));
      await tester.pumpAndSettle();

      expect(find.text('Ruy Lopez'), findsOneWidget);
      await tester.tap(find.text('Trainieren').first);
      await tester.pumpAndSettle();

      // The shell switched tabs and handed the line over with its ECO code.
      expect(find.byType(TrainingArenaScreen), findsOneWidget);
      expect(
        find.textContaining('C65 · Ruy Lopez'),
        findsOneWidget,
        reason: 'the opening card should pin the deep-linked line',
      );

      // …and the opening lab opens on it.
      await tester.tap(find.text('Linien trainieren'));
      await tester.pumpAndSettle();
      expect(find.text('Ruy Lopez'), findsOneWidget);
      expect(find.widgetWithText(Chip, 'C65'), findsOneWidget);
    });

    testWidgets('leaving the training tab clears the deep-linked line', (
      tester,
    ) async {
      await pumpShell(tester);
      await tester.tap(find.text('Statistiken'));
      await tester.pumpAndSettle();
      await tester.tap(find.text('Trainieren').first);
      await tester.pumpAndSettle();
      expect(find.textContaining('C65 · Ruy Lopez'), findsOneWidget);

      await tester.tap(find.text('Favoriten'));
      await tester.pumpAndSettle();
      await tester.tap(find.text('Training'));
      await tester.pumpAndSettle();

      expect(find.textContaining('C65 · Ruy Lopez'), findsNothing);
    });
  });

  group('endgame exercise player', () {
    /// Pumps the player on the scripted drill and returns the progress service
    /// it writes to.
    Future<TrainingProgressService> pumpPlayer(WidgetTester tester) async {
      tester.view.physicalSize = const Size(900, 1600);
      tester.view.devicePixelRatio = 1.0;
      addTearDown(tester.view.reset);

      final gateway = FakeCoreGateway(initialProfiles: const [profile]);
      _scriptDrill(gateway);
      final service = TrainingProgressService(store: InMemoryProgressStore());
      addTearDown(service.dispose);

      await tester.pumpWidget(
        _localized(
          EndgameExercisePlayer(
            exercise: _drillExercise,
            gateway: gateway,
            progress: service,
          ),
        ),
      );
      await tester.pumpAndSettle();
      return service;
    }

    testWidgets('opens on the starting position with the goal stated', (
      tester,
    ) async {
      await pumpPlayer(tester);

      expect(find.text('Weiß am Zug — Gewinne die Stellung'), findsOneWidget);
      expect(find.text('Zug 0 von 2'), findsOneWidget);
      expect(find.text('Du bist am Zug'), findsOneWidget);

      // The board must actually occupy space — a collapsed board would still
      // satisfy a finder-only assertion.
      final board = tester.renderObject<RenderBox>(
        find.byKey(const Key('training-board')),
      );
      expect(board.size.width, greaterThan(200));
      expect(board.size.height, greaterThan(200));
    });

    testWidgets('the expected move advances and the opponent replies', (
      tester,
    ) async {
      await pumpPlayer(tester);

      await _dragSquare(tester, 'g2', 'a2');
      await tester.pump();
      expect(find.text('Zug 1 von 2'), findsOneWidget);
      // The reply is deliberately delayed, so the board waits first.
      expect(find.text('Gegenzug …'), findsOneWidget);

      await tester.pump(const Duration(milliseconds: 400));
      await tester.pumpAndSettle();
      expect(find.text('Du bist am Zug'), findsOneWidget);
      expect(find.text('Zug 1 von 2'), findsOneWidget);
    });

    testWidgets('a legal but wrong move is rejected with a hint', (
      tester,
    ) async {
      await pumpPlayer(tester);

      // Rg3 is legal here but is not the line.
      await _dragSquare(tester, 'g2', 'g3');
      await tester.pumpAndSettle();

      expect(
        find.text('Nicht der beste Zug. Probiere es noch einmal.'),
        findsOneWidget,
      );
      // The board never left the last correct position.
      expect(find.text('Zug 0 von 2'), findsOneWidget);
    });

    testWidgets('an illegal drag is rejected the same way', (tester) async {
      await pumpPlayer(tester);

      // No legal move leaves g2 for b5, so the core offers nothing to match.
      await _dragSquare(tester, 'g2', 'b5');
      await tester.pumpAndSettle();

      expect(
        find.text('Nicht der beste Zug. Probiere es noch einmal.'),
        findsOneWidget,
      );
      expect(find.text('Zug 0 von 2'), findsOneWidget);
    });

    testWidgets('completing the line records a clean solve and celebrates', (
      tester,
    ) async {
      final service = await pumpPlayer(tester);

      await _dragSquare(tester, 'g2', 'a2');
      await tester.pump(const Duration(milliseconds: 400));
      await tester.pumpAndSettle();
      await _dragSquare(tester, 'a2', 'a7');
      await tester.pumpAndSettle();

      expect(find.byKey(const Key('training-solved-overlay')), findsOneWidget);
      expect(find.text('Nochmal üben'), findsOneWidget);

      final progress = service.progressFor(_drillExercise.id);
      expect(progress.successCount, 1);
      expect(progress.successStreak, 1);
      expect(progress.attemptCount, 1);
    });

    testWidgets('a run that needed a correction does not count as clean', (
      tester,
    ) async {
      final service = await pumpPlayer(tester);

      await _dragSquare(tester, 'g2', 'g3');
      await tester.pumpAndSettle();
      await _dragSquare(tester, 'g2', 'a2');
      await tester.pump(const Duration(milliseconds: 400));
      await tester.pumpAndSettle();
      await _dragSquare(tester, 'a2', 'a7');
      await tester.pumpAndSettle();

      expect(find.byKey(const Key('training-solved-overlay')), findsOneWidget);
      // "Mastered" means three runs without an error, so a corrected run has
      // to record a failed attempt rather than build the streak.
      final progress = service.progressFor(_drillExercise.id);
      expect(progress.successStreak, 0);
      expect(progress.isMastered, isFalse);
      expect(progress.attemptCount, 1);
    });

    testWidgets('practising again returns to the starting position', (
      tester,
    ) async {
      await pumpPlayer(tester);

      await _dragSquare(tester, 'g2', 'a2');
      await tester.pump(const Duration(milliseconds: 400));
      await tester.pumpAndSettle();
      await _dragSquare(tester, 'a2', 'a7');
      await tester.pumpAndSettle();

      await tester.tap(find.text('Nochmal üben'));
      await tester.pumpAndSettle();

      expect(find.byKey(const Key('training-solved-overlay')), findsNothing);
      expect(find.text('Zug 0 von 2'), findsOneWidget);
      expect(find.text('Du bist am Zug'), findsOneWidget);
    });

    testWidgets('leaving mid-flash disposes without a pending timer', (
      tester,
    ) async {
      tester.view.physicalSize = const Size(900, 1600);
      tester.view.devicePixelRatio = 1.0;
      addTearDown(tester.view.reset);

      final gateway = FakeCoreGateway(initialProfiles: const [profile]);
      _scriptDrill(gateway);
      final service = TrainingProgressService(store: InMemoryProgressStore());
      addTearDown(service.dispose);

      await tester.pumpWidget(
        _localized(
          Builder(
            builder: (context) => ElevatedButton(
              onPressed: () => Navigator.of(context).push(
                MaterialPageRoute<void>(
                  builder: (_) => EndgameExercisePlayer(
                    exercise: _drillExercise,
                    gateway: gateway,
                    progress: service,
                  ),
                ),
              ),
              child: const Text('open'),
            ),
          ),
        ),
      );
      await tester.tap(find.text('open'));
      await tester.pumpAndSettle();

      // Start a flash, then leave before its timer fires. A timer that still
      // called setState here would fail the test on a disposed State.
      await _dragSquare(tester, 'g2', 'g3');
      await tester.pump(const Duration(milliseconds: 50));
      tester.state<NavigatorState>(find.byType(Navigator)).pop();
      await tester.pumpAndSettle(const Duration(milliseconds: 600));

      expect(tester.takeException(), isNull);
      expect(find.byType(EndgameExercisePlayer), findsNothing);
    });

    testWidgets('the hint is available but not shown up front', (tester) async {
      await pumpPlayer(tester);

      expect(find.text(_drillExercise.hintText), findsNothing);
      await tester.tap(find.text('Tipp anzeigen'));
      await tester.pumpAndSettle();
      expect(find.text(_drillExercise.hintText), findsOneWidget);
    });
  });
}
