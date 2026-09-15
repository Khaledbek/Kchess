// -----------------------------------------------------------------------------
// Section: The drill board holds still while moves are played
// -----------------------------------------------------------------------------

import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_localizations/flutter_localizations.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:kchess/features/training/presentation/opening/opening_trainer_screen.dart';
import 'package:kchess/ffi/core_gateway.dart';
import 'package:kchess/localization/generated/app_localizations.dart';
import 'package:kchess/shared/widgets/chess_board_view.dart';

/// Holds every move request open until the test releases it, so the screen
/// can be measured while it waits for native.
class _SlowGateway implements CoreGateway {
  @override
  dynamic noSuchMethod(Invocation invocation) => super.noSuchMethod(invocation);

  Completer<Object?>? pending;
  Map<String, Object?> start = _snapshot();

  @override
  Future<Object?> practiceCommand(Map<String, Object?> request) {
    if (request['op'] == 'move') {
      pending = Completer<Object?>();
      return pending!.future;
    }
    if (request['op'] == 'cancel') return Future.value(<String, Object?>{});
    return Future.value(start);
  }
}

const _pieces = <String>[
  'r', 'n', 'b', 'q', 'k', 'b', 'n', 'r', 'p', 'p', 'p', 'p', '', 'p', 'p', 'p', //
  '', '', '', '', '', '', '', '', '', '', '', '', 'p', '', '', '', //
  '', '', '', '', 'P', '', '', '', '', '', '', '', '', '', '', '', //
  'P', 'P', 'P', 'P', '', 'P', 'P', 'P', 'R', 'N', 'B', 'Q', 'K', 'B', 'N', 'R', //
];

Map<String, Object?> _snapshot({
  int depth = 1,
  bool? accepted,
  int attempts = 0,
  String? hint,
  String? hintSan,
  Map<String, Object?>? answer,
  String replySan = 'e5',
  String status = 'active',
}) => <String, Object?>{
  'session': 'practice-1',
  'status': status,
  'solverColor': 'white',
  'position': const <String, Object?>{
    'fen': 'rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq e6 0 2',
    'pieces': _pieces,
    'sideToMove': 'white',
    'draggableColor': 'white',
  },
  'played': depth,
  'maxMoves': 10,
  'clean': attempts == 0,
  'accepted': ?accepted,
  'depth': depth,
  'targetDepth': 10,
  'attempts': attempts,
  'openingMoves': const <String>['e4'],
  'opponentMove': <String, Object?>{
    'uci': 'e7e5',
    'san': replySan,
    'side': 'black',
    'moveNumber': 1,
  },
  'answer': ?answer,
  'hint': ?hint,
  'hintSan': ?hintSan,
};

Future<void> _pumpDrill(WidgetTester tester, _SlowGateway gateway, Size size) async {
  tester.view.physicalSize = size;
  tester.view.devicePixelRatio = 1;
  addTearDown(tester.view.reset);
  await tester.pumpWidget(
    MaterialApp(
      locale: const Locale('en'),
      supportedLocales: AppLocalizations.supportedLocales,
      localizationsDelegates: const [
        AppLocalizations.delegate,
        GlobalMaterialLocalizations.delegate,
        GlobalWidgetsLocalizations.delegate,
        GlobalCupertinoLocalizations.delegate,
      ],
      home: OpeningTrainerScreen(
        gateway: gateway,
        title: 'Italian Game',
        request: const {'kind': 'opening', 'id': 1, 'color': 'white'},
      ),
    ),
  );
  await tester.pump();
  await tester.pump(const Duration(milliseconds: 50));
}

Rect _board(WidgetTester tester) => tester.getRect(find.byType(ChessBoardView));

Future<void> _drop(WidgetTester tester, String source, String target) async {
  tester.widget<ChessBoardView>(find.byType(ChessBoardView)).onPieceDrop(source, target);
  await tester.pump();
}

/// Lets every animation run to its end without waiting on repeating ones.
Future<void> _settle(WidgetTester tester) async {
  for (var i = 0; i < 12; i++) {
    await tester.pump(const Duration(milliseconds: 100));
  }
}

void main() {
  for (final size in const [Size(375, 812), Size(1280, 800)]) {
    testWidgets('the board neither moves nor resizes across moves at $size', (
      tester,
    ) async {
      final gateway = _SlowGateway();
      await _pumpDrill(tester, gateway, size);
      final before = _board(tester);
      expect(before.width, greaterThan(200));
      expect(before.width, before.height);

      // Waiting for native.
      await _drop(tester, 'g1', 'f3');
      await tester.pump(const Duration(milliseconds: 16));
      expect(_board(tester), before, reason: 'while the move is being judged');

      // Accepted, and the book replied with a longer move.
      gateway.pending!.complete(
        _snapshot(
          depth: 2,
          accepted: true,
          replySan: 'Nc6',
          answer: const {'uci': 'g1f3', 'san': 'Nf3', 'rank': 1},
        ),
      );
      await tester.pump();
      expect(_board(tester), before, reason: 'right after an accepted move');
      await _settle(tester);
      expect(_board(tester), before, reason: 'after the accepted move settled');

      // A miss reveals the answer.
      await _drop(tester, 'a2', 'a3');
      gateway.pending!.complete(
        _snapshot(
          depth: 2,
          accepted: false,
          attempts: 1,
          hint: 'f1c4',
          hintSan: 'Bc4',
        ),
      );
      await tester.pump();
      expect(_board(tester), before, reason: 'right after a miss');
      await _settle(tester);
      expect(_board(tester), before, reason: 'after the miss settled');
      expect(tester.takeException(), isNull);
    });
  }

  testWidgets('nothing on the drill screen keeps animating on its own', (tester) async {
    final gateway = _SlowGateway()
      ..start = _snapshot(attempts: 1, hint: 'f1c4', hintSan: 'Bc4');
    await _pumpDrill(tester, gateway, const Size(375, 812));
    // A pulsing hint or a looping glow would never let the frame settle.
    await tester.pumpAndSettle(const Duration(milliseconds: 100), EnginePhase.sendSemanticsUpdate, const Duration(seconds: 3));
  });
}
