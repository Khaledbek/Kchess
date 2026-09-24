// -----------------------------------------------------------------------------
// Section: Accuracy card, background progress and its settings switch
// -----------------------------------------------------------------------------

import 'package:flutter/material.dart';
import 'package:flutter_localizations/flutter_localizations.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:kchess/features/app/application/app_controller.dart';
import 'package:kchess/ffi/core_gateway.dart';
import 'package:kchess/localization/generated/app_localizations.dart';
import 'package:kchess/shared/models/models.dart';
import 'package:kchess/ui/app_root.dart';

/// Accuracy over 40 analysed games: 71% before, 76% over the last 20.
Map<String, Object?> _accuracyJson({int games = 40, String verdict = 'improving'}) =>
    <String, Object?>{
      'hasProfile': true,
      'analysedGames': games,
      'averageAccuracy': 73.4,
      'blundersPerGame': 1.7,
      'byColor': {
        'white': {'games': 22, 'accuracy': 75.0},
        'black': {'games': 18, 'accuracy': 71.4},
      },
      'byTimeControl': [
        {'timeControl': 'blitz', 'games': 30, 'accuracy': 72.0},
        {'timeControl': 'rapid', 'games': 10, 'accuracy': 77.5},
      ],
      'byPhase': [
        {'phase': 'opening', 'games': 40, 'accuracy': 82.0, 'errorsPerGame': 0.4},
        {'phase': 'middlegame', 'games': 38, 'accuracy': 70.0, 'errorsPerGame': 1.6},
        {'phase': 'endgame', 'games': 12, 'accuracy': 64.0, 'errorsPerGame': 1.1},
      ],
      'timeline': [
        for (var i = 0; i < games; i++)
          {'endedAt': 1788000000 + i * 86400, 'accuracy': 65.0 + (i % 7) * 2, 'average': 70.0 + i * 0.15},
      ],
      'timePressure': <String, Object?>{
        'games': 30,
        'moves': 900,
        'blunders': 20,
        'blunderShareInTimeTrouble': 0.65,
        'gamesInTimeTrouble': 0.4,
        'buckets': <Object?>[
          {'bucket': 'comfortable', 'moves': 600, 'blunders': 4, 'errors': 30,
           'errorsPerHundredMoves': 5.0, 'accuracy': 80.0},
          {'bucket': 'fair', 'moves': 200, 'blunders': 3, 'errors': 20,
           'errorsPerHundredMoves': 10.0, 'accuracy': 72.0},
          {'bucket': 'low', 'moves': 60, 'blunders': 0, 'errors': 9,
           'errorsPerHundredMoves': 15.0, 'accuracy': 66.0},
          {'bucket': 'critical', 'moves': 40, 'blunders': 13, 'errors': 16,
           'errorsPerHundredMoves': 40.0, 'accuracy': 48.0},
        ],
      },
      'trend': verdict == 'insufficient'
          ? {'verdict': 'insufficient', 'window': 0, 'gamesNeeded': 8}
          : {
              'verdict': verdict,
              'window': 20,
              'gamesNeeded': 0,
              'recentAccuracy': 76.2,
              'previousAccuracy': 71.1,
              'recentBlunders': 1.3,
              'previousBlunders': 2.1,
            },
    };

class _Gateway implements CoreGateway {
  _Gateway({required this.accuracy, required this.status});

  @override
  dynamic noSuchMethod(Invocation invocation) => super.noSuchMethod(invocation);

  Map<String, Object?> accuracy;
  BackgroundAnalysisStatus status;
  int accuracyCalls = 0;
  int openingsCalls = 0;
  final List<bool> enabledCalls = [];

  @override
  Future<AccuracyStats> accuracyStats({String timeControl = 'all'}) async {
    accuracyCalls++;
    return AccuracyStats.fromJson(accuracy);
  }

  @override
  Future<BackgroundAnalysisStatus> backgroundAnalysisStatus() async => status;

  @override
  Future<void> setBackgroundAnalysisEnabled(bool enabled) async {
    enabledCalls.add(enabled);
    status = BackgroundAnalysisStatus(
      enabled: enabled,
      state: enabled ? 'running' : 'disabled',
      analysedGames: status.analysedGames,
      totalGames: status.totalGames,
    );
  }

  @override
  Future<OpeningsStats> openingsStats({String timeControl = 'all'}) async {
    openingsCalls++;
    return const OpeningsStats(hasProfile: true);
  }

  // Cards not under test show their error state.
  @override
  Future<StatisticsOverview> statisticsOverview() => Future.error('not scripted');
  @override
  Future<TerminationStats> terminationStats() => Future.error('not scripted');
  @override
  Future<PhaseStats> phaseStats() => Future.error('not scripted');
  @override
  Future<StatisticsTimeline> statisticsTimeline(GameQuery query) =>
      Future.error('not scripted');
}

Widget _app(Widget home) => MaterialApp(
  locale: const Locale('en'),
  supportedLocales: AppLocalizations.supportedLocales,
  localizationsDelegates: const [
    AppLocalizations.delegate,
    GlobalMaterialLocalizations.delegate,
    GlobalWidgetsLocalizations.delegate,
    GlobalCupertinoLocalizations.delegate,
  ],
  home: home,
);

Future<void> _pumpStatistics(WidgetTester tester, _Gateway gateway, {Size size = const Size(420, 2600)}) async {
  tester.view.physicalSize = size;
  tester.view.devicePixelRatio = 1;
  addTearDown(tester.view.reset);
  await tester.pumpWidget(_app(StatisticsScreen(controller: AppController(gateway))));
  await tester.pump();
  await tester.pump(const Duration(milliseconds: 100));
}

void main() {
  testWidgets('the accuracy card says whether the player is improving', (tester) async {
    final gateway = _Gateway(
      accuracy: _accuracyJson(),
      status: const BackgroundAnalysisStatus(state: 'running', analysedGames: 40, totalGames: 120),
    );
    await _pumpStatistics(tester, gateway);

    final card = find.byKey(const Key('stats-accuracy'));
    expect(card, findsOneWidget);
    expect(find.text('73%'), findsOneWidget);
    expect(find.text('Analysed 40 of 120 games'), findsOneWidget);
    expect(find.text('Analysing in the background'), findsOneWidget);

    final trend = find.byKey(const Key('stats-accuracy-trend'));
    expect(find.descendant(of: trend, matching: find.text('Improving')), findsOneWidget);
    expect(
      find.descendant(of: trend, matching: find.text('Last 20 games: 76% (before: 71%)')),
      findsOneWidget,
    );
    expect(
      find.descendant(of: trend, matching: find.text('Blunders per game: 1.3 (before: 2.1)')),
      findsOneWidget,
    );

    // The chart and the bars really render, not just reserve space.
    final chart = tester.getSize(find.byKey(const Key('stats-accuracy-chart')));
    expect(chart.width, greaterThan(200));
    expect(chart.height, 170);
    final opening = find.byKey(const ValueKey('stats-accuracy-phase-opening'));
    expect(find.descendant(of: opening, matching: find.text('82%')), findsOneWidget);
    final fills = tester
        .widgetList<Container>(find.descendant(of: opening, matching: find.byType(Container)))
        .toList();
    final boxes = [
      for (final element in find.descendant(of: opening, matching: find.byType(Container)).evaluate())
        (element.renderObject! as RenderBox).size,
    ];
    expect(fills, hasLength(2));
    // Track and fill: the fill is 82% of the track.
    expect(boxes[0].height, 8);
    expect(boxes[1].width, closeTo(boxes[0].width * 0.82, 0.5));
    expect(boxes[1].width, greaterThan(50));
    expect(find.descendant(of: card, matching: find.text('Endgame')), findsOneWidget);
    expect(find.descendant(of: card, matching: find.text('Rapid')), findsOneWidget);
    expect(tester.takeException(), isNull);
  });

  testWidgets('the clock breakdown shows where the blunders happen', (tester) async {
    final gateway = _Gateway(
      accuracy: _accuracyJson(),
      status: const BackgroundAnalysisStatus(state: 'running', analysedGames: 40, totalGames: 120),
    );
    await _pumpStatistics(tester, gateway);

    final clock = find.byKey(const Key('stats-accuracy-clock'));
    expect(clock, findsOneWidget);
    expect(find.text('By time left on the clock'), findsOneWidget);
    expect(
      find.text('65% of your blunders come with under a tenth of the clock left.'),
      findsOneWidget,
    );

    // Every bucket that was played shows its accuracy and its error rate.
    final critical = find.byKey(const ValueKey('stats-accuracy-clock-critical'));
    expect(find.descendant(of: critical, matching: find.text('Under a tenth')), findsOneWidget);
    expect(find.descendant(of: critical, matching: find.text('48%')), findsOneWidget);
    expect(
      find.descendant(of: critical, matching: find.text('40.0 mistakes per 100 moves')),
      findsOneWidget,
    );
    final bars = tester.widgetList(find.descendant(of: clock, matching: find.byType(LayoutBuilder)));
    expect(bars, hasLength(4));
  });

  testWidgets('one bad evening does not become a clock claim', (tester) async {
    final json = _accuracyJson();
    (json['timePressure']! as Map<String, Object?>)
      ..['blunders'] = 3
      ..['blunderShareInTimeTrouble'] = 1.0;
    final gateway = _Gateway(
      accuracy: json,
      status: const BackgroundAnalysisStatus(state: 'running', analysedGames: 4, totalGames: 120),
    );
    await _pumpStatistics(tester, gateway);

    expect(find.byKey(const Key('stats-accuracy-clock')), findsOneWidget);
    expect(find.byKey(const Key('stats-accuracy-clock-share')), findsNothing);
  });

  testWidgets('too few analysed games say how many more are needed', (tester) async {
    final gateway = _Gateway(
      accuracy: _accuracyJson(games: 2, verdict: 'insufficient'),
      status: const BackgroundAnalysisStatus(state: 'paused', analysedGames: 2, totalGames: 50),
    );
    await _pumpStatistics(tester, gateway);

    expect(find.text('Not enough analysed games yet'), findsOneWidget);
    expect(
      find.text('8 more analysed games to see whether you are improving.'),
      findsOneWidget,
    );
    expect(find.text('Paused while you analyse or play'), findsOneWidget);
  });

  testWidgets('newly analysed games refresh accuracy and the warnings', (tester) async {
    final gateway = _Gateway(
      accuracy: _accuracyJson(),
      status: const BackgroundAnalysisStatus(state: 'running', analysedGames: 40, totalGames: 120),
    );
    await _pumpStatistics(tester, gateway);
    final accuracyBefore = gateway.accuracyCalls;
    final openingsBefore = gateway.openingsCalls;

    // One more game is not worth a reload...
    gateway.status = const BackgroundAnalysisStatus(state: 'running', analysedGames: 41, totalGames: 120);
    await tester.pump(const Duration(seconds: 11));
    expect(gateway.accuracyCalls, accuracyBefore);

    // ...three more are, and the weakness warnings refresh with them.
    gateway.status = const BackgroundAnalysisStatus(state: 'running', analysedGames: 43, totalGames: 120);
    await tester.pump(const Duration(seconds: 11));
    await tester.pump();
    expect(gateway.accuracyCalls, accuracyBefore + 1);
    expect(gateway.openingsCalls, openingsBefore + 1);
    expect(find.text('Analysed 43 of 120 games'), findsOneWidget);
    // The card keeps its numbers on screen while it reloads.
    expect(find.byKey(const Key('stats-accuracy-trend')), findsOneWidget);

    await tester.pumpWidget(const SizedBox());
  });

  testWidgets('settings switch turns background analysis off', (tester) async {
    tester.view.physicalSize = const Size(600, 1400);
    tester.view.devicePixelRatio = 1;
    addTearDown(tester.view.reset);
    final gateway = _Gateway(
      accuracy: _accuracyJson(),
      status: const BackgroundAnalysisStatus(state: 'running', analysedGames: 40, totalGames: 120),
    );
    await tester.pumpWidget(_app(SettingsScreen(controller: AppController(gateway))));
    await tester.pumpAndSettle();
    await tester.tap(find.text('General'));
    await tester.pumpAndSettle();

    final toggle = find.byKey(const Key('settings-background-analysis'));
    await tester.ensureVisible(toggle);
    expect(tester.widget<SwitchListTile>(toggle).value, isTrue);
    expect(find.textContaining('Analysed 40 of 120 games'), findsOneWidget);

    await tester.tap(toggle);
    await tester.pumpAndSettle();
    expect(gateway.enabledCalls, [false]);
    expect(tester.widget<SwitchListTile>(toggle).value, isFalse);
  });
}
