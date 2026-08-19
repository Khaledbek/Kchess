import 'package:flutter/material.dart';
import 'package:flutter_localizations/flutter_localizations.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:kchess/localization/generated/app_localizations.dart';
import 'package:kchess/models/models.dart';
import 'package:kchess/ui/app_root.dart';
import 'package:kchess/view_models/app_controller.dart';

import 'support/fake_core_gateway.dart';

const localProfile = AppProfile(
  id: 'local-profile',
  type: ProfileType.localPgnFen,
  displayName: 'Local',
  avatarAsset: 'profile_unknown.png',
);

const onlineProfile = AppProfile(
  id: 'online-profile',
  type: ProfileType.chessCom,
  displayName: 'Online',
  providerUsername: 'Online',
  avatarAsset: 'provider_chesscom_fallback.png',
);

void main() {
  testWidgets(
    'profile deletion asks for confirmation and selects a replacement',
    (tester) async {
      final gateway = FakeCoreGateway(
        initialProfiles: const [localProfile, onlineProfile],
      );
      final controller = AppController(gateway);
      await controller.initialize();
      await tester.pumpWidget(
        _localized(ProfileScreen(controller: controller)),
      );

      await tester.tap(find.byKey(const Key('delete-active-profile')));
      await tester.pumpAndSettle();
      expect(find.text('Konto löschen?'), findsOneWidget);
      expect(
        find.text(
          'Dieses Profil und seine lokal gespeicherten PGN-/FEN-Daten werden von KChess entfernt.',
        ),
        findsOneWidget,
      );

      await tester.tap(find.text('Abbrechen'));
      await tester.pumpAndSettle();
      expect(gateway.storedProfiles.length, 2);

      await tester.tap(find.byKey(const Key('delete-active-profile')));
      await tester.pumpAndSettle();
      await tester.tap(find.byKey(const Key('confirm-delete-profile')));
      await tester.pumpAndSettle();
      expect(gateway.storedProfiles.map((value) => value.id), [
        'online-profile',
      ]);
      expect(controller.activeProfile?.id, 'online-profile');
      expect(controller.phase, AppPhase.ready);
    },
  );

  testWidgets('deleting the last profile returns to first run', (tester) async {
    final gateway = FakeCoreGateway(initialProfiles: const [localProfile]);
    final controller = AppController(gateway);
    await controller.initialize();

    await controller.deleteProfile(localProfile);

    expect(controller.profiles, isEmpty);
    expect(controller.activeProfile, isNull);
    expect(controller.phase, AppPhase.firstRun);
  });

  testWidgets('engine controls persist values without starting analysis', (
    tester,
  ) async {
    final gateway = FakeCoreGateway(initialProfiles: const [localProfile]);
    final controller = AppController(gateway);
    await controller.initialize();
    await tester.pumpWidget(_localized(SettingsScreen(controller: controller)));

    await tester.tap(
      find.descendant(
        of: find.byKey(const Key('engine-depth')),
        matching: find.byIcon(Icons.add),
      ),
    );
    await tester.pump();
    await tester.tap(
      find.descendant(
        of: find.byKey(const Key('engine-lines')),
        matching: find.byIcon(Icons.remove),
      ),
    );
    await tester.pump();
    await tester.tap(
      find.descendant(
        of: find.byKey(const Key('engine-time-limit')),
        matching: find.byIcon(Icons.add),
      ),
    );
    await tester.pumpAndSettle();

    expect(gateway.currentSettings.depth, 19);
    expect(gateway.currentSettings.multiPv, 2);
    expect(gateway.currentSettings.timeLimitSeconds, 1);
    expect(gateway.engineSettingWrites, 3);
    expect(gateway.startAnalysisCalls, 0);

    await tester.tap(find.byKey(const Key('show-board-arrows')));
    await tester.pumpAndSettle();
    expect(gateway.currentSettings.showBoardArrows, isFalse);
    expect(gateway.startAnalysisCalls, 0);
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
