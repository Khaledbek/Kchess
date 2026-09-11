// -----------------------------------------------------------------------------
// Section: Comparison presentation fixtures
// -----------------------------------------------------------------------------

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:kchess/localization/generated/app_localizations.dart';
import 'package:kchess/shared/models/models.dart';
import 'package:kchess/ui/app_root.dart';
import 'package:kchess/features/app/application/app_controller.dart';

import 'support/fake_core_gateway.dart';

/// The fixture library is "Ada vs Turing", so a profile playing as Ada that
/// scouts "Ada" is exactly the self-comparison case: head-to-head used to match
/// on "is this handle present in the game", which made every game in the library
/// count as a meeting with yourself.
const adaProfile = AppProfile(
  id: 'ada-profile',
  type: ProfileType.chessCom,
  displayName: 'Ada',
  providerUsername: 'Ada',
  avatarAsset: 'provider_chesscom_fallback.png',
);

Widget _localized(Widget child) => MaterialApp(
  locale: const Locale('de'),
  localizationsDelegates: AppLocalizations.localizationsDelegates,
  supportedLocales: AppLocalizations.supportedLocales,
  home: child,
);

Future<void> _openComparisonFor(WidgetTester tester, String handle) async {
  final gateway = FakeCoreGateway(initialProfiles: const [adaProfile]);
  final controller = AppController(gateway);
  await controller.initialize();

  await tester.pumpWidget(_localized(StatisticsScreen(controller: controller)));
  await tester.pumpAndSettle();

  await tester.tap(find.byIcon(Icons.compare_arrows).first);
  await tester.pumpAndSettle();

  await tester.enterText(find.byType(TextField).first, handle);
  await tester.tap(find.widgetWithText(FilledButton, 'Vergleichen'));
  await tester.pumpAndSettle();
}

void main() {
  testWidgets('comparing a profile with itself reports no head-to-head games', (
    tester,
  ) async {
    tester.view.physicalSize = const Size(1400, 2600);
    tester.view.devicePixelRatio = 1.0;
    addTearDown(tester.view.reset);

    await _openComparisonFor(tester, 'Ada');

    // The self-audit is announced rather than shown as a 0-0-0 record.
    expect(find.text('Selbstvergleich (Spiegelung)'), findsOneWidget);
    expect(
      find.textContaining('Keine Partien gegen dich selbst'),
      findsOneWidget,
    );

    // The head-to-head banner must not appear at all: its footer is the only
    // place "direkte Partien" is rendered, and any phantom count would show it.
    expect(find.textContaining('direkte Partien'), findsNothing);
  });

  testWidgets('comparing against a different player is not a self-comparison', (
    tester,
  ) async {
    tester.view.physicalSize = const Size(1400, 2600);
    tester.view.devicePixelRatio = 1.0;
    addTearDown(tester.view.reset);

    await _openComparisonFor(tester, 'Turing');

    expect(find.text('Selbstvergleich (Spiegelung)'), findsNothing);
    expect(
      find.textContaining('Keine Partien gegen dich selbst'),
      findsNothing,
    );
  });
}
