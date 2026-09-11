// -----------------------------------------------------------------------------
// Section: Statistics bar presentation fixtures
// -----------------------------------------------------------------------------

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:kchess/localization/generated/app_localizations.dart';
import 'package:kchess/ui/app_root.dart';
import 'package:kchess/features/app/application/app_controller.dart';

import 'support/fake_core_gateway.dart';

/// Regression guard for a whole class of silent layout bug on the statistics
/// tab: every proportion bar is built from childless [ColoredBox] segments, and
/// a childless ColoredBox is a proxy box that takes `constraints.smallest`. Under
/// a Row's default (loose) cross-axis constraints that is zero height, so the
/// bars vanish while still reserving their space — visible only by eye, never by
/// the analyzer.
void main() {
  testWidgets('statistics proportion bars have non-zero size', (tester) async {
    tester.view.physicalSize = const Size(1400, 2400);
    tester.view.devicePixelRatio = 1.0;
    addTearDown(tester.view.reset);

    final gateway = FakeCoreGateway();
    final controller = AppController(gateway);
    await controller.initialize();

    await tester.pumpWidget(
      MaterialApp(
        locale: const Locale('de'),
        localizationsDelegates: AppLocalizations.localizationsDelegates,
        supportedLocales: AppLocalizations.supportedLocales,
        home: StatisticsScreen(controller: controller),
      ),
    );
    await tester.pumpAndSettle();

    final segments = tester
        .renderObjectList<RenderBox>(
          find.descendant(
            of: find.byType(StatisticsScreen),
            matching: find.byType(ColoredBox),
          ),
        )
        .toList();

    // The overview, phase and termination cards all draw bars with this data.
    expect(
      segments.length,
      greaterThan(4),
      reason: 'expected the statistics cards to render several bar segments',
    );
    for (final segment in segments) {
      expect(
        segment.size.height,
        greaterThan(0),
        reason: 'a bar segment collapsed to zero height',
      );
      expect(
        segment.size.width,
        greaterThan(0),
        reason: 'a bar segment collapsed to zero width',
      );
    }
  });
}
