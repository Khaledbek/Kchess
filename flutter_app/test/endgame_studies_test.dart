import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:kchess/features/training/data/endgame_defender_bot.dart';
import 'package:kchess/features/training/data/endgame_studies_library.dart';
import 'package:kchess/features/training/models/endgame_study.dart';
import 'package:kchess/features/training/presentation/endgame_studies_screen.dart';
import 'package:kchess/features/training/presentation/endgame_study_player.dart';
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

class _TestAssetBundle extends CachingAssetBundle {
  _TestAssetBundle(this._json);

  final String _json;

  @override
  Future<String> loadString(String key, {bool cache = true}) async => _json;

  @override
  Future<ByteData> load(String key) async => throw UnimplementedError();
}

class _ScriptedBot extends EndgameDefenderBot {
  _ScriptedBot(super.gateway);

  @override
  Future<DefenderReply?> reply({
    required String fenBeforePlayerMove,
    required String playerUci,
    required List<BoardMoveOption> defenderMoves,
  }) async {
    if (defenderMoves.isEmpty) return null;
    return DefenderReply(move: defenderMoves.first, fromEngine: true);
  }
}

Future<void> _dragSquare(WidgetTester tester, String from, String to) async {
  final start = tester.getCenter(find.byKey(Key('board-square-$from')));
  final end = tester.getCenter(find.byKey(Key('board-square-$to')));
  await tester.dragFrom(start, end - start);
  await tester.pump();
}

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  late String rawJson;

  setUpAll(() {
    // Read the asset file directly from the filesystem for unit tests.
    final file = File('assets/endgame_studies.json');
    expect(file.existsSync(), isTrue);
    rawJson = file.readAsStringSync();
  });

  tearDown(() {
    EndgameStudiesLibrary.resetCache();
  });

  group('EndgameStudiesLibrary & Models', () {
    test('loads and parses all 86 studies across 11 sections', () async {
      final bundle = _TestAssetBundle(rawJson);
      final catalogue = await EndgameStudiesLibrary.load(bundle: bundle);

      expect(catalogue.sections.length, 11);
      expect(catalogue.studyCount, 90);
      expect(catalogue.sourceTitle, contains('Chess Studies'));
      expect(catalogue.sourceAuthors, contains('Kling'));
      expect(catalogue.sourceYear, 1851);

      // Verify each section has studies and valid IDs
      for (final section in catalogue.sections) {
        expect(section.id, isNotEmpty);
        expect(section.title, isNotEmpty);
        expect(section.studies, isNotEmpty);
        for (final study in section.studies) {
          expect(study.id, startsWith(section.id));
          expect(study.fen, isNotEmpty);
          expect(study.number, greaterThan(0));
        }
      }
    });

    test(
      'nextStudyInSection finds the next study or null at the end',
      () async {
        final bundle = _TestAssetBundle(rawJson);
        final catalogue = await EndgameStudiesLibrary.load(bundle: bundle);
        final firstSection = catalogue.sections.first;

        final first = firstSection.studies[0];
        final second = firstSection.studies[1];
        final last = firstSection.studies.last;

        expect(
          EndgameStudiesLibrary.nextStudyInSection(firstSection, first)?.id,
          second.id,
        );
        expect(
          EndgameStudiesLibrary.nextStudyInSection(firstSection, last),
          isNull,
        );
      },
    );
  });

  group('EndgameStudiesScreen Widget', () {
    testWidgets('renders study catalogue and sections', (tester) async {
      final bundle = _TestAssetBundle(rawJson);
      await EndgameStudiesLibrary.load(bundle: bundle);

      tester.view.physicalSize = const Size(900, 1600);
      tester.view.devicePixelRatio = 1.0;
      addTearDown(tester.view.reset);

      final gateway = FakeCoreGateway();
      final progress = TrainingProgressService(store: InMemoryProgressStore());
      addTearDown(progress.dispose);

      await tester.pumpWidget(
        _localized(EndgameStudiesScreen(gateway: gateway, progress: progress)),
      );
      await tester.pumpAndSettle();

      expect(find.text('Klassische Studien (1851)'), findsWidgets);
      expect(
        find.text('Josef Kling and Bernhard Horwitz (1851)'),
        findsOneWidget,
      );
      expect(find.text('König & Bauer'), findsOneWidget);
      expect(find.text('Läufer & Bauern'), findsOneWidget);
      expect(find.text('Dame gegen Turm'), findsOneWidget);

      // Expand a section and verify studies are listed
      await tester.tap(find.byKey(const Key('study-section-king_and_pawn')));
      await tester.pumpAndSettle();

      expect(find.text('Studie 1'), findsOneWidget);
      expect(find.text('Studie 2'), findsOneWidget);
    });
  });

  group('EndgameStudyPlayer Widget', () {
    const studyFen = '7k/8/6K1/8/8/8/8/Q7 w - - 0 1';
    const mateFen = 'Q6k/8/6K1/8/8/8/8/8 b - - 1 1';

    const testStudy = EndgameStudy(
      id: 'king_and_pawn_01',
      fen: studyFen,
      solverColor: 'white',
      difficulty: 'beginner',
    );

    const testSection = EndgameStudySection(
      id: 'king_and_pawn',
      title: 'König & Bauer',
      description: 'Endspiele mit Bauern.',
      studies: [testStudy],
    );

    testWidgets('opens on study position and checkmates into victory', (
      tester,
    ) async {
      tester.view.physicalSize = const Size(900, 1600);
      tester.view.devicePixelRatio = 1.0;
      addTearDown(tester.view.reset);

      final gateway = FakeCoreGateway();
      gateway.boardScript[studyFen] = const [
        BoardMoveOption(uci: 'a1a8', san: 'Qa8#', fenAfter: mateFen),
      ];
      gateway.boardStatuses[mateFen] = BoardStatus.checkmate;

      final progress = TrainingProgressService(store: InMemoryProgressStore());
      addTearDown(progress.dispose);

      await tester.pumpWidget(
        _localized(
          EndgameStudyPlayer(
            study: testStudy,
            section: testSection,
            gateway: gateway,
            progress: progress,
            bot: _ScriptedBot(gateway),
            replyDelay: Duration.zero,
          ),
        ),
      );
      await tester.pumpAndSettle();

      expect(find.text('Studie 1'), findsOneWidget);
      expect(find.text('Zug 0'), findsOneWidget);
      expect(find.text('Anfänger'), findsOneWidget);

      // Drag queen a1 -> a8 for checkmate
      await _dragSquare(tester, 'a1', 'a8');
      await tester.pumpAndSettle();

      expect(find.byKey(const Key('study-outcome-checkmate')), findsOneWidget);
      expect(find.text('Matt! Sauber gespielt.'), findsOneWidget);

      final saved = progress.progressFor('king_and_pawn_01');
      expect(saved.successCount, 1);
    });

    testWidgets('a lone king facing a pawn is not called a draw', (
      tester,
    ) async {
      tester.view.physicalSize = const Size(900, 1600);
      tester.view.devicePixelRatio = 1.0;
      addTearDown(tester.view.reset);
      // The reported bug: the solver having no pieces was read as "no mating
      // material", so a King-and-Pawn study was declared drawn on move one.
      const start = '8/1p6/1k6/8/8/2K5/8/8 w - - 0 1';
      const after = '8/1p6/1k6/8/8/8/2K5/8 b - - 1 1';
      final gateway = FakeCoreGateway();
      gateway.boardScript[start] = const [
        BoardMoveOption(uci: 'c3c2', san: 'Kc2', fenAfter: after),
      ];
      gateway.boardScript[after] = const [
        BoardMoveOption(uci: 'b7b5', san: 'b5', fenAfter: start),
      ];

      final progress = TrainingProgressService(store: InMemoryProgressStore());
      addTearDown(progress.dispose);

      await tester.pumpWidget(
        _localized(
          EndgameStudyPlayer(
            study: const EndgameStudy(
              id: 'king_and_pawn_01',
              fen: start,
              solverColor: 'white',
              difficulty: 'beginner',
            ),
            section: testSection,
            gateway: gateway,
            progress: progress,
            bot: _ScriptedBot(gateway),
            replyDelay: Duration.zero,
          ),
        ),
      );
      await tester.pumpAndSettle();

      final from = tester.getCenter(find.byKey(const Key('board-square-c3')));
      final to = tester.getCenter(find.byKey(const Key('board-square-c2')));
      await tester.dragFrom(from, to - from);
      await tester.pumpAndSettle();

      // Guard against a vacuous pass: the move must actually have happened.
      expect(find.text('Zug 1'), findsOneWidget);
      // Black still has a pawn, so nothing is decided yet.
      expect(find.text('Remis'), findsNothing);
      expect(
        find.text('Ohne Mattmaterial ist die Stellung nicht mehr zu gewinnen.'),
        findsNothing,
      );
    });

    testWidgets('selecting a piece shows dots on its legal squares', (
      tester,
    ) async {
      tester.view.physicalSize = const Size(900, 1600);
      tester.view.devicePixelRatio = 1.0;
      addTearDown(tester.view.reset);
      const start = '8/1p6/1k6/8/8/2K5/8/8 w - - 0 1';
      final gateway = FakeCoreGateway();
      gateway.boardScript[start] = const [
        BoardMoveOption(uci: 'c3c2', san: 'Kc2', fenAfter: start),
        BoardMoveOption(uci: 'c3d4', san: 'Kd4', fenAfter: start),
      ];

      final progress = TrainingProgressService(store: InMemoryProgressStore());
      addTearDown(progress.dispose);

      await tester.pumpWidget(
        _localized(
          EndgameStudyPlayer(
            study: const EndgameStudy(
              id: 'king_and_pawn_01',
              fen: start,
              solverColor: 'white',
              difficulty: 'beginner',
            ),
            section: testSection,
            gateway: gateway,
            progress: progress,
            bot: _ScriptedBot(gateway),
            replyDelay: Duration.zero,
          ),
        ),
      );
      await tester.pumpAndSettle();

      ChessBoardView board() =>
          tester.widget<ChessBoardView>(find.byType(ChessBoardView));
      expect(board().moveTargets, isEmpty, reason: 'nothing selected yet');

      await tester.tap(find.byKey(const Key('board-square-c3')));
      await tester.pumpAndSettle();
      expect(board().moveTargets, {'c2', 'd4'});

      // Tapping an empty square with no moves clears the selection.
      await tester.tap(find.byKey(const Key('board-square-h8')));
      await tester.pumpAndSettle();
      expect(board().moveTargets, isEmpty);
    });
  });
}
