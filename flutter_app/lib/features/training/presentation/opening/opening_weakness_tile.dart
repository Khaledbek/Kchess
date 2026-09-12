// -----------------------------------------------------------------------------
// Section: One opening weakness, as the statistics and training tabs show it
// -----------------------------------------------------------------------------

import 'package:flutter/material.dart';

import '../../../../localization/generated/app_localizations.dart';
import '../../../../shared/models/models.dart';

/// Why native flagged a line, most actionable first: the repeated move, then
/// the opening errors, then the results.
List<String> openingWeaknessReasons(
  AppLocalizations strings,
  OpeningWeakness weakness,
) {
  final mistake = weakness.recurringMistake;
  return [
    if (mistake != null)
      mistake.recommended == null || mistake.recommended!.isEmpty
          ? strings.openingWeaknessRecurringFlagged(
              mistake.notation(mistake.san),
              mistake.count,
            )
          : strings.openingWeaknessRecurring(
              mistake.notation(mistake.san),
              mistake.count,
              mistake.notation(mistake.recommended!),
            ),
    if (weakness.frequentErrors)
      strings.openingWeaknessErrors(
        weakness.gamesWithOpeningErrors,
        weakness.analysedGames,
      ),
    if (weakness.poorResults || (mistake == null && !weakness.frequentErrors))
      strings.openingWeaknessLost(weakness.tally.losses, weakness.tally.games),
  ];
}

/// A weak line in two short lines — name, then side, ECO and the reason to act
/// on — with a small train button: the statistics tab's dense list.
class OpeningWeaknessRow extends StatelessWidget {
  const OpeningWeaknessRow({
    required this.weakness,
    required this.colorLabel,
    this.onTrain,
    this.trainLabel,
    super.key,
  });

  final OpeningWeakness weakness;
  final String colorLabel;
  final VoidCallback? onTrain;
  final String? trainLabel;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;
    final reasons = openingWeaknessReasons(strings, weakness);
    final muted = theme.textTheme.bodySmall?.copyWith(color: scheme.onSurfaceVariant);
    return Padding(
      key: ValueKey('opening-weakness-${weakness.name}-${weakness.color}'),
      padding: const EdgeInsets.symmetric(vertical: 6),
      child: Row(
        children: [
          Icon(Icons.warning_amber_rounded, size: 18, color: scheme.error),
          const SizedBox(width: 10),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  weakness.name,
                  maxLines: 1,
                  overflow: TextOverflow.ellipsis,
                  style: theme.textTheme.bodyMedium?.copyWith(fontWeight: FontWeight.w700),
                ),
                const SizedBox(height: 1),
                Text.rich(
                  TextSpan(
                    style: muted,
                    children: [
                      WidgetSpan(
                        alignment: PlaceholderAlignment.middle,
                        child: Container(
                          width: 8,
                          height: 8,
                          margin: const EdgeInsetsDirectional.only(end: 5),
                          decoration: BoxDecoration(
                            shape: BoxShape.circle,
                            color: weakness.color == 'white' ? Colors.white : Colors.black,
                            border: Border.all(color: scheme.outline),
                          ),
                        ),
                      ),
                      TextSpan(text: colorLabel),
                      if (weakness.eco.isNotEmpty) TextSpan(text: ' · ${weakness.eco}'),
                      if (reasons.isNotEmpty) ...[
                        const TextSpan(text: ' · '),
                        TextSpan(
                          text: reasons.first,
                          style: TextStyle(color: scheme.error, fontWeight: FontWeight.w600),
                        ),
                      ],
                    ],
                  ),
                  key: const Key('opening-weakness-reason'),
                  maxLines: 2,
                  overflow: TextOverflow.ellipsis,
                ),
              ],
            ),
          ),
          if (onTrain != null) ...[
            const SizedBox(width: 8),
            IconButton.filledTonal(
              key: ValueKey('opening-weakness-train-${weakness.name}-${weakness.color}'),
              onPressed: onTrain,
              tooltip: trainLabel ?? strings.trainingOpeningAction,
              visualDensity: VisualDensity.compact,
              iconSize: 18,
              icon: const Icon(Icons.school_rounded),
            ),
          ],
        ],
      ),
    );
  }
}

/// A weak line with its evidence and a way into its drill.
///
/// Colours come from the ambient theme, so the same tile sits in the light or
/// dark statistics tab and on the training tab's studio backdrop.
class OpeningWeaknessTile extends StatelessWidget {
  const OpeningWeaknessTile({
    required this.weakness,
    required this.colorLabel,
    this.onTrain,
    this.trainLabel,
    super.key,
  });

  final OpeningWeakness weakness;

  /// "White" / "Black" in the host tab's wording.
  final String colorLabel;

  /// Starts the drill; null hides the button (no training tab to go to).
  final VoidCallback? onTrain;
  final String? trainLabel;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;
    final reasons = openingWeaknessReasons(strings, weakness);
    final button = onTrain == null
        ? null
        : FilledButton.tonalIcon(
            key: ValueKey('opening-weakness-train-${weakness.name}-${weakness.color}'),
            onPressed: onTrain,
            style: FilledButton.styleFrom(visualDensity: VisualDensity.compact),
            icon: const Icon(Icons.school_rounded, size: 18),
            label: Text(trainLabel ?? strings.trainingOpeningAction),
          );

    final details = Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(
          weakness.name,
          maxLines: 2,
          overflow: TextOverflow.ellipsis,
          style: theme.textTheme.titleSmall?.copyWith(fontWeight: FontWeight.w800),
        ),
        const SizedBox(height: 4),
        Wrap(
          spacing: 8,
          runSpacing: 4,
          crossAxisAlignment: WrapCrossAlignment.center,
          children: [
            Row(
              mainAxisSize: MainAxisSize.min,
              children: [
                Container(
                  width: 10,
                  height: 10,
                  decoration: BoxDecoration(
                    shape: BoxShape.circle,
                    color: weakness.color == 'white' ? Colors.white : Colors.black,
                    border: Border.all(color: scheme.outline),
                  ),
                ),
                const SizedBox(width: 6),
                Text(colorLabel, style: theme.textTheme.labelMedium),
              ],
            ),
            if (weakness.eco.isNotEmpty)
              Text(weakness.eco, style: theme.textTheme.labelMedium),
            if (weakness.level == 'family')
              Text(
                strings.openingWeaknessFamily,
                style: theme.textTheme.labelMedium?.copyWith(
                  color: scheme.onSurfaceVariant,
                ),
              ),
          ],
        ),
        const SizedBox(height: 8),
        for (final (index, reason) in reasons.indexed)
          Padding(
            padding: const EdgeInsets.only(bottom: 3),
            child: Text(
              reason,
              key: index == 0 ? const Key('opening-weakness-reason') : null,
              style: theme.textTheme.bodySmall?.copyWith(
                height: 1.35,
                // The first reason is the one to act on.
                color: index == 0 ? scheme.error : scheme.onSurfaceVariant,
                fontWeight: index == 0 ? FontWeight.w700 : FontWeight.w500,
              ),
            ),
          ),
      ],
    );

    return Container(
      key: ValueKey('opening-weakness-${weakness.name}-${weakness.color}'),
      padding: const EdgeInsets.all(12),
      decoration: BoxDecoration(
        color: scheme.errorContainer.withValues(alpha: 0.18),
        borderRadius: BorderRadius.circular(14),
        border: Border.all(color: scheme.error.withValues(alpha: 0.35)),
      ),
      child: LayoutBuilder(
        builder: (context, constraints) {
          final icon = Padding(
            padding: const EdgeInsets.only(top: 2),
            child: Icon(Icons.warning_amber_rounded, color: scheme.error, size: 22),
          );
          // Below ~420 px the button would squeeze the reasons into a sliver.
          if (button == null || constraints.maxWidth >= 420) {
            return Row(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                icon,
                const SizedBox(width: 10),
                Expanded(child: details),
                if (button != null) ...[const SizedBox(width: 10), button],
              ],
            );
          }
          return Column(
            crossAxisAlignment: CrossAxisAlignment.stretch,
            children: [
              Row(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [icon, const SizedBox(width: 10), Expanded(child: details)],
              ),
              const SizedBox(height: 8),
              Align(alignment: AlignmentDirectional.centerEnd, child: button),
            ],
          );
        },
      ),
    );
  }
}
