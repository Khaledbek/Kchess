// -----------------------------------------------------------------------------
// Section: openings section presentation
// -----------------------------------------------------------------------------

part of '../../../ui/app_root.dart';

class _OpeningsCard extends StatelessWidget {
  const _OpeningsCard({
    required this.future,
    required this.onRetry,
    required this.controller,
    required this.timeControl,
  });

  final Future<OpeningsStats> future;
  final VoidCallback onRetry;
  final AppController controller;
  final String timeControl;

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
                height: 160,
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
              // "No classified openings yet" would be wrong here: the library
              // may be full of them, just none in the selected bucket.
              return timeControl == 'all'
                  ? _OverviewMessage(
                      icon: Icons.account_tree_outlined,
                      text: labels.empty,
                    )
                  : _OverviewMessage(
                      icon: Icons.filter_alt_off_outlined,
                      text: _overviewText(context).noGamesForFilter,
                    );
            }
            return _OpeningsContent(
              stats: stats,
              labels: labels,
              controller: controller,
              timeControl: timeControl,
            );
          },
        ),
      ),
    );
  }
}

enum _OpeningSort { mostPlayed, bestWinRate }

class _OpeningsContent extends StatefulWidget {
  const _OpeningsContent({
    required this.stats,
    required this.labels,
    required this.controller,
    required this.timeControl,
  });

  final OpeningsStats stats;
  final _OpeningsText labels;
  final AppController controller;
  final String timeControl;

  @override
  State<_OpeningsContent> createState() => _OpeningsContentState();
}

class _OpeningsContentState extends State<_OpeningsContent> {
  static const _maxRows = 12;

  late String _color;
  _OpeningSort _sort = _OpeningSort.mostPlayed;

  List<String> get _availableColors {
    final colors = <String>[];
    if (_familiesFor('white').isNotEmpty) colors.add('white');
    if (_familiesFor('black').isNotEmpty) colors.add('black');
    if (_familiesFor('unknown').isNotEmpty) colors.add('unknown');
    return colors;
  }

  List<OpeningFamily> _familiesFor(String color) {
    if (color == 'unknown') {
      return widget.stats.families
          .where((f) => f.color != 'white' && f.color != 'black')
          .toList(growable: false);
    }
    return widget.stats.families
        .where((f) => f.color == color)
        .toList(growable: false);
  }

  @override
  void initState() {
    super.initState();
    _color = _defaultColor();
  }

  String _defaultColor() => widget.stats.defaultColor;

  List<OpeningFamily> _rows() {
    final families = _sort == _OpeningSort.bestWinRate
        ? widget.stats.bestWinRateFamilies
        : widget.stats.families;
    return families
        .where((family) => family.color == _color)
        .take(_maxRows)
        .toList(growable: false);
  }

  String _colorLabel(String color) => switch (color) {
    'white' => widget.labels.white,
    'black' => widget.labels.black,
    _ => widget.labels.unknownColor,
  };

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final labels = widget.labels;
    final colors = _availableColors;
    if (colors.isNotEmpty && !colors.contains(_color)) _color = colors.first;
    final rows = _rows();

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Row(
          children: [
            Icon(Icons.auto_stories_outlined, color: theme.colorScheme.primary),
            const SizedBox(width: 8),
            Expanded(
              child: Text(
                labels.title,
                style: theme.textTheme.titleMedium?.copyWith(
                  fontWeight: FontWeight.w800,
                ),
              ),
            ),
            if (widget.timeControl != 'all')
              _FilterPill(
                label: _statsLabels(context).timeControl(widget.timeControl),
              ),
          ],
        ),
        const SizedBox(height: 4),
        Text(
          '${widget.stats.gamesWithOpening} ${labels.classifiedGames}',
          style: theme.textTheme.bodySmall?.copyWith(
            color: theme.colorScheme.onSurfaceVariant,
          ),
        ),
        const SizedBox(height: 16),
        // Colour "tabs" + sort toggle. Both wrap so they stay usable on a
        // narrow card.
        Wrap(
          spacing: 12,
          runSpacing: 12,
          alignment: WrapAlignment.spaceBetween,
          crossAxisAlignment: WrapCrossAlignment.center,
          children: [
            if (colors.length > 1)
              SegmentedButton<String>(
                showSelectedIcon: false,
                segments: [
                  for (final color in colors)
                    ButtonSegment<String>(
                      value: color,
                      icon: _ColorDot(color: color),
                      label: Text(_colorLabel(color)),
                    ),
                ],
                selected: {_color},
                onSelectionChanged: (selection) =>
                    setState(() => _color = selection.first),
              )
            else if (colors.length == 1)
              Row(
                mainAxisSize: MainAxisSize.min,
                children: [
                  _ColorDot(color: colors.first),
                  const SizedBox(width: 8),
                  Text(
                    _colorLabel(colors.first),
                    style: theme.textTheme.titleSmall?.copyWith(
                      fontWeight: FontWeight.w700,
                    ),
                  ),
                ],
              ),
            SegmentedButton<_OpeningSort>(
              showSelectedIcon: false,
              segments: [
                ButtonSegment(
                  value: _OpeningSort.mostPlayed,
                  icon: const Icon(Icons.bar_chart, size: 18),
                  label: Text(labels.mostPlayed),
                ),
                ButtonSegment(
                  value: _OpeningSort.bestWinRate,
                  icon: const Icon(Icons.trending_up, size: 18),
                  label: Text(labels.bestWinRate),
                ),
              ],
              selected: {_sort},
              onSelectionChanged: (selection) =>
                  setState(() => _sort = selection.first),
            ),
          ],
        ),
        if (_sort == _OpeningSort.bestWinRate) ...[
          const SizedBox(height: 8),
          Text(
            labels.minGamesHint,
            style: theme.textTheme.bodySmall?.copyWith(
              color: theme.colorScheme.onSurfaceVariant,
            ),
          ),
        ],
        const SizedBox(height: 12),
        if (rows.isEmpty)
          _OverviewMessage(
            icon: Icons.filter_alt_off_outlined,
            text: _sort == _OpeningSort.bestWinRate
                ? labels.noOpeningsForWinRate
                : labels.noOpeningsForColor,
          )
        else
          for (var i = 0; i < rows.length; i++) ...[
            if (i > 0) const SizedBox(height: 8),
            _OpeningFamilyTile(
              // Reset expansion state when the colour or ordering changes.
              key: ValueKey('${_color}_${_sort.name}_${rows[i].familyName}'),
              family: rows[i],
              labels: labels,
              controller: widget.controller,
            ),
          ],
      ],
    );
  }
}

/// A drill-down family row: a tappable header (family, games, win/draw/loss bar,
/// win rate) that expands to reveal its individual variations.
class _OpeningFamilyTile extends StatefulWidget {
  const _OpeningFamilyTile({
    required this.family,
    required this.labels,
    required this.controller,
    super.key,
  });

  final OpeningFamily family;
  final _OpeningsText labels;
  final AppController controller;

  @override
  State<_OpeningFamilyTile> createState() => _OpeningFamilyTileState();
}

class _OpeningFamilyTileState extends State<_OpeningFamilyTile> {
  bool _expanded = false;

  String _variationLabel(OpeningVariation variation) {
    final family = widget.family.familyName;
    final name = variation.name;
    if (name == family) return widget.labels.baseLine;
    if (name.startsWith(family)) {
      final rest = name
          .substring(family.length)
          .replaceFirst(RegExp(r'^\s*[:,]\s*'), '')
          .trim();
      if (rest.isNotEmpty) return rest;
    }
    return name;
  }

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;
    final family = widget.family;
    final tally = family.tally;
    final expandable = family.hasDistinctVariations;
    final subtitle = <String>[
      if (family.baseEco.isNotEmpty) family.baseEco,
      '${tally.games} ${widget.labels.games}',
      if (expandable) '${family.variations.length} ${widget.labels.variations}',
    ].join(' · ');

    final header = Padding(
      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 10),
      child: Row(
        children: [
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  family.familyName,
                  maxLines: 1,
                  overflow: TextOverflow.ellipsis,
                  style: const TextStyle(fontWeight: FontWeight.w700),
                ),
                const SizedBox(height: 3),
                Text(
                  subtitle,
                  style: theme.textTheme.bodySmall?.copyWith(
                    color: scheme.onSurfaceVariant,
                  ),
                ),
                const SizedBox(height: 8),
                _WinLossDrawBar(tally: tally, height: 8),
              ],
            ),
          ),
          const SizedBox(width: 14),
          SizedBox(
            width: 56,
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.end,
              children: [
                Text(
                  _formatPercent(tally.winRate),
                  style: theme.textTheme.titleMedium?.copyWith(
                    fontWeight: FontWeight.w800,
                  ),
                ),
                Text(
                  widget.labels.winRateShort,
                  style: theme.textTheme.bodySmall?.copyWith(
                    color: scheme.onSurfaceVariant,
                  ),
                ),
              ],
            ),
          ),
          const SizedBox(width: 6),
          _TrainOpeningButton(family: family),
          SizedBox(
            width: 28,
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

    return DecoratedBox(
      decoration: BoxDecoration(
        color: scheme.surfaceContainerLow,
        borderRadius: BorderRadius.circular(12),
        border: Border.all(color: scheme.outlineVariant),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          if (expandable)
            InkWell(
              onTap: () => setState(() => _expanded = !_expanded),
              borderRadius: BorderRadius.circular(12),
              child: header,
            )
          else
            header,
          AnimatedCrossFade(
            duration: const Duration(milliseconds: 180),
            crossFadeState: _expanded
                ? CrossFadeState.showSecond
                : CrossFadeState.showFirst,
            firstChild: const SizedBox(width: double.infinity),
            secondChild: Padding(
              padding: const EdgeInsets.fromLTRB(12, 0, 12, 10),
              child: Column(
                children: [
                  Divider(height: 1, color: scheme.outlineVariant),
                  const SizedBox(height: 6),
                  for (final variation in family.variations)
                    _OpeningVariationRow(
                      label: _variationLabel(variation),
                      variation: variation,
                      labels: widget.labels,
                      onTap: () => _showOpeningGames(
                        context: context,
                        controller: widget.controller,
                        family: family,
                        variation: variation,
                        variationLabel: _variationLabel(variation),
                      ),
                    ),
                ],
              ),
            ),
          ),
        ],
      ),
    );
  }
}

class _OpeningVariationRow extends StatelessWidget {
  const _OpeningVariationRow({
    required this.label,
    required this.variation,
    required this.labels,
    required this.onTap,
  });

  final String label;
  final OpeningVariation variation;
  final _OpeningsText labels;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;
    final tally = variation.tally;
    return InkWell(
      onTap: onTap,
      borderRadius: BorderRadius.circular(8),
      child: Padding(
        padding: const EdgeInsets.symmetric(vertical: 6),
        child: Row(
          children: [
            Padding(
              padding: const EdgeInsets.only(right: 8),
              child: Icon(
                Icons.subdirectory_arrow_right,
                size: 16,
                color: scheme.onSurfaceVariant,
              ),
            ),
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(
                    label,
                    maxLines: 1,
                    overflow: TextOverflow.ellipsis,
                    style: theme.textTheme.bodyMedium,
                  ),
                  const SizedBox(height: 2),
                  Text(
                    '${tally.games} ${labels.games}',
                    style: theme.textTheme.bodySmall?.copyWith(
                      color: scheme.onSurfaceVariant,
                    ),
                  ),
                ],
              ),
            ),
            const SizedBox(width: 10),
            SizedBox(
              width: 96,
              child: _WinLossDrawBar(tally: tally, height: 6),
            ),
            const SizedBox(width: 10),
            SizedBox(
              width: 42,
              child: Text(
                _formatPercent(tally.winRate),
                textAlign: TextAlign.end,
                style: const TextStyle(fontWeight: FontWeight.w700),
              ),
            ),
            Icon(Icons.chevron_right, size: 18, color: scheme.onSurfaceVariant),
          ],
        ),
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
    required this.bestWinRate,
    required this.minGamesHint,
    required this.classifiedGames,
    required this.games,
    required this.variations,
    required this.baseLine,
    required this.white,
    required this.black,
    required this.unknownColor,
    required this.winRateShort,
    required this.noOpeningsForColor,
    required this.noOpeningsForWinRate,
    required this.empty,
    required this.noProfile,
    required this.error,
    required this.retry,
  });

  final String title;
  final String mostPlayed;
  final String bestWinRate;
  final String minGamesHint;
  final String classifiedGames;
  final String games;
  final String variations;
  final String baseLine;
  final String white;
  final String black;
  final String unknownColor;
  final String winRateShort;
  final String noOpeningsForColor;
  final String noOpeningsForWinRate;
  final String empty;
  final String noProfile;
  final String error;
  final String retry;
}

_OpeningsText _openingsText(BuildContext context) {
  final strings = AppLocalizations.of(context);
  return _OpeningsText(
    title: strings.statsOpeningsTitle,
    mostPlayed: strings.statsOpeningsMostPlayed,
    bestWinRate: strings.statsOpeningsBestWinRate,
    minGamesHint: strings.statsOpeningsMinGamesHint,
    classifiedGames: strings.statsOpeningsClassifiedGames,
    games: strings.statsOpeningsGames,
    variations: strings.statsOpeningsVariations,
    baseLine: strings.statsOpeningsBaseLine,
    white: strings.statsOpeningsWhite,
    black: strings.statsOpeningsBlack,
    unknownColor: strings.statsOpeningsUnknownColor,
    winRateShort: strings.statsOpeningsWinRateShort,
    noOpeningsForColor: strings.statsOpeningsNoOpeningsForColor,
    noOpeningsForWinRate: strings.statsOpeningsNoOpeningsForWinRate,
    empty: strings.statsOpeningsEmpty,
    noProfile: strings.statsOpeningsNoProfile,
    error: strings.statsOpeningsError,
    retry: strings.statsOpeningsRetry,
  );
}
