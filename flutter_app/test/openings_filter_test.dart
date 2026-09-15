import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:kchess/localization/generated/app_localizations.dart';
import 'package:kchess/ui/app_root.dart';
import 'package:kchess/features/app/application/app_controller.dart';

import 'support/fake_core_gateway.dart';

/// The openings card used to stay all-time while every other card followed the
/// time-control filter, because openings_json took no filter argument. The fake
/// gateway models a blitz-only library, so Rapid must come back empty.
void main() {
  testWidgets('openings follow the time-control filter', (tester) async {
    tester.view.physicalSize = const Size(1500, 2800);
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

    // Unfiltered: the classified families are listed.
    expect(find.text('Ruy Lopez'), findsOneWidget);

    // Rapid holds none of the fixture games, so the card must say so rather
    // than keep showing the all-time repertoire.
    await tester.tap(find.text('Rapid'));
    await tester.pumpAndSettle();
    expect(find.text('Ruy Lopez'), findsNothing);
    expect(
      find.text('Keine Partien für die gewählte Zeitkontrolle.'),
      findsWidgets,
    );

    // Blitz does, so they come back.
    await tester.tap(find.text('Blitz'));
    await tester.pumpAndSettle();
    expect(find.text('Ruy Lopez'), findsOneWidget);
  });
}
