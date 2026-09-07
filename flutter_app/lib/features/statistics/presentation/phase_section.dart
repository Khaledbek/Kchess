part of '../../../ui/app_root.dart';

/// "Nach Spielphase": where the profile's games end (opening / middlegame /
/// endgame, by ending move number) and how they score there.
///
/// The card leads with a segmented distribution strip so the spread reads at a
/// glance, then gives each phase a "volume × outcome" bar: the bar's *length* is
/// that phase's share of all games, and its *segments* are the win/draw/loss
/// split inside that phase. A "where games conclude" heuristic, not engine-based
/// blunder finding.
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

  // Stable phase order so colours and positions never shuffle.
  static const _order = ['opening', 'middlegame', 'endgame'];

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final byPhase = {for (final p in stats.phases) p.phase: p.tally};
    final tallies = {
      for (final phase in _order) phase: byPhase[phase] ?? const StatTally(),
    };
    final total = _order.fold<int>(0, (sum, p) => sum + tallies[p]!.games);

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        // Macro distribution strip: how the games split across phases.
        if (total > 0) ...[
          _PhaseDistributionStrip(tallies: tallies, total: total, labels: labels),
          const SizedBox(height: 8),
          _distributionCaption(context, tallies, total),
          const SizedBox(height: 16),
        ],
        for (var i = 0; i < _order.length; i++) ...[
          if (i > 0) const SizedBox(height: 14),
          _PhaseRow(
            phase: _order[i],
            tally: tallies[_order[i]]!,
            total: total,
            labels: labels,
          ),
        ],
        const SizedBox(height: 14),
        // Key for the win/draw/loss segments inside each phase bar.
        _WdlLegend(
          tally: StatTally(
            wins: _order.fold(0, (s, p) => s + tallies[p]!.wins),
            draws: _order.fold(0, (s, p) => s + tallies[p]!.draws),
            losses: _order.fold(0, (s, p) => s + tallies[p]!.losses),
          ),
        ),
        if (stats.classified < stats.totalGames) ...[
          const SizedBox(height: 10),
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

  /// "15% Eröffnung · 56% Mittelspiel · 29% Endspiel" — the shares spelled out,
  /// each percentage tinted with its phase colour.
  Widget _distributionCaption(
    BuildContext context,
    Map<String, StatTally> tallies,
    int total,
  ) {
    final theme = Theme.of(context);
    final spans = <InlineSpan>[];
    for (final phase in _order) {
      final games = tallies[phase]!.games;
      if (games <= 0) continue;
      if (spans.isNotEmpty) {
        spans.add(
          TextSpan(
            text: '  ·  ',
            style: TextStyle(color: theme.colorScheme.outline),
          ),
        );
      }
      spans.add(
        TextSpan(
          text: '${(games / total * 100).round()}% ',
          style: TextStyle(
            color: _phaseColor(phase),
            fontWeight: FontWeight.w800,
          ),
        ),
      );
      spans.add(TextSpan(text: labels.shortPhase(phase)));
    }
    return Text.rich(
      TextSpan(
        style: theme.textTheme.bodySmall?.copyWith(
          color: theme.colorScheme.onSurfaceVariant,
        ),
        children: spans,
      ),
    );
  }
}

/// Full-width segmented strip weighted by each phase's game count. Segments wide
/// enough to hold text label themselves ("56% Mittelspiel"); narrower ones fall
/// back to just the percentage, then to colour alone — so it never overflows.
class _PhaseDistributionStrip extends StatelessWidget {
  const _PhaseDistributionStrip({
    required this.tallies,
    required this.total,
    required this.labels,
  });

  final Map<String, StatTally> tallies;
  final int total;
  final _PhaseText labels;

  static const _inkColor = Color(0xFF0B1220); // dark ink on bright segments
  static const _gap = 2.0;

  @override
  Widget build(BuildContext context) {
    final phases = [
      for (final phase in _PhaseContent._order)
        if (tallies[phase]!.games > 0) phase,
    ];
    if (phases.isEmpty) return const SizedBox.shrink();

    return LayoutBuilder(
      builder: (context, constraints) {
        final available =
            constraints.maxWidth - _gap * (phases.length - 1);
        final children = <Widget>[];
        for (final phase in phases) {
          if (children.isNotEmpty) children.add(const SizedBox(width: _gap));
          final games = tallies[phase]!.games;
          final share = games / total;
          final width = available * share;
          final percent = '${(share * 100).round()}%';
          // Only draw text the segment can actually hold.
          final label = width >= 96
              ? '$percent ${labels.shortPhase(phase)}'
              : width >= 42
              ? percent
              : '';
          children.add(
            Expanded(
              flex: games,
              child: Tooltip(
                message:
                    '${labels.shortPhase(phase)} · $games ($percent)',
                child: Container(
                  color: _phaseColor(phase),
                  alignment: Alignment.center,
                  child: label.isEmpty
                      ? null
                      : Text(
                          label,
                          maxLines: 1,
                          overflow: TextOverflow.clip,
                          style: const TextStyle(
                            color: _inkColor,
                            fontSize: 11,
                            fontWeight: FontWeight.w800,
                          ),
                        ),
                ),
              ),
            ),
          );
        }
        return ClipRRect(
          borderRadius: BorderRadius.circular(6),
          child: SizedBox(
            height: 22,
            width: double.infinity,
            child: Row(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: children,
            ),
          ),
        );
      },
    );
  }
}

class _PhaseRow extends StatelessWidget {
  const _PhaseRow({
    required this.phase,
    required this.tally,
    required this.total,
    required this.labels,
  });

  final String phase;
  final StatTally tally;
  final int total; // games across all three phases
  final _PhaseText labels;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;
    final winRate = tally.games > 0 ? tally.wins / tally.games : null;
    final sharePct = total == 0 ? 0 : (tally.games / total * 100).round();

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        // Line 1: phase dot + label (left), win rate + game volume (right).
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
                          ' ${labels.winWord} · ${tally.games} ${labels.games} ($sharePct%)',
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
        // Line 2: volume × outcome bar.
        _PhaseOutcomeBar(tally: tally, total: total),
      ],
    );
  }
}

/// One phase's bar: its **length** is the phase's share of all classified games,
/// and the fill is split into win / draw / loss segments, so a single bar answers
/// both "how often do my games end here?" and "how do I score when they do?".
/// Pure flex weights, so it scales with the card and never overflows.
class _PhaseOutcomeBar extends StatelessWidget {
  const _PhaseOutcomeBar({required this.tally, required this.total});

  final StatTally tally;
  final int total;

  @override
  Widget build(BuildContext context) {
    final labels = _statsLabels(context);
    // A theme token rather than a translucent white, so the unfilled remainder
    // reads as a track on light surfaces as well as dark.
    final trackColor = Theme.of(context).colorScheme.outlineVariant;
    // Everything is weighted in units of games out of [total], so each phase's
    // fill occupies exactly its share of the row.
    final decided = tally.wins + tally.draws + tally.losses;
    final undecided = (tally.games - decided).clamp(0, tally.games);
    final rest = (total - tally.games).clamp(0, total);

    final message = tally.games == 0
        ? null
        : [
            if (tally.wins > 0) '${tally.wins} ${labels.wins}',
            if (tally.draws > 0) '${tally.draws} ${labels.draws}',
            if (tally.losses > 0) '${tally.losses} ${labels.losses}',
          ].join(', ');

    Widget segment(int weight, Color color) => Expanded(
      flex: weight,
      child: message == null
          ? ColoredBox(color: color)
          : Tooltip(message: message, child: ColoredBox(color: color)),
    );

    return ClipRRect(
      borderRadius: BorderRadius.circular(5),
      // `stretch` + an explicit width: a childless ColoredBox otherwise
      // collapses to constraints.smallest and the bar renders zero-height.
      child: SizedBox(
        height: 10,
        width: double.infinity,
        child: total == 0
            ? ColoredBox(color: trackColor)
            : Row(
                crossAxisAlignment: CrossAxisAlignment.stretch,
                children: [
                  if (tally.wins > 0) segment(tally.wins, _kWinColor),
                  if (tally.draws > 0) segment(tally.draws, _kDrawColor),
                  if (tally.losses > 0) segment(tally.losses, _kLossColor),
                  if (undecided > 0)
                    Expanded(
                      flex: undecided,
                      child: ColoredBox(
                        color: Theme.of(context).colorScheme.outlineVariant,
                      ),
                    ),
                  if (rest > 0)
                    Expanded(flex: rest, child: ColoredBox(color: trackColor)),
                ],
              ),
      ),
    );
  }
}

Color _rateColor(BuildContext context, double? rate) {
  if (rate == null) return Theme.of(context).colorScheme.onSurfaceVariant;
  if (rate >= 0.5) return AppTheme.success;
  if (rate < 0.45) return Theme.of(context).colorScheme.error;
  return Theme.of(context).colorScheme.onSurface;
}

/// Phase identity colours. Kept in the same soft, desaturated register as the
/// app's own accents (primary #7CA2FF, tertiary #E5C07B) so the strip reads as
/// part of the surface rather than sitting on top of it — while staying light
/// enough for the dark ink label inside each segment (>6:1 contrast).
Color _phaseColor(String phase) => switch (phase) {
  'opening' => const Color(0xFF5FA8D3), // soft steel blue
  'middlegame' => const Color(0xFFD69A57), // soft amber
  'endgame' => const Color(0xFF9C87C8), // soft violet
  _ => const Color(0xFF6E7A90),
};

// Note: `_percent` is shared from termination_section.dart (same library).

class _PhaseText {
  const _PhaseText({
    required this.title,
    required this.subtitle,
    required this.opening,
    required this.middlegame,
    required this.endgame,
    required this.openingShort,
    required this.middlegameShort,
    required this.endgameShort,
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
  final String openingShort;
  final String middlegameShort;
  final String endgameShort;
  final String games;
  final String winWord;
  final String empty;
  final String noProfile;
  final String error;
  final String retry;
  final String Function(int classified, int total) classifiedNote;

  /// Phase name including its move range, e.g. "Mittelspiel (13–30)".
  String phase(String phase) => switch (phase) {
    'opening' => opening,
    'middlegame' => middlegame,
    'endgame' => endgame,
    _ => phase,
  };

  /// Phase name on its own, for compact captions and strip segments.
  String shortPhase(String phase) => switch (phase) {
    'opening' => openingShort,
    'middlegame' => middlegameShort,
    'endgame' => endgameShort,
    _ => phase,
  };
}

_PhaseText _phaseText(BuildContext context) {
  final strings = AppLocalizations.of(context);
  return _PhaseText(
    title: strings.statsPhaseTitle,
    subtitle: strings.statsPhaseSubtitle,
    opening: strings.statsPhaseOpening,
    middlegame: strings.statsPhaseMiddlegame,
    endgame: strings.statsPhaseEndgame,
    openingShort: strings.statsPhaseOpeningShort,
    middlegameShort: strings.statsPhaseMiddlegameShort,
    endgameShort: strings.statsPhaseEndgameShort,
    games: strings.statsPhaseGames,
    winWord: strings.statsPhaseWinWord,
    empty: strings.statsPhaseEmpty,
    noProfile: strings.statsPhaseNoProfile,
    error: strings.statsPhaseError,
    retry: strings.statsPhaseRetry,
    classifiedNote: strings.statsPhaseClassifiedNote,
  );
}
