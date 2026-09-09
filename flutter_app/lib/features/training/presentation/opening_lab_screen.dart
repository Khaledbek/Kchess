import 'package:flutter/material.dart';

import '../../../localization/generated/app_localizations.dart';
import '../models/opening_training_request.dart';

/// Target of the statistics deep link: shows which line was handed over and
/// what will be drilled here.
///
/// Replaying a repertoire line needs the core's move generation, so this screen
/// currently confirms the selection instead of pretending to train it.
class OpeningLabScreen extends StatelessWidget {
  const OpeningLabScreen({this.request, super.key});

  /// Opening picked in the statistics tab, or null when the lab was opened
  /// straight from the hub.
  final OpeningTrainingRequest? request;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;
    final selected = request;

    return Scaffold(
      appBar: AppBar(title: Text(strings.trainingOpeningTitle)),
      body: Center(
        child: ConstrainedBox(
          constraints: const BoxConstraints(maxWidth: 640),
          child: ListView(
            padding: const EdgeInsets.fromLTRB(16, 16, 16, 28),
            shrinkWrap: true,
            children: [
              Card(
                child: Padding(
                  padding: const EdgeInsets.all(18),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(
                        strings.trainingOpeningLabSelected,
                        style: theme.textTheme.labelLarge?.copyWith(
                          fontWeight: FontWeight.w700,
                          color: scheme.onSurfaceVariant,
                        ),
                      ),
                      const SizedBox(height: 8),
                      if (selected == null)
                        Text(
                          strings.trainingOpeningLabEmpty,
                          style: theme.textTheme.bodyMedium?.copyWith(
                            color: scheme.onSurfaceVariant,
                          ),
                        )
                      else ...[
                        Text(
                          selected.openingName,
                          style: theme.textTheme.titleMedium?.copyWith(
                            fontWeight: FontWeight.w800,
                          ),
                        ),
                        const SizedBox(height: 10),
                        Wrap(
                          spacing: 8,
                          runSpacing: 8,
                          children: [
                            if (selected.eco.isNotEmpty)
                              Chip(label: Text(selected.eco)),
                            Chip(
                              label: Text(_colorLabel(strings, selected.color)),
                            ),
                          ],
                        ),
                      ],
                    ],
                  ),
                ),
              ),
              const SizedBox(height: 16),
              Text(
                strings.trainingOpeningLabPending,
                style: theme.textTheme.bodySmall?.copyWith(
                  color: scheme.onSurfaceVariant,
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }
}

String _colorLabel(AppLocalizations strings, String color) => switch (color) {
  'white' => strings.statsCompareColorWhite,
  'black' => strings.statsCompareColorBlack,
  _ => strings.statsOpeningsUnknownColor,
};
