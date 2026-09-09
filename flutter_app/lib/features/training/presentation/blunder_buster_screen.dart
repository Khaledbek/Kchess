import 'package:flutter/material.dart';

import '../../../localization/generated/app_localizations.dart';
import '../../../services/training_progress_service.dart';
import '../data/training_library.dart';
import '../models/models.dart';

/// Middlegame tactics drill.
///
/// The catalogue is empty until critical positions are mined from the user's own
/// analysed games — that mining is native work, so the screen currently reports
/// the solved counter and the empty state.
class BlunderBusterScreen extends StatelessWidget {
  const BlunderBusterScreen({required this.progress, super.key});

  final TrainingProgressService progress;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;
    final puzzles = TrainingLibrary.byCategory(TrainingCategories.tactics);

    return Scaffold(
      appBar: AppBar(title: Text(strings.trainingTacticsTitle)),
      body: StreamBuilder<TrainingProgressSnapshot>(
        stream: progress.changes,
        initialData: progress.snapshot,
        builder: (context, snapshot) {
          final solved =
              (snapshot.data ?? const TrainingProgressSnapshot.empty())
                  .solvedCount(TrainingCategories.tactics, TrainingLibrary.all);
          return Center(
            child: ConstrainedBox(
              constraints: const BoxConstraints(maxWidth: 520),
              child: Column(
                mainAxisSize: MainAxisSize.min,
                children: [
                  Icon(
                    Icons.psychology_alt_outlined,
                    size: 40,
                    color: scheme.onSurfaceVariant,
                  ),
                  const SizedBox(height: 12),
                  Text(
                    strings.trainingTacticsSolved(solved),
                    style: theme.textTheme.titleMedium?.copyWith(
                      fontWeight: FontWeight.w700,
                    ),
                  ),
                  const SizedBox(height: 8),
                  Padding(
                    padding: const EdgeInsets.symmetric(horizontal: 24),
                    child: Text(
                      puzzles.isEmpty
                          ? strings.trainingTacticsEmpty
                          : strings.trainingBoardPending,
                      textAlign: TextAlign.center,
                      style: theme.textTheme.bodyMedium?.copyWith(
                        color: scheme.onSurfaceVariant,
                      ),
                    ),
                  ),
                ],
              ),
            ),
          );
        },
      ),
    );
  }
}
