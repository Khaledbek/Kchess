part of '../../../ui/app_root.dart';

/// "Nach Spielphase": the profile's win rate in each phase a game ends in
/// (opening / middlegame / endgame), by ending move number. A single-glance
/// card — one row per phase with a win/draw/loss ratio bar and the win rate.
/// This is a "where games conclude" heuristic, not engine-based blunder finding.
class _PhaseCard extends StatelessWidget {
  const _PhaseCard({required this.future, required this.onRetry});

  final Future<PhaseStats> future;
  final VoidCallback onRetry;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final labels = _phaseText(context);
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(18),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                Icon(Icons.timeline, color: theme.colorScheme.primary),
                const SizedBox(width: 8),
                Expanded(
                  child: Text(
                    labels.title,
                    style: theme.textTheme.titleMedium?.copyWith(
                      fontWeight: FontWeight.w800,
                    ),
                  ),
                ),
              ],
            ),
            const SizedBox(height: 4),
            Text(
              labels.subtitle,
              style: theme.textTheme.bodySmall?.copyWith(
                color: theme.colorScheme.onSurfaceVariant,
              ),
            ),
            const SizedBox(height: 16),
            FutureBuilder<PhaseStats>(
              future: future,
              builder: (context, snapshot) {
                if (snapshot.connectionState != ConnectionState.done) {
                  return const SizedBox(
                    height: 120,
                    child: Center(child: CircularProgressIndicator()),
                  );
                }
                if (snapshot.hasError || !snapshot.hasData) {
                  return _OverviewMessage(
                    icon: Icons.error_outline,
                    text: labels.error,
                    action: TextButton(
                      onPressed: onRetry,
                      child: Text(labels.retry),
                    ),
                  );
                }
                final stats = snapshot.data!;
                if (!stats.hasProfile) {
                  return _OverviewMessage(
                    icon: Icons.person_outline,
                    text: labels.noProfile,
                  );
                }
                if (stats.isEmpty) {
                  return _OverviewMessage(
                    icon: Icons.timeline_outlined,
                    text: labels.empty,
                  );
                }
                return _PhaseContent(stats: stats, labels: labels);
              },
            ),
          ],
        ),
      ),
    );
  }
}

class _PhaseContent extends StatelessWidget {
  const _PhaseContent({required this.stats, required this.labels});

  final PhaseStats stats;
  final _PhaseText labels;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    // Stable phase order for colours and positions.
    const order = ['opening', 'middlegame', 'endgame'];
    final byPhase = {for (final p in stats.phases) p.phase: p.tally};

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        for (final phase in order)
          _PhaseRow(
            phase: phase,
            tally: byPhase[phase] ?? const StatTally(),
            labels: labels,
          ),
        if (stats.classified < stats.totalGames) ...[
          const SizedBox(height: 10),
          Text(
            labels.classifiedNote(stats.classified, stats.totalGames),
            style: theme.textTheme.bodySmall?.copyWith(
              color: theme.colorScheme.onSurfaceVariant,
            ),
          ),
        ],
      ],
    );
  }
}

class _PhaseRow extends StatelessWidget {
  const _PhaseRow({
    required this.phase,
    required this.tally,
    required this.labels,
  });

  final String phase;
  final StatTally tally;
  final _PhaseText labels;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 8),
      child: Row(
        children: [
          // Left: phase dot + label with move range.
          SizedBox(
            width: 156,
            child: Row(
              children: [
                Container(
                  width: 12,
                  height: 12,
                  decoration: BoxDecoration(
                    color: _phaseColor(phase),
                    borderRadius: BorderRadius.circular(3),
                  ),
                ),
                const SizedBox(width: 8),
                Expanded(
                  child: Text(
                    labels.phase(phase),
                    maxLines: 1,
                    overflow: TextOverflow.ellipsis,
                    style: const TextStyle(fontWeight: FontWeight.w600),
                  ),
                ),
              ],
            ),
          ),
          const SizedBox(width: 12),
          // Center: win/draw/loss proportion for this phase.
          Expanded(
            child: _WinLossDrawRatioBar(
              wins: tally.wins,
              draws: tally.draws,
              losses: tally.losses,
              height: 8,
            ),
          ),
          const SizedBox(width: 12),
          // Right: win rate (wins / total) and sample size.
          SizedBox(
            width: 128,
            child: Text(
              '${_percent(tally.wins, tally.games)} · ${tally.games} ${labels.games}',
              textAlign: TextAlign.end,
              maxLines: 1,
              overflow: TextOverflow.ellipsis,
              style: theme.textTheme.bodySmall?.copyWith(
                color: scheme.onSurfaceVariant,
              ),
            ),
          ),
        ],
      ),
    );
  }
}

Color _phaseColor(String phase) => switch (phase) {
  'opening' => const Color(0xFF38BDF8), // sky
  'middlegame' => const Color(0xFFFB923C), // orange
  'endgame' => const Color(0xFFA78BFA), // violet
  _ => const Color(0xFF64748B),
};

// Note: `_percent` is shared from termination_section.dart (same library).

class _PhaseText {
  const _PhaseText({
    required this.title,
    required this.subtitle,
    required this.opening,
    required this.middlegame,
    required this.endgame,
    required this.games,
    required this.empty,
    required this.noProfile,
    required this.error,
    required this.retry,
    required this.classifiedNote,
  });

  final String title;
  final String subtitle;
  final String opening;
  final String middlegame;
  final String endgame;
  final String games;
  final String empty;
  final String noProfile;
  final String error;
  final String retry;
  final String Function(int classified, int total) classifiedNote;

  String phase(String phase) => switch (phase) {
    'opening' => opening,
    'middlegame' => middlegame,
    'endgame' => endgame,
    _ => phase,
  };
}

_PhaseText _phaseText(BuildContext context) {
  switch (Localizations.localeOf(context).languageCode) {
    case 'ar':
      return _PhaseText(
        title: 'حسب مرحلة اللعب',
        subtitle: 'المرحلة التي تنتهي فيها المباريات.',
        opening: 'الافتتاح (1–12)',
        middlegame: 'وسط اللعب (13–30)',
        endgame: 'النهاية (+31)',
        games: 'مباراة',
        empty: 'لا توجد بيانات كافية عن مراحل اللعب.',
        noProfile: 'أنشئ أو اختر ملفًا شخصيًا لعرض الإحصاءات.',
        error: 'تعذّر تحميل مراحل اللعب.',
        retry: 'إعادة المحاولة',
        classifiedNote: (classified, total) => '‏$classified من $total مباراة',
      );
    case 'en':
      return _PhaseText(
        title: 'By game phase',
        subtitle: 'The phase your games end in.',
        opening: 'Opening (1–12)',
        middlegame: 'Middlegame (13–30)',
        endgame: 'Endgame (31+)',
        games: 'games',
        empty: 'Not enough data on game phases.',
        noProfile: 'Create or select a profile to see statistics.',
        error: 'Could not load game phases.',
        retry: 'Retry',
        classifiedNote: (classified, total) => '$classified of $total games',
      );
    default:
      return _PhaseText(
        title: 'Nach Spielphase',
        subtitle: 'In welcher Phase deine Partien enden.',
        opening: 'Eröffnung (1–12)',
        middlegame: 'Mittelspiel (13–30)',
        endgame: 'Endspiel (31+)',
        games: 'Partien',
        empty: 'Nicht genügend Daten zu Spielphasen.',
        noProfile: 'Erstelle oder wähle ein Profil, um Statistiken zu sehen.',
        error: 'Spielphasen konnten nicht geladen werden.',
        retry: 'Erneut versuchen',
        classifiedNote: (classified, total) => '$classified von $total Partien',
      );
  }
}
