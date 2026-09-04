part of '../../../ui/app_root.dart';

/// Time-control buckets the statistics tab can be filtered by. `all` keeps every
/// game; the others map 1:1 onto the native `time_control_type` values so the
/// overview payload's `byTimeControl` buckets and `queryGames` filtering agree.
const _statTimeControls = <String>['all', 'bullet', 'blitz', 'rapid'];

/// Segmented filter shown at the top of the statistics tab. Selecting a bucket
/// recomputes the metrics that the current data allows (overview headline via
/// the pre-aggregated `byTimeControl`, and the form strip / rating trend via a
/// filtered `queryGames`).
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

/// Proportional, themed win / draw / loss ribbon. Replaces the old raw-colour
/// `_WdlBar`; segments carry tooltips and can optionally show a count legend.
class _WinLossDrawBar extends StatelessWidget {
  const _WinLossDrawBar({required this.tally, this.height = 12});

  final StatTally tally;
  final double height;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;
    final labels = _statsLabels(context);
    final radius = height;
    final decided = tally.decided;
    if (decided == 0) {
      return ClipRRect(
        borderRadius: BorderRadius.circular(radius),
        child: SizedBox(
          height: height,
          child: ColoredBox(color: scheme.surfaceContainerHighest),
        ),
      );
    }

    Widget segment(int flex, Color color, String label, int count) {
      if (flex == 0) return const SizedBox.shrink();
      final percent = (count / decided * 100).round();
      return Expanded(
        flex: flex,
        child: Tooltip(
          message: '$label · $count ($percent%)',
          child: ColoredBox(color: color),
        ),
      );
    }

    return ClipRRect(
      borderRadius: BorderRadius.circular(radius),
      child: SizedBox(
        height: height,
        child: Row(
          children: [
            segment(tally.wins, AppTheme.success, labels.wins, tally.wins),
            segment(
              tally.draws,
              scheme.onSurfaceVariant,
              labels.draws,
              tally.draws,
            ),
            segment(tally.losses, scheme.error, labels.losses, tally.losses),
          ],
        ),
      ),
    );
  }
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
          style: Theme.of(context).textTheme.bodySmall?.copyWith(
            color: scheme.onSurfaceVariant,
          ),
        ),
      ],
    );

    return Wrap(
      spacing: 16,
      runSpacing: 4,
      children: [
        dot(AppTheme.success, labels.wins, tally.wins),
        dot(scheme.onSurfaceVariant, labels.draws, tally.draws),
        dot(scheme.error, labels.losses, tally.losses),
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
      'win' => (AppTheme.success, Icons.check_rounded, _statsLabels(context).wins),
      'loss' => (scheme.error, Icons.close_rounded, _statsLabels(context).losses),
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

/// Resolve one game to a win/draw/loss/unknown outcome from the profile's
/// perspective. Mirrors the native `effective_outcome`: trust the provider
/// outcome, else derive from the result and the colour the profile played.
/// This is per-row display selection, not aggregation — the tallying stays in
/// the C++ core.
String _statGameOutcome(GameSummary game) {
  final provider = game.providerOutcome;
  if (provider == 'win' || provider == 'loss' || provider == 'draw') {
    return provider;
  }
  final color = game.profileColor;
  if (color != 'white' && color != 'black') return 'unknown';
  return switch (game.result) {
    '1/2-1/2' => 'draw',
    '1-0' => color == 'white' ? 'win' : 'loss',
    '0-1' => color == 'white' ? 'loss' : 'win',
    _ => 'unknown',
  };
}

/// The rating the profile carried in a game, or null when it cannot be
/// attributed (unknown colour / missing rating).
int? _statProfileRating(GameSummary game) => switch (game.profileColor) {
  'white' => game.whiteRating,
  'black' => game.blackRating,
  _ => null,
};

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
  });

  final String wins;
  final String draws;
  final String losses;
  final String all;
  final String allTimeControlsNote;

  String timeControl(String value) => switch (value) {
    'bullet' => 'Bullet',
    'blitz' => 'Blitz',
    'rapid' => 'Rapid',
    _ => all,
  };
}

_StatsLabels _statsLabels(BuildContext context) {
  switch (Localizations.localeOf(context).languageCode) {
    case 'ar':
      return const _StatsLabels(
        wins: 'انتصارات',
        draws: 'تعادلات',
        losses: 'هزائم',
        all: 'الكل',
        allTimeControlsNote: 'كل أنواع الوقت',
      );
    case 'en':
      return const _StatsLabels(
        wins: 'Wins',
        draws: 'Draws',
        losses: 'Losses',
        all: 'All',
        allTimeControlsNote: 'All time controls',
      );
    default:
      return const _StatsLabels(
        wins: 'Siege',
        draws: 'Remis',
        losses: 'Niederlagen',
        all: 'Alle',
        allTimeControlsNote: 'Alle Zeitkontrollen',
      );
  }
}
