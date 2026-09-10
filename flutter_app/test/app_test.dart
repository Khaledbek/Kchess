import 'package:flutter/material.dart';
import 'package:flutter_localizations/flutter_localizations.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:kchess/app/kchess_app.dart';
import 'package:kchess/localization/generated/app_localizations.dart';
import 'package:kchess/models/models.dart';
import 'package:kchess/ui/app_root.dart';
import 'package:kchess/ui/screens/analysis_screen.dart';
import 'package:kchess/view_models/app_controller.dart';

import 'support/fake_core_gateway.dart';

// -----------------------------------------------------------------------------
// Section: Widget regression coverage
// -----------------------------------------------------------------------------

const profile = AppProfile(
  id: 'profile-1',
  type: ProfileType.localPgnFen,
  displayName: 'Local',
  avatarAsset: 'profile_unknown.png',
);

const onlineProfile = AppProfile(
  id: 'online-profile-1',
  type: ProfileType.chessCom,
  displayName: 'Online Fixture',
  providerUsername: 'OnlineFixture',
  avatarAsset: 'provider_chesscom_fallback.png',
  title: 'GM',
);

void main() {
  testWidgets('first run creates a local profile and opens games', (
    tester,
  ) async {
    final gateway = FakeCoreGateway();
    await tester.pumpWidget(KChessApp(controller: AppController(gateway)));
    await tester.pumpAndSettle();

    expect(find.byKey(const Key('profile-type-selector')), findsOneWidget);
    await tester.tap(find.text('PGN / FEN'));
    await tester.pump();
    await tester.enterText(
      find.byKey(const Key('profile-input')),
      'Mein Profil',
    );
    await tester.tap(find.byKey(const Key('create-profile')));
    await tester.pumpAndSettle();

    expect(gateway.storedProfiles.single.displayName, 'Mein Profil');
    expect(find.byKey(const Key('game-fixture-test')), findsOneWidget);
  });

  testWidgets(
    'complete cached analysis opens directly without preparation modal',
    (tester) async {
      final gateway = FakeCoreGateway(initialProfiles: const [profile]);
      await tester.pumpWidget(KChessApp(controller: AppController(gateway)));
      await tester.pumpAndSettle();

      await tester.tap(find.byKey(const Key('game-fixture-test')));
      await tester.pumpAndSettle();

      expect(gateway.startAnalysisCalls, 1);
      expect(find.byKey(const Key('analysis-board')), findsOneWidget);
      expect(find.byKey(const Key('analysis-preparation-modal')), findsNothing);
      expect(find.byKey(const Key('reopen-summary')), findsOneWidget);
      await tester.tap(find.byKey(const Key('reopen-summary')));
      await tester.pumpAndSettle();
      expect(find.text('Vollanalyse abgeschlossen'), findsOneWidget);
      expect(find.text('Halbzüge: 8'), findsOneWidget);
      expect(find.text('Patzer: 1'), findsOneWidget);
      expect(find.text('Lokale Genauigkeit: 78.6%'), findsOneWidget);
      expect(find.byKey(const Key('summary-player-toggle')), findsOneWidget);
      await tester.tap(find.text('Beide'));
      await tester.pumpAndSettle();
      expect(find.byKey(const Key('white-summary')), findsOneWidget);
      expect(find.byKey(const Key('black-summary')), findsOneWidget);
    },
  );

  testWidgets('pasted PGN import opens analysis and uses the native gateway', (
    tester,
  ) async {
    final gateway = FakeCoreGateway(initialProfiles: const [profile]);
    await tester.pumpWidget(KChessApp(controller: AppController(gateway)));
    await tester.pumpAndSettle();

    await tester.tap(find.byKey(const Key('import-pgn-fen')));
    await tester.pumpAndSettle();
    await tester.tap(find.text('PGN-Text einfügen'));
    await tester.pumpAndSettle();
    await tester.enterText(find.byKey(const Key('pgn-text')), '1. e4 e5 *');
    await tester.tap(find.text('Importieren'));
    await tester.pumpAndSettle();

    expect(gateway.importPgnCalls, 1);
    expect(gateway.startAnalysisCalls, 1);
    expect(find.byKey(const Key('analysis-board')), findsOneWidget);
  });

  testWidgets('running analysis stays in modal with live counts until opened', (
    tester,
  ) async {
    final gateway = FakeCoreGateway(
      initialProfiles: const [profile],
      analysisCompletesImmediately: false,
    );
    await tester.pumpWidget(KChessApp(controller: AppController(gateway)));
    await tester.pumpAndSettle();

    await tester.tap(find.byKey(const Key('game-fixture-test')));
    await tester.pump();
    await tester.pump(const Duration(milliseconds: 50));
    expect(find.byKey(const Key('analysis-preparation-modal')), findsOneWidget);
    expect(find.byKey(const Key('analysis-modal-progress')), findsOneWidget);
    expect(find.byKey(const Key('analysis-board')), findsNothing);
    expect(find.text('1 / 8 Halbzüge analysiert'), findsOneWidget);
    expect(find.text('1'), findsWidgets);
    await tester.pump(const Duration(milliseconds: 350));
    await tester.pump();
    expect(gateway.analysisStatusCalls, greaterThanOrEqualTo(1));
    expect(find.byKey(const Key('analysis-modal-accuracy')), findsNWidgets(2));
    expect(find.byKey(const Key('open-analysis')), findsOneWidget);
    expect(find.byKey(const Key('analysis-board')), findsNothing);
    await tester.tap(find.byKey(const Key('open-analysis')));
    await tester.pumpAndSettle();
    expect(find.byKey(const Key('analysis-board')), findsOneWidget);
  });

  testWidgets('Arabic locale renders the shell right-to-left', (tester) async {
    final gateway = FakeCoreGateway(
      initialProfiles: const [profile],
      currentSettings: const AppSettings(locale: 'ar'),
    );
    await tester.pumpWidget(KChessApp(controller: AppController(gateway)));
    await tester.pumpAndSettle();

    final arabicGames = find.text('المباريات').first;
    expect(arabicGames, findsOneWidget);
    expect(Directionality.of(tester.element(arabicGames)), TextDirection.rtl);
  });

  testWidgets('board arrow setting only changes board presentation', (
    tester,
  ) async {
    final gateway = FakeCoreGateway(initialProfiles: const [profile]);
    final controller = AppController(gateway);
    await controller.initialize();
    await controller.setShowBoardArrows(false);

    expect(gateway.settingWrites, 1);
    expect(gateway.startAnalysisCalls, 0);

    await tester.pumpWidget(
      _localized(
        AnalysisScreen(
          gateway: gateway,
          game: FakeCoreGateway.fixtureGame,
          settings: controller.settings,
        ),
      ),
    );
    await tester.pumpAndSettle();
    expect(find.byKey(const Key('best-move-arrow')), findsNothing);
    expect(gateway.startAnalysisCalls, 1);
  });

  testWidgets('play section opens bot setup with 100-Elo strength steps', (
    tester,
  ) async {
    final gateway = FakeCoreGateway(initialProfiles: const [profile]);
    await tester.pumpWidget(_localized(PlayScreen(gateway: gateway)));
    await tester.pumpAndSettle();

    expect(find.byKey(const Key('play-against-bot')), findsOneWidget);
    await tester.tap(find.byKey(const Key('play-against-bot')));
    await tester.pumpAndSettle();

    expect(find.byKey(const Key('bot-setup-icon')), findsOneWidget);
    expect(find.text('Gegen einen Bot spielen'), findsWidgets);
    expect(find.text('Elo: 1500'), findsOneWidget);
    expect(find.text('Stockfish 18'), findsOneWidget);

    await tester.tap(find.byKey(const Key('bot-elo-increase')));
    await tester.pump();
    expect(find.text('Elo: 1600'), findsOneWidget);

    await tester.tap(find.byKey(const Key('bot-elo-decrease')));
    await tester.pump();
    expect(find.text('Elo: 1500'), findsOneWidget);

    final slider = tester.widget<Slider>(
      find.byKey(const Key('bot-elo-slider')),
    );
    expect(slider.min, 100);
    expect(slider.max, 3200);
    expect(slider.divisions, 31);

    await tester.tap(find.byKey(const Key('start-bot-game')));
    await tester.pumpAndSettle();
    expect(find.byKey(const Key('bot-game-board')), findsOneWidget);
    expect(find.byKey(const Key('bot-game-status')), findsOneWidget);
    expect(find.text('Du bist am Zug.'), findsOneWidget);
    expect(find.byKey(const Key('bot-game-back')), findsOneWidget);
    expect(find.byKey(const Key('bot-game-forward')), findsOneWidget);
  });

  testWidgets('analysis board exports FEN instead of importing it', (tester) async {
    final gateway = FakeCoreGateway(initialProfiles: const [profile]);
    await tester.pumpWidget(
      _localized(
        AnalysisScreen(
          gateway: gateway,
          game: FakeCoreGateway.fixtureGame,
          settings: const AppSettings(),
        ),
      ),
    );
    await tester.pumpAndSettle();

    expect(find.byKey(const Key('import-fen-from-analysis')), findsNothing);
    expect(find.byKey(const Key('export-fen-from-analysis')), findsOneWidget);
  });

  testWidgets('classified theory move uses the mapped icon and offline stats', (
    tester,
  ) async {
    final gateway = FakeCoreGateway(initialProfiles: const [profile]);
    await tester.pumpWidget(
      _localized(
        AnalysisScreen(
          gateway: gateway,
          game: FakeCoreGateway.fixtureGame,
          settings: const AppSettings(),
        ),
      ),
    );
    await tester.pumpAndSettle();
    expect(find.byKey(const Key('move-classification-theory')), findsOneWidget);
    expect(find.byKey(const Key('move-icon-theory')), findsOneWidget);
    expect(find.text('Book-Partien: 1284211'), findsOneWidget);
    await tester.fling(find.byType(ListView), const Offset(0, -600), 1000);
    await tester.pumpAndSettle();
    expect(find.text('e2e4'), findsWidgets);
  });

  testWidgets('sideline graph keeps nested branches and uses shared controls', (
    tester,
  ) async {
    final gateway = FakeCoreGateway(initialProfiles: const [profile]);
    await tester.pumpWidget(
      _localized(
        AnalysisScreen(
          gateway: gateway,
          game: FakeCoreGateway.fixtureGame,
          settings: const AppSettings(),
        ),
      ),
    );
    await tester.pumpAndSettle();

    Future<void> play(String from, String to) async {
      await tester.tap(find.byKey(Key('board-square-$from')));
      await tester.tap(find.byKey(Key('board-square-$to')));
      await tester.pump();
      expect(find.byKey(const Key('variation-progress')), findsOneWidget);
      await tester.pump(const Duration(milliseconds: 350));
      await tester.pumpAndSettle();
    }

    await play('g1', 'f3');
    await play('b8', 'c6');
    await play('f1', 'b5');

    expect(gateway.variationAnalysisCalls, 3);
    expect(find.byKey(const Key('sideline-graph-card')), findsOneWidget);
    expect(find.byKey(const Key('sideline-graph-node-1')), findsNothing);
    expect(find.byKey(const Key('sideline-graph-node-2')), findsNothing);
    expect(find.byKey(const Key('sideline-graph-node-3')), findsOneWidget);
    expect(find.byKey(const Key('variation-first')), findsNothing);
    expect(find.byKey(const Key('return-main-line')), findsNothing);

    await tester.tap(find.byKey(const Key('analysis-previous')));
    await tester.pumpAndSettle();
    expect(find.textContaining('(2. Nf3 Nc6)'), findsOneWidget);
    await tester.tap(find.byKey(const Key('analysis-previous')));
    await tester.pumpAndSettle();
    expect(find.textContaining('(2. Nf3)'), findsOneWidget);

    // Branch from an already explored sideline node. The old Nc6 -> Bb5 branch
    // must stay in the graph instead of being truncated.
    await play('d7', 'd6');
    expect(gateway.variationAnalysisCalls, 4);
    expect(find.byKey(const Key('sideline-graph-node-1')), findsNothing);
    expect(find.byKey(const Key('sideline-graph-node-2')), findsNothing);
    expect(find.byKey(const Key('sideline-graph-node-3')), findsOneWidget);
    expect(find.byKey(const Key('sideline-graph-node-4')), findsOneWidget);
    expect(find.textContaining('(2. Nf3 d6)'), findsOneWidget);

    // Walk back to the main-line anchor with the same bottom controls. The
    // graph remains available for the whole lifetime of this analysis screen.
    await tester.tap(find.byKey(const Key('analysis-previous')));
    await tester.pumpAndSettle();
    await tester.tap(find.byKey(const Key('analysis-previous')));
    await tester.pumpAndSettle();
    expect(find.byKey(const Key('variation-analysis')), findsNothing);
    expect(find.byKey(const Key('sideline-graph-card')), findsOneWidget);

    final branch = find.byKey(const Key('sideline-graph-node-4'));
    await tester.ensureVisible(branch);
    await tester.tap(branch);
    await tester.pumpAndSettle();
    expect(find.byKey(const Key('variation-analysis')), findsOneWidget);
    expect(find.textContaining('(2. Nf3 d6)'), findsOneWidget);

    // The main-line PGN remains clickable even while a sideline is selected.
    final firstPgnMove = find.byKey(const Key('analysis-pgn-ply-0'));
    await tester.ensureVisible(firstPgnMove);
    await tester.tap(firstPgnMove);
    await tester.pumpAndSettle();
    expect(find.byKey(const Key('variation-analysis')), findsNothing);
    expect(find.byKey(const Key('sideline-graph-card')), findsOneWidget);
  });

  testWidgets('analysis modal keeps white and black live summaries separate', (
    tester,
  ) async {
    final gateway = FakeCoreGateway(
      initialProfiles: const [profile],
      analysisCompletesImmediately: false,
    );
    await tester.pumpWidget(KChessApp(controller: AppController(gateway)));
    await tester.pumpAndSettle();
    await tester.tap(find.byKey(const Key('game-fixture-test')));
    await tester.pump(const Duration(milliseconds: 50));

    final white = find.byKey(const Key('analysis-modal-first-side'));
    final black = find.byKey(const Key('analysis-modal-second-side'));
    expect(find.descendant(of: white, matching: find.text('Weiß')), findsOneWidget);
    expect(find.descendant(of: black, matching: find.text('Schwarz')), findsOneWidget);
    expect(find.descendant(of: white, matching: find.text('1')), findsOneWidget);
    expect(find.descendant(of: black, matching: find.text('1')), findsNothing);

    await tester.pump(const Duration(milliseconds: 350));
    await tester.pump();
    expect(find.text('Lokale Genauigkeit: 78.6%'), findsOneWidget);
    expect(find.text('Lokale Genauigkeit: 87.2%'), findsOneWidget);
  });

  testWidgets('black profile is shown as my player in preparation summary', (
    tester,
  ) async {
    final gateway = FakeCoreGateway(
      initialProfiles: const [profile],
      analysisCompletesImmediately: false,
      analysisProfileSide: 'black',
    );
    await tester.pumpWidget(KChessApp(controller: AppController(gateway)));
    await tester.pumpAndSettle();
    await tester.tap(find.byKey(const Key('game-fixture-test')));
    await tester.pump(const Duration(milliseconds: 50));
    await tester.pump(const Duration(milliseconds: 350));
    await tester.pump();

    final mine = find.byKey(const Key('analysis-modal-first-side'));
    final opponent = find.byKey(const Key('analysis-modal-second-side'));
    expect(
      find.descendant(of: mine, matching: find.text('Mein Spieler')),
      findsOneWidget,
    );
    expect(
      find.descendant(of: mine, matching: find.text('Patzer: 0')),
      findsNothing,
    );
    expect(
      find.descendant(of: opponent, matching: find.text('Gegner')),
      findsOneWidget,
    );
    expect(
      find.descendant(
        of: mine,
        matching: find.text('Lokale Genauigkeit: 87.2%'),
      ),
      findsOneWidget,
    );
  });

  testWidgets('move comparison uses SAN and existing C++ classification', (
    tester,
  ) async {
    final gateway = FakeCoreGateway(
      initialProfiles: const [profile],
      analysisClassification: MoveClassification.okay,
      analysisRecommendedMove: 'Nf3',
    );
    await tester.pumpWidget(
      _localized(
        AnalysisScreen(
          gateway: gateway,
          game: FakeCoreGateway.fixtureGame,
          settings: const AppSettings(),
        ),
      ),
    );
    await tester.pumpAndSettle();

    expect(find.text('e5 war Okay. Nf3 ist der beste Zug.'), findsOneWidget);
    expect(find.byKey(const Key('move-comparison')), findsOneWidget);
  });

  testWidgets(
    'online games expose month, local filters and preferred accuracy',
    (tester) async {
      final gateway = FakeCoreGateway(initialProfiles: const [onlineProfile]);
      await tester.pumpWidget(KChessApp(controller: AppController(gateway)));
      await tester.pumpAndSettle();

      expect(find.byKey(const Key('month-selector')), findsOneWidget);
      expect(find.byKey(const Key('game-search')), findsOneWidget);
      expect(find.byKey(const Key('result-filter')), findsOneWidget);
      expect(find.byKey(const Key('time-filter')), findsOneWidget);
      expect(find.byKey(const Key('color-filter')), findsOneWidget);
      expect(find.byKey(const Key('sort-games')), findsOneWidget);
      expect(find.text('92.4%'), findsOneWidget);
      expect(find.byKey(const Key('favorite-fixture-test')), findsOneWidget);
      expect(find.byKey(const Key('download-fixture-test')), findsOneWidget);
    },
  );

  testWidgets(
    'provider profile shows only available normalized performance data',
    (tester) async {
      final gateway = FakeCoreGateway(initialProfiles: const [onlineProfile]);
      final controller = AppController(gateway);
      await controller.initialize();
      await tester.pumpWidget(
        _localized(ProfileScreen(controller: controller)),
      );
      await tester.pumpAndSettle();

      expect(find.text('GM Online Fixture'), findsOneWidget);
      expect(find.byKey(const Key('provider-stat-rapid')), findsOneWidget);
      expect(find.text('1840'), findsOneWidget);
      expect(find.text('Bestwert: 1902'), findsOneWidget);
      expect(find.text('Partien: 42'), findsOneWidget);
    },
  );

  testWidgets(
    'invalid public username leaves first run without a broken profile',
    (tester) async {
      final gateway = FakeCoreGateway(
        createProviderError: 'Benutzer nicht gefunden.',
      );
      await tester.pumpWidget(KChessApp(controller: AppController(gateway)));
      await tester.pumpAndSettle();
      await tester.enterText(
        find.byKey(const Key('profile-input')),
        'missing-user',
      );
      await tester.tap(find.byKey(const Key('create-profile')));
      await tester.pumpAndSettle();

      expect(find.text('Benutzer nicht gefunden.'), findsOneWidget);
      expect(gateway.storedProfiles, isEmpty);
      expect(find.byKey(const Key('profile-input')), findsOneWidget);
    },
  );

  testWidgets('offline sync keeps cached online game and reports the state', (
    tester,
  ) async {
    final gateway = FakeCoreGateway(
      initialProfiles: const [onlineProfile],
      syncProviderError: 'Offline – gespeicherte Daten werden angezeigt.',
    );
    await tester.pumpWidget(KChessApp(controller: AppController(gateway)));
    await tester.pumpAndSettle();

    expect(
      find.text('Offline – gespeicherte Daten werden angezeigt.'),
      findsOneWidget,
    );
    expect(find.byKey(const Key('game-fixture-test')), findsOneWidget);
  });

  testWidgets('local library supports favorites and deleting an entry', (
    tester,
  ) async {
    final gateway = FakeCoreGateway(initialProfiles: const [profile]);
    await tester.pumpWidget(KChessApp(controller: AppController(gateway)));
    await tester.pumpAndSettle();

    expect(find.byKey(const Key('favorite-fixture-test')), findsOneWidget);
    expect(find.byKey(const Key('delete-local-fixture-test')), findsOneWidget);

    await tester.tap(find.byKey(const Key('favorite-fixture-test')));
    await tester.pumpAndSettle();
    expect(gateway.storedGames.single.favorite, isTrue);

    await tester.tap(find.byKey(const Key('delete-local-fixture-test')));
    await tester.pumpAndSettle();
    expect(find.text('Lokalen Eintrag löschen?'), findsOneWidget);
    await tester.tap(find.text('Löschen'));
    await tester.pumpAndSettle();

    expect(gateway.storedGames, isEmpty);
    expect(find.byKey(const Key('game-fixture-test')), findsNothing);
  });

  testWidgets('download library is not restricted to the selected month', (
    tester,
  ) async {
    const olderDownload = GameSummary(
      id: 'download-old-month',
      kind: 'pgn',
      whiteName: 'OnlineFixture',
      blackName: 'OlderOpponent',
      result: '1-0',
      timeControl: '300+0',
      startingFen: 'start',
      providerOutcome: 'win',
      timeControlType: 'blitz',
      downloaded: true,
      endedAt: 1782864000,
    );
    final gateway = FakeCoreGateway(
      initialProfiles: const [onlineProfile],
      initialGames: const [olderDownload],
    );
    final controller = AppController(gateway);
    await controller.initialize();

    await tester.pumpWidget(
      _localized(
        GamesScreen(controller: controller, savedFilter: 'downloaded'),
      ),
    );
    await tester.pumpAndSettle();

    expect(find.byKey(const Key('game-download-old-month')), findsOneWidget);
    expect(find.byKey(const Key('month-selector')), findsNothing);
  });

  testWidgets('online month cache can be cleared without running a sync', (
    tester,
  ) async {
    final gateway = FakeCoreGateway(initialProfiles: const [onlineProfile]);
    final controller = AppController(gateway);
    await controller.initialize();

    await tester.pumpWidget(_localized(GamesScreen(controller: controller)));
    await tester.pumpAndSettle();

    await tester.tap(find.byKey(const Key('clear-month-cache')));
    await tester.pumpAndSettle();
    expect(find.text('Monatscache löschen?'), findsOneWidget);
    await tester.tap(find.text('Cache löschen'));
    await tester.pumpAndSettle();

    expect(gateway.clearCachedMonthCalls, 1);
    expect(gateway.clearedMonths, contains(controller.selectedMonth));
  });
}

Widget _localized(Widget child) => MaterialApp(
  locale: const Locale('de'),
  supportedLocales: AppLocalizations.supportedLocales,
  localizationsDelegates: const [
    AppLocalizations.delegate,
    GlobalMaterialLocalizations.delegate,
    GlobalWidgetsLocalizations.delegate,
    GlobalCupertinoLocalizations.delegate,
  ],
  home: child,
);
