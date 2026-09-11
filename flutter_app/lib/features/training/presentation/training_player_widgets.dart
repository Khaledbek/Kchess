// -----------------------------------------------------------------------------
// Section: Training player feedback widgets
// -----------------------------------------------------------------------------

import 'package:flutter/material.dart';

import '../../../localization/generated/app_localizations.dart';
import '../../../shared/theme/app_theme.dart';

class TrainingStatusLine extends StatelessWidget {
  const TrainingStatusLine({
    required this.busy,
    required this.solved,
    required this.feedback,
    super.key,
  });

  final bool busy;
  final bool solved;
  final String? feedback;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final scheme = Theme.of(context).colorScheme;
    final text =
        feedback ??
        (solved ? strings.trainingSolvedTitle : strings.trainingYourMove);
    final color = feedback != null
        ? scheme.error
        : solved
        ? AppTheme.success
        : scheme.onSurfaceVariant;
    return Row(
      key: const Key('training-status'),
      children: [
        if (busy)
          const SizedBox.square(
            dimension: 18,
            child: CircularProgressIndicator(strokeWidth: 2),
          )
        else
          Icon(
            solved ? Icons.check_circle_outline : Icons.touch_app_outlined,
            size: 18,
            color: color,
          ),
        const SizedBox(width: 8),
        Expanded(
          child: Text(
            text,
            style: TextStyle(color: color, fontWeight: FontWeight.w600),
          ),
        ),
      ],
    );
  }
}

class TrainingSolvedOverlay extends StatelessWidget {
  const TrainingSolvedOverlay({
    required this.clean,
    required this.onPractiseAgain,
    required this.onNext,
    super.key,
  });

  final bool clean;
  final VoidCallback onPractiseAgain;
  final VoidCallback? onNext;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final scheme = Theme.of(context).colorScheme;
    return Container(
      key: const Key('training-solved-overlay'),
      alignment: Alignment.center,
      color: scheme.surface.withValues(alpha: 0.88),
      child: Padding(
        padding: const EdgeInsets.all(20),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Icon(
              Icons.emoji_events_rounded,
              size: 44,
              color: clean ? AppTheme.success : scheme.tertiary,
            ),
            const SizedBox(height: 12),
            Text(
              strings.trainingSolvedTitle,
              textAlign: TextAlign.center,
              style: const TextStyle(fontWeight: FontWeight.w800),
            ),
            const SizedBox(height: 6),
            Text(
              clean
                  ? strings.trainingSolvedClean
                  : strings.trainingSolvedWithErrors,
              textAlign: TextAlign.center,
            ),
            const SizedBox(height: 18),
            Wrap(
              spacing: 10,
              runSpacing: 10,
              alignment: WrapAlignment.center,
              children: [
                OutlinedButton(
                  onPressed: onPractiseAgain,
                  child: Text(strings.trainingPracticeAgain),
                ),
                FilledButton(
                  onPressed: onNext ?? () => Navigator.of(context).pop(),
                  child: Text(
                    onNext == null
                        ? strings.trainingBackToList
                        : strings.trainingNextEndgame,
                  ),
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }
}

class TrainingPlayerMessage extends StatelessWidget {
  const TrainingPlayerMessage({
    required this.icon,
    required this.text,
    this.action,
    super.key,
  });

  final IconData icon;
  final String text;
  final Widget? action;

  @override
  Widget build(BuildContext context) {
    final scheme = Theme.of(context).colorScheme;
    return Container(
      padding: const EdgeInsets.all(14),
      decoration: BoxDecoration(
        color: scheme.surfaceContainerHigh,
        borderRadius: BorderRadius.circular(12),
        border: Border.all(color: scheme.outlineVariant),
      ),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Icon(icon, size: 18, color: scheme.onSurfaceVariant),
          const SizedBox(width: 10),
          Expanded(child: Text(text)),
          ?action,
        ],
      ),
    );
  }
}
