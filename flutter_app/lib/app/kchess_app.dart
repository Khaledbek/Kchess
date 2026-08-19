import 'package:flutter/material.dart';
import 'package:flutter_localizations/flutter_localizations.dart';

import '../localization/generated/app_localizations.dart';
import '../shared/models/models.dart';
import '../shared/theme/app_theme.dart';
import '../ui/app_root.dart';
import '../features/app/application/app_controller.dart';

class KChessApp extends StatefulWidget {
  const KChessApp({required this.controller, super.key});

  final AppController controller;

  @override
  State<KChessApp> createState() => _KChessAppState();
}

class _KChessAppState extends State<KChessApp> {
  @override
  void initState() {
    super.initState();
    widget.controller.addListener(_refresh);
    widget.controller.initialize();
  }

  @override
  void didUpdateWidget(covariant KChessApp oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.controller != widget.controller) {
      oldWidget.controller.removeListener(_refresh);
      widget.controller.addListener(_refresh);
    }
  }

  @override
  void dispose() {
    widget.controller.removeListener(_refresh);
    widget.controller.dispose();
    super.dispose();
  }

  void _refresh() => setState(() {});

  @override
  Widget build(BuildContext context) => MaterialApp(
    debugShowCheckedModeBanner: false,
    onGenerateTitle: (context) => AppLocalizations.of(context).appTitle,
    locale: Locale(widget.controller.settings.locale),
    supportedLocales: AppLocalizations.supportedLocales,
    localizationsDelegates: const [
      AppLocalizations.delegate,
      GlobalMaterialLocalizations.delegate,
      GlobalWidgetsLocalizations.delegate,
      GlobalCupertinoLocalizations.delegate,
    ],
    theme: AppTheme.light(),
    darkTheme: AppTheme.dark(),
    themeMode: switch (widget.controller.settings.themeMode) {
      AppThemeMode.system => ThemeMode.system,
      AppThemeMode.light => ThemeMode.light,
      AppThemeMode.dark => ThemeMode.dark,
    },
    home: AppRoot(controller: widget.controller),
  );
}
