part of '../../../ui/app_root.dart';

/// "Partie-Ende Statistik": how the profile's games end (checkmate, resignation,
/// on time, draw, other), as a proportion bar plus a count/percentage list.
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
    final byType = {for (final t in stats.terminations) t.type: t.count};
    final entries = [
      for (final type in order)
        if ((byType[type] ?? 0) > 0) (type: type, count: byType[type]!),
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
        const SizedBox(height: 14),
        for (final entry in entries)
          _TerminationRow(
            type: entry.type,
            count: entry.count,
            total: total,
            labels: labels,
          ),
      ],
    );
  }
}

class _TerminationRow extends StatelessWidget {
  const _TerminationRow({
    required this.type,
    required this.count,
    required this.total,
    required this.labels,
  });

  final String type;
  final int count;
  final int total;
  final _TerminationText labels;

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
              color: _terminationColor(context, type),
              borderRadius: BorderRadius.circular(3),
            ),
          ),
          const SizedBox(width: 10),
          Expanded(
            child: Text(
              labels.label(type),
              style: const TextStyle(fontWeight: FontWeight.w600),
            ),
          ),
          Text(
            '$count · ${_percent(count, total)}',
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
        empty: 'Nicht genügend Daten zum Partie-Ende.',
        noProfile: 'Erstelle oder wähle ein Profil, um Statistiken zu sehen.',
        error: 'Partie-Enden konnten nicht geladen werden.',
        retry: 'Erneut versuchen',
      );
  }
}
