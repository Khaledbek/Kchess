// -----------------------------------------------------------------------------
// Section: Opening weaknesses from statistics into training
// -----------------------------------------------------------------------------

import 'package:flutter/material.dart';
import 'package:flutter_localizations/flutter_localizations.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:kchess/features/app/application/app_controller.dart';
import 'package:kchess/features/training/models/opening_training_request.dart';
import 'package:kchess/features/training/presentation/opening/opening_trainer_screen.dart';
import 'package:kchess/features/training/presentation/opening/opening_weakness_tile.dart';
import 'package:kchess/features/training/presentation/opening_lab_screen.dart';
import 'package:kchess/features/training/presentation/training_navigation.dart';
import 'package:kchess/ffi/core_gateway.dart';
import 'package:kchess/localization/generated/app_localizations.dart';
import 'package:kchess/shared/models/models.dart';
import 'package:kchess/ui/app_root.dart';

const _friedLiver = 'Italian Game: Two Knights Defense, Fried Liver Attack';

/// Opening statistics as native reports them: the Fried Liver lost 5 of 6 as
/// Black, with the same 5... Nxd5 flagged in 3 analysed games.
Map<String, Object?> _openingsJson() => <String, Object?>{
  'hasProfile': true,
  'gamesWithOpening': 40,
  'analysedOpeningGames': 3,
  'families': <Object?>[],
  'weaknesses': <Object?>[
    <String, Object?>{
      'level': 'variation',
      'name': _friedLiver,
      'family': 'Italian Game',
      'eco': 'C57',
      'color': 'black',
      'games': 6,
      'wins': 1,
      'draws': 0,
      'losses': 5,
      'winRate': 1 / 6,
      'analysedGames': 3,
      'gamesWithOpeningErrors': 3,
      'openingErrors': 4,
      'openingBlunders': 2,
      'poorResults': true,
      'frequentErrors': true,
      'severity': 1.1,
      'recurringMistake': <String, Object?>{
        'fen': 'r1bqkb1r/ppp2ppp/2n2n2/3Pp1N1/2B5/8/PPPP1PPP/RNBQK2R b KQkq -',
        'san': 'Nxd5',
        'recommended': 'Na5',
        'category': 'blunder',
        'moveNumber': 5,
        'side': 'black',
        'count': 3,
      },
    },
    <String, Object?>{
      'level': 'family',
      'name': 'Sicilian Defense',
      'family': 'Sicilian Defense',
      'eco': 'B20',
      'color': 'white',
      'games': 8,
      'wins': 2,
      'draws': 0,
      'losses': 6,
      'analysedGames': 0,
      'poorResults': true,
      'recurringMistake': null,
    },
  ],
};

class _Gateway implements CoreGateway {
  _Gateway({this.families = const []});

  @override
  dynamic noSuchMethod(Invocation invocation) => super.noSuchMethod(invocation);

  final List<Map<String, Object?>> families;
  final List<Map<String, Object?>> commands = [];

  @override
  Future<OpeningsStats> openingsStats({String timeControl = 'all'}) async =>
      OpeningsStats.fromJson(_openingsJson());

  // The other statistics cards are not under test; they show their error state.
  @override
  Future<StatisticsOverview> statisticsOverview() => Future.error('not scripted');
  @override
  Future<TerminationStats> terminationStats() => Future.error('not scripted');
  @override
  Future<PhaseStats> phaseStats() => Future.error('not scripted');
  @override
  Future<StatisticsTimeline> statisticsTimeline(GameQuery query) =>
      Future.error('not scripted');

  @override
  Future<Object?> practiceCommand(Map<String, Object?> request) async {
    commands.add(request);
    return switch (request['op']) {
      'families' => families,
      'match' => <String, Object?>{'id': 2587},
      'cancel' => <String, Object?>{},
      _ => _drillStart,
    };
  }
}

const _position = <String, Object?>{
  'fen': '8/8/8/8/8/8/8/8 b - - 0 5',
  'pieces': <String>[
    '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', //
    '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', //
    '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', //
    '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', //
  ],
  'sideToMove': 'black',
  'draggableColor': 'black',
};

const _drillStart = <String, Object?>{
  'session': 'practice-1',
  'status': 'active',
  'solverColor': 'black',
  'position': _position,
  'played': 0,
  'maxMoves': 10,
  'clean': true,
  'depth': 0,
  'targetDepth': 10,
  'openingMoves': <String>['e4', 'e5', 'Nf3', 'Nc6', 'Bc4', 'Nf6', 'Ng5', 'd5', 'exd5'],
};

Map<String, Object?> _family(String name, int id) => <String, Object?>{
  'id': id,
  'openingId': id,
  'name': name,
  'childCount': 4,
  'masteryThreshold': 3,
  'position': _position,
  'progress': <String, Object?>{'isMastered': false, 'bestDepth': 0},
};

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

void main() {
  test('native weaknesses parse with their evidence', () {
    final stats = OpeningsStats.fromJson(_openingsJson());
    expect(stats.weaknesses, hasLength(2));
    final friedLiver = stats.weaknesses.first;
    expect(friedLiver.name, _friedLiver);
    expect(friedLiver.color, 'black');
    expect(friedLiver.tally.losses, 5);
    expect(friedLiver.recurringMistake!.notation('Nxd5'), '5... Nxd5');
    expect(stats.weaknesses.last.recurringMistake, isNull);
    expect(stats.analysedOpeningGames, 3);
    // Older native builds send neither field.
    expect(OpeningsStats.fromJson(const {'hasProfile': true}).weaknesses, isEmpty);
  });

  testWidgets('the tile leads with the move the user keeps getting wrong', (
    tester,
  ) async {
    final stats = OpeningsStats.fromJson(_openingsJson());
    await tester.pumpWidget(
      _app(
        Builder(
          builder: (context) {
            final strings = AppLocalizations.of(context);
            expect(openingWeaknessReasons(strings, stats.weaknesses.first), [
              'You played 5... Nxd5 here 3 times — the engine prefers 5... Na5.',
              'Opening mistakes in 3 of 3 analysed games',
              'Lost 5 of 6 games',
            ]);
            // Results alone still explain themselves.
            expect(openingWeaknessReasons(strings, stats.weaknesses.last), [
              'Lost 6 of 8 games',
            ]);
            return const SizedBox();
          },
        ),
      ),
    );
  });

  testWidgets('statistics warns about the weak line and trains it', (tester) async {
    tester.view.physicalSize = const Size(900, 1600);
    tester.view.devicePixelRatio = 1;
    addTearDown(tester.view.reset);

    OpeningTrainingRequest? requested;
    final gateway = _Gateway();
    await tester.pumpWidget(
      _app(
        TrainingNavigator(
          openTraining: ({opening}) => requested = opening,
          child: StatisticsScreen(controller: AppController(gateway)),
        ),
      ),
    );
    await tester.pumpAndSettle();

    final card = find.byKey(const Key('stats-opening-weaknesses'));
    expect(card, findsOneWidget);
    expect(find.text('Openings that need training'), findsOneWidget);
    // The warning sits above every other statistics card.
    final cardTop = tester.getTopLeft(card).dy;
    for (final other in find.byType(Card).evaluate()) {
      final box = other.renderObject! as RenderBox;
      if (other.widget.key == const Key('stats-opening-weaknesses')) continue;
      expect(box.localToGlobal(Offset.zero).dy, greaterThan(cardTop));
    }
    expect(
      find.descendant(
        of: card,
        matching: find.text(
          'You played 5... Nxd5 here 3 times — the engine prefers 5... Na5.',
        ),
      ),
      findsOneWidget,
    );
    expect(tester.getSize(card).height, greaterThan(150));

    await tester.tap(
      find.byKey(const ValueKey('opening-weakness-train-$_friedLiver-black')),
    );
    await tester.pump();
    expect(requested?.openingName, _friedLiver);
    expect(requested?.eco, 'C57');
    expect(requested?.color, 'black');
  });

  testWidgets('the training dashboard lists weak spots and drills them', (
    tester,
  ) async {
    tester.view.physicalSize = const Size(375, 1400);
    tester.view.devicePixelRatio = 1;
    addTearDown(tester.view.reset);

    final gateway = _Gateway(
      families: [_family('Italian Game', 1), _family('Sicilian Defense', 2)],
    );
    await tester.pumpWidget(_app(OpeningLabScreen(gateway: gateway)));
    await tester.pumpAndSettle();

    expect(gateway.commands.first, containsPair('op', 'families'));
    expect(find.byKey(const Key('opening-weak-spots')), findsOneWidget);
    expect(find.text(_friedLiver), findsOneWidget);

    // Badges follow the colour toggle: White shows the Sicilian, not the
    // Italian Game that was lost with Black.
    final badges = find.byKey(const Key('opening-scenario-weak-spot'));
    expect(badges, findsOneWidget);
    expect(
      find.ancestor(of: badges, matching: find.byKey(const ValueKey('opening-scenario-2'))),
      findsOneWidget,
    );

    await tester.tap(
      find.byKey(const ValueKey('opening-weakness-train-$_friedLiver-black')),
    );
    await tester.pumpAndSettle();

    expect(
      gateway.commands.firstWhere((command) => command['op'] == 'match'),
      allOf(containsPair('name', _friedLiver), containsPair('eco', 'C57')),
    );
    // Drilled with the side it was lost with, whatever the toggle says.
    expect(
      gateway.commands.firstWhere((command) => command['op'] == 'start'),
      allOf(containsPair('id', 2587), containsPair('color', 'black')),
    );
    expect(find.byType(OpeningTrainerScreen), findsOneWidget);
  });
}
