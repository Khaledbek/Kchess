// -----------------------------------------------------------------------------
// Section: Training category dashboard
// -----------------------------------------------------------------------------

import 'package:flutter/material.dart';

import '../../../localization/generated/app_localizations.dart';
import '../../../shared/models/models.dart';
import '../models/opening_training_request.dart';
import 'training_mastery_progress.dart';

class TrainingDashboardCards extends StatelessWidget {
  const TrainingDashboardCards({
    required this.overview,
    required this.openings,
    required this.loading,
    required this.openingRequest,
    required this.onOpenOpeningLab,
    required this.onOpenTactics,
    required this.onOpenEndgames,
    super.key,
  });

  final TrainingOverview? overview;
  final Future<OpeningsStats> openings;
  final bool loading;
  final OpeningTrainingRequest? openingRequest;
  final VoidCallback onOpenOpeningLab;
  final VoidCallback onOpenTactics;
  final VoidCallback onOpenEndgames;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final tactics = overview?.category('tactics');
    final endgames = overview?.category('endgame');
    final cards = <Widget Function(bool)>[
      (stretched) => _TrainingCategoryCard(
        icon: Icons.auto_stories_outlined,
        title: strings.trainingOpeningTitle,
        actionLabel: strings.trainingOpeningAction,
        onAction: onOpenOpeningLab,
        detail: _OpeningMetric(openings: openings, request: openingRequest),
        stretched: stretched,
      ),
      (stretched) => _TrainingCategoryCard(
        icon: Icons.psychology_alt_outlined,
        title: strings.trainingTacticsTitle,
        actionLabel: strings.trainingTacticsAction,
        onAction: onOpenTactics,
        detail: loading
            ? const _MetricPlaceholder()
            : _Metric(
                icon: Icons.task_alt,
                label: strings.trainingTacticsSolved(tactics?.solved ?? 0),
              ),
        stretched: stretched,
      ),
      (stretched) => _TrainingCategoryCard(
        icon: Icons.school_rounded,
        title: strings.trainingEndgameTitle,
        actionLabel: strings.trainingEndgameAction,
        onAction: onOpenEndgames,
        detail: TrainingMasteryProgress(
          mastered: endgames?.mastered ?? 0,
          total: endgames?.total ?? 0,
        ),
        stretched: stretched,
      ),
    ];

    return LayoutBuilder(
      builder: (context, constraints) {
        if (constraints.maxWidth >= 820) {
          return IntrinsicHeight(
            child: Row(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                for (var index = 0; index < cards.length; index++) ...[
                  if (index > 0) const SizedBox(width: 20),
                  Expanded(child: cards[index](true)),
                ],
              ],
            ),
          );
        }
        return Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            for (var index = 0; index < cards.length; index++) ...[
              if (index > 0) const SizedBox(height: 20),
              cards[index](false),
            ],
          ],
        );
      },
    );
  }
}

class _TrainingCategoryCard extends StatelessWidget {
  const _TrainingCategoryCard({
    required this.icon,
    required this.title,
    required this.actionLabel,
    required this.onAction,
    required this.detail,
    required this.stretched,
  });

  final IconData icon;
  final String title;
  final String actionLabel;
  final VoidCallback onAction;
  final Widget detail;
  final bool stretched;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(18),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Container(
              padding: const EdgeInsets.all(10),
              decoration: BoxDecoration(
                color: scheme.primaryContainer,
                borderRadius: BorderRadius.circular(12),
              ),
              child: Icon(icon, color: scheme.onPrimaryContainer),
            ),
            const SizedBox(height: 14),
            Text(
              title,
              style: theme.textTheme.titleMedium?.copyWith(
                fontWeight: FontWeight.w800,
              ),
            ),
            const SizedBox(height: 14),
            detail,
            if (stretched) const Spacer(),
            const SizedBox(height: 16),
            Align(
              alignment: AlignmentDirectional.centerStart,
              child: FilledButton.icon(
                onPressed: onAction,
                iconAlignment: IconAlignment.end,
                icon: const Icon(Icons.arrow_forward_rounded, size: 18),
                label: Text(actionLabel),
              ),
            ),
          ],
        ),
      ),
    );
  }
}

class _OpeningMetric extends StatelessWidget {
  const _OpeningMetric({required this.openings, required this.request});

  final Future<OpeningsStats> openings;
  final OpeningTrainingRequest? request;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    if (request != null) {
      return _Metric(
        icon: Icons.push_pin_outlined,
        label: '${strings.trainingOpeningLabSelected}: ${request!.label}',
        emphasised: true,
      );
    }
    return FutureBuilder<OpeningsStats>(
      future: openings,
      builder: (context, snapshot) {
        if (snapshot.connectionState != ConnectionState.done) {
          return const _MetricPlaceholder();
        }
        final nemesis = snapshot.data?.nemesis;
        if (nemesis == null) {
          return _Metric(
            icon: Icons.check_circle_outline,
            label: strings.trainingNemesisNone,
          );
        }
        final rate = nemesis.tally.winRate ?? 0;
        return _Metric(
          icon: Icons.warning_amber_rounded,
          label: strings.trainingNemesisBadge(
            nemesis.familyName,
            '${(rate * 100).round()}%',
          ),
          emphasised: true,
        );
      },
    );
  }
}

class _MetricPlaceholder extends StatelessWidget {
  const _MetricPlaceholder();

  @override
  Widget build(BuildContext context) {
    final scheme = Theme.of(context).colorScheme;
    return Container(
      height: 38,
      decoration: BoxDecoration(
        color: scheme.surfaceContainerHigh,
        borderRadius: BorderRadius.circular(10),
        border: Border.all(color: scheme.outlineVariant),
      ),
    );
  }
}

class _Metric extends StatelessWidget {
  const _Metric({
    required this.icon,
    required this.label,
    this.emphasised = false,
  });

  final IconData icon;
  final String label;
  final bool emphasised;

  @override
  Widget build(BuildContext context) {
    final scheme = Theme.of(context).colorScheme;
    final foreground = emphasised
        ? scheme.onPrimaryContainer
        : scheme.onSurfaceVariant;
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 9),
      decoration: BoxDecoration(
        color: emphasised
            ? scheme.primaryContainer.withValues(alpha: 0.55)
            : scheme.surfaceContainerHigh,
        borderRadius: BorderRadius.circular(10),
        border: Border.all(color: scheme.outlineVariant),
      ),
      child: Row(
        children: [
          Icon(icon, size: 18, color: foreground),
          const SizedBox(width: 8),
          Expanded(
            child: Text(
              label,
              maxLines: 2,
              overflow: TextOverflow.ellipsis,
              style: TextStyle(color: foreground),
            ),
          ),
        ],
      ),
    );
  }
}
