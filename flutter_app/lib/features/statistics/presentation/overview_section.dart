part of '../../../ui/app_root.dart';

class _OverviewCard extends StatelessWidget {
  const _OverviewCard({
    required this.future,
    required this.timeControl,
    required this.onRetry,
  });

  final Future<StatisticsOverview> future;
  final String timeControl;
  final VoidCallback onRetry;

  @override
  Widget build(BuildContext context) {
    final labels = _overviewText(context);
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(18),
        child: FutureBuilder<StatisticsOverview>(
          future: future,
          builder: (context, snapshot) {
            if (snapshot.connectionState != ConnectionState.done) {
              return const SizedBox(
                height: 180,
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
            final overview = snapshot.data!;
            if (!overview.hasProfile) {
              return _OverviewMessage(
                icon: Icons.person_outline,
                text: labels.noProfile,
              );
            }
            if (overview.isEmpty) {
              return _OverviewMessage(
                icon: Icons.insights_outlined,
                text: labels.empty,
              );
            }
            return _OverviewContent(
              overview: overview,
              timeControl: timeControl,
              labels: labels,
            );
          },
        ),
      ),
    );
  }
}

class _OverviewMessage extends StatelessWidget {
  const _OverviewMessage({required this.icon, required this.text, this.action});

  final IconData icon;
  final String text;
  final Widget? action;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 24),
      child: Column(
        children: [
          Icon(icon, size: 40, color: theme.colorScheme.onSurfaceVariant),
          const SizedBox(height: 12),
          Text(
            text,
            textAlign: TextAlign.center,
            style: theme.textTheme.bodyMedium?.copyWith(
              color: theme.colorScheme.onSurfaceVariant,
            ),
          ),
          if (action != null) ...[const SizedBox(height: 8), action!],
        ],
      ),
    );
  }
}

class _OverviewContent extends StatelessWidget {
  const _OverviewContent({
    required this.overview,
    required this.timeControl,
    required this.labels,
  });

  final StatisticsOverview overview;
  final String timeControl;
  final _OverviewText labels;

  /// The tally the headline reflects: overall for "all", otherwise the matching
  /// pre-aggregated time-control bucket (null when that bucket has no games).
  StatTally? get _activeTally {
    if (timeControl == 'all') return overview.overall;
    for (final control in overview.byTimeControl) {
      if (control.type == timeControl) return control.tally;
    }
    return null;
  }

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final isAll = timeControl == 'all';
    final tally = _activeTally;

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Row(
          children: [
            Icon(Icons.dashboard_outlined, color: theme.colorScheme.primary),
            const SizedBox(width: 8),
            Expanded(
              child: Text(
                labels.title,
                style: theme.textTheme.titleMedium?.copyWith(
                  fontWeight: FontWeight.w800,
                ),
              ),
            ),
            if (!isAll)
              _FilterPill(
                label: _statsLabels(context).timeControl(timeControl),
              ),
          ],
        ),
        const SizedBox(height: 16),
        if (tally == null || tally.games == 0)
          _OverviewMessage(
            icon: Icons.filter_alt_off_outlined,
            text: labels.noGamesForFilter,
          )
        else ...[
          _StatTileGrid(
            tiles: [
              _StatTile(
                value: '${tally.games}',
                label: labels.games,
                icon: Icons.tag,
              ),
              _StatTile(
                value: _formatPercent(tally.winRate),
                label: labels.winRate,
                icon: Icons.emoji_events_outlined,
                accent: AppTheme.success,
              ),
              _StatTile(
                value: _formatPercent(tally.scorePercent),
                label: labels.score,
                icon: Icons.speed_outlined,
              ),
              _StatTile(
                value: _formatRecord(tally),
                label: labels.record,
                icon: Icons.military_tech_outlined,
              ),
            ],
          ),
          const SizedBox(height: 18),
          _WinLossDrawBar(tally: tally, height: 14),
          const SizedBox(height: 10),
          _WdlLegend(tally: tally),
          if (isAll &&
              (overview.white.games > 0 || overview.black.games > 0)) ...[
            const SizedBox(height: 22),
            Text(labels.byColor, style: _overviewSectionLabel(theme)),
            const SizedBox(height: 10),
            if (overview.white.games > 0)
              _TallyRow(label: labels.white, tally: overview.white),
            if (overview.black.games > 0)
              _TallyRow(label: labels.black, tally: overview.black),
          ],
          if (isAll && overview.byTimeControl.isNotEmpty) ...[
            const SizedBox(height: 22),
            Text(labels.byTimeControl, style: _overviewSectionLabel(theme)),
            const SizedBox(height: 10),
            for (final control in overview.byTimeControl)
              _TallyRow(
                label: _timeControlLabel(control.type),
                tally: control.tally,
              ),
          ],
        ],
      ],
    );
  }
}

/// Compact label + win/draw/loss bar + "games · score" row used for the
/// by-colour and by-time-control breakdowns.
class _TallyRow extends StatelessWidget {
  const _TallyRow({required this.label, required this.tally});

  final String label;
  final StatTally tally;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 6),
      child: Row(
        children: [
          SizedBox(
            width: 92,
            child: Text(
              label,
              style: const TextStyle(fontWeight: FontWeight.w600),
            ),
          ),
          Expanded(child: _WinLossDrawBar(tally: tally, height: 10)),
          const SizedBox(width: 12),
          SizedBox(
            width: 96,
            child: Text(
              '${tally.games} · ${_formatPercent(tally.scorePercent)}',
              textAlign: TextAlign.end,
              style: theme.textTheme.bodySmall?.copyWith(
                color: theme.colorScheme.onSurfaceVariant,
              ),
            ),
          ),
        ],
      ),
    );
  }
}

/// Small rounded chip that echoes the active time-control filter on a card.
class _FilterPill extends StatelessWidget {
  const _FilterPill({required this.label});

  final String label;

  @override
  Widget build(BuildContext context) {
    final scheme = Theme.of(context).colorScheme;
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 4),
      decoration: BoxDecoration(
        color: scheme.primaryContainer,
        borderRadius: BorderRadius.circular(999),
      ),
      child: Text(
        label,
        style: Theme.of(context).textTheme.labelSmall?.copyWith(
          color: scheme.onPrimaryContainer,
          fontWeight: FontWeight.w700,
        ),
      ),
    );
  }
}

String _formatPercent(double? value) =>
    value == null ? '—' : '${(value * 100).round()}%';

String _formatRecord(StatTally tally) =>
    '${tally.wins}–${tally.draws}–${tally.losses}';

TextStyle? _overviewSectionLabel(ThemeData theme) =>
    theme.textTheme.labelLarge?.copyWith(
      fontWeight: FontWeight.w700,
      color: theme.colorScheme.onSurfaceVariant,
    );

String _timeControlLabel(String type) => switch (type) {
  'bullet' => 'Bullet',
  'blitz' => 'Blitz',
  'rapid' => 'Rapid',
  'classical' => 'Classical',
  'daily' => 'Daily',
  'correspondence' => 'Correspondence',
  _ => 'Other',
};

class _OverviewText {
  const _OverviewText({
    required this.title,
    required this.games,
    required this.winRate,
    required this.score,
    required this.record,
    required this.byColor,
    required this.byTimeControl,
    required this.white,
    required this.black,
    required this.empty,
    required this.noProfile,
    required this.noGamesForFilter,
    required this.error,
    required this.retry,
  });

  final String title;
  final String games;
  final String winRate;
  final String score;
  final String record;
  final String byColor;
  final String byTimeControl;
  final String white;
  final String black;
  final String empty;
  final String noProfile;
  final String noGamesForFilter;
  final String error;
  final String retry;
}

_OverviewText _overviewText(BuildContext context) {
  switch (Localizations.localeOf(context).languageCode) {
    case 'ar':
      return const _OverviewText(
        title: 'نظرة عامة',
        games: 'المباريات',
        winRate: 'نسبة الفوز',
        score: 'النتيجة',
        record: 'السجل',
        byColor: 'حسب اللون',
        byTimeControl: 'حسب نوع الوقت',
        white: 'أبيض',
        black: 'أسود',
        empty: 'لا توجد مباريات بعد. زامِن حسابًا على الإنترنت أو استورد مباريات لعرض إحصاءاتك.',
        noProfile: 'أنشئ أو اختر ملفًا شخصيًا لعرض الإحصاءات.',
        noGamesForFilter: 'لا توجد مباريات لنوع الوقت المحدد.',
        error: 'تعذّر تحميل الإحصاءات.',
        retry: 'إعادة المحاولة',
      );
    case 'en':
      return const _OverviewText(
        title: 'Overview',
        games: 'Games',
        winRate: 'Win rate',
        score: 'Score',
        record: 'Record',
        byColor: 'By color',
        byTimeControl: 'By time control',
        white: 'White',
        black: 'Black',
        empty: 'No games yet. Sync an online profile or import games to see your statistics.',
        noProfile: 'Create or select a profile to see statistics.',
        noGamesForFilter: 'No games for the selected time control.',
        error: 'Could not load statistics.',
        retry: 'Retry',
      );
    default:
      return const _OverviewText(
        title: 'Übersicht',
        games: 'Partien',
        winRate: 'Siegquote',
        score: 'Score',
        record: 'Bilanz',
        byColor: 'Nach Farbe',
        byTimeControl: 'Nach Zeitkontrolle',
        white: 'Weiß',
        black: 'Schwarz',
        empty: 'Noch keine Partien. Synchronisiere ein Online-Profil oder importiere Partien, um deine Statistik zu sehen.',
        noProfile: 'Erstelle oder wähle ein Profil, um Statistiken zu sehen.',
        noGamesForFilter: 'Keine Partien für die gewählte Zeitkontrolle.',
        error: 'Statistik konnte nicht geladen werden.',
        retry: 'Erneut versuchen',
      );
  }
}
