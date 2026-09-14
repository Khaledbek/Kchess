import 'package:flutter/material.dart';
import 'package:flutter/scheduler.dart';
import 'package:flutter_localizations/flutter_localizations.dart';

import '../diagnostics/app_startup_diagnostics.dart';
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
  bool _readyFrameScheduled = false;

  @override
  void initState() {
    super.initState();
    final startup = AppStartupDiagnostics.instance;
    startup.mark('appInitStateMs');
    SchedulerBinding.instance.addTimingsCallback(_recordFrameTimings);
    WidgetsBinding.instance.addPostFrameCallback((_) {
      startup.mark('firstFrameMs');
    });
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
    SchedulerBinding.instance.removeTimingsCallback(_recordFrameTimings);
    widget.controller.removeListener(_refresh);
    widget.controller.dispose();
    super.dispose();
  }

  void _recordFrameTimings(List<FrameTiming> timings) {
    final startup = AppStartupDiagnostics.instance;
    startup.recordFrameTimings(timings);
    if (startup.frameSamplingComplete) {
      SchedulerBinding.instance.removeTimingsCallback(_recordFrameTimings);
    }
  }

  void _refresh() {
    setState(() {});
    if (widget.controller.phase != AppPhase.ready || _readyFrameScheduled) {
      return;
    }
    _readyFrameScheduled = true;
    WidgetsBinding.instance.addPostFrameCallback((_) {
      AppStartupDiagnostics.instance.mark('firstReadyFrameMs');
    });
  }

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
    // Keep the app layout left-to-right for every locale. Arabic changes the
    // displayed language only; it must not mirror the board or navigation.
    builder: (context, child) => Directionality(
      textDirection: TextDirection.ltr,
      child: child ?? const SizedBox.shrink(),
    ),
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
