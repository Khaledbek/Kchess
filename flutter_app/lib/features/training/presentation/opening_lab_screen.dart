// -----------------------------------------------------------------------------
// Section: Opening Lab — the opening scenarios dashboard
// -----------------------------------------------------------------------------

import 'dart:async';

import 'package:flutter/material.dart';

import '../../../ffi/core_gateway.dart';
import '../../../localization/generated/app_localizations.dart';
import '../../../shared/models/models.dart';
import '../models/opening_training_request.dart';
import 'opening/opening_scenarios.dart';
import 'opening/opening_trainer_panels.dart';
import 'opening/opening_trainer_screen.dart';

class OpeningLabScreen extends StatefulWidget {
  const OpeningLabScreen({required this.gateway, this.request, super.key});
  final CoreGateway gateway;
  final OpeningTrainingRequest? request;
  @override
  State<OpeningLabScreen> createState() => _OpeningLabScreenState();
}

class _OpeningLabScreenState extends State<OpeningLabScreen> {
  String _color = 'white';
  OpeningsStats? _stats;

  @override
  void initState() {
    super.initState();
    if (widget.request?.color == 'black') _color = 'black';
    WidgetsBinding.instance.addPostFrameCallback((_) {
      unawaited(_openRequest());
      unawaited(_loadStats());
    });
  }

  /// The statistics only decide which scenario to call out; the dashboard
  /// works without them.
  Future<void> _loadStats() async {
    try {
      final stats = await widget.gateway.openingsStats();
      if (mounted) setState(() => _stats = stats);
    } catch (_) {}
  }

  /// Deep link from the statistics "Train" button: straight into the drill.
  Future<void> _openRequest() async {
    final request = widget.request;
    if (request == null) return;
    try {
      final match = (await widget.gateway.practiceCommand({
        'op': 'match',
        'eco': request.eco,
        'name': request.openingName,
      }))! as Map<String, Object?>;
      final id = match['id']! as int;
      if (!mounted) return;
      if (id <= 0) {
        _showError();
        return;
      }
      await Navigator.of(context).push(
        MaterialPageRoute<void>(
          builder: (_) => OpeningTrainerScreen(
            gateway: widget.gateway,
            title: request.openingName,
            request: {'kind': 'opening', 'id': id, 'color': _color},
          ),
        ),
      );
    } catch (_) {
      if (mounted) _showError();
    }
  }

  void _showError() {
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(
        content: Text(AppLocalizations.of(context).trainingOpeningTreeLoadFailed),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final nemesis = _stats?.nemesis;
    return OpeningScenarioPage(
      title: strings.trainingOpeningScenariosTitle,
      body: OpeningScenarioList(
        // A new colour is a different drill, so the list starts fresh with it.
        key: ValueKey('opening-scenarios-$_color'),
        gateway: widget.gateway,
        parent: 0,
        color: _color,
        emphasis: nemesis == null
            ? null
            : (
                name: nemesis.familyName,
                label: strings.trainingNemesisBadge(
                  nemesis.familyName,
                  '${((nemesis.tally.winRate ?? 0) * 100).round()}%',
                ),
              ),
        header: Padding(
          padding: const EdgeInsets.only(bottom: 16),
          child: Column(
            children: [
              Text(
                strings.trainingOpeningScenariosCaption,
                textAlign: TextAlign.center,
                style: const TextStyle(
                  fontSize: 14,
                  height: 1.35,
                  color: OpeningStudio.textMuted,
                ),
              ),
              const SizedBox(height: 12),
              Row(
                mainAxisAlignment: MainAxisAlignment.center,
                children: [
                  Text(
                    strings.trainingOpeningPlayAs,
                    style: const TextStyle(
                      fontSize: 13,
                      fontWeight: FontWeight.w600,
                      color: OpeningStudio.textMuted,
                    ),
                  ),
                  const SizedBox(width: 10),
                  SegmentedButton<String>(
                    key: const Key('opening-scenarios-color'),
                    showSelectedIcon: false,
                    style: SegmentedButton.styleFrom(
                      visualDensity: VisualDensity.compact,
                      foregroundColor: OpeningStudio.textMuted,
                      selectedForegroundColor: OpeningStudio.backdrop,
                      selectedBackgroundColor: OpeningStudio.accent,
                      side: const BorderSide(color: OpeningStudio.hairline),
                    ),
                    segments: [
                      ButtonSegment(
                        value: 'white',
                        label: Text(strings.statsCompareColorWhite),
                      ),
                      ButtonSegment(
                        value: 'black',
                        label: Text(strings.statsCompareColorBlack),
                      ),
                    ],
                    selected: {_color},
                    onSelectionChanged: (selection) =>
                        setState(() => _color = selection.first),
                  ),
                ],
              ),
            ],
          ),
        ),
      ),
    );
  }
}
