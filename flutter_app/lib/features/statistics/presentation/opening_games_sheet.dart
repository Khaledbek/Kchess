part of '../../../ui/app_root.dart';

/// Opens the games for one opening variation as a modal bottom sheet (narrow) or
/// a centered dialog (wide), then hands a tapped game to the analysis workflow.
Future<void> _showOpeningGames({
  required BuildContext context,
  required AppController controller,
  required OpeningFamily family,
  required OpeningVariation variation,
  required String variationLabel,
}) {
  Widget content() => _OpeningGamesBottomSheet(
    controller: controller,
    family: family,
    variation: variation,
    variationLabel: variationLabel,
    onOpenGame: (game) async {
      Navigator.of(context).pop();
      await _openAnalysisAndRefreshSettings(
        context: context,
        controller: controller,
        game: game,
      );
    },
  );

  if (MediaQuery.sizeOf(context).width >= 700) {
    return showDialog<void>(
      context: context,
      builder: (_) => Dialog(
        clipBehavior: Clip.antiAlias,
        child: ConstrainedBox(
          constraints: const BoxConstraints(maxWidth: 560),
          child: content(),
        ),
      ),
    );
  }
  return showModalBottomSheet<void>(
    context: context,
    showDragHandle: true,
    isScrollControlled: true,
    builder: (_) => content(),
  );
}

class _OpeningGamesBottomSheet extends StatefulWidget {
  const _OpeningGamesBottomSheet({
    required this.controller,
    required this.family,
    required this.variation,
    required this.variationLabel,
    required this.onOpenGame,
  });

  final AppController controller;
  final OpeningFamily family;
  final OpeningVariation variation;
  final String variationLabel;
  final ValueChanged<GameSummary> onOpenGame;

  @override
  State<_OpeningGamesBottomSheet> createState() =>
      _OpeningGamesBottomSheetState();
}

class _OpeningGamesBottomSheetState extends State<_OpeningGamesBottomSheet> {
  late Future<List<GameSummary>> _games;
  String _filter = 'all'; // all | win | loss

  @override
  void initState() {
    super.initState();
    _games = _load();
  }

  Future<List<GameSummary>> _load() async {
    final color = widget.family.color;
    final games = await widget.controller.queryGames(
      GameQuery(
        color: color == 'white' || color == 'black' ? color : 'all',
        sort: 'newest',
      ),
    );
    // Filter to the exact variation. Selecting rows by opening name is display
    // selection, not aggregation — the per-opening tallies stay in the C++ core.
    return games
        .where((game) => game.openingName == widget.variation.name)
        .toList(growable: false);
  }

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final labels = _openingGamesText(context);
    final tally = widget.variation.tally;
    final record = '${tally.wins}W - ${tally.draws}D - ${tally.losses}L';
    final subtitle = <String>[
      if (widget.variation.eco.isNotEmpty) widget.variation.eco,
      record,
    ].join('  ·  ');

    return ConstrainedBox(
      constraints: BoxConstraints(
        maxHeight: MediaQuery.sizeOf(context).height * 0.82,
      ),
      child: Padding(
        padding: const EdgeInsets.fromLTRB(20, 8, 20, 16),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(
              widget.variationLabel,
              style: theme.textTheme.titleMedium?.copyWith(
                fontWeight: FontWeight.w800,
              ),
            ),
            const SizedBox(height: 4),
            Text(
              subtitle,
              style: theme.textTheme.bodySmall?.copyWith(
                color: theme.colorScheme.onSurfaceVariant,
              ),
            ),
            const SizedBox(height: 14),
            Wrap(
              spacing: 8,
              children: [
                for (final value in const ['all', 'win', 'loss'])
                  ChoiceChip(
                    label: Text(labels.filter(value)),
                    selected: _filter == value,
                    onSelected: (_) => setState(() => _filter = value),
                  ),
              ],
            ),
            const SizedBox(height: 8),
            Flexible(
              child: FutureBuilder<List<GameSummary>>(
                future: _games,
                builder: (context, snapshot) {
                  if (snapshot.connectionState != ConnectionState.done) {
                    return const SizedBox(
                      height: 140,
                      child: Center(child: CircularProgressIndicator()),
                    );
                  }
                  if (snapshot.hasError || !snapshot.hasData) {
                    return _OpeningGamesMessage(text: labels.error);
                  }
                  final games = snapshot.data!
                      .where(
                        (game) =>
                            _filter == 'all' ||
                            _statGameOutcome(game) == _filter,
                      )
                      .toList(growable: false);
                  if (games.isEmpty) {
                    return _OpeningGamesMessage(text: labels.empty);
                  }
                  return ListView.separated(
                    shrinkWrap: true,
                    padding: const EdgeInsets.only(top: 4),
                    itemCount: games.length,
                    separatorBuilder: (_, _) => const SizedBox(height: 8),
                    itemBuilder: (context, index) => _OpeningGameRow(
                      game: games[index],
                      labels: labels,
                      onTap: () => widget.onOpenGame(games[index]),
                    ),
                  );
                },
              ),
            ),
          ],
        ),
      ),
    );
  }
}

class _OpeningGamesMessage extends StatelessWidget {
  const _OpeningGamesMessage({required this.text});

  final String text;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 28),
      child: Center(
        child: Text(
          text,
          textAlign: TextAlign.center,
          style: theme.textTheme.bodyMedium?.copyWith(
            color: theme.colorScheme.onSurfaceVariant,
          ),
        ),
      ),
    );
  }
}

class _OpeningGameRow extends StatelessWidget {
  const _OpeningGameRow({
    required this.game,
    required this.labels,
    required this.onTap,
  });

  final GameSummary game;
  final _OpeningGamesText labels;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;
    final outcome = _statGameOutcome(game);
    final opponent = _statOpponentName(game);
    final rating = _statOpponentRating(game);
    final reason = labels.reason(game.termination);
    final date = game.date.isNotEmpty
        ? game.date
        : (game.endedAt > 0 ? _formatPointDate(game.endedAt) : '');
    final subtitle = [
      if (reason.isNotEmpty) reason,
      if (date.isNotEmpty) date,
    ].join('  ·  ');

    return DecoratedBox(
      decoration: BoxDecoration(
        color: scheme.surfaceContainerLow,
        borderRadius: BorderRadius.circular(12),
        border: Border.all(color: scheme.outlineVariant),
      ),
      child: InkWell(
        onTap: onTap,
        borderRadius: BorderRadius.circular(12),
        child: Padding(
          padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 10),
          child: Row(
            children: [
              _OpeningResultBadge(outcome: outcome),
              const SizedBox(width: 12),
              Expanded(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Row(
                      children: [
                        Flexible(
                          child: Text(
                            opponent,
                            maxLines: 1,
                            overflow: TextOverflow.ellipsis,
                            style: const TextStyle(fontWeight: FontWeight.w600),
                          ),
                        ),
                        if (rating != null) ...[
                          const SizedBox(width: 7),
                          Container(
                            padding: const EdgeInsets.symmetric(
                              horizontal: 6,
                              vertical: 1,
                            ),
                            decoration: BoxDecoration(
                              color: scheme.surfaceContainerHigh,
                              borderRadius: BorderRadius.circular(6),
                            ),
                            child: Text(
                              '$rating',
                              style: theme.textTheme.bodySmall?.copyWith(
                                color: scheme.onSurfaceVariant,
                              ),
                            ),
                          ),
                        ],
                      ],
                    ),
                    if (subtitle.isNotEmpty) ...[
                      const SizedBox(height: 3),
                      Text(
                        subtitle,
                        maxLines: 1,
                        overflow: TextOverflow.ellipsis,
                        style: theme.textTheme.bodySmall?.copyWith(
                          color: scheme.onSurfaceVariant,
                        ),
                      ),
                    ],
                  ],
                ),
              ),
              const SizedBox(width: 8),
              Icon(Icons.chevron_right, color: scheme.onSurfaceVariant),
            ],
          ),
        ),
      ),
    );
  }
}

class _OpeningResultBadge extends StatelessWidget {
  const _OpeningResultBadge({required this.outcome});

  final String outcome; // win | loss | draw

  @override
  Widget build(BuildContext context) {
    final scheme = Theme.of(context).colorScheme;
    final (color, icon) = switch (outcome) {
      'win' => (AppTheme.success, Icons.check_rounded),
      'loss' => (scheme.error, Icons.close_rounded),
      _ => (scheme.onSurfaceVariant, Icons.remove_rounded),
    };
    return Container(
      width: 34,
      height: 34,
      decoration: BoxDecoration(
        color: color.withValues(alpha: 0.16),
        shape: BoxShape.circle,
        border: Border.all(color: color.withValues(alpha: 0.55)),
      ),
      alignment: Alignment.center,
      child: Icon(icon, size: 20, color: color),
    );
  }
}

class _OpeningGamesText {
  const _OpeningGamesText({
    required this.all,
    required this.won,
    required this.lost,
    required this.byCheckmate,
    required this.byResignation,
    required this.byTimeout,
    required this.byDraw,
    required this.empty,
    required this.error,
  });

  final String all;
  final String won;
  final String lost;
  final String byCheckmate;
  final String byResignation;
  final String byTimeout;
  final String byDraw;
  final String empty;
  final String error;

  String filter(String value) => switch (value) {
    'win' => won,
    'loss' => lost,
    _ => all,
  };

  String reason(String termination) => switch (termination) {
    'checkmate' => byCheckmate,
    'resignation' => byResignation,
    'timeout' => byTimeout,
    'draw' => byDraw,
    _ => '',
  };
}

_OpeningGamesText _openingGamesText(BuildContext context) {
  switch (Localizations.localeOf(context).languageCode) {
    case 'ar':
      return const _OpeningGamesText(
        all: 'الكل',
        won: 'فوز',
        lost: 'خسارة',
        byCheckmate: 'بكش ملك',
        byResignation: 'بالاستسلام',
        byTimeout: 'بانتهاء الوقت',
        byDraw: 'تعادل',
        empty: 'لا توجد مباريات لهذه الاختيار.',
        error: 'تعذّر تحميل المباريات.',
      );
    case 'en':
      return const _OpeningGamesText(
        all: 'All',
        won: 'Won',
        lost: 'Lost',
        byCheckmate: 'by checkmate',
        byResignation: 'by resignation',
        byTimeout: 'on time',
        byDraw: 'draw',
        empty: 'No games for this selection.',
        error: 'Could not load games.',
      );
    default:
      return const _OpeningGamesText(
        all: 'Alle',
        won: 'Gewonnen',
        lost: 'Verloren',
        byCheckmate: 'durch Matt',
        byResignation: 'durch Aufgabe',
        byTimeout: 'durch Zeit',
        byDraw: 'Remis',
        empty: 'Keine Partien für diese Auswahl.',
        error: 'Partien konnten nicht geladen werden.',
      );
  }
}
