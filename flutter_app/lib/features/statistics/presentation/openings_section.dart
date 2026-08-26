part of '../../../ui/app_root.dart';

class _OpeningsCard extends StatelessWidget {
  const _OpeningsCard({required this.future, required this.onRetry});

  final Future<OpeningsStats> future;
  final VoidCallback onRetry;

  @override
  Widget build(BuildContext context) {
    final labels = _openingsText(context);
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(18),
        child: FutureBuilder<OpeningsStats>(
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
                icon: Icons.account_tree_outlined,
                text: labels.empty,
              );
            }
            return _OpeningsContent(stats: stats, labels: labels);
          },
        ),
      ),
    );
  }
}

class _OpeningsContent extends StatelessWidget {
  const _OpeningsContent({required this.stats, required this.labels});

  final OpeningsStats stats;
  final _OpeningsText labels;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final whiteOpenings = stats.openings
        .where((opening) => opening.color == 'white')
        .take(8)
        .toList(growable: false);
    final blackOpenings = stats.openings
        .where((opening) => opening.color == 'black')
        .take(8)
        .toList(growable: false);
    final unknownOpenings = stats.openings
        .where(
          (opening) => opening.color != 'white' && opening.color != 'black',
        )
        .take(8)
        .toList(growable: false);
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Row(
          children: [
            Icon(Icons.account_tree_outlined, color: theme.colorScheme.primary),
            const SizedBox(width: 8),
            Text(
              labels.title,
              style: theme.textTheme.titleMedium?.copyWith(
                fontWeight: FontWeight.w800,
              ),
            ),
          ],
        ),
        const SizedBox(height: 4),
        Text(
          '${stats.gamesWithOpening} ${labels.classifiedGames}',
          style: theme.textTheme.bodySmall?.copyWith(
            color: theme.colorScheme.onSurfaceVariant,
          ),
        ),
        const SizedBox(height: 16),
        Text(labels.mostPlayed, style: _overviewSectionLabel(theme)),
        const SizedBox(height: 8),
        LayoutBuilder(
          builder: (context, constraints) {
            final sections = <Widget>[
              if (whiteOpenings.isNotEmpty)
                _OpeningColorSection(
                  color: 'white',
                  title: labels.white,
                  openings: whiteOpenings,
                  labels: labels,
                ),
              if (blackOpenings.isNotEmpty)
                _OpeningColorSection(
                  color: 'black',
                  title: labels.black,
                  openings: blackOpenings,
                  labels: labels,
                ),
              if (unknownOpenings.isNotEmpty)
                _OpeningColorSection(
                  color: 'unknown',
                  title: labels.unknownColor,
                  openings: unknownOpenings,
                  labels: labels,
                ),
            ];
            if (constraints.maxWidth >= 720 && sections.length == 2) {
              return Row(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Expanded(child: sections[0]),
                  const SizedBox(width: 20),
                  Expanded(child: sections[1]),
                ],
              );
            }
            return Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                for (var index = 0; index < sections.length; index++) ...[
                  if (index > 0) const SizedBox(height: 16),
                  sections[index],
                ],
              ],
            );
          },
        ),
      ],
    );
  }
}

class _OpeningColorSection extends StatelessWidget {
  const _OpeningColorSection({
    required this.color,
    required this.title,
    required this.openings,
    required this.labels,
  });

  final String color;
  final String title;
  final List<OpeningStat> openings;
  final _OpeningsText labels;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return DecoratedBox(
      decoration: BoxDecoration(
        color: theme.colorScheme.surfaceContainerLow,
        borderRadius: BorderRadius.circular(12),
        border: Border.all(color: theme.colorScheme.outlineVariant),
      ),
      child: Padding(
        padding: const EdgeInsets.all(12),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                _ColorDot(color: color),
                const SizedBox(width: 8),
                Text(
                  title,
                  style: theme.textTheme.titleSmall?.copyWith(
                    fontWeight: FontWeight.w800,
                  ),
                ),
              ],
            ),
            const SizedBox(height: 4),
            for (final opening in openings)
              _OpeningLine(opening: opening, labels: labels),
          ],
        ),
      ),
    );
  }
}

class _OpeningLine extends StatelessWidget {
  const _OpeningLine({required this.opening, required this.labels});

  final OpeningStat opening;
  final _OpeningsText labels;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final tally = opening.tally;
    final subtitle = <String>[
      if (opening.eco.isNotEmpty) opening.eco,
      '${tally.games} ${labels.games}',
    ].join(' · ');
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 6),
      child: Row(
        children: [
          _ColorDot(color: opening.color),
          const SizedBox(width: 10),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  opening.name,
                  maxLines: 1,
                  overflow: TextOverflow.ellipsis,
                  style: const TextStyle(fontWeight: FontWeight.w600),
                ),
                const SizedBox(height: 2),
                Text(
                  subtitle,
                  style: theme.textTheme.bodySmall?.copyWith(
                    color: theme.colorScheme.onSurfaceVariant,
                  ),
                ),
              ],
            ),
          ),
          const SizedBox(width: 12),
          SizedBox(width: 84, child: _WdlBar(tally: tally)),
          const SizedBox(width: 10),
          SizedBox(
            width: 92,
            child: Text(
              '${labels.score}: ${_formatPercent(tally.scorePercent)}',
              textAlign: TextAlign.end,
              maxLines: 1,
              style: const TextStyle(fontWeight: FontWeight.w700),
            ),
          ),
        ],
      ),
    );
  }
}

class _ColorDot extends StatelessWidget {
  const _ColorDot({required this.color});

  final String color; // white | black | unknown

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final Color fill = switch (color) {
      'white' => Colors.white,
      'black' => Colors.black,
      _ => theme.colorScheme.onSurfaceVariant,
    };
    return Container(
      width: 14,
      height: 14,
      decoration: BoxDecoration(
        color: fill,
        shape: BoxShape.circle,
        border: Border.all(color: theme.colorScheme.outline),
      ),
    );
  }
}

class _OpeningsText {
  const _OpeningsText({
    required this.title,
    required this.mostPlayed,
    required this.classifiedGames,
    required this.games,
    required this.white,
    required this.black,
    required this.unknownColor,
    required this.score,
    required this.empty,
    required this.noProfile,
    required this.error,
    required this.retry,
  });

  final String title;
  final String mostPlayed;
  final String classifiedGames;
  final String games;
  final String white;
  final String black;
  final String unknownColor;
  final String score;
  final String empty;
  final String noProfile;
  final String error;
  final String retry;
}

_OpeningsText _openingsText(BuildContext context) {
  switch (Localizations.localeOf(context).languageCode) {
    case 'ar':
      return const _OpeningsText(
        title: 'الافتتاحيات',
        mostPlayed: 'الأكثر لعبًا',
        classifiedGames: 'مباراة بافتتاحية معروفة',
        games: 'مباراة',
        white: 'الأبيض',
        black: 'الأسود',
        unknownColor: 'لون غير معروف',
        score: 'النقاط',
        empty: 'لا توجد افتتاحيات مُصنّفة بعد. تُصنَّف المباريات المستوردة والمتزامنة تلقائيًا.',
        noProfile: 'أنشئ أو اختر ملفًا شخصيًا لعرض الافتتاحيات.',
        error: 'تعذّر تحميل الافتتاحيات.',
        retry: 'إعادة المحاولة',
      );
    case 'en':
      return const _OpeningsText(
        title: 'Openings',
        mostPlayed: 'Most played',
        classifiedGames: 'games with a named opening',
        games: 'games',
        white: 'White',
        black: 'Black',
        unknownColor: 'Unassigned color',
        score: 'Score',
        empty: 'No named openings yet. Synced and imported games are classified automatically.',
        noProfile: 'Create or select a profile to see openings.',
        error: 'Could not load openings.',
        retry: 'Retry',
      );
    default:
      return const _OpeningsText(
        title: 'Eröffnungen',
        mostPlayed: 'Meistgespielt',
        classifiedGames: 'Partien mit benannter Eröffnung',
        games: 'Partien',
        white: 'Weiß',
        black: 'Schwarz',
        unknownColor: 'Farbe nicht zugeordnet',
        score: 'Punkte',
        empty: 'Noch keine benannten Eröffnungen. Synchronisierte und importierte Partien werden automatisch klassifiziert.',
        noProfile: 'Erstelle oder wähle ein Profil, um Eröffnungen zu sehen.',
        error: 'Eröffnungen konnten nicht geladen werden.',
        retry: 'Erneut versuchen',
      );
  }
}

({String title, String introTitle, String introBody}) _statisticsText(
  BuildContext context,
) {
  return switch (Localizations.localeOf(context).languageCode) {
    'ar' => (
      title: 'الإحصائيات',
      introTitle: 'أداؤك في الشطرنج',
      introBody: 'اعرض نتائجك وأداءك الأخير وسجل افتتاحياتك مفصولًا حسب اللون.',
    ),
    'en' => (
      title: 'Statistics',
      introTitle: 'Your chess performance',
      introBody: 'See your results, recent form and opening record separated by color.',
    ),
    _ => (
      title: 'Statistiken',
      introTitle: 'Deine Schachleistung',
      introBody: 'Sieh deine Ergebnisse, aktuelle Form und Eröffnungsbilanz getrennt nach Farbe.',
    ),
  };
}

