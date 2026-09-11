import 'package:flutter/material.dart';

import '../../../ffi/core_gateway.dart';
import '../../../localization/generated/app_localizations.dart';
import '../../../services/training_progress_service.dart';
import '../../../shared/theme/app_theme.dart';
import '../models/endgame_drill.dart';
import '../models/training_progress.dart';
import 'endgame_drill_player.dart';

/// The drills inside one category, each as a row of difficulty levels.
///
/// A level opens once the one before it is mastered, so the user drills a
/// position type repeatedly before the harder version appears.
class DrillLevelsScreen extends StatelessWidget {
  const DrillLevelsScreen({
    required this.category,
    required this.gateway,
    required this.progress,
    super.key,
  });

  final EndgameCategory category;
  final CoreGateway gateway;
  final TrainingProgressService progress;

  /// Level 1 is always open; every later level needs the previous one mastered.
  static bool isUnlocked(
    EndgameDrill drill,
    DrillLevel level,
    TrainingProgressSnapshot snapshot,
  ) {
    if (level.index <= 1) return true;
    return snapshot.isMastered(drill.progressId(level.index - 1));
  }

  void _play(BuildContext context, EndgameDrill drill, DrillLevel level) {
    Navigator.of(context).push(
      MaterialPageRoute<void>(
        builder: (_) => EndgameDrillPlayer(
          drill: drill,
          level: level,
          gateway: gateway,
          progress: progress,
        ),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: Text(category.title)),
      body: StreamBuilder<TrainingProgressSnapshot>(
        stream: progress.changes,
        initialData: progress.snapshot,
        builder: (context, asyncSnapshot) {
          final snapshot =
              asyncSnapshot.data ?? const TrainingProgressSnapshot.empty();
          return ListView(
            padding: const EdgeInsets.fromLTRB(16, 12, 16, 28),
            children: [
              Center(
                child: ConstrainedBox(
                  constraints: const BoxConstraints(maxWidth: 780),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.stretch,
                    children: [
                      for (var i = 0; i < category.drills.length; i++) ...[
                        if (i > 0) const SizedBox(height: 14),
                        _DrillCard(
                          drill: category.drills[i],
                          snapshot: snapshot,
                          onPlay: (level) =>
                              _play(context, category.drills[i], level),
                        ),
                      ],
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

class _DrillCard extends StatelessWidget {
  const _DrillCard({
    required this.drill,
    required this.snapshot,
    required this.onPlay,
  });

  final EndgameDrill drill;
  final TrainingProgressSnapshot snapshot;
  final ValueChanged<DrillLevel> onPlay;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;

    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(
              drill.title,
              style: theme.textTheme.titleSmall?.copyWith(
                fontWeight: FontWeight.w800,
              ),
            ),
            const SizedBox(height: 6),
            Text(
              drill.description,
              style: theme.textTheme.bodySmall?.copyWith(
                color: scheme.onSurfaceVariant,
              ),
            ),
            const SizedBox(height: 14),
            for (var i = 0; i < drill.levels.length; i++) ...[
              if (i > 0) const SizedBox(height: 8),
              _LevelRow(
                drill: drill,
                level: drill.levels[i],
                progress: snapshot.progressFor(
                  drill.progressId(drill.levels[i].index),
                ),
                unlocked: DrillLevelsScreen.isUnlocked(
                  drill,
                  drill.levels[i],
                  snapshot,
                ),
                onPlay: () => onPlay(drill.levels[i]),
              ),
            ],
          ],
        ),
      ),
    );
  }
}

class _LevelRow extends StatelessWidget {
  const _LevelRow({
    required this.drill,
    required this.level,
    required this.progress,
    required this.unlocked,
    required this.onPlay,
  });

  final EndgameDrill drill;
  final DrillLevel level;
  final ExerciseProgress progress;
  final bool unlocked;
  final VoidCallback onPlay;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;

    return Opacity(
      opacity: unlocked ? 1 : 0.55,
      child: DecoratedBox(
        decoration: BoxDecoration(
          color: scheme.surfaceContainerLow,
          borderRadius: BorderRadius.circular(12),
          border: Border.all(color: scheme.outlineVariant),
        ),
        child: InkWell(
          onTap: unlocked ? onPlay : null,
          borderRadius: BorderRadius.circular(12),
          child: Padding(
            padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 10),
            child: Row(
              children: [
                Icon(
                  unlocked ? Icons.play_circle_outline : Icons.lock_outline,
                  size: 20,
                  color: unlocked ? scheme.primary : scheme.onSurfaceVariant,
                ),
                const SizedBox(width: 10),
                Expanded(
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(
                        strings.trainingDrillLevel(level.index),
                        style: const TextStyle(fontWeight: FontWeight.w700),
                      ),
                      const SizedBox(height: 3),
                      Text(
                        unlocked
                            ? strings.trainingDrillMoveCounter(
                                0,
                                level.maxMoves,
                              )
                            : strings.trainingDrillLocked(level.index - 1),
                        style: theme.textTheme.bodySmall?.copyWith(
                          color: scheme.onSurfaceVariant,
                        ),
                      ),
                    ],
                  ),
                ),
                const SizedBox(width: 8),
                DrillDifficultyChip(difficulty: level.difficulty),
                const SizedBox(width: 8),
                _LevelBadge(progress: progress),
              ],
            ),
          ),
        ),
      ),
    );
  }
}

/// `Gemeistert` after three clean runs, `Gelöst` after the first, else `Offen`.
class _LevelBadge extends StatelessWidget {
  const _LevelBadge({required this.progress});

  final ExerciseProgress progress;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;

    final (label, color) = progress.isMastered
        ? (strings.trainingMastered, AppTheme.success)
        : progress.successCount > 0
        ? (strings.trainingDrillSolved, AppTheme.warning)
        : (strings.trainingDrillOpen, scheme.onSurfaceVariant);

    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 9, vertical: 4),
      decoration: BoxDecoration(
        color: color.withValues(alpha: 0.14),
        borderRadius: BorderRadius.circular(8),
        border: Border.all(color: color.withValues(alpha: 0.6)),
      ),
      child: Text(
        label,
        style: theme.textTheme.bodySmall?.copyWith(
          fontWeight: FontWeight.w700,
          color: color,
        ),
      ),
    );
  }
}
