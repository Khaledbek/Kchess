part of '../../../ui/app_root.dart';

/// "Nach Spielphase": the profile's win rate in each phase a game ends in
/// (opening / middlegame / endgame), by ending move number. Each phase shows a
/// prominent win/draw/loss bar and win rate so the user can see at a glance
/// where they are strongest and weakest. A "where games conclude" heuristic,
/// not engine-based blunder finding.
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
            const SizedBox(height: 14),
            FutureBuilder<PhaseStats>(
              future: future,
              builder: (context, snapshot) {
                if (snapshot.connectionState != ConnectionState.done) {
                  return const SizedBox(
                    height: 140,
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
    // Stable phase order so colours and positions never shuffle.
    const order = ['opening', 'middlegame', 'endgame'];
    final byPhase = {for (final p in stats.phases) p.phase: p.tally};

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        for (var i = 0; i < order.length; i++) ...[
          if (i > 0) const SizedBox(height: 14),
          _PhaseRow(
            phase: order[i],
            tally: byPhase[order[i]] ?? const StatTally(),
            labels: labels,
          ),
        ],
        if (stats.classified < stats.totalGames) ...[
          const SizedBox(height: 14),
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
    final winRate = tally.games > 0 ? tally.wins / tally.games : null;

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        // Line 1: phase dot + label (left), win rate + game count (right).
        Row(
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
                style: const TextStyle(fontWeight: FontWeight.w700),
              ),
            ),
            const SizedBox(width: 8),
            if (tally.games > 0)
              Text.rich(
                TextSpan(
                  children: [
                    TextSpan(
                      text: _percent(tally.wins, tally.games),
                      style: theme.textTheme.titleSmall?.copyWith(
                        fontWeight: FontWeight.w800,
                        color: _rateColor(context, winRate),
                      ),
                    ),
                    TextSpan(
                      text:
                          ' ${labels.winWord} · ${tally.games} ${labels.games}',
                      style: theme.textTheme.bodySmall?.copyWith(
                        color: scheme.onSurfaceVariant,
                      ),
                    ),
                  ],
                ),
              )
            else
              Text(
                '${tally.games} ${labels.games}',
                style: theme.textTheme.bodySmall?.copyWith(
                  color: scheme.onSurfaceVariant,
                ),
              ),
          ],
        ),
        const SizedBox(height: 6),
        // Line 2: full-width green/grey/red segmented win/draw/loss bar.
        _WinLossDrawRatioBar(
          wins: tally.wins,
          draws: tally.draws,
          losses: tally.losses,
          height: 6,
        ),
      ],
    );
  }
}

Color _rateColor(BuildContext context, double? rate) {
  if (rate == null) return Theme.of(context).colorScheme.onSurfaceVariant;
  if (rate >= 0.5) return AppTheme.success;
  if (rate < 0.45) return Theme.of(context).colorScheme.error;
  return Theme.of(context).colorScheme.onSurface;
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
    required this.winWord,
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
  final String winWord;
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
        subtitle: 'في أي مرحلة تنتهي مبارياتك وكيف تكون نتيجتك.',
        opening: 'الافتتاح (1–12)',
        middlegame: 'وسط اللعب (13–30)',
        endgame: 'النهاية (+31)',
        games: 'مباراة',
        winWord: 'فوز',
        empty: 'لا توجد بيانات كافية عن مراحل اللعب.',
        noProfile: 'أنشئ أو اختر ملفًا شخصيًا لعرض الإحصاءات.',
        error: 'تعذّر تحميل مراحل اللعب.',
        retry: 'إعادة المحاولة',
        classifiedNote: (classified, total) => '‏$classified من $total مباراة',
      );
    case 'en':
      return _PhaseText(
        title: 'By game phase',
        subtitle: 'Where your games end and how you score there.',
        opening: 'Opening (1–12)',
        middlegame: 'Middlegame (13–30)',
        endgame: 'Endgame (31+)',
        games: 'games',
        winWord: 'win',
        empty: 'Not enough data on game phases.',
        noProfile: 'Create or select a profile to see statistics.',
        error: 'Could not load game phases.',
        retry: 'Retry',
        classifiedNote: (classified, total) => '$classified of $total games',
      );
    default:
      return _PhaseText(
        title: 'Nach Spielphase',
        subtitle: 'In welcher Phase deine Partien enden und wie du abschneidest.',
        opening: 'Eröffnung (1–12)',
        middlegame: 'Mittelspiel (13–30)',
        endgame: 'Endspiel (31+)',
        games: 'Partien',
        winWord: 'Sieg',
        empty: 'Nicht genügend Daten zu Spielphasen.',
        noProfile: 'Erstelle oder wähle ein Profil, um Statistiken zu sehen.',
        error: 'Spielphasen konnten nicht geladen werden.',
        retry: 'Erneut versuchen',
        classifiedNote: (classified, total) => '$classified von $total Partien',
      );
  }
}
