// -----------------------------------------------------------------------------
// Section: Training mastery presentation
// -----------------------------------------------------------------------------

import 'package:flutter/material.dart';

import '../../../localization/generated/app_localizations.dart';
import '../../../shared/theme/app_theme.dart';

class TrainingMasteryProgress extends StatelessWidget {
  const TrainingMasteryProgress({
    required this.mastered,
    required this.total,
    super.key,
  });

  final int mastered;
  final int total;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
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
          style: Theme.of(context).textTheme.bodySmall
              ?.copyWith(fontWeight: FontWeight.w700),
        ),
        const SizedBox(height: 8),
        ClipRRect(
          borderRadius: BorderRadius.circular(999),
          child: LinearProgressIndicator(
            value: ratio,
            minHeight: 8,
            color: AppTheme.success,
          ),
        ),
      ],
    );
  }
}
