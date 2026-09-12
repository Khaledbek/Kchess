// -----------------------------------------------------------------------------
// Section: Application shell and feature composition
// -----------------------------------------------------------------------------

import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:file_picker/file_picker.dart';
import 'package:fl_chart/fl_chart.dart';
import 'package:flutter/material.dart';
import 'package:flutter_svg/flutter_svg.dart';

import '../ffi/core_gateway.dart';
import '../localization/generated/app_localizations.dart';
import '../shared/models/models.dart';
import '../shared/theme/app_theme.dart';
import 'shared/board_endgame_presentation.dart';
import 'shared/promotion_dialog.dart';
import '../features/app/application/app_controller.dart';
import '../features/analysis/presentation/analysis_screen.dart';
import '../features/training/models/opening_training_request.dart';
import '../features/training/presentation/opening/opening_weakness_tile.dart';
import '../features/training/presentation/training_arena_screen.dart';
import '../features/training/presentation/training_navigation.dart';

part 'shared/brand_widgets.dart';
part 'startup/splash_error_screens.dart';
part 'startup/first_run_screen.dart';
part '../features/profile/presentation/profile_support.dart';
part '../features/games/presentation/games_screen.dart';
part '../features/games/presentation/games_filters.dart';
part '../features/games/presentation/games_empty_state.dart';
part '../features/favorites/presentation/favorites_screen.dart';
part '../features/favorites/presentation/favorite_collection_screen.dart';
part '../features/favorites/presentation/favorite_dialogs.dart';
part '../features/games/presentation/import_dialogs.dart';
part '../features/profile/presentation/profile_screen.dart';
part '../features/statistics/presentation/statistics_screen.dart';
part '../features/statistics/presentation/stats_widgets.dart';
part '../features/statistics/presentation/overview_section.dart';
part '../features/statistics/presentation/form_section.dart';
part '../features/statistics/presentation/rating_section.dart';
part '../features/statistics/presentation/termination_section.dart';
part '../features/statistics/presentation/phase_section.dart';
part '../features/statistics/presentation/openings_section.dart';
part '../features/statistics/presentation/opening_games_sheet.dart';
part '../features/statistics/presentation/player_comparison.dart';
part '../features/settings/presentation/settings_screen.dart';
part '../features/settings/presentation/engine_settings_page.dart';
part '../features/settings/presentation/analysis_settings_page.dart';
part '../features/settings/presentation/design_settings_page.dart';
part '../features/settings/presentation/general_settings_page.dart';
part '../features/settings/presentation/data_storage_settings_page.dart';
part '../features/settings/presentation/setting_controls.dart';
part '../features/play/presentation/play_screen.dart';
part '../features/play/presentation/bot_game_log_screen.dart';
part '../features/play/presentation/bot_game_setup_screen.dart';
part '../features/play/presentation/bot_game_screen.dart';
part 'shared/game_widgets.dart';
part '../features/settings/presentation/settings_section.dart';

class AppRoot extends StatelessWidget {
  const AppRoot({required this.controller, super.key});

  final AppController controller;

  @override
  Widget build(BuildContext context) => switch (controller.phase) {
    AppPhase.loading => const _SplashScreen(),
    AppPhase.firstRun => FirstRunScreen(controller: controller),
    AppPhase.ready => HomeShell(controller: controller),
    AppPhase.error => _ErrorScreen(controller: controller),
  };
}

class HomeShell extends StatefulWidget {
  const HomeShell({required this.controller, super.key});

  final AppController controller;

  @override
  State<HomeShell> createState() => _HomeShellState();
}

class _HomeShellState extends State<HomeShell> {
  static const _trainingIndex = 2;

  int _selectedIndex = 0;
  OpeningTrainingRequest? _openingRequest;

  void _select(int index) {
    setState(() {
      if (index != _trainingIndex) _openingRequest = null;
      _selectedIndex = index;
    });
  }

  void _openTraining({OpeningTrainingRequest? opening}) {
    setState(() {
      _openingRequest = opening;
      _selectedIndex = _trainingIndex;
    });
  }

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final destinations = [
      _Destination(
        strings.gameSection,
        Icons.view_list_outlined,
        Icons.view_list,
      ),
      _Destination(
        strings.play,
        Icons.sports_esports_outlined,
        Icons.sports_esports,
      ),
      _Destination(
        strings.trainingSection,
        Icons.school_outlined,
        Icons.school_rounded,
      ),
      _Destination(strings.favorites, Icons.favorite_border, Icons.favorite),
      _Destination(
        _statisticsText(context).title,
        Icons.insights_outlined,
        Icons.insights,
      ),
      _Destination(strings.settings, Icons.settings_outlined, Icons.settings),
    ];
    final content = switch (_selectedIndex) {
      0 => GamesScreen(controller: widget.controller),
      1 => PlayScreen(gateway: widget.controller.gateway),
      2 => TrainingArenaScreen(
        controller: widget.controller,
        openingRequest: _openingRequest,
      ),
      3 => FavoritesScreen(controller: widget.controller),
      4 => StatisticsScreen(controller: widget.controller),
      5 => SettingsScreen(controller: widget.controller),
      _ => _EmptySection(title: destinations[_selectedIndex].label),
    };

    void openProfile() {
      Navigator.of(context).push(
        MaterialPageRoute<void>(
          builder: (_) => ProfileScreen(controller: widget.controller),
        ),
      );
    }

    return TrainingNavigator(
      openTraining: _openTraining,
      child: LayoutBuilder(
        builder: (context, constraints) {
        if (constraints.maxWidth >= 900) {
          return Scaffold(
            body: Row(
              children: [
                SafeArea(
                  child: Container(
                    width: 264,
                    color: Theme.of(context).colorScheme.surfaceContainerLow,
                    child: Column(
                      children: [
                        _ProfileHeader(
                          controller: widget.controller,
                          onOpenProfile: openProfile,
                        ),
                        const SizedBox(height: 4),
                        Expanded(
                          child: NavigationRail(
                            backgroundColor: Colors.transparent,
                            extended: true,
                            selectedIndex: _selectedIndex,
                            onDestinationSelected: _select,
                            destinations: [
                              for (final destination in destinations)
                                NavigationRailDestination(
                                  icon: Icon(destination.icon),
                                  selectedIcon: Icon(destination.selectedIcon),
                                  label: Text(destination.label),
                                ),
                            ],
                          ),
                        ),
                      ],
                    ),
                  ),
                ),
                const VerticalDivider(width: 1),
                Expanded(child: content),
              ],
            ),
          );
        }
        return Scaffold(
          appBar: AppBar(title: Text(destinations[_selectedIndex].label)),
          drawer: Drawer(
            child: SafeArea(
              child: Column(
                children: [
                  _ProfileHeader(
                    controller: widget.controller,
                    onOpenProfile: () {
                      Navigator.pop(context);
                      openProfile();
                    },
                  ),
                  Expanded(
                    child: ListView.builder(
                      itemCount: destinations.length,
                      itemBuilder: (context, index) => ListTile(
                        selected: index == _selectedIndex,
                        leading: Icon(destinations[index].icon),
                        title: Text(destinations[index].label),
                        onTap: () {
                          _select(index);
                          Navigator.pop(context);
                        },
                      ),
                    ),
                  ),
                ],
              ),
            ),
          ),
          body: content,
        );
        },
      ),
    );
  }
}

class _Destination {
  const _Destination(this.label, this.icon, this.selectedIcon);
  final String label;
  final IconData icon;
  final IconData selectedIcon;
}
