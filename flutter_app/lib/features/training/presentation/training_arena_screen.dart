// -----------------------------------------------------------------------------
// Section: Training hub composition
// -----------------------------------------------------------------------------

import 'package:flutter/material.dart';

import '../../../localization/generated/app_localizations.dart';
import '../../../shared/models/models.dart';
import '../../app/application/app_controller.dart';
import '../models/opening_training_request.dart';
import 'blunder_buster_screen.dart';
import 'endgame_academy_screen.dart';
import 'opening_lab_screen.dart';
import 'training_dashboard_cards.dart';

class TrainingArenaScreen extends StatefulWidget {
  const TrainingArenaScreen({
    required this.controller,
    this.openingRequest,
    super.key,
  });

  final AppController controller;
  final OpeningTrainingRequest? openingRequest;

  @override
  State<TrainingArenaScreen> createState() => _TrainingArenaScreenState();
}

class _TrainingArenaScreenState extends State<TrainingArenaScreen> {
  late Future<TrainingOverview> _overview;
  late Future<OpeningsStats> _openings;

  @override
  void initState() {
    super.initState();
    _reload();
  }

  @override
  void didUpdateWidget(covariant TrainingArenaScreen oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.controller != widget.controller) _reload();
  }

  void _reload() {
    _overview = widget.controller.gateway.trainingOverview();
    _openings = widget.controller.gateway.openingsStats();
  }

  Future<void> _openEndgames() async {
    await Navigator.of(context).push(
      MaterialPageRoute<void>(
        builder: (_) =>
            EndgameAcademyScreen(gateway: widget.controller.gateway),
      ),
    );
    if (mounted) setState(_reload);
  }

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final theme = Theme.of(context);
    return Scaffold(
      appBar: MediaQuery.sizeOf(context).width >= 900
          ? AppBar(title: Text(strings.trainingSection))
          : null,
      body: FutureBuilder<TrainingOverview>(
        future: _overview,
        builder: (context, snapshot) => ListView(
          padding: const EdgeInsets.fromLTRB(16, 12, 16, 28),
          children: [
            Center(
              child: ConstrainedBox(
                constraints: const BoxConstraints(maxWidth: 1100),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(
                      strings.trainingIntroTitle,
                      style: theme.textTheme.headlineSmall?.copyWith(
                        fontWeight: FontWeight.w800,
                      ),
                    ),
                    const SizedBox(height: 20),
                    TrainingDashboardCards(
                      overview: snapshot.data,
                      openings: _openings,
                      loading: snapshot.connectionState != ConnectionState.done,
                      openingRequest: widget.openingRequest,
                      onOpenOpeningLab: () => Navigator.of(context).push(
                        MaterialPageRoute<void>(
                          builder: (_) => OpeningLabScreen(
                            gateway: widget.controller.gateway,
                            request: widget.openingRequest,
                          ),
                        ),
                      ),
                      onOpenTactics: () => Navigator.of(context).push(
                        MaterialPageRoute<void>(
                          builder: (_) =>
                              BlunderBusterScreen(overview: snapshot.data),
                        ),
                      ),
                      onOpenEndgames: _openEndgames,
                    ),
                  ],
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}
