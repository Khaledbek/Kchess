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
import 'opening/opening_weakness_tile.dart';

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

  /// The statistics only decide which lines to warn about; the dashboard works
  /// without them.
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
    await _drillLine(name: request.openingName, eco: request.eco, color: _color);
  }

  /// Resolves a named line against the catalogue natively and drills it with
  /// the side the user plays it with.
  Future<void> _drillLine({
    required String name,
    required String eco,
    required String color,
  }) async {
    try {
      final match = (await widget.gateway.practiceCommand({
        'op': 'match',
        'eco': eco,
        'name': name,
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
            title: name,
            request: {
              'kind': 'opening',
              'id': id,
              'color': color == 'black' ? 'black' : 'white',
            },
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
    final weaknesses = _stats?.weaknesses ?? const <OpeningWeakness>[];
    return OpeningScenarioPage(
      title: strings.trainingOpeningScenariosTitle,
      body: OpeningScenarioList(
        // A new colour is a different drill, so the list starts fresh with it.
        key: ValueKey('opening-scenarios-$_color'),
        gateway: widget.gateway,
        parent: null,
        color: _color,
        weakFamilies: {
          for (final weakness in weaknesses)
            if (weakness.color == _color) weakness.family.toLowerCase(),
        },
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
              if (weaknesses.isNotEmpty) ...[
                const SizedBox(height: 20),
                _WeakSpots(
                  weaknesses: weaknesses,
                  onDrill: (weakness) => _drillLine(
                    name: weakness.name,
                    eco: weakness.eco,
                    color: weakness.color,
                  ),
                ),
              ],
            ],
          ),
        ),
      ),
    );
  }
}

/// The lines the statistics flagged, above the scenario cards: whatever the
/// colour toggle says, each drills with the side it was lost with.
class _WeakSpots extends StatelessWidget {
  const _WeakSpots({required this.weaknesses, required this.onDrill});

  static const _maxRows = 3;

  final List<OpeningWeakness> weaknesses;
  final ValueChanged<OpeningWeakness> onDrill;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    return Column(
      key: const Key('opening-weak-spots'),
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        Row(
          children: [
            const Icon(Icons.healing_rounded, size: 18, color: OpeningStudio.danger),
            const SizedBox(width: 8),
            Text(
              strings.trainingWeakSpotsTitle.toUpperCase(),
              style: const TextStyle(
                fontSize: 12.5,
                fontWeight: FontWeight.w800,
                letterSpacing: 0.9,
                color: OpeningStudio.textPrimary,
              ),
            ),
          ],
        ),
        const SizedBox(height: 10),
        for (final weakness in weaknesses.take(_maxRows)) ...[
          OpeningWeaknessTile(
            weakness: weakness,
            colorLabel: weakness.color == 'white'
                ? strings.statsCompareColorWhite
                : strings.statsCompareColorBlack,
            trainLabel: strings.statsTrainOpening,
            onTrain: () => onDrill(weakness),
          ),
          const SizedBox(height: 10),
        ],
      ],
    );
  }
}
