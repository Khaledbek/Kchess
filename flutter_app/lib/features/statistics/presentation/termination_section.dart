// -----------------------------------------------------------------------------
// Section: termination section presentation
// -----------------------------------------------------------------------------

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

    // Shared scale for the diverging bars: the widest single side across every
    // category. Counted in half-units (1 game = 2 units) so a draw can straddle
    // the axis evenly. Sharing one scale is what makes the rows comparable.
    var maxUnits = 0;
    for (final entry in entries) {
      final tally = entry.tally;
      final side = tally.wins > tally.losses ? tally.wins : tally.losses;
      final units = side * 2 + tally.draws;
      if (units > maxUnits) maxUnits = units;
    }

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        ClipRRect(
          borderRadius: BorderRadius.circular(8),
          child: SizedBox(
            height: 14,
            width: double.infinity,
            child: Row(
              crossAxisAlignment: CrossAxisAlignment.stretch,
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
        _TerminationSpotlight(spotlight: stats.spotlight, labels: labels),
        const SizedBox(height: 14),
        const _DivergingAxisKey(),
        for (final entry in entries)
          _TerminationTile(
            termination: entry,
            totalGames: total,
            maxUnits: maxUnits,
            labels: labels,
          ),
      ],
    );
  }
}

/// Names the single ending that accounts for the most games and says whether it
/// is costing the profile points — the one sentence worth reading on this card.
/// Tinted by that verdict rather than decoratively.
class _TerminationSpotlight extends StatelessWidget {
  const _TerminationSpotlight({required this.spotlight, required this.labels});

  final TerminationSpotlight? spotlight;
  final _TerminationText labels;

  @override
  Widget build(BuildContext context) {
    final data = spotlight;
    if (data == null) return const SizedBox.shrink();
    final top = data.termination;
    final tally = top.tally;
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;
    final sharePercent = data.sharePercent;
    final lossPercent = data.lossPercent;
    final costly = data.costly;
    final accent = costly ? scheme.error : scheme.primary;

    return Container(
      padding: const EdgeInsets.all(12),
      decoration: BoxDecoration(
        color: accent.withValues(alpha: 0.08),
        borderRadius: BorderRadius.circular(12),
        border: Border.all(color: accent.withValues(alpha: 0.30)),
      ),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Icon(
            costly ? Icons.warning_amber_rounded : Icons.insights_outlined,
            size: 18,
            color: accent,
          ),
          const SizedBox(width: 10),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  AppLocalizations.of(context).statsTerminationSpotlight(
                    labels.label(top.type),
                    sharePercent,
                    lossPercent,
                  ),
                  style: theme.textTheme.bodySmall?.copyWith(
                    color: scheme.onSurface,
                    height: 1.35,
                  ),
                ),
                const SizedBox(height: 8),
                _WinLossDrawRatioBar(
                  wins: tally.wins,
                  draws: tally.draws,
                  losses: tally.losses,
                  height: 4,
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}

/// Tiny key telling the reader which way the diverging bars run.
class _DivergingAxisKey extends StatelessWidget {
  const _DivergingAxisKey();

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final labels = _statsLabels(context);
    Widget side(Color color, String text, {required bool trailing}) {
      final dot = Container(
        width: 8,
        height: 8,
        decoration: BoxDecoration(color: color, shape: BoxShape.circle),
      );
      final label = Text(
        text,
        style: theme.textTheme.bodySmall?.copyWith(
          color: theme.colorScheme.onSurfaceVariant,
        ),
      );
      return Row(
        mainAxisSize: MainAxisSize.min,
        children: trailing
            ? [dot, const SizedBox(width: 6), label]
            : [label, const SizedBox(width: 6), dot],
      );
    }

    return Padding(
      padding: const EdgeInsets.only(bottom: 2),
      child: Row(
        children: [
          Expanded(
            child: Align(
              alignment: AlignmentDirectional.centerEnd,
              child: side(_kLossColor, labels.losses, trailing: false),
            ),
          ),
          const SizedBox(width: 10),
          Expanded(
            child: Align(
              alignment: AlignmentDirectional.centerStart,
              child: side(_kWinColor, labels.wins, trailing: true),
            ),
          ),
        ],
      ),
    );
  }
}

/// Win/loss balance for one ending, drawn either side of a shared centre axis:
/// losses run left, wins run right, and draws straddle the axis as a neutral
/// grey core. Every row uses the same [maxUnits] scale, so the bars are
/// comparable down the card — the point being to see at a glance which endings
/// pay. Segments are laid out in pixels rather than flex so a single game can be
/// given a visible floor: against a 280-game category one draw is otherwise a
/// sub-pixel sliver and reads as nothing at all.
class _TerminationDivergingBar extends StatelessWidget {
  const _TerminationDivergingBar({required this.tally, required this.maxUnits});

  final StatTally tally;
  final int maxUnits;

  static const _height = 10.0;
  static const _axisWidth = 1.0;

  /// Smallest width a non-empty segment may take, so one draw or one loss stays
  /// legible next to a category hundreds of games wide.
  static const _minSegment = 3.0;

  @override
  Widget build(BuildContext context) {
    final scheme = Theme.of(context).colorScheme;
    final labels = _statsLabels(context);
    if (maxUnits <= 0) {
      return SizedBox(
        height: _height,
        width: double.infinity,
        child: ColoredBox(color: scheme.outlineVariant),
      );
    }

    // 1 game = 2 units, so half a draw lands on each side of the axis.
    final lossUnits = tally.losses * 2;
    final winUnits = tally.wins * 2;
    final drawUnits = tally.draws;

    final message = [
      if (tally.wins > 0) '${tally.wins} ${labels.wins}',
      if (tally.draws > 0) '${tally.draws} ${labels.draws}',
      if (tally.losses > 0) '${tally.losses} ${labels.losses}',
    ].join(', ');

    return Tooltip(
      message: message,
      child: SizedBox(
        height: _height,
        width: double.infinity,
        child: LayoutBuilder(
          builder: (context, constraints) {
            final half = (constraints.maxWidth - _axisWidth) / 2;
            if (!half.isFinite || half <= 0) return const SizedBox.shrink();

            double widthFor(int units) {
              if (units <= 0) return 0;
              final raw = units / maxUnits * half;
              return raw < _minSegment ? _minSegment : raw;
            }

            var loss = widthFor(lossUnits);
            var win = widthFor(winUnits);
            var drawLeft = widthFor(drawUnits);
            var drawRight = drawLeft;
            // The floor can push a side past its half; scale that side back so
            // the axis stays exactly in the middle.
            if (loss + drawLeft > half) {
              final scale = half / (loss + drawLeft);
              loss *= scale;
              drawLeft *= scale;
            }
            if (win + drawRight > half) {
              final scale = half / (win + drawRight);
              win *= scale;
              drawRight *= scale;
            }

            Widget segment(double width, Color color) => width <= 0
                ? const SizedBox.shrink()
                : SizedBox(
                    width: width,
                    child: ColoredBox(color: color),
                  );

            return Row(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                // Losses grow leftward from the axis, draws hug it.
                SizedBox(
                  width: half,
                  child: Row(
                    crossAxisAlignment: CrossAxisAlignment.stretch,
                    mainAxisAlignment: MainAxisAlignment.end,
                    children: [
                      segment(loss, _kLossColor),
                      segment(drawLeft, _kDrawColor),
                    ],
                  ),
                ),
                SizedBox(
                  width: _axisWidth,
                  child: ColoredBox(color: scheme.outline),
                ),
                // Draws hug the axis, wins grow rightward.
                SizedBox(
                  width: half,
                  child: Row(
                    crossAxisAlignment: CrossAxisAlignment.stretch,
                    mainAxisAlignment: MainAxisAlignment.start,
                    children: [
                      segment(drawRight, _kDrawColor),
                      segment(win, _kWinColor),
                    ],
                  ),
                ),
              ],
            );
          },
        ),
      ),
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
    required this.maxUnits,
    required this.labels,
  });

  final GameTermination termination;
  final int totalGames;
  final int maxUnits;
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
    final outcomeKinds = [
      tally.wins > 0,
      tally.draws > 0,
      tally.losses > 0,
    ].where((b) => b).length;
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
                _TerminationDivergingBar(
                  tally: tally,
                  maxUnits: widget.maxUnits,
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
          crossFadeState: _expanded
              ? CrossFadeState.showSecond
              : CrossFadeState.showFirst,
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
          Expanded(child: Text(label, style: theme.textTheme.bodyMedium)),
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
    'timeout' => const Color(0xFF8B7BC8), // soft violet, matching the palette
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
  final strings = AppLocalizations.of(context);
  return _TerminationText(
    title: strings.statsTerminationTitle,
    checkmate: strings.statsTerminationCheckmate,
    resignation: strings.statsTerminationResignation,
    timeout: strings.statsTerminationTimeout,
    draw: strings.statsTerminationDraw,
    other: strings.statsTerminationOther,
    wonByCheckmate: strings.statsTerminationWonByCheckmate,
    lostByCheckmate: strings.statsTerminationLostByCheckmate,
    opponentResigned: strings.statsTerminationOpponentResigned,
    selfResigned: strings.statsTerminationSelfResigned,
    opponentFlagged: strings.statsTerminationOpponentFlagged,
    selfFlagged: strings.statsTerminationSelfFlagged,
    wonGeneric: strings.statsTerminationWonGeneric,
    lostGeneric: strings.statsTerminationLostGeneric,
    empty: strings.statsTerminationEmpty,
    noProfile: strings.statsTerminationNoProfile,
    error: strings.statsTerminationError,
    retry: strings.statsTerminationRetry,
  );
}
