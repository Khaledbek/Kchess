part of '../../../ui/app_root.dart';

/// "Partie-Ende Statistik": how the profile's games end (checkmate, resignation,
/// on time, draw, other) — a category proportion bar plus an accordion that
/// drills into the win/draw/loss split within each termination type.
class _TerminationCard extends StatelessWidget {
  const _TerminationCard({required this.future, required this.onRetry});

  final Future<TerminationStats> future;
  final VoidCallback onRetry;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final labels = _terminationText(context);
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(18),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                Icon(Icons.flag_outlined, color: theme.colorScheme.primary),
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
            const SizedBox(height: 16),
            FutureBuilder<TerminationStats>(
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
                if (stats.isEmpty || stats.totalGames == 0) {
                  return _OverviewMessage(
                    icon: Icons.flag_outlined,
                    text: labels.empty,
                  );
                }
                return _TerminationContent(stats: stats, labels: labels);
              },
            ),
          ],
        ),
      ),
    );
  }
}

class _TerminationContent extends StatelessWidget {
  const _TerminationContent({required this.stats, required this.labels});

  final TerminationStats stats;
  final _TerminationText labels;

  @override
  Widget build(BuildContext context) {
    // Fixed order so colours/positions stay stable regardless of the payload.
    const order = ['checkmate', 'resignation', 'timeout', 'draw', 'other'];
    final byType = {for (final t in stats.terminations) t.type: t};
    final entries = [
      for (final type in order)
        if ((byType[type]?.count ?? 0) > 0) byType[type]!,
    ];
    final total = stats.totalGames;

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        ClipRRect(
          borderRadius: BorderRadius.circular(8),
          child: SizedBox(
            height: 14,
            child: Row(
              children: [
                for (final entry in entries)
                  Expanded(
                    flex: entry.count,
                    child: Tooltip(
                      message:
                          '${labels.label(entry.type)} · ${entry.count} (${_percent(entry.count, total)})',
                      child: ColoredBox(
                        color: _terminationColor(context, entry.type),
                      ),
                    ),
                  ),
              ],
            ),
          ),
        ),
        const SizedBox(height: 10),
        for (final entry in entries)
          _TerminationTile(
            termination: entry,
            totalGames: total,
            labels: labels,
          ),
      ],
    );
  }
}

/// One termination category as a drill-down row: a header (category, total,
/// share of all games, win/loss balance bar) that expands to the win / draw /
/// loss breakdown within the category. Background stays seamless with the card.
class _TerminationTile extends StatefulWidget {
  const _TerminationTile({
    required this.termination,
    required this.totalGames,
    required this.labels,
  });

  final GameTermination termination;
  final int totalGames;
  final _TerminationText labels;

  @override
  State<_TerminationTile> createState() => _TerminationTileState();
}

class _TerminationTileState extends State<_TerminationTile> {
  bool _expanded = false;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;
    final labels = widget.labels;
    final termination = widget.termination;
    final tally = termination.tally;
    // Only worth expanding when there is more than one outcome to reveal.
    final outcomeKinds =
        [tally.wins > 0, tally.draws > 0, tally.losses > 0].where((b) => b).length;
    final expandable = outcomeKinds >= 2;

    final header = Padding(
      padding: const EdgeInsets.symmetric(vertical: 8),
      child: Row(
        children: [
          Container(
            width: 12,
            height: 12,
            decoration: BoxDecoration(
              color: _terminationColor(context, termination.type),
              borderRadius: BorderRadius.circular(3),
            ),
          ),
          const SizedBox(width: 10),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Row(
                  children: [
                    Expanded(
                      child: Text(
                        labels.label(termination.type),
                        style: const TextStyle(fontWeight: FontWeight.w600),
                      ),
                    ),
                    Text(
                      '${tally.games} · ${_percent(tally.games, widget.totalGames)}',
                      style: theme.textTheme.bodySmall?.copyWith(
                        color: scheme.onSurfaceVariant,
                      ),
                    ),
                  ],
                ),
                const SizedBox(height: 8),
                _WinLossDrawRatioBar(
                  wins: tally.wins,
                  draws: tally.draws,
                  losses: tally.losses,
                ),
              ],
            ),
          ),
          SizedBox(
            width: 26,
            child: expandable
                ? AnimatedRotation(
                    turns: _expanded ? 0.5 : 0,
                    duration: const Duration(milliseconds: 180),
                    child: Icon(
                      Icons.expand_more,
                      color: scheme.onSurfaceVariant,
                    ),
                  )
                : null,
          ),
        ],
      ),
    );

    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        if (expandable)
          InkWell(
            onTap: () => setState(() => _expanded = !_expanded),
            borderRadius: BorderRadius.circular(10),
            child: header,
          )
        else
          header,
        AnimatedCrossFade(
          duration: const Duration(milliseconds: 180),
          crossFadeState:
              _expanded ? CrossFadeState.showSecond : CrossFadeState.showFirst,
          firstChild: const SizedBox(width: double.infinity),
          secondChild: Padding(
            padding: const EdgeInsets.only(left: 22, bottom: 6),
            child: Column(
              children: [
                if (tally.wins > 0)
                  _TerminationSubRow(
                    color: _kWinColor,
                    label: labels.outcomeLabel(termination.type, 'win'),
                    count: tally.wins,
                    categoryTotal: tally.games,
                  ),
                if (tally.draws > 0)
                  _TerminationSubRow(
                    color: _kDrawColor,
                    label: labels.outcomeLabel(termination.type, 'draw'),
                    count: tally.draws,
                    categoryTotal: tally.games,
                  ),
                if (tally.losses > 0)
                  _TerminationSubRow(
                    color: _kLossColor,
                    label: labels.outcomeLabel(termination.type, 'loss'),
                    count: tally.losses,
                    categoryTotal: tally.games,
                  ),
              ],
            ),
          ),
        ),
      ],
    );
  }
}

class _TerminationSubRow extends StatelessWidget {
  const _TerminationSubRow({
    required this.color,
    required this.label,
    required this.count,
    required this.categoryTotal,
  });

  final Color color;
  final String label;
  final int count;
  final int categoryTotal;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 5),
      child: Row(
        children: [
          Container(
            width: 10,
            height: 10,
            decoration: BoxDecoration(color: color, shape: BoxShape.circle),
          ),
          const SizedBox(width: 10),
          Expanded(
            child: Text(label, style: theme.textTheme.bodyMedium),
          ),
          Text(
            '$count · ${_percent(count, categoryTotal)}',
            style: theme.textTheme.bodySmall?.copyWith(
              color: theme.colorScheme.onSurfaceVariant,
            ),
          ),
        ],
      ),
    );
  }
}

Color _terminationColor(BuildContext context, String type) {
  final scheme = Theme.of(context).colorScheme;
  return switch (type) {
    'checkmate' => scheme.primary,
    'resignation' => AppTheme.warning,
    'timeout' => const Color(0xFF7C6FF0),
    'draw' => scheme.onSurfaceVariant,
    _ => scheme.outline,
  };
}

String _percent(int count, int total) =>
    total == 0 ? '0%' : '${(count / total * 100).round()}%';

class _TerminationText {
  const _TerminationText({
    required this.title,
    required this.checkmate,
    required this.resignation,
    required this.timeout,
    required this.draw,
    required this.other,
    required this.wonByCheckmate,
    required this.lostByCheckmate,
    required this.opponentResigned,
    required this.selfResigned,
    required this.opponentFlagged,
    required this.selfFlagged,
    required this.wonGeneric,
    required this.lostGeneric,
    required this.empty,
    required this.noProfile,
    required this.error,
    required this.retry,
  });

  final String title;
  final String checkmate;
  final String resignation;
  final String timeout;
  final String draw;
  final String other;
  final String wonByCheckmate;
  final String lostByCheckmate;
  final String opponentResigned;
  final String selfResigned;
  final String opponentFlagged;
  final String selfFlagged;
  final String wonGeneric;
  final String lostGeneric;
  final String empty;
  final String noProfile;
  final String error;
  final String retry;

  String label(String type) => switch (type) {
    'checkmate' => checkmate,
    'resignation' => resignation,
    'timeout' => timeout,
    'draw' => draw,
    _ => other,
  };

  /// Human sub-row label for a category × outcome (win/draw/loss).
  String outcomeLabel(String type, String outcome) {
    if (outcome == 'draw') return draw;
    final win = outcome == 'win';
    return switch (type) {
      'checkmate' => win ? wonByCheckmate : lostByCheckmate,
      'resignation' => win ? opponentResigned : selfResigned,
      'timeout' => win ? opponentFlagged : selfFlagged,
      _ => win ? wonGeneric : lostGeneric,
    };
  }
}

_TerminationText _terminationText(BuildContext context) {
  switch (Localizations.localeOf(context).languageCode) {
    case 'ar':
      return const _TerminationText(
        title: 'طريقة انتهاء المباريات',
        checkmate: 'كش ملك',
        resignation: 'استسلام',
        timeout: 'انتهاء الوقت',
        draw: 'تعادل',
        other: 'أخرى',
        wonByCheckmate: 'فوز بكش ملك',
        lostByCheckmate: 'خسارة بكش ملك',
        opponentResigned: 'استسلم الخصم',
        selfResigned: 'استسلمت',
        opponentFlagged: 'نفد وقت الخصم',
        selfFlagged: 'نفد وقتك',
        wonGeneric: 'فوز',
        lostGeneric: 'خسارة',
        empty: 'لا توجد بيانات كافية عن نهايات المباريات.',
        noProfile: 'أنشئ أو اختر ملفًا شخصيًا لعرض الإحصاءات.',
        error: 'تعذّر تحميل نهايات المباريات.',
        retry: 'إعادة المحاولة',
      );
    case 'en':
      return const _TerminationText(
        title: 'How games end',
        checkmate: 'Checkmate',
        resignation: 'Resignation',
        timeout: 'On time',
        draw: 'Draw',
        other: 'Other',
        wonByCheckmate: 'Won by checkmate',
        lostByCheckmate: 'Lost by checkmate',
        opponentResigned: 'Opponent resigned',
        selfResigned: 'Resigned',
        opponentFlagged: 'Opponent ran out of time',
        selfFlagged: 'Ran out of time',
        wonGeneric: 'Won',
        lostGeneric: 'Lost',
        empty: 'Not enough data on how games ended.',
        noProfile: 'Create or select a profile to see statistics.',
        error: 'Could not load game endings.',
        retry: 'Retry',
      );
    default:
      return const _TerminationText(
        title: 'Partie-Ende Statistik',
        checkmate: 'Matt',
        resignation: 'Aufgabe',
        timeout: 'Zeit',
        draw: 'Remis',
        other: 'Andere',
        wonByCheckmate: 'Gewonnen durch Matt',
        lostByCheckmate: 'Verloren durch Matt',
        opponentResigned: 'Gegner gab auf',
        selfResigned: 'Selbst aufgegeben',
        opponentFlagged: 'Gegner-Zeit abgelaufen',
        selfFlagged: 'Eigene Zeit abgelaufen',
        wonGeneric: 'Gewonnen',
        lostGeneric: 'Verloren',
        empty: 'Nicht genügend Daten zum Partie-Ende.',
        noProfile: 'Erstelle oder wähle ein Profil, um Statistiken zu sehen.',
        error: 'Partie-Enden konnten nicht geladen werden.',
        retry: 'Erneut versuchen',
      );
  }
}
