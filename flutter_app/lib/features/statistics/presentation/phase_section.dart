part of '../../../ui/app_root.dart';

/// "Nach Spielphase": distribution of the profile's games across the phase in
/// which they ended (opening / middlegame / endgame), with an All ↔ Losses
/// toggle. This is a "where games conclude" heuristic from the ending move
/// number — not an engine-based blunder location.
class _PhaseCard extends StatefulWidget {
  const _PhaseCard({required this.future, required this.onRetry});

  final Future<PhaseStats> future;
  final VoidCallback onRetry;

  @override
  State<_PhaseCard> createState() => _PhaseCardState();
}

class _PhaseCardState extends State<_PhaseCard> {
  bool _lossesOnly = true;

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
            const SizedBox(height: 12),
            SizedBox(
              width: double.infinity,
              child: SegmentedButton<bool>(
                showSelectedIcon: false,
                segments: [
                  ButtonSegment(value: true, label: Text(labels.losses)),
                  ButtonSegment(value: false, label: Text(labels.all)),
                ],
                selected: {_lossesOnly},
                onSelectionChanged: (selection) =>
                    setState(() => _lossesOnly = selection.first),
              ),
            ),
            const SizedBox(height: 14),
            FutureBuilder<PhaseStats>(
              future: widget.future,
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
                      onPressed: widget.onRetry,
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
                return _PhaseContent(
                  stats: stats,
                  lossesOnly: _lossesOnly,
                  labels: labels,
                );
              },
            ),
          ],
        ),
      ),
    );
  }
}

class _PhaseContent extends StatelessWidget {
  const _PhaseContent({
    required this.stats,
    required this.lossesOnly,
    required this.labels,
  });

  final PhaseStats stats;
  final bool lossesOnly;
  final _PhaseText labels;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    // Stable phase order for colours and positions.
    const order = ['opening', 'middlegame', 'endgame'];
    final byPhase = {for (final p in stats.phases) p.phase: p.tally};
    final entries = [
      for (final phase in order)
        (
          phase: phase,
          value: lossesOnly
              ? (byPhase[phase]?.losses ?? 0)
              : (byPhase[phase]?.games ?? 0),
        ),
    ];
    final total = entries.fold(0, (sum, e) => sum + e.value);

    if (total == 0) {
      return _OverviewMessage(
        icon: lossesOnly ? Icons.sentiment_satisfied_outlined : Icons.timeline_outlined,
        text: lossesOnly ? labels.noLosses : labels.empty,
      );
    }

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        ClipRRect(
          borderRadius: BorderRadius.circular(3),
          child: SizedBox(
            height: 14,
            child: Row(
              children: [
                for (final entry in entries)
                  if (entry.value > 0)
                    Expanded(
                      flex: entry.value,
                      child: Tooltip(
                        message:
                            '${labels.phase(entry.phase)} · ${entry.value} (${_percent(entry.value, total)})',
                        child: ColoredBox(color: _phaseColor(entry.phase)),
                      ),
                    ),
              ],
            ),
          ),
        ),
        const SizedBox(height: 14),
        for (final entry in entries)
          _PhaseRow(
            phase: entry.phase,
            value: entry.value,
            total: total,
            labels: labels,
          ),
        if (stats.classified < stats.totalGames) ...[
          const SizedBox(height: 8),
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
    required this.value,
    required this.total,
    required this.labels,
  });

  final String phase;
  final int value;
  final int total;
  final _PhaseText labels;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 5),
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
          const SizedBox(width: 10),
          Expanded(
            child: Text(
              labels.phase(phase),
              style: const TextStyle(fontWeight: FontWeight.w600),
            ),
          ),
          Text(
            '$value · ${_percent(value, total)}',
            style: theme.textTheme.bodySmall?.copyWith(
              color: theme.colorScheme.onSurfaceVariant,
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
    required this.all,
    required this.losses,
    required this.opening,
    required this.middlegame,
    required this.endgame,
    required this.noLosses,
    required this.empty,
    required this.noProfile,
    required this.error,
    required this.retry,
    required this.classifiedNote,
  });

  final String title;
  final String subtitle;
  final String all;
  final String losses;
  final String opening;
  final String middlegame;
  final String endgame;
  final String noLosses;
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
        all: 'الكل',
        losses: 'الهزائم',
        opening: 'الافتتاح (1–12)',
        middlegame: 'وسط اللعب (13–30)',
        endgame: 'النهاية (+31)',
        noLosses: 'لا توجد هزائم لعرضها.',
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
        all: 'All',
        losses: 'Losses',
        opening: 'Opening (1–12)',
        middlegame: 'Middlegame (13–30)',
        endgame: 'Endgame (31+)',
        noLosses: 'No losses to show.',
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
        all: 'Alle',
        losses: 'Niederlagen',
        opening: 'Eröffnung (1–12)',
        middlegame: 'Mittelspiel (13–30)',
        endgame: 'Endspiel (31+)',
        noLosses: 'Keine Niederlagen vorhanden.',
        empty: 'Nicht genügend Daten zu Spielphasen.',
        noProfile: 'Erstelle oder wähle ein Profil, um Statistiken zu sehen.',
        error: 'Spielphasen konnten nicht geladen werden.',
        retry: 'Erneut versuchen',
        classifiedNote: (classified, total) => '$classified von $total Partien',
      );
  }
}
