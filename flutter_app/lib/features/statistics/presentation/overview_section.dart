part of '../../../ui/app_root.dart';

class _OverviewCard extends StatelessWidget {
  const _OverviewCard({required this.future, required this.onRetry});

  final Future<StatisticsOverview> future;
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
            return _OverviewContent(overview: overview, labels: labels);
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
  const _OverviewContent({required this.overview, required this.labels});

  final StatisticsOverview overview;
  final _OverviewText labels;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final overall = overview.overall;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Row(
          children: [
            Icon(Icons.dashboard_outlined, color: theme.colorScheme.primary),
            const SizedBox(width: 8),
            Text(
              labels.title,
              style: theme.textTheme.titleMedium?.copyWith(
                fontWeight: FontWeight.w800,
              ),
            ),
          ],
        ),
        const SizedBox(height: 16),
        Wrap(
          spacing: 28,
          runSpacing: 12,
          children: [
            _StatBig(value: '${overview.totalGames}', label: labels.games),
            _StatBig(
              value: _formatPercent(overall.winRate),
              label: labels.winRate,
            ),
            _StatBig(
              value: _formatPercent(overall.scorePercent),
              label: labels.score,
            ),
            _StatBig(value: _formatRecord(overall), label: labels.record),
          ],
        ),
        if (overview.recentForm.isNotEmpty) ...[
          const SizedBox(height: 18),
          Text(labels.recentForm, style: _overviewSectionLabel(theme)),
          const SizedBox(height: 8),
          Wrap(
            spacing: 4,
            runSpacing: 4,
            children: [
              for (final outcome in overview.recentForm)
                _ResultIcon(outcome: outcome, size: 30),
            ],
          ),
        ],
        if (overview.white.games > 0 || overview.black.games > 0) ...[
          const SizedBox(height: 18),
          Text(labels.byColor, style: _overviewSectionLabel(theme)),
          const SizedBox(height: 8),
          if (overview.white.games > 0)
            _TallyLine(label: labels.white, tally: overview.white),
          if (overview.black.games > 0)
            _TallyLine(label: labels.black, tally: overview.black),
        ],
        if (overview.byTimeControl.isNotEmpty) ...[
          const SizedBox(height: 18),
          Text(labels.byTimeControl, style: _overviewSectionLabel(theme)),
          const SizedBox(height: 8),
          for (final control in overview.byTimeControl)
            _TallyLine(
              label: _timeControlLabel(control.type),
              tally: control.tally,
            ),
        ],
      ],
    );
  }
}

class _StatBig extends StatelessWidget {
  const _StatBig({required this.value, required this.label});

  final String value;
  final String label;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      mainAxisSize: MainAxisSize.min,
      children: [
        Text(
          value,
          style: theme.textTheme.headlineSmall?.copyWith(
            fontWeight: FontWeight.w800,
          ),
        ),
        Text(
          label,
          style: theme.textTheme.bodySmall?.copyWith(
            color: theme.colorScheme.onSurfaceVariant,
          ),
        ),
      ],
    );
  }
}

class _ResultIcon extends StatelessWidget {
  const _ResultIcon({required this.outcome, this.size = 30});

  final String outcome; // win | loss | draw
  final double size;

  @override
  Widget build(BuildContext context) {
    final asset = switch (outcome) {
      'win' => 'assets/analysis_img/result_win.svg',
      'loss' => 'assets/analysis_img/result_loss.svg',
      'draw' => 'assets/analysis_img/result_draw.svg',
      _ => null,
    };
    if (asset == null) return SizedBox(width: size, height: size);
    final label = switch (outcome) {
      'win' => 'Win',
      'loss' => 'Loss',
      _ => 'Draw',
    };
    return SizedBox(
      width: size,
      height: size,
      child: SvgPicture.asset(
        asset,
        fit: BoxFit.contain,
        semanticsLabel: label,
      ),
    );
  }
}

class _TallyLine extends StatelessWidget {
  const _TallyLine({required this.label, required this.tally});

  final String label;
  final StatTally tally;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 5),
      child: Row(
        children: [
          SizedBox(
            width: 92,
            child: Text(
              label,
              style: const TextStyle(fontWeight: FontWeight.w600),
            ),
          ),
          Expanded(child: _WdlBar(tally: tally)),
          const SizedBox(width: 12),
          Text(
            '${tally.games} · ${_formatPercent(tally.scorePercent)}',
            style: theme.textTheme.bodySmall?.copyWith(
              color: theme.colorScheme.onSurfaceVariant,
            ),
          ),
        ],
      ),
    );
  }
}

class _WdlBar extends StatelessWidget {
  const _WdlBar({required this.tally});

  final StatTally tally;

  @override
  Widget build(BuildContext context) {
    if (tally.decided == 0) {
      return Text('—', style: Theme.of(context).textTheme.bodySmall);
    }
    return ClipRRect(
      borderRadius: BorderRadius.circular(4),
      child: SizedBox(
        height: 10,
        child: Row(
          children: [
            if (tally.wins > 0)
              Expanded(
                flex: tally.wins,
                child: const ColoredBox(color: Color(0xFF2E7D32)),
              ),
            if (tally.draws > 0)
              Expanded(
                flex: tally.draws,
                child: const ColoredBox(color: Color(0xFF757575)),
              ),
            if (tally.losses > 0)
              Expanded(
                flex: tally.losses,
                child: const ColoredBox(color: Color(0xFFC62828)),
              ),
          ],
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
    required this.recentForm,
    required this.byColor,
    required this.byTimeControl,
    required this.white,
    required this.black,
    required this.empty,
    required this.noProfile,
    required this.error,
    required this.retry,
  });

  final String title;
  final String games;
  final String winRate;
  final String score;
  final String record;
  final String recentForm;
  final String byColor;
  final String byTimeControl;
  final String white;
  final String black;
  final String empty;
  final String noProfile;
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
        recentForm: 'الأداء الأخير',
        byColor: 'حسب اللون',
        byTimeControl: 'حسب نوع الوقت',
        white: 'أبيض',
        black: 'أسود',
        empty: 'لا توجد مباريات بعد. زامِن حسابًا على الإنترنت أو استورد مباريات لعرض إحصاءاتك.',
        noProfile: 'أنشئ أو اختر ملفًا شخصيًا لعرض الإحصاءات.',
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
        recentForm: 'Recent form',
        byColor: 'By color',
        byTimeControl: 'By time control',
        white: 'White',
        black: 'Black',
        empty: 'No games yet. Sync an online profile or import games to see your statistics.',
        noProfile: 'Create or select a profile to see statistics.',
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
        recentForm: 'Aktuelle Form',
        byColor: 'Nach Farbe',
        byTimeControl: 'Nach Zeitkontrolle',
        white: 'Weiß',
        black: 'Schwarz',
        empty: 'Noch keine Partien. Synchronisiere ein Online-Profil oder importiere Partien, um deine Statistik zu sehen.',
        noProfile: 'Erstelle oder wähle ein Profil, um Statistiken zu sehen.',
        error: 'Statistik konnte nicht geladen werden.',
        retry: 'Erneut versuchen',
      );
  }
}

