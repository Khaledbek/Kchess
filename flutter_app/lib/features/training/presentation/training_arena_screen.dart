import 'package:flutter/material.dart';

import '../../../localization/generated/app_localizations.dart';
import '../../../services/training_progress_service.dart';
import '../../../shared/models/models.dart';
import '../../app/application/app_controller.dart';
import '../data/training_library.dart';
import '../models/models.dart';
import 'blunder_buster_screen.dart';
import 'endgame_academy_screen.dart';
import 'opening_lab_screen.dart';

/// Hub of the training tab: one card per training category, each showing a live
/// figure taken from the user's own data (nemesis opening, solved tactics,
/// endgame mastery) so the dashboard is never a static menu.
class TrainingArenaScreen extends StatefulWidget {
  const TrainingArenaScreen({
    required this.controller,
    required this.progress,
    this.openingRequest,
    super.key,
  });

  final AppController controller;
  final TrainingProgressService progress;

  /// Set when the statistics tab deep-linked into this tab; the opening lab
  /// opens on that line.
  final OpeningTrainingRequest? openingRequest;

  @override
  State<TrainingArenaScreen> createState() => _TrainingArenaScreenState();
}

class _TrainingArenaScreenState extends State<TrainingArenaScreen> {
  late Future<OpeningsStats> _openings;

  @override
  void initState() {
    super.initState();
    widget.progress.load();
    _openings = widget.controller.gateway.openingsStats();
  }

  @override
  void didUpdateWidget(covariant TrainingArenaScreen oldWidget) {
    super.didUpdateWidget(oldWidget);
    // A fresh deep link should train against current numbers, and switching
    // profiles changes whose openings these are.
    if (oldWidget.controller != widget.controller ||
        oldWidget.openingRequest != widget.openingRequest) {
      setState(() {
        _openings = widget.controller.gateway.openingsStats();
      });
    }
  }

  void _openOpeningLab(OpeningTrainingRequest? request) {
    Navigator.of(context).push(
      MaterialPageRoute<void>(
        builder: (_) => OpeningLabScreen(request: request),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final theme = Theme.of(context);

    return Scaffold(
      appBar: MediaQuery.sizeOf(context).width >= 900
          ? AppBar(title: Text(strings.trainingSection))
          : null,
      body: StreamBuilder<TrainingProgressSnapshot>(
        stream: widget.progress.changes,
        initialData: widget.progress.snapshot,
        builder: (context, snapshot) {
          final progress =
              snapshot.data ?? const TrainingProgressSnapshot.empty();
          return ListView(
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
                      const SizedBox(height: 8),
                      Text(
                        strings.trainingIntroBody,
                        style: theme.textTheme.bodyMedium?.copyWith(
                          color: theme.colorScheme.onSurfaceVariant,
                        ),
                      ),
                      const SizedBox(height: 20),
                      _TrainingCards(
                        openings: _openings,
                        progress: progress,
                        openingRequest: widget.openingRequest,
                        onOpenOpeningLab: _openOpeningLab,
                        onOpenTactics: () => Navigator.of(context).push(
                          MaterialPageRoute<void>(
                            builder: (_) =>
                                BlunderBusterScreen(progress: widget.progress),
                          ),
                        ),
                        onOpenEndgames: () => Navigator.of(context).push(
                          MaterialPageRoute<void>(
                            builder: (_) => EndgameAcademyScreen(
                              gateway: widget.controller.gateway,
                              progress: widget.progress,
                            ),
                          ),
                        ),
                      ),
                    ],
                  ),
                ),
              ),
            ],
          );
        },
      ),
    );
  }
}

/// The three category cards, side by side on a desktop window and stacked once
/// the pane gets narrow.
class _TrainingCards extends StatelessWidget {
  const _TrainingCards({
    required this.openings,
    required this.progress,
    required this.openingRequest,
    required this.onOpenOpeningLab,
    required this.onOpenTactics,
    required this.onOpenEndgames,
  });

  final Future<OpeningsStats> openings;
  final TrainingProgressSnapshot progress;
  final OpeningTrainingRequest? openingRequest;
  final void Function(OpeningTrainingRequest?) onOpenOpeningLab;
  final VoidCallback onOpenTactics;
  final VoidCallback onOpenEndgames;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final catalogue = TrainingLibrary.all;

    final mastered = progress.masteryCount(
      TrainingCategories.endgame,
      catalogue,
    );
    final total = TrainingLibrary.byCategory(TrainingCategories.endgame).length;
    final solved = progress.solvedCount(TrainingCategories.tactics, catalogue);

    // Built per layout: only the row layout hands the cards a bounded height,
    // and the button-aligning spacer needs one.
    final cards = <Widget Function(bool stretched)>[
      (stretched) => _TrainingCategoryCard(
        icon: Icons.auto_stories_outlined,
        title: strings.trainingOpeningTitle,
        subtitle: strings.trainingOpeningSubtitle,
        actionLabel: strings.trainingOpeningAction,
        onAction: () => onOpenOpeningLab(openingRequest),
        detail: _NemesisBadge(future: openings, request: openingRequest),
        stretched: stretched,
      ),
      (stretched) => _TrainingCategoryCard(
        icon: Icons.psychology_alt_outlined,
        title: strings.trainingTacticsTitle,
        subtitle: strings.trainingTacticsSubtitle,
        actionLabel: strings.trainingTacticsAction,
        onAction: onOpenTactics,
        detail: _TrainingMetric(
          icon: Icons.task_alt,
          label: strings.trainingTacticsSolved(solved),
        ),
        stretched: stretched,
      ),
      (stretched) => _TrainingCategoryCard(
        icon: Icons.school_rounded,
        title: strings.trainingEndgameTitle,
        subtitle: strings.trainingEndgameSubtitle,
        actionLabel: strings.trainingEndgameAction,
        onAction: onOpenEndgames,
        detail: MasteryProgressBar(mastered: mastered, total: total),
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
                for (var i = 0; i < cards.length; i++) ...[
                  if (i > 0) const SizedBox(width: 20),
                  Expanded(child: cards[i](true)),
                ],
              ],
            ),
          );
        }
        return Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            for (var i = 0; i < cards.length; i++) ...[
              if (i > 0) const SizedBox(height: 20),
              cards[i](false),
            ],
          ],
        );
      },
    );
  }
}

/// One dashboard card: accent icon, title, subtitle, a live [detail] figure and
/// the call to action.
class _TrainingCategoryCard extends StatelessWidget {
  const _TrainingCategoryCard({
    required this.icon,
    required this.title,
    required this.subtitle,
    required this.actionLabel,
    required this.onAction,
    required this.detail,
    required this.stretched,
  });

  final IconData icon;
  final String title;
  final String subtitle;
  final String actionLabel;
  final VoidCallback onAction;
  final Widget detail;

  /// True when the card is stretched to a shared height by [IntrinsicHeight].
  /// Only then may the column push its button down with a flexible spacer — in
  /// the stacked layout the height is unbounded and a [Spacer] would throw.
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
            const SizedBox(height: 6),
            Text(
              subtitle,
              style: theme.textTheme.bodyMedium?.copyWith(
                color: scheme.onSurfaceVariant,
              ),
            ),
            const SizedBox(height: 14),
            detail,
            // Keeps every call to action on the same baseline across the row.
            if (stretched) const Spacer(),
            const SizedBox(height: 16),
            Align(
              alignment: AlignmentDirectional.centerStart,
              child: FilledButton(
                onPressed: onAction,
                child: Row(
                  mainAxisSize: MainAxisSize.min,
                  children: [
                    // Flexible, because three cards side by side leave the
                    // button barely wider than its own label: without this the
                    // longest label overflows the row by a few pixels.
                    Flexible(
                      child: Text(
                        actionLabel,
                        maxLines: 1,
                        overflow: TextOverflow.ellipsis,
                      ),
                    ),
                    const SizedBox(width: 8),
                    const Icon(Icons.arrow_forward_rounded, size: 18),
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

/// Minimum games before a line may be called a weakness, and the win rate below
/// which it is one. Same floor the statistics self-audit uses, so both screens
/// agree on which lines are weak.
const nemesisMinGames = 5;
const nemesisWeakRate = 0.45;

/// The line the user scores worst with on a usable sample, or null when nothing
/// qualifies.
OpeningFamily? nemesisOpening(OpeningsStats stats) {
  OpeningFamily? worst;
  double? worstRate;
  for (final family in stats.families) {
    if (family.tally.games < nemesisMinGames) continue;
    final decided = family.tally.decided;
    if (decided == 0) continue;
    final rate = family.tally.wins / decided;
    if (rate >= nemesisWeakRate) continue;
    if (worstRate == null || rate < worstRate) {
      worst = family;
      worstRate = rate;
    }
  }
  return worst;
}

/// The opening card's dynamic badge: the nemesis line, or the deep-linked line
/// when the statistics tab picked one.
class _NemesisBadge extends StatelessWidget {
  const _NemesisBadge({required this.future, required this.request});

  final Future<OpeningsStats> future;
  final OpeningTrainingRequest? request;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final pinned = request;
    if (pinned != null) {
      return _TrainingMetric(
        icon: Icons.push_pin_outlined,
        label: '${strings.trainingOpeningLabSelected}: ${pinned.label}',
        emphasised: true,
      );
    }
    return FutureBuilder<OpeningsStats>(
      future: future,
      builder: (context, snapshot) {
        if (snapshot.connectionState != ConnectionState.done) {
          return const _TrainingMetricPlaceholder();
        }
        final stats = snapshot.data;
        final nemesis = stats == null ? null : nemesisOpening(stats);
        if (nemesis == null) {
          return _TrainingMetric(
            icon: Icons.check_circle_outline,
            label: strings.trainingNemesisNone,
          );
        }
        final decided = nemesis.tally.decided;
        final rate = decided == 0 ? 0.0 : nemesis.tally.wins / decided;
        return _TrainingMetric(
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

/// A pill with an icon and one line of live data.
class _TrainingMetric extends StatelessWidget {
  const _TrainingMetric({
    required this.icon,
    required this.label,
    this.emphasised = false,
  });

  final IconData icon;
  final String label;

  /// Draws the pill in the accent colour when the figure is the point of the
  /// card (a nemesis line, a pinned deep link).
  final bool emphasised;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;
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
              style: theme.textTheme.bodySmall?.copyWith(
                color: foreground,
                fontWeight: FontWeight.w600,
              ),
            ),
          ),
        ],
      ),
    );
  }
}

class _TrainingMetricPlaceholder extends StatelessWidget {
  const _TrainingMetricPlaceholder();

  @override
  Widget build(BuildContext context) {
    final scheme = Theme.of(context).colorScheme;
    return Container(
      height: 38,
      alignment: AlignmentDirectional.centerStart,
      padding: const EdgeInsets.symmetric(horizontal: 12),
      decoration: BoxDecoration(
        color: scheme.surfaceContainerHigh,
        borderRadius: BorderRadius.circular(10),
        border: Border.all(color: scheme.outlineVariant),
      ),
      child: const SizedBox(
        width: 18,
        height: 18,
        child: CircularProgressIndicator(strokeWidth: 2),
      ),
    );
  }
}

/// "3 / 12 Stellungen gemeistert (25%)" plus the matching bar. Shared by the
/// hub card and the academy header.
class MasteryProgressBar extends StatelessWidget {
  const MasteryProgressBar({
    required this.mastered,
    required this.total,
    super.key,
  });

  final int mastered;
  final int total;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final theme = Theme.of(context);
    final ratio = total == 0 ? 0.0 : mastered / total;

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(
          strings.trainingEndgameProgress(
            mastered,
            total,
            (ratio * 100).round(),
          ),
          style: theme.textTheme.bodySmall?.copyWith(
            color: theme.colorScheme.onSurfaceVariant,
            fontWeight: FontWeight.w600,
          ),
        ),
        const SizedBox(height: 8),
        ClipRRect(
          borderRadius: BorderRadius.circular(4),
          child: LinearProgressIndicator(value: ratio, minHeight: 8),
        ),
      ],
    );
  }
}
