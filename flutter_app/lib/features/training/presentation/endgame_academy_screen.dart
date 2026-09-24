// -----------------------------------------------------------------------------
// Section: Native endgame catalogue presentation
// -----------------------------------------------------------------------------

import 'package:flutter/material.dart';

import '../../../ffi/core_gateway.dart';
import '../../../localization/generated/app_localizations.dart';
import '../../../shared/models/models.dart';
import '../../../shared/theme/app_theme.dart';
import 'endgame_exercise_player.dart';
import 'training_localizations.dart';
import 'training_mastery_progress.dart';
import 'drill_levels_screen.dart';
import 'endgame_studies_screen.dart';

class EndgameAcademyScreen extends StatefulWidget {
  const EndgameAcademyScreen({required this.gateway, super.key});

  final CoreGateway gateway;

  @override
  State<EndgameAcademyScreen> createState() => _EndgameAcademyScreenState();
}

class _EndgameAcademyScreenState extends State<EndgameAcademyScreen> {
  late Future<TrainingOverview> _overview;

  @override
  void initState() {
    super.initState();
    _reload();
  }

  void _reload() {
    _overview = widget.gateway.trainingOverview();
  }

  Future<void> _open(TrainingExercise exercise) async {
    await Navigator.of(context).push(
      MaterialPageRoute<void>(
        builder: (_) =>
            EndgameExercisePlayer(exercise: exercise, gateway: widget.gateway),
      ),
    );
    if (mounted) setState(_reload);
  }

  void _openCategory() {
    Navigator.of(context).push(
      MaterialPageRoute<void>(
        builder: (_) => DrillLevelsScreen(gateway: widget.gateway),
      ),
    );
  }

  void _openStudies() {
    Navigator.of(context).push(
      MaterialPageRoute<void>(
        builder: (_) => EndgameStudiesScreen(gateway: widget.gateway),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    return Scaffold(
      appBar: AppBar(title: Text(strings.trainingEndgameTitle)),
      body: FutureBuilder<TrainingOverview>(
        future: _overview,
        builder: (context, snapshot) {
          if (!snapshot.hasData) {
            return const Center(child: CircularProgressIndicator());
          }
          final overview = snapshot.data!;
          final exercises = overview.exercises
              .where((exercise) => exercise.category == 'endgame')
              .toList(growable: false);
          final summary = overview.category('endgame');
          return ListView(
            padding: const EdgeInsets.fromLTRB(16, 12, 16, 28),
            children: [
              Center(
                child: ConstrainedBox(
                  constraints: const BoxConstraints(maxWidth: 780),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.stretch,
                    children: [
                      ListTile(
                        title: Text(strings.practiceDrills),
                        trailing: const Icon(Icons.chevron_right),
                        onTap: _openCategory,
                      ),
                      ListTile(
                        title: Text(strings.practiceStudies),
                        trailing: const Icon(Icons.chevron_right),
                        onTap: _openStudies,
                      ),
                      Card(
                        child: Padding(
                          padding: const EdgeInsets.all(18),
                          child: TrainingMasteryProgress(
                            mastered: summary.mastered,
                            total: summary.total,
                          ),
                        ),
                      ),
                      const SizedBox(height: 16),
                      for (
                        var index = 0;
                        index < exercises.length;
                        index++
                      ) ...[
                        if (index > 0) const SizedBox(height: 12),
                        _ExerciseCard(
                          exercise: exercises[index],
                          threshold: overview.masteryThreshold,
                          onTap: () => _open(exercises[index]),
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

class _ExerciseCard extends StatelessWidget {
  const _ExerciseCard({
    required this.exercise,
    required this.threshold,
    required this.onTap,
  });

  final TrainingExercise exercise;
  final int threshold;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final theme = Theme.of(context);
    final progress = exercise.progress;
    return Card(
      child: ListTile(
        onTap: onTap,
        contentPadding: const EdgeInsets.all(16),
        title: Text(
          trainingExerciseTitle(strings, exercise.id),
          style: theme.textTheme.titleSmall?.copyWith(
            fontWeight: FontWeight.w800,
          ),
        ),
        subtitle: Padding(
          padding: const EdgeInsets.only(top: 8),
          child: Text(trainingExerciseHint(strings, exercise.id)),
        ),
        trailing: _MasteryBadge(progress: progress, threshold: threshold),
      ),
    );
  }
}

class _MasteryBadge extends StatelessWidget {
  const _MasteryBadge({required this.progress, required this.threshold});

  final ExerciseProgress progress;
  final int threshold;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final color = progress.isMastered
        ? AppTheme.success
        : Theme.of(context).colorScheme.onSurfaceVariant;
    return Row(
      mainAxisSize: MainAxisSize.min,
      children: [
        Icon(
          progress.isMastered ? Icons.star_rounded : Icons.timeline,
          size: 16,
          color: color,
        ),
        const SizedBox(width: 5),
        Text(
          progress.isMastered
              ? strings.trainingMastered
              : strings.trainingStreak(progress.successStreak, threshold),
          style: TextStyle(color: color, fontWeight: FontWeight.w700),
        ),
      ],
    );
  }
}
