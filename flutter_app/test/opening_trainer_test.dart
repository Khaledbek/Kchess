// -----------------------------------------------------------------------------
// Section: Opening scenarios dashboard and dynamic drill presentation
// -----------------------------------------------------------------------------

import 'package:flutter/material.dart';
import 'package:flutter_localizations/flutter_localizations.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:kchess/features/analysis/presentation/analysis_move_arrow.dart';
import 'package:kchess/features/training/presentation/opening/opening_scenarios.dart';
import 'package:kchess/features/training/presentation/opening/opening_trainer_panels.dart';
import 'package:kchess/features/training/presentation/opening/opening_trainer_screen.dart';
import 'package:kchess/features/training/presentation/opening_lab_screen.dart';
import 'package:kchess/ffi/core_gateway.dart';
import 'package:kchess/localization/generated/app_localizations.dart';
import 'package:kchess/shared/models/models.dart';
import 'package:kchess/shared/widgets/chess_board_view.dart';

/// Answers practice commands from a script instead of the native core.
///
/// Only the practice channel (and the stats the dashboard reads) is scripted;
/// every other gateway call would be a bug and fails loudly via [noSuchMethod].
class _PracticeGateway implements CoreGateway {
  _PracticeGateway({required this.start, this.move, this.nodes = const []});

  @override
  dynamic noSuchMethod(Invocation invocation) => super.noSuchMethod(invocation);

  final Map<String, Object?> start;
  final Map<String, Object?>? move;
  final List<Map<String, Object?>> nodes;
  final List<Map<String, Object?>> commands = [];

  @override
  Future<Object?> practiceCommand(Map<String, Object?> request) async {
    commands.add(request);
    return switch (request['op']) {
      'cancel' => <String, Object?>{},
      'nodes' => nodes,
      'move' => move,
      _ => start,
    };
  }

  @override
  Future<OpeningsStats> openingsStats({String timeControl = 'all'}) async =>
      const OpeningsStats();
}

const _position = <String, Object?>{
  'fen': '8/8/8/8/8/8/8/8 w - - 0 1',
  'pieces': <String>[
    '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', //
    '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', //
    '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', //
    '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', '', //
  ],
  'sideToMove': 'white',
  'draggableColor': 'white',
  'fullmoveNumber': 2,
};

Map<String, Object?> _drill({
  String status = 'active',
  int depth = 1,
  int targetDepth = 10,
  int attempts = 0,
  bool clean = true,
  bool? accepted,
  Map<String, Object?>? opponentMove,
  Map<String, Object?>? answer,
  String? hint,
  String? hintSan,
  bool bookExhausted = false,
}) => <String, Object?>{
  'session': 'practice-1',
  'kind': 'opening',
  'status': status,
  'solverColor': 'white',
  'position': _position,
  'played': depth,
  'maxMoves': targetDepth,
  'clean': clean,
  'accepted': ?accepted,
  'depth': depth,
  'targetDepth': targetDepth,
  'attempts': attempts,
  'bookExhausted': bookExhausted,
  'bookMoves': 3,
  'openingMoves': <String>['e4'],
  'opponentMove': ?opponentMove,
  'answer': ?answer,
  'hint': ?hint,
  'hintSan': ?hintSan,
};

const _reply = <String, Object?>{
  'uci': 'e7e5',
  'san': 'e5',
  'side': 'black',
  'moveNumber': 1,
  'alternatives': 4,
};

Map<String, Object?> _node({int bestDepth = 6}) => <String, Object?>{
  'id': 11,
  'openingId': 57,
  'name': 'Sicilian Defense',
  'eco': 'B20',
  'childCount': 3,
  'masteryThreshold': 3,
  'targetDepth': 10,
  'moves': <String>['e4', 'c5'],
  'position': _position,
  'progress': <String, Object?>{
    'isMastered': false,
    'successStreak': 0,
    'attemptCount': 4,
    'bestDepth': bestDepth,
  },
};

void _phone(WidgetTester tester) {
  tester.view.physicalSize = const Size(375, 812);
  tester.view.devicePixelRatio = 1;
  addTearDown(tester.view.reset);
}

void main() {
  testWidgets('announces the book reply above the board and the depth below', (
    tester,
  ) async {
    _phone(tester);
    final gateway = _PracticeGateway(start: _drill(opponentMove: _reply));
    await tester.pumpWidget(_localized(_drillScreen(gateway)));
    await tester.pumpAndSettle();

    expect(_headline(tester), 'Opponent played 1... e5.');
    expect(find.text('Find the best engine response.'), findsOneWidget);
    expect(find.text('CURRENT DEPTH: MOVE 2 / 10'), findsOneWidget);

    // Exactly one arrow: the reply, not an answer.
    final arrow = tester.widget<AnalysisMoveArrow>(find.byType(AnalysisMoveArrow));
    expect(arrow.move, 'e7e5');
    expect(find.byKey(const Key('opening-trainer-hint-glow')), findsNothing);

    // Banner, board and meter stack top to bottom and all really render: the
    // board stays square inside the page padding on a 375x812 phone.
    final banner = tester.getRect(find.byKey(const Key('opening-trainer-banner')));
    final frame = tester.getRect(find.byKey(const Key('opening-trainer-board-frame')));
    final meter = tester.getRect(find.byKey(const Key('opening-trainer-progress')));
    expect(banner.height, greaterThan(40));
    expect(frame.width, frame.height);
    expect(frame.width, inInclusiveRange(260, 343));
    expect(meter.height, greaterThan(30));
    expect(banner.bottom, lessThanOrEqualTo(frame.top));
    expect(frame.bottom, lessThanOrEqualTo(meter.top));
    expect(meter.bottom, lessThanOrEqualTo(812));

    // One of ten segments reached: the glowing run spans about a tenth.
    final bar = tester.getSize(find.byType(OpeningSegmentedMeter));
    expect(bar.height, greaterThan(0));
    expect(tester.takeException(), isNull);
  });

  testWidgets('a miss points at the book move and lets the user try again', (
    tester,
  ) async {
    _phone(tester);
    final gateway = _PracticeGateway(
      start: _drill(opponentMove: _reply),
      move: _drill(
        accepted: false,
        attempts: 1,
        clean: false,
        opponentMove: _reply,
        hint: 'g1f3',
        hintSan: 'Nf3',
      ),
    );
    await tester.pumpWidget(_localized(_drillScreen(gateway)));
    await tester.pumpAndSettle();

    tester.widget<ChessBoardView>(find.byType(ChessBoardView)).onPieceDrop('a2', 'a3');
    await tester.pump();
    await tester.pump(const Duration(milliseconds: 50));

    expect(
      gateway.commands.where((command) => command['op'] == 'move').single,
      allOf(containsPair('source', 'a2'), containsPair('target', 'a3')),
    );
    expect(_headline(tester), 'The move was Nf3');
    expect(find.text('Incorrect move. Try again.'), findsOneWidget);

    // The reply arrow gives way to one green arrow for the answer, and the
    // piece to move glows.
    final arrow = tester.widget<AnalysisMoveArrow>(find.byType(AnalysisMoveArrow));
    expect(arrow.move, 'g1f3');
    expect(arrow.color, OpeningStudio.success);
    expect(find.byKey(const Key('opening-trainer-hint-glow')), findsOneWidget);

    // Same position, still the user's move: the board takes another try.
    expect(tester.widget<ChessBoardView>(find.byType(ChessBoardView)).interactive, isTrue);
    expect(find.byKey(const Key('opening-trainer-completed')), findsNothing);
    expect(find.text('CURRENT DEPTH: MOVE 2 / 10'), findsOneWidget);
  });

  testWidgets('a finished drill reports its depth and offers another run', (
    tester,
  ) async {
    _phone(tester);
    final gateway = _PracticeGateway(
      start: _drill(
        status: 'completed',
        depth: 10,
        answer: const {'uci': 'g1f3', 'san': 'Nf3', 'rank': 1},
      ),
    );
    await tester.pumpWidget(_localized(_drillScreen(gateway)));
    await tester.pumpAndSettle();

    expect(_headline(tester), 'Depth 10 reached');
    expect(find.text('CURRENT DEPTH: MOVE 10 / 10'), findsOneWidget);
    expect(find.byKey(const Key('opening-trainer-completed')), findsOneWidget);
    expect(tester.widget<ChessBoardView>(find.byType(ChessBoardView)).interactive, isFalse);

    final again = find.byKey(const Key('opening-trainer-practise-again'));
    await tester.ensureVisible(again);
    await tester.pumpAndSettle();
    await tester.tap(again);
    await tester.pumpAndSettle();

    final ops = gateway.commands.map((command) => command['op']).toList();
    expect(ops, ['start', 'cancel', 'start']);
  });

  testWidgets('the dashboard shows depth mastery and starts the drill on tap', (
    tester,
  ) async {
    _phone(tester);
    final gateway = _PracticeGateway(
      start: _drill(opponentMove: _reply),
      nodes: [_node()],
    );
    await tester.pumpWidget(_localized(OpeningLabScreen(gateway: gateway)));
    await tester.pumpAndSettle();

    expect(find.text('OPENING SCENARIOS'), findsOneWidget);
    expect(find.text('Sicilian Defense'), findsOneWidget);
    expect(find.text('B20  ·  1. e4 c5'), findsOneWidget);
    expect(find.text('DEPTH MASTERY: 6/10 MOVES'), findsOneWidget);
    // No variation graphs on the dashboard — just a way into the variations.
    expect(find.text('3 variations'), findsOneWidget);

    final card = tester.getSize(find.byType(OpeningScenarioCard));
    expect(card.width, greaterThan(300));
    expect(card.height, greaterThan(104));
    final meter = tester.getSize(
      find.descendant(
        of: find.byType(OpeningScenarioCard),
        matching: find.byType(OpeningSegmentedMeter),
      ),
    );
    expect(meter.width, greaterThan(100));
    expect(meter.height, 6);

    await tester.tap(find.text('Sicilian Defense'));
    await tester.pumpAndSettle();
    expect(find.byType(OpeningTrainerScreen), findsOneWidget);
    expect(
      gateway.commands.where((command) => command['op'] == 'start').single,
      allOf(
        containsPair('kind', 'opening'),
        containsPair('id', 57),
        containsPair('color', 'white'),
      ),
    );
  });
}

/// The banner headline as read aloud, without the marks that keep notation
/// left-to-right inside right-to-left text.
String _headline(WidgetTester tester) => tester
    .widget<Text>(find.byKey(const Key('opening-trainer-banner-headline')))
    .textSpan!
    .toPlainText()
    .replaceAll(RegExp('[\u2066\u2069]'), '');

Widget _drillScreen(_PracticeGateway gateway) => OpeningTrainerScreen(
  gateway: gateway,
  title: 'Sicilian Defense',
  request: const {'kind': 'opening', 'id': 57, 'color': 'white'},
);

Widget _localized(Widget home) => MaterialApp(
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
