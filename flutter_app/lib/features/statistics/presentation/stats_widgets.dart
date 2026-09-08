// -----------------------------------------------------------------------------
// Section: stats widgets presentation
// -----------------------------------------------------------------------------

part of '../../../ui/app_root.dart';

/// Time-control buckets the statistics tab can be filtered by. `all` keeps every
/// game; the others map 1:1 onto the native `time_control_type` values so the
/// overview payload's `byTimeControl` buckets and native timeline filtering agree.
const _statTimeControls = <String>['all', 'bullet', 'blitz', 'rapid'];

/// Segmented filter shown at the top of the statistics tab. Selecting a bucket
/// recomputes the metrics that the current data allows (overview headline via
/// the pre-aggregated `byTimeControl`, and the form strip / rating trend via a
/// filtered native timeline).
class _TimeControlFilterBar extends StatelessWidget {
  const _TimeControlFilterBar({
    required this.selected,
    required this.onChanged,
  });

  final String selected;
  final ValueChanged<String> onChanged;

  IconData _iconFor(String value) => switch (value) {
    'bullet' => Icons.bolt,
    'blitz' => Icons.flash_on,
    'rapid' => Icons.timer_outlined,
    _ => Icons.all_inclusive,
  };

  @override
  Widget build(BuildContext context) {
    final labels = _statsLabels(context);
    // SegmentedButton sizes to its content, so allow it to scroll on narrow
    // windows instead of overflowing.
    return SingleChildScrollView(
      scrollDirection: Axis.horizontal,
      child: SegmentedButton<String>(
        showSelectedIcon: false,
        segments: [
          for (final value in _statTimeControls)
            ButtonSegment<String>(
              value: value,
              icon: Icon(_iconFor(value), size: 18),
              label: Text(labels.timeControl(value)),
            ),
        ],
        selected: {selected},
        onSelectionChanged: (selection) => onChanged(selection.first),
      ),
    );
  }
}

/// Shared win / draw / loss palette for every proportion bar and legend in the
/// statistics tab, so the mini bars and their keys always agree.
// Tuned to the app's own palette rather than to a bright chart palette: the
// dark scheme is deliberately soft (primary #7CA2FF, tertiary #E5C07B, error
// #EB6B72), so saturated chart colours sit on top of the surface instead of in
// it. These are mid-tones, which keeps them legible on both themes.
const _kWinColor = AppTheme.success; // #2E9E5B, the theme's own win green
const _kDrawColor = Color(0xFF6E7A90); // muted slate
const _kLossColor = Color(0xFFD2555F); // muted brick red, between the two
// error tones the light/dark schemes use

/// Reusable horizontal stacked win/draw/loss proportion bar. Segments are sized
/// by count; an all-zero tally shows a muted empty track.
class _WinLossDrawRatioBar extends StatelessWidget {
  const _WinLossDrawRatioBar({
    required this.wins,
    required this.draws,
    required this.losses,
    this.height = 6,
  });

  final int wins;
  final int draws;
  final int losses;
  final double height;

  @override
  Widget build(BuildContext context) {
    final labels = _statsLabels(context);
    final total = wins + draws + losses;
    // Hovering any segment surfaces the whole win/draw/loss breakdown, so the
    // bar reads the same wherever the cursor lands.
    final message = _breakdown(labels, total);
    return ClipRRect(
      borderRadius: BorderRadius.circular(3),
      // A childless ColoredBox is a proxy box, so it takes constraints.smallest
      // — zero height under a Row's default (loose) cross-axis constraints, and
      // zero width under a loose parent. The explicit width plus `stretch` keep
      // every segment tight in both axes; without them the bar is invisible.
      child: SizedBox(
        height: height,
        width: double.infinity,
        child: total == 0
            // A theme token, so the empty track stays visible on light surfaces
            // too (a translucent white track vanishes there).
            ? ColoredBox(color: Theme.of(context).colorScheme.outlineVariant)
            : Row(
                crossAxisAlignment: CrossAxisAlignment.stretch,
                children: [
                  _segment(wins, _kWinColor, message),
                  _segment(draws, _kDrawColor, message),
                  _segment(losses, _kLossColor, message),
                ],
              ),
      ),
    );
  }

  /// e.g. "61 Siege (41%), 16 Remis (11%), 70 Niederlagen (48%)" — only the
  /// outcomes that actually occurred are listed.
  String _breakdown(_StatsLabels labels, int total) {
    String part(int count, String label) =>
        '$count $label (${total == 0 ? 0 : (count / total * 100).round()}%)';
    return [
      if (wins > 0) part(wins, labels.wins),
      if (draws > 0) part(draws, labels.draws),
      if (losses > 0) part(losses, labels.losses),
    ].join(', ');
  }

  Widget _segment(int count, Color color, String message) {
    if (count == 0) return const SizedBox.shrink();
    return Expanded(
      flex: count,
      child: Tooltip(
        message: message,
        child: ColoredBox(color: color),
      ),
    );
  }
}

/// Convenience wrapper that draws a [_WinLossDrawRatioBar] from a [StatTally].
class _WinLossDrawBar extends StatelessWidget {
  const _WinLossDrawBar({required this.tally, this.height = 12});

  final StatTally tally;
  final double height;

  @override
  Widget build(BuildContext context) => _WinLossDrawRatioBar(
    wins: tally.wins,
    draws: tally.draws,
    losses: tally.losses,
    height: height,
  );
}

/// Small win/draw/loss colour key shown beside a [_WinLossDrawBar].
class _WdlLegend extends StatelessWidget {
  const _WdlLegend({required this.tally});

  final StatTally tally;

  @override
  Widget build(BuildContext context) {
    final scheme = Theme.of(context).colorScheme;
    final labels = _statsLabels(context);
    Widget dot(Color color, String label, int count) => Row(
      mainAxisSize: MainAxisSize.min,
      children: [
        Container(
          width: 9,
          height: 9,
          decoration: BoxDecoration(color: color, shape: BoxShape.circle),
        ),
        const SizedBox(width: 6),
        Text(
          '$label $count',
          style: Theme.of(context).textTheme.bodySmall
              ?.copyWith(color: scheme.onSurfaceVariant),
        ),
      ],
    );

    return Wrap(
      spacing: 16,
      runSpacing: 4,
      children: [
        dot(_kWinColor, labels.wins, tally.wins),
        dot(_kDrawColor, labels.draws, tally.draws),
        dot(_kLossColor, labels.losses, tally.losses),
      ],
    );
  }
}

/// A single headline metric rendered as a bordered mini-card (games, win rate,
/// score, record …).
class _StatTile extends StatelessWidget {
  const _StatTile({
    required this.value,
    required this.label,
    required this.icon,
    this.accent,
  });

  final String value;
  final String label;
  final IconData icon;
  final Color? accent;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;
    final accentColor = accent ?? scheme.primary;
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 14),
      decoration: BoxDecoration(
        color: scheme.surfaceContainerLow,
        borderRadius: BorderRadius.circular(14),
        border: Border.all(color: scheme.outlineVariant),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        mainAxisSize: MainAxisSize.min,
        children: [
          Row(
            children: [
              Icon(icon, size: 16, color: accentColor),
              const SizedBox(width: 6),
              Expanded(
                child: Text(
                  label,
                  maxLines: 1,
                  overflow: TextOverflow.ellipsis,
                  style: theme.textTheme.bodySmall?.copyWith(
                    color: scheme.onSurfaceVariant,
                    fontWeight: FontWeight.w600,
                  ),
                ),
              ),
            ],
          ),
          const SizedBox(height: 8),
          Text(
            value,
            maxLines: 1,
            overflow: TextOverflow.ellipsis,
            style: theme.textTheme.headlineSmall?.copyWith(
              fontWeight: FontWeight.w800,
            ),
          ),
        ],
      ),
    );
  }
}

/// Lays out [tiles] as a responsive grid: four across on wide cards, two on
/// narrow ones, sized so they share the row evenly.
class _StatTileGrid extends StatelessWidget {
  const _StatTileGrid({required this.tiles});

  final List<Widget> tiles;

  @override
  Widget build(BuildContext context) {
    return LayoutBuilder(
      builder: (context, constraints) {
        const spacing = 12.0;
        final columns = constraints.maxWidth >= 520
            ? 4
            : constraints.maxWidth >= 300
            ? 2
            : 1;
        final width =
            (constraints.maxWidth - spacing * (columns - 1)) / columns;
        return Wrap(
          spacing: spacing,
          runSpacing: spacing,
          children: [
            for (final tile in tiles) SizedBox(width: width, child: tile),
          ],
        );
      },
    );
  }
}

/// One circular result indicator in the "Aktuelle Form" strip.
class _StatResultChip extends StatelessWidget {
  const _StatResultChip({required this.outcome, required this.onTap});

  final String outcome; // win | loss | draw
  final VoidCallback onTap;

  static const double size = 34;

  @override
  Widget build(BuildContext context) {
    final scheme = Theme.of(context).colorScheme;
    final (color, icon, label) = switch (outcome) {
      'win' => (
        AppTheme.success,
        Icons.check_rounded,
        _statsLabels(context).wins,
      ),
      'loss' => (
        scheme.error,
        Icons.close_rounded,
        _statsLabels(context).losses,
      ),
      _ => (
        scheme.onSurfaceVariant,
        Icons.remove_rounded,
        _statsLabels(context).draws,
      ),
    };
    return Semantics(
      label: label,
      button: true,
      child: InkWell(
        onTap: onTap,
        borderRadius: BorderRadius.circular(size),
        child: Container(
          width: size,
          height: size,
          decoration: BoxDecoration(
            color: color.withValues(alpha: 0.16),
            shape: BoxShape.circle,
            border: Border.all(color: color.withValues(alpha: 0.55)),
          ),
          alignment: Alignment.center,
          child: Icon(icon, size: size * 0.56, color: color),
        ),
      ),
    );
  }
}

// Native outcome DTO; Flutter only chooses the matching visual.
String _statGameOutcome(GameSummary game) => game.statisticsOutcome;

/// The opponent's display name for a game from the profile's perspective.
String _statOpponentName(GameSummary game) => switch (game.profileColor) {
  'white' => game.blackName,
  'black' => game.whiteName,
  _ => '${game.whiteName} – ${game.blackName}',
};

/// The opponent's rating for a game from the profile's perspective.
int? _statOpponentRating(GameSummary game) => switch (game.profileColor) {
  'white' => game.blackRating,
  'black' => game.whiteRating,
  _ => null,
};

/// Shared, cross-section strings for the statistics tab (result nouns, the
/// time-control filter names). Section-specific copy lives with each section.
class _StatsLabels {
  const _StatsLabels({
    required this.wins,
    required this.draws,
    required this.losses,
    required this.all,
    required this.allTimeControlsNote,
    required this.controlLabel,
  });

  final String wins;
  final String draws;
  final String losses;
  final String all;
  final String allTimeControlsNote;
  final String Function(String) controlLabel;

  String timeControl(String value) =>
      value == 'all' ? all : controlLabel(value);
}

_StatsLabels _statsLabels(BuildContext context) {
  final strings = AppLocalizations.of(context);
  return _StatsLabels(
    wins: strings.statsWins,
    draws: strings.statsDraws,
    losses: strings.statsLosses,
    all: strings.statsAll,
    allTimeControlsNote: strings.statsAllTimeControlsNote,
    controlLabel: (value) => _timeControlLabel(context, value),
  );
}
