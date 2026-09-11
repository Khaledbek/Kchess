import 'dart:math';

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:kchess/features/training/data/endgame_catalog.dart';
import 'package:kchess/features/training/data/endgame_defender_bot.dart';
import 'package:kchess/features/training/data/endgame_position_generator.dart';
import 'package:kchess/features/training/models/endgame_drill.dart';
import 'package:kchess/features/training/models/training_progress.dart';
import 'package:kchess/features/training/presentation/drill_levels_screen.dart';
import 'package:kchess/features/training/presentation/endgame_drill_player.dart';
import 'package:kchess/localization/generated/app_localizations.dart';
import 'package:kchess/models/models.dart';
import 'package:kchess/shared/widgets/chess_board_view.dart';
import 'package:kchess/services/training_progress_service.dart';

import 'support/fake_core_gateway.dart';

Widget _localized(Widget child) => MaterialApp(
  locale: const Locale('de'),
  localizationsDelegates: AppLocalizations.localizationsDelegates,
  supportedLocales: AppLocalizations.supportedLocales,
  home: child,
);

/// White mates in one with Qa1-a8; the black king on h8 has g7/h7 covered by
/// the white king on g6 and g8 covered along the eighth rank.
const _mateInOne = '7k/8/6K1/8/8/8/8/Q7 w - - 0 1';
const _afterMate = 'Q6k/8/6K1/8/8/8/8/8 b - - 1 1';

/// The classic stalemate trap: Qc5-c7 takes every square from the cornered
/// king without giving check.
const _stalemateTrap = 'k7/8/8/2Q5/8/8/8/6K1 w - - 0 1';
const _afterStalemate = 'k7/2Q5/8/8/8/8/8/6K1 b - - 1 1';

/// The queen is gone — bare kings, so no mate is possible any more.
const _afterQueenLost = 'k7/8/8/8/8/8/8/6K1 b - - 0 1';

/// A quiet queen move that neither mates nor stalemates.
const _afterQuietMove = 'k7/8/8/8/2Q5/8/8/6K1 b - - 1 1';

/// Generator that hands the player a fixed position instead of shuffling.
class _FixedGenerator extends EndgamePositionGenerator {
  _FixedGenerator(super.gateway, this.fen);

  final String fen;

  @override
  Future<String?> generate(EndgameDrill drill, DrillLevel level) async => fen;
}

/// Generator that always fails, for the error path.
class _FailingGenerator extends EndgamePositionGenerator {
  _FailingGenerator(super.gateway);

  @override
  Future<String?> generate(EndgameDrill drill, DrillLevel level) async => null;
}

/// Defender that plays a scripted move without touching the engine.
class _ScriptedBot extends EndgameDefenderBot {
  _ScriptedBot(super.gateway, {this.replyUci});

  final String? replyUci;

  @override
  Future<DefenderReply?> reply({
    required String fenBeforePlayerMove,
    required String playerUci,
    required List<BoardMoveOption> defenderMoves,
  }) async {
    if (defenderMoves.isEmpty) return null;
    for (final move in defenderMoves) {
      if (move.uci == replyUci) {
        return DefenderReply(move: move, fromEngine: true);
      }
    }
    return DefenderReply(move: defenderMoves.first, fromEngine: true);
  }
}

/// Defender that blows up, standing in for any engine failure the bot itself
/// does not absorb.
class _ThrowingBot extends EndgameDefenderBot {
  _ThrowingBot(super.gateway);

  @override
  Future<DefenderReply?> reply({
    required String fenBeforePlayerMove,
    required String playerUci,
    required List<BoardMoveOption> defenderMoves,
  }) async => throw StateError('engine exploded');
}

const _drill = EndgameDrill(
  id: 'endgame_kq_vs_k',
  title: 'König & Dame gegen König',
  description: 'Treib den König an den Rand und setze matt.',
  tip: 'Lass dem König ein Fluchtfeld, sonst wird es Patt.',
  goal: EndgameGoal.checkmate,
  attackerPieces: ['K', 'Q'],
  defenderPieces: ['k'],
  levels: [
    DrillLevel(
      index: 1,
      difficulty: DrillDifficulties.beginner,
      maxMoves: 10,
      minEdgeDistance: 0,
      maxEdgeDistance: 0,
      minKingDistance: 2,
    ),
  ],
);

/// Two tiers, so mastering the first has a next level to offer.
const _tieredDrill = EndgameDrill(
  id: 'endgame_kq_vs_k',
  title: 'König & Dame gegen König',
  description: 'Treib den König an den Rand und setze matt.',
  tip: 'Lass dem König ein Fluchtfeld, sonst wird es Patt.',
  goal: EndgameGoal.checkmate,
  attackerPieces: ['K', 'Q'],
  defenderPieces: ['k'],
  levels: [
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
  ],
);

/// One-move budget, so the limit path is reachable in a single drag.
const _tightLevel = DrillLevel(
  index: 1,
  difficulty: DrillDifficulties.beginner,
  maxMoves: 1,
  minEdgeDistance: 0,
  maxEdgeDistance: 0,
  minKingDistance: 2,
);

Future<void> _dragSquare(WidgetTester tester, String from, String to) async {
  final start = tester.getCenter(find.byKey(Key('board-square-$from')));
  final end = tester.getCenter(find.byKey(Key('board-square-$to')));
  await tester.dragFrom(start, end - start);
  await tester.pump();
}

void main() {
  group('EndgamePositionGenerator', () {
    test('square maths matches the board', () {
      // a1 is index 0, h8 is 63.
      expect(EndgamePositionGenerator.edgeDistance(0), 0);
      expect(EndgamePositionGenerator.edgeDistance(63), 0);
      // d4 = file 3, rank 3 -> dead centre.
      expect(EndgamePositionGenerator.edgeDistance(3 * 8 + 3), 3);
      // a1 to h8 is seven king moves.
      expect(EndgamePositionGenerator.chebyshev(0, 63), 7);
      expect(EndgamePositionGenerator.chebyshev(0, 9), 1);
    });

    test('renders a placement as a FEN the core would accept', () {
      final fen = EndgamePositionGenerator.fenFor(const {
        0: 'K', // a1
        63: 'k', // h8
      }, 'w');
      expect(fen, '7k/8/8/8/8/8/8/K7 w - - 0 1');
    });

    test('generated positions honour the level shape', () async {
      final gateway = FakeCoreGateway();
      final generator = EndgamePositionGenerator(gateway, random: Random(7));
      const level = DrillLevel(
        index: 3,
        difficulty: DrillDifficulties.master,
        maxMoves: 30,
        minEdgeDistance: 3,
        maxEdgeDistance: 3,
        minKingDistance: 4,
      );

      for (var i = 0; i < 12; i++) {
        final fen = await generator.generate(_drill, level);
        expect(fen, isNotNull, reason: 'the generator must find a position');

        // Read the kings back through the core's piece list rather than
        // re-parsing the FEN: index 0 is a8, so square = (7 - row) * 8 + file.
        final position = await gateway.boardPosition(fen!);
        int? defender;
        int? attacker;
        for (var i = 0; i < 64; i++) {
          final square = (7 - i ~/ 8) * 8 + i % 8;
          if (position.pieces[i] == 'k') defender = square;
          if (position.pieces[i] == 'K') attacker = square;
        }
        expect(defender, isNotNull);
        expect(attacker, isNotNull);
        expect(
          EndgamePositionGenerator.edgeDistance(defender!),
          3,
          reason: 'this level pins the defending king to the centre squares',
        );
        expect(
          EndgamePositionGenerator.chebyshev(attacker!, defender),
          greaterThanOrEqualTo(4),
          reason: 'level 3 starts the kings far apart',
        );
      }
    });

    test('rejects a position where the defender can take the piece', () async {
      final gateway = FakeCoreGateway();
      // Any placement the generator proposes will offer a capture, so no
      // candidate can ever be accepted.
      final generator = _CaptureEverywhereGenerator(gateway, Random(1));
      final fen = await generator.generate(_drill, _drill.levels.first);
      expect(fen, isNull);
    });
  });

  group('EndgameDefenderBot', () {
    // The scripted bot the widget tests use bypasses the engine entirely, so
    // these drive the real one against the fake gateway's contract.
    test('asks the engine and plays the move it names', () async {
      final gateway = FakeCoreGateway();
      gateway.botReplies['a1a8'] = 'h8g8';
      final bot = EndgameDefenderBot(gateway, pollInterval: Duration.zero);
      addTearDown(bot.dispose);

      final reply = await bot.reply(
        fenBeforePlayerMove: _mateInOne,
        playerUci: 'a1a8',
        defenderMoves: const [
          BoardMoveOption(uci: 'h8g8', san: 'Kg8', fenAfter: _afterQuietMove),
          BoardMoveOption(uci: 'h8h7', san: 'Kh7', fenAfter: _afterQuietMove),
        ],
      );

      expect(reply, isNotNull);
      expect(reply!.fromEngine, isTrue, reason: 'the engine answered');
      expect(reply.move.uci, 'h8g8');
      expect(gateway.variationAnalysisCalls, 1);
    });

    test('falls back to a legal move when the engine fails', () async {
      final gateway = FakeCoreGateway();
      // No scripted reply and no opening fixture for this uci, so the fake
      // throws exactly as the core would on an unusable request.
      final bot = EndgameDefenderBot(gateway, pollInterval: Duration.zero);
      addTearDown(bot.dispose);

      final reply = await bot.reply(
        fenBeforePlayerMove: _mateInOne,
        playerUci: 'a1a8',
        defenderMoves: const [
          BoardMoveOption(uci: 'h8g8', san: 'Kg8', fenAfter: _afterQuietMove),
        ],
      );

      // The drill must continue rather than freeze; the UI flags the weaker
      // defence via fromEngine.
      expect(reply, isNotNull);
      expect(reply!.fromEngine, isFalse);
      expect(reply.move.uci, 'h8g8');
    });

    test('returns null only when there is nothing to play', () async {
      final gateway = FakeCoreGateway();
      final bot = EndgameDefenderBot(gateway, pollInterval: Duration.zero);
      addTearDown(bot.dispose);

      final reply = await bot.reply(
        fenBeforePlayerMove: _mateInOne,
        playerUci: 'a1a8',
        defenderMoves: const [],
      );
      expect(reply, isNull);
    });
  });

  group('EndgameDrillPlayer', () {
    Future<TrainingProgressService> pumpDrill(
      WidgetTester tester, {
      required String startFen,
      required FakeCoreGateway gateway,
      EndgameDrill? drill,
      DrillLevel? level,
      EndgamePositionGenerator? generator,
      String? botReply,
      TrainingProgressService? service,
    }) async {
      tester.view.physicalSize = const Size(900, 1600);
      tester.view.devicePixelRatio = 1.0;
      addTearDown(tester.view.reset);

      final active = drill ?? _drill;
      final progress =
          service ?? TrainingProgressService(store: InMemoryProgressStore());
      addTearDown(progress.dispose);

      await tester.pumpWidget(
        _localized(
          EndgameDrillPlayer(
            drill: active,
            level: level ?? active.levels.first,
            gateway: gateway,
            progress: progress,
            generator: generator ?? _FixedGenerator(gateway, startFen),
            bot: _ScriptedBot(gateway, replyUci: botReply),
            replyDelay: Duration.zero,
          ),
        ),
      );
      await tester.pumpAndSettle();
      return progress;
    }

    testWidgets('opens on the generated position with a live board', (
      tester,
    ) async {
      final gateway = FakeCoreGateway();
      gateway.boardScript[_mateInOne] = const [
        BoardMoveOption(uci: 'a1a8', san: 'Qa8#', fenAfter: _afterMate),
      ];
      await pumpDrill(tester, startFen: _mateInOne, gateway: gateway);

      expect(find.text('Setze Schwarz matt'), findsOneWidget);
      expect(find.text('Zug 0 / 10'), findsOneWidget);
      expect(find.text('Anfänger'), findsOneWidget);

      final board = tester.renderObject<RenderBox>(
        find.byKey(const Key('drill-board')),
      );
      expect(board.size.width, greaterThan(200));
      expect(board.size.height, greaterThan(200));
    });

    testWidgets('checkmate wins the drill and records a clean solve', (
      tester,
    ) async {
      final gateway = FakeCoreGateway();
      gateway.boardScript[_mateInOne] = const [
        BoardMoveOption(uci: 'a1a8', san: 'Qa8#', fenAfter: _afterMate),
      ];
      gateway.boardStatuses[_afterMate] = BoardStatus.checkmate;
      final service = await pumpDrill(
        tester,
        startFen: _mateInOne,
        gateway: gateway,
      );

      await _dragSquare(tester, 'a1', 'a8');
      await tester.pumpAndSettle();

      expect(find.byKey(const Key('drill-outcome-checkmate')), findsOneWidget);
      expect(find.text('Gebraucht: 1 Züge.'), findsOneWidget);
      final progress = service.progressFor(_drill.progressId(1));
      expect(progress.successCount, 1);
      expect(progress.successStreak, 1);
    });

    testWidgets('stalemate fails the drill instead of celebrating', (
      tester,
    ) async {
      final gateway = FakeCoreGateway();
      gateway.boardScript[_stalemateTrap] = const [
        BoardMoveOption(uci: 'c5c7', san: 'Qc7', fenAfter: _afterStalemate),
      ];
      gateway.boardStatuses[_afterStalemate] = BoardStatus.stalemate;
      final service = await pumpDrill(
        tester,
        startFen: _stalemateTrap,
        gateway: gateway,
      );

      await _dragSquare(tester, 'c5', 'c7');
      await tester.pumpAndSettle();

      expect(find.byKey(const Key('drill-outcome-stalemate')), findsOneWidget);
      expect(find.text('Patt!'), findsOneWidget);
      expect(
        find.text('Der König hat keine Züge mehr. Remis statt Sieg.'),
        findsOneWidget,
      );
      // A stalemate is a failed attempt, so it must not build the streak.
      final progress = service.progressFor(_drill.progressId(1));
      expect(progress.successCount, 0);
      expect(progress.successStreak, 0);
      expect(progress.attemptCount, 1);
    });

    testWidgets('running out of moves fails the drill', (tester) async {
      final gateway = FakeCoreGateway();
      gateway.boardScript[_stalemateTrap] = const [
        BoardMoveOption(uci: 'c5c4', san: 'Qc4', fenAfter: _afterQuietMove),
      ];
      final service = await pumpDrill(
        tester,
        startFen: _stalemateTrap,
        gateway: gateway,
        level: _tightLevel,
      );

      await _dragSquare(tester, 'c5', 'c4');
      await tester.pumpAndSettle();

      expect(find.byKey(const Key('drill-outcome-moveLimit')), findsOneWidget);
      expect(find.text('Zuglimit überschritten'), findsOneWidget);
      expect(service.progressFor(_drill.progressId(1)).successCount, 0);
    });

    testWidgets('mastering a level offers the next one, not just a rerun', (
      tester,
    ) async {
      final gateway = FakeCoreGateway();
      gateway.boardScript[_mateInOne] = const [
        BoardMoveOption(uci: 'a1a8', san: 'Qa8#', fenAfter: _afterMate),
      ];
      gateway.boardStatuses[_afterMate] = BoardStatus.checkmate;

      // Two clean runs already banked, so this win is the third and masters it.
      final service = TrainingProgressService(store: InMemoryProgressStore());
      addTearDown(service.dispose);
      for (var i = 0; i < 2; i++) {
        await service.markExerciseCompleted(_tieredDrill.progressId(1), true);
      }

      await pumpDrill(
        tester,
        startFen: _mateInOne,
        gateway: gateway,
        drill: _tieredDrill,
        service: service,
      );

      await _dragSquare(tester, 'a1', 'a8');
      await tester.pumpAndSettle();

      expect(find.byKey(const Key('drill-outcome-checkmate')), findsOneWidget);
      expect(find.byKey(const Key('drill-next-level')), findsOneWidget);
      expect(find.text('Nächstes Level'), findsOneWidget);
    });

    testWidgets(
      'an unmastered win offers another position, not the next tier',
      (tester) async {
        final gateway = FakeCoreGateway();
        gateway.boardScript[_mateInOne] = const [
          BoardMoveOption(uci: 'a1a8', san: 'Qa8#', fenAfter: _afterMate),
        ];
        gateway.boardStatuses[_afterMate] = BoardStatus.checkmate;

        await pumpDrill(
          tester,
          startFen: _mateInOne,
          gateway: gateway,
          drill: _tieredDrill,
        );

        await _dragSquare(tester, 'a1', 'a8');
        await tester.pumpAndSettle();

        // One clean run is not mastery, so the tier stays shut.
        expect(find.byKey(const Key('drill-next-level')), findsNothing);
        expect(find.text('Neue Stellung'), findsOneWidget);
      },
    );

    testWidgets('losing the mating piece ends the drill as a draw', (
      tester,
    ) async {
      final gateway = FakeCoreGateway();
      // Qc5xa7?? walks into ...Kxa7 in spirit: the move leaves bare kings, so
      // the drill is dead regardless of the move budget.
      gateway.boardScript[_stalemateTrap] = const [
        BoardMoveOption(uci: 'c5a7', san: 'Qa7+', fenAfter: _afterQueenLost),
      ];
      final service = await pumpDrill(
        tester,
        startFen: _stalemateTrap,
        gateway: gateway,
      );

      await _dragSquare(tester, 'c5', 'a7');
      await tester.pumpAndSettle();

      expect(find.byKey(const Key('drill-outcome-drawn')), findsOneWidget);
      expect(
        find.text('Ohne Mattmaterial ist die Stellung nicht mehr zu gewinnen.'),
        findsOneWidget,
      );
      // A dead draw is a failed attempt, not a win.
      expect(service.progressFor(_drill.progressId(1)).successCount, 0);
    });

    testWidgets('a throwing defender never leaves the board locked', (
      tester,
    ) async {
      final gateway = FakeCoreGateway();
      gateway.boardScript[_mateInOne] = const [
        BoardMoveOption(uci: 'a1a4', san: 'Qa4', fenAfter: _afterQuietMove),
      ];
      gateway.boardScript[_afterQuietMove] = const [
        BoardMoveOption(uci: 'a8b8', san: 'Kb8', fenAfter: _mateInOne),
      ];

      tester.view.physicalSize = const Size(900, 1600);
      tester.view.devicePixelRatio = 1.0;
      addTearDown(tester.view.reset);
      final service = TrainingProgressService(store: InMemoryProgressStore());
      addTearDown(service.dispose);

      await tester.pumpWidget(
        _localized(
          EndgameDrillPlayer(
            drill: _drill,
            level: _drill.levels.first,
            gateway: gateway,
            progress: service,
            generator: _FixedGenerator(gateway, _mateInOne),
            bot: _ThrowingBot(gateway),
            replyDelay: Duration.zero,
          ),
        ),
      );
      await tester.pumpAndSettle();

      await _dragSquare(tester, 'a1', 'a4');
      await tester.pumpAndSettle();

      // The move counted and the board is playable again: the busy flag must
      // not survive the failure, or the drill is dead.
      expect(find.text('Zug 1 / 10'), findsOneWidget);
      final board = tester.widget<ChessBoardView>(find.byType(ChessBoardView));
      expect(
        board.interactive,
        isTrue,
        reason: 'a failed defender reply must not lock the board',
      );
      // And the failure is visible rather than silent.
      expect(
        find.text(
          'Engine nicht verfügbar — der Verteidiger spielt gerade schwächer.',
        ),
        findsOneWidget,
      );
    });

    testWidgets('an illegal drag is ignored and costs no move', (tester) async {
      final gateway = FakeCoreGateway();
      gateway.boardScript[_mateInOne] = const [
        BoardMoveOption(uci: 'a1a8', san: 'Qa8#', fenAfter: _afterMate),
      ];
      await pumpDrill(tester, startFen: _mateInOne, gateway: gateway);

      // The queen cannot reach b5 from a1 in this position.
      await _dragSquare(tester, 'a1', 'b5');
      await tester.pumpAndSettle();

      expect(find.text('Zug 0 / 10'), findsOneWidget);
      expect(find.byKey(const Key('drill-outcome-checkmate')), findsNothing);
    });

    testWidgets('a failed generation offers a retry instead of a blank board', (
      tester,
    ) async {
      final gateway = FakeCoreGateway();
      await pumpDrill(
        tester,
        startFen: _mateInOne,
        gateway: gateway,
        generator: _FailingGenerator(gateway),
      );

      expect(
        find.text('Es konnte keine passende Stellung erzeugt werden.'),
        findsOneWidget,
      );
      expect(find.text('Erneut versuchen'), findsOneWidget);
    });
  });

  group('level unlocking', () {
    test('level 1 is open and later levels need the previous mastered', () {
      final drill = EndgameCatalog.checkmating.drills.first;
      final levels = drill.levels;
      expect(levels.length, 3);

      var snapshot = const TrainingProgressSnapshot.empty();
      expect(DrillLevelsScreen.isUnlocked(drill, levels[0], snapshot), isTrue);
      expect(DrillLevelsScreen.isUnlocked(drill, levels[1], snapshot), isFalse);

      snapshot = TrainingProgressSnapshot({
        drill.progressId(1): ExerciseProgress(
          exerciseId: drill.progressId(1),
          isMastered: true,
        ),
      });
      expect(DrillLevelsScreen.isUnlocked(drill, levels[1], snapshot), isTrue);
      expect(DrillLevelsScreen.isUnlocked(drill, levels[2], snapshot), isFalse);
    });

    test('every catalogue level has its own progress key', () {
      final ids = EndgameCatalog.checkmating.progressIds;
      expect(ids.toSet().length, ids.length);
      expect(ids, contains('endgame_kq_vs_k_l1'));
      expect(ids, contains('endgame_kr_vs_k_l3'));
      expect(ids, contains('endgame_q_vs_r_l1'));
      expect(ids, contains('endgame_q_vs_r_l3'));
    });

    test('endgame_q_vs_r Queen vs Rook drill is registered with 3 tiers', () {
      final drill = EndgameCatalog.drillById('endgame_q_vs_r');
      expect(drill, isNotNull);
      expect(drill!.attackerPieces, ['K', 'Q']);
      expect(drill.defenderPieces, ['k', 'r']);
      expect(drill.levels.length, 3);
      expect(drill.levels[0].maxMoves, 20);
      expect(drill.levels[1].maxMoves, 30);
      expect(drill.levels[2].maxMoves, 45);
    });
  });
}

/// Generator whose candidates always look capturable, to exercise rejection.
class _CaptureEverywhereGenerator extends EndgamePositionGenerator {
  _CaptureEverywhereGenerator(this._gateway, Random random)
    : super(_gateway, random: random) {
    _gateway.captureEverywhere = true;
  }

  final FakeCoreGateway _gateway;
}
