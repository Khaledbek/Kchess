import 'package:flutter/material.dart';

import '../../../ffi/core_gateway.dart';
import '../../../localization/generated/app_localizations.dart';
import '../../../services/training_progress_service.dart';
import '../../../shared/theme/app_theme.dart';
import '../data/endgame_studies_library.dart';
import '../models/endgame_study.dart';
import '../models/training_progress.dart';
import 'endgame_drill_player.dart' show DrillDifficultyChip;
import 'endgame_study_player.dart';
import 'training_arena_screen.dart' show MasteryProgressBar;

/// Displays the collection of 86 classical endgame studies from Kling & Horwitz (1851),
/// organised by material category.
class EndgameStudiesScreen extends StatefulWidget {
  const EndgameStudiesScreen({
    required this.gateway,
    required this.progress,
    super.key,
  });

  final CoreGateway gateway;
  final TrainingProgressService progress;

  @override
  State<EndgameStudiesScreen> createState() => _EndgameStudiesScreenState();
}

class _EndgameStudiesScreenState extends State<EndgameStudiesScreen> {
  late Future<EndgameStudyCatalogue> _catalogueFuture;

  @override
  void initState() {
    super.initState();
    widget.progress.load();
    _catalogueFuture = EndgameStudiesLibrary.load();
  }

  void _openStudy(EndgameStudySection section, EndgameStudy study) {
    Navigator.of(context).push(
      MaterialPageRoute<void>(
        builder: (_) => EndgameStudyPlayer(
          study: study,
          section: section,
          gateway: widget.gateway,
          progress: widget.progress,
        ),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;

    return Scaffold(
      appBar: AppBar(title: Text(strings.trainingStudiesTitle)),
      body: FutureBuilder<EndgameStudyCatalogue>(
        future: _catalogueFuture,
        builder: (context, catalogueSnapshot) {
          if (catalogueSnapshot.connectionState != ConnectionState.done ||
              !catalogueSnapshot.hasData) {
            return const Center(child: CircularProgressIndicator());
          }
          final catalogue = catalogueSnapshot.data!;

          return StreamBuilder<TrainingProgressSnapshot>(
            stream: widget.progress.changes,
            initialData: widget.progress.snapshot,
            builder: (context, progressSnapshot) {
              final progress =
                  progressSnapshot.data ??
                  const TrainingProgressSnapshot.empty();

              var masteredCount = 0;
              for (final section in catalogue.sections) {
                for (final study in section.studies) {
                  if (progress.isMastered(study.id)) masteredCount++;
                }
              }

              return ListView(
                padding: const EdgeInsets.fromLTRB(16, 12, 16, 28),
                children: [
                  Center(
                    child: ConstrainedBox(
                      constraints: const BoxConstraints(maxWidth: 780),
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.stretch,
                        children: [
                          Card(
                            child: Padding(
                              padding: const EdgeInsets.all(18),
                              child: Column(
                                crossAxisAlignment: CrossAxisAlignment.start,
                                children: [
                                  Row(
                                    children: [
                                      Container(
                                        padding: const EdgeInsets.all(10),
                                        decoration: BoxDecoration(
                                          color: scheme.primaryContainer,
                                          borderRadius: BorderRadius.circular(
                                            12,
                                          ),
                                        ),
                                        child: Icon(
                                          Icons.menu_book_rounded,
                                          color: scheme.onPrimaryContainer,
                                        ),
                                      ),
                                      const SizedBox(width: 14),
                                      Expanded(
                                        child: Column(
                                          crossAxisAlignment:
                                              CrossAxisAlignment.start,
                                          children: [
                                            Text(
                                              catalogue.sourceTitle.isNotEmpty
                                                  ? catalogue.sourceTitle
                                                  : 'Chess Studies (1851)',
                                              style: theme.textTheme.titleMedium
                                                  ?.copyWith(
                                                    fontWeight: FontWeight.w800,
                                                  ),
                                            ),
                                            const SizedBox(height: 3),
                                            Text(
                                              catalogue.sourceAuthors.isNotEmpty
                                                  ? '${catalogue.sourceAuthors} (${catalogue.sourceYear})'
                                                  : 'Josef Kling & Bernhard Horwitz (1851)',
                                              style: theme.textTheme.bodySmall
                                                  ?.copyWith(
                                                    color:
                                                        scheme.onSurfaceVariant,
                                                  ),
                                            ),
                                          ],
                                        ),
                                      ),
                                    ],
                                  ),
                                  const SizedBox(height: 16),
                                  MasteryProgressBar(
                                    mastered: masteredCount,
                                    total: catalogue.studyCount,
                                  ),
                                ],
                              ),
                            ),
                          ),
                          const SizedBox(height: 16),
                          for (final section in catalogue.sections) ...[
                            _SectionCard(
                              section: section,
                              progress: progress,
                              onOpenStudy: (study) =>
                                  _openStudy(section, study),
                            ),
                            const SizedBox(height: 12),
                          ],
                        ],
                      ),
                    ),
                  ),
                ],
              );
            },
          );
        },
      ),
    );
  }
}

class _SectionCard extends StatelessWidget {
  const _SectionCard({
    required this.section,
    required this.progress,
    required this.onOpenStudy,
  });

  final EndgameStudySection section;
  final TrainingProgressSnapshot progress;
  final ValueChanged<EndgameStudy> onOpenStudy;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;
    final mastered = section.studies
        .where((study) => progress.isMastered(study.id))
        .length;

    return Card(
      child: ExpansionTile(
        key: Key('study-section-${section.id}'),
        leading: Container(
          padding: const EdgeInsets.all(8),
          decoration: BoxDecoration(
            color: scheme.surfaceContainerHigh,
            borderRadius: BorderRadius.circular(10),
          ),
          child: Icon(Icons.school_outlined, color: scheme.primary, size: 20),
        ),
        title: Text(
          section.title,
          style: theme.textTheme.titleSmall?.copyWith(
            fontWeight: FontWeight.w800,
          ),
        ),
        subtitle: Padding(
          padding: const EdgeInsets.only(top: 4),
          child: Text(
            strings.trainingCategoryProgress(mastered, section.studies.length),
            style: theme.textTheme.bodySmall?.copyWith(
              color: scheme.onSurfaceVariant,
              fontWeight: FontWeight.w600,
            ),
          ),
        ),
        childrenPadding: const EdgeInsets.fromLTRB(16, 0, 16, 16),
        children: [
          if (section.description.isNotEmpty) ...[
            Align(
              alignment: AlignmentDirectional.centerStart,
              child: Text(
                section.description,
                style: theme.textTheme.bodySmall?.copyWith(
                  color: scheme.onSurfaceVariant,
                ),
              ),
            ),
            const SizedBox(height: 12),
          ],
          for (var i = 0; i < section.studies.length; i++) ...[
            if (i > 0) const SizedBox(height: 8),
            _StudyRow(
              study: section.studies[i],
              progress: progress.progressFor(section.studies[i].id),
              onPlay: () => onOpenStudy(section.studies[i]),
            ),
          ],
        ],
      ),
    );
  }
}

class _StudyRow extends StatelessWidget {
  const _StudyRow({
    required this.study,
    required this.progress,
    required this.onPlay,
  });

  final EndgameStudy study;
  final ExerciseProgress progress;
  final VoidCallback onPlay;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;

    return DecoratedBox(
      decoration: BoxDecoration(
        color: scheme.surfaceContainerLow,
        borderRadius: BorderRadius.circular(12),
        border: Border.all(color: scheme.outlineVariant),
      ),
      child: InkWell(
        onTap: onPlay,
        borderRadius: BorderRadius.circular(12),
        child: Padding(
          padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 10),
          child: Row(
            children: [
              Icon(Icons.play_circle_outline, size: 20, color: scheme.primary),
              const SizedBox(width: 10),
              Expanded(
                child: Text(
                  strings.trainingStudyNumber(study.number),
                  style: const TextStyle(fontWeight: FontWeight.w700),
                ),
              ),
              const SizedBox(width: 8),
              DrillDifficultyChip(difficulty: study.difficulty),
              const SizedBox(width: 8),
              _StudyBadge(progress: progress),
            ],
          ),
        ),
      ),
    );
  }
}

class _StudyBadge extends StatelessWidget {
  const _StudyBadge({required this.progress});

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
