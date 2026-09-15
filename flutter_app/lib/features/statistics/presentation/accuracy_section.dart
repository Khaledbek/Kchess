// -----------------------------------------------------------------------------
// Section: accuracy section presentation
// -----------------------------------------------------------------------------

part of '../../../ui/app_root.dart';

/// Accuracy over the analysed games: how far background analysis has come,
/// the average, whether the player is improving, the trend line, and where
/// accuracy is lost (phase, colour, time control). Every number is native.
class _AccuracyCard extends StatefulWidget {
  const _AccuracyCard({
    required this.controller,
    required this.future,
    required this.timeControl,
    required this.onRetry,
    required this.onMoreAnalysed,
  });

  final AppController controller;
  final Future<AccuracyStats> future;
  final String timeControl;
  final VoidCallback onRetry;

  /// Background analysis has finished enough new games to be worth reloading.
  final VoidCallback onMoreAnalysed;

  @override
  State<_AccuracyCard> createState() => _AccuracyCardState();
}

class _AccuracyCardState extends State<_AccuracyCard> {
  static const _pollInterval = Duration(seconds: 10);

  /// Reload after this many newly analysed games, not after every one.
  static const _reloadEvery = 3;

  BackgroundAnalysisStatus? _status;
  int? _analysedAtLoad;
  int? _revisionAtLoad;
  Timer? _poll;

  @override
  void initState() {
    super.initState();
    unawaited(_refreshStatus());
    _poll = Timer.periodic(_pollInterval, (_) => unawaited(_refreshStatus()));
  }

  @override
  void dispose() {
    _poll?.cancel();
    super.dispose();
  }

  Future<void> _refreshStatus() async {
    final BackgroundAnalysisStatus status;
    try {
      status = await widget.controller.gateway.backgroundAnalysisStatus();
    } catch (_) {
      return;
    }
    if (!mounted) return;
    final loadedAt = _analysedAtLoad;
    setState(() => _status = status);
    if (loadedAt == null) {
      _analysedAtLoad = status.analysedGames;
      _revisionAtLoad = status.sourceRevision;
      return;
    }
    final finishedMore = status.analysedGames - loadedAt >= _reloadEvery ||
        (status.state == 'complete' && status.analysedGames != loadedAt);
    if (finishedMore || status.sourceRevision != _revisionAtLoad) {
      _analysedAtLoad = status.analysedGames;
      _revisionAtLoad = status.sourceRevision;
      widget.onMoreAnalysed();
    }
  }

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final strings = AppLocalizations.of(context);
    final status = _status;
    return Card(
      key: const Key('stats-accuracy'),
      child: Padding(
        padding: const EdgeInsets.all(18),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Row(
              children: [
                Icon(Icons.gps_fixed_rounded, color: theme.colorScheme.primary),
                const SizedBox(width: 8),
                Expanded(
                  child: Text(
                    strings.statsAccuracyTitle,
                    style: theme.textTheme.titleMedium?.copyWith(fontWeight: FontWeight.w800),
                  ),
                ),
                if (widget.timeControl != 'all')
                  _FilterPill(label: _statsLabels(context).timeControl(widget.timeControl)),
              ],
            ),
            if (status != null && status.totalGames > 0) ...[
              const SizedBox(height: 10),
              _BackgroundAnalysisProgress(status: status),
            ],
            const SizedBox(height: 14),
            FutureBuilder<AccuracyStats>(
              future: widget.future,
              builder: (context, snapshot) {
                // Keep the previous numbers on screen while a reload runs, so
                // the card does not collapse to a spinner every few games.
                final stats = snapshot.data;
                if (stats == null) {
                  if (snapshot.hasError) {
                    return _OverviewMessage(
                      icon: Icons.error_outline,
                      text: strings.statsAccuracyError,
                      action: TextButton(
                        onPressed: widget.onRetry,
                        child: Text(strings.statsOpeningsRetry),
                      ),
                    );
                  }
                  return const SizedBox(
                    height: 160,
                    child: Center(child: CircularProgressIndicator()),
                  );
                }
                if (!stats.hasProfile) {
                  return _OverviewMessage(
                    icon: Icons.person_outline,
                    text: strings.statsPhaseNoProfile,
                  );
                }
                if (stats.analysedGames == 0) {
                  return _OverviewMessage(
                    icon: Icons.insights_outlined,
                    text: strings.statsAccuracyEmpty,
                  );
                }
                return _AccuracyContent(stats: stats);
              },
            ),
          ],
        ),
      ),
    );
  }
}

/// One line on how far background analysis has come.
class _BackgroundAnalysisProgress extends StatelessWidget {
  const _BackgroundAnalysisProgress({required this.status});

  final BackgroundAnalysisStatus status;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;
    final strings = AppLocalizations.of(context);
    final (IconData icon, String label) = switch (status.state) {
      'running' => (Icons.autorenew_rounded, strings.backgroundAnalysisRunning),
      'paused' => (Icons.pause_circle_outline_rounded, strings.backgroundAnalysisPaused),
      'complete' => (Icons.check_circle_outline_rounded, strings.backgroundAnalysisComplete),
      'disabled' => (Icons.do_not_disturb_on_outlined, strings.backgroundAnalysisDisabled),
      'unavailable' => (Icons.error_outline_rounded, strings.backgroundAnalysisUnavailable),
      _ => (Icons.schedule_rounded, strings.backgroundAnalysisRunning),
    };
    final total = status.totalGames;
    final share = total == 0 ? 0.0 : status.analysedGames / total;
    return Column(
      key: const Key('stats-accuracy-progress'),
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        Row(
          children: [
            Icon(icon, size: 16, color: scheme.onSurfaceVariant),
            const SizedBox(width: 6),
            Expanded(
              child: Text(
                label,
                maxLines: 1,
                overflow: TextOverflow.ellipsis,
                style: theme.textTheme.bodySmall?.copyWith(color: scheme.onSurfaceVariant),
              ),
            ),
            const SizedBox(width: 8),
            // The count matters more than the state label, so it gets more
            // room, but neither may push the row past the card.
            Flexible(
              flex: 2,
              child: Text(
                strings.backgroundAnalysisProgress(status.analysedGames, total),
                maxLines: 1,
                overflow: TextOverflow.ellipsis,
                textAlign: TextAlign.end,
                style: theme.textTheme.bodySmall?.copyWith(
                  color: scheme.onSurfaceVariant,
                  fontWeight: FontWeight.w600,
                ),
              ),
            ),
          ],
        ),
        const SizedBox(height: 6),
        ClipRRect(
          borderRadius: BorderRadius.circular(3),
          child: LinearProgressIndicator(
            value: share.clamp(0.0, 1.0),
            minHeight: 4,
            backgroundColor: scheme.surfaceContainerHighest,
          ),
        ),
      ],
    );
  }
}

class _AccuracyContent extends StatelessWidget {
  const _AccuracyContent({required this.stats});

  final AccuracyStats stats;

  static String _percent(double? value) => value == null ? '–' : '${value.round()}%';

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;
    final strings = AppLocalizations.of(context);
    final phaseLabels = [
      strings.statsPhaseOpeningShort,
      strings.statsPhaseMiddlegameShort,
      strings.statsPhaseEndgameShort,
    ];
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        Row(
          crossAxisAlignment: CrossAxisAlignment.end,
          children: [
            Flexible(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(
                    _percent(stats.averageAccuracy),
                    key: const Key('stats-accuracy-average'),
                    style: theme.textTheme.headlineMedium?.copyWith(fontWeight: FontWeight.w800),
                  ),
                  Text(
                    strings.statsAccuracyAverage,
                    style: theme.textTheme.bodySmall?.copyWith(color: scheme.onSurfaceVariant),
                  ),
                ],
              ),
            ),
            const SizedBox(width: 16),
            // Long translations wrap here instead of pushing the row wide.
            Expanded(
              child: stats.blundersPerGame == null
                  ? const SizedBox.shrink()
                  : Text(
                      strings.statsAccuracyBlundersPerGame(stats.blundersPerGame!.toStringAsFixed(1)),
                      textAlign: TextAlign.end,
                      style: theme.textTheme.bodySmall?.copyWith(color: scheme.onSurfaceVariant),
                    ),
            ),
          ],
        ),
        const SizedBox(height: 14),
        _AccuracyTrendBanner(trend: stats.trend),
        if (stats.timeline.length >= 2) ...[
          const SizedBox(height: 16),
          _AccuracyChart(points: stats.timeline),
        ],
        const SizedBox(height: 18),
        _AccuracySubheading(label: strings.statsPhaseTitle),
        for (final (index, phase) in stats.byPhase.indexed)
          if (phase.games > 0)
            _AccuracyBar(
              key: ValueKey('stats-accuracy-phase-${phase.phase}'),
              label: index < phaseLabels.length ? phaseLabels[index] : phase.phase,
              accuracy: phase.accuracy,
              detail: strings.statsAccuracyErrorsPerGame(phase.errorsPerGame.toStringAsFixed(1)),
            ),
        const SizedBox(height: 10),
        _AccuracySubheading(label: strings.statsAccuracyByColor),
        for (final (label, group) in [
          (strings.statsOpeningsWhite, stats.white),
          (strings.statsOpeningsBlack, stats.black),
        ])
          if (group.games > 0)
            _AccuracyBar(
              label: label,
              accuracy: group.accuracy,
              detail: strings.statsAccuracyGames(group.games),
            ),
        if (stats.byTimeControl.length > 1) ...[
          const SizedBox(height: 10),
          _AccuracySubheading(label: strings.statsAccuracyByTimeControl),
          for (final group in stats.byTimeControl)
            _AccuracyBar(
              label: _timeControlLabel(context, group.timeControl),
              accuracy: group.accuracy,
              detail: strings.statsAccuracyGames(group.games),
            ),
        ],
      ],
    );
  }
}

/// "Improving / Holding steady / Declining" with the numbers behind it.
class _AccuracyTrendBanner extends StatelessWidget {
  const _AccuracyTrendBanner({required this.trend});

  final AccuracyTrend trend;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;
    final strings = AppLocalizations.of(context);
    final (IconData icon, Color color, String title) = switch (trend.verdict) {
      'improving' => (Icons.trending_up_rounded, AppTheme.success, strings.statsAccuracyImproving),
      'declining' => (Icons.trending_down_rounded, scheme.error, strings.statsAccuracyDeclining),
      'steady' => (Icons.trending_flat_rounded, scheme.primary, strings.statsAccuracySteady),
      _ => (Icons.hourglass_empty_rounded, scheme.onSurfaceVariant, strings.statsAccuracyTrendInsufficient),
    };
    String percent(double? value) => value == null ? '–' : '${value.round()}%';
    final details = trend.verdict == 'insufficient'
        ? [strings.statsAccuracyTrendNeeded(trend.gamesNeeded)]
        : [
            strings.statsAccuracyTrendDetail(
              trend.window,
              percent(trend.recentAccuracy),
              percent(trend.previousAccuracy),
            ),
            if (trend.recentBlunders != null && trend.previousBlunders != null)
              strings.statsAccuracyTrendBlunders(
                trend.recentBlunders!.toStringAsFixed(1),
                trend.previousBlunders!.toStringAsFixed(1),
              ),
          ];
    return Container(
      key: const Key('stats-accuracy-trend'),
      padding: const EdgeInsets.all(12),
      decoration: BoxDecoration(
        color: color.withValues(alpha: 0.10),
        borderRadius: BorderRadius.circular(12),
        border: Border.all(color: color.withValues(alpha: 0.35)),
      ),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Icon(icon, color: color, size: 26),
          const SizedBox(width: 10),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  title,
                  style: theme.textTheme.titleSmall?.copyWith(
                    fontWeight: FontWeight.w800,
                    color: trend.verdict == 'insufficient' ? null : color,
                  ),
                ),
                for (final line in details) ...[
                  const SizedBox(height: 2),
                  Text(
                    line,
                    style: theme.textTheme.bodySmall?.copyWith(color: scheme.onSurfaceVariant),
                  ),
                ],
              ],
            ),
          ),
        ],
      ),
    );
  }
}

/// Each analysed game as a faint dot, the rolling average as the line.
class _AccuracyChart extends StatelessWidget {
  const _AccuracyChart({required this.points});

  final List<AccuracyPoint> points;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;
    var lowest = 100.0;
    var highest = 0.0;
    for (final point in points) {
      if (point.accuracy < lowest) lowest = point.accuracy;
      if (point.accuracy > highest) highest = point.accuracy;
    }
    // Fit the scale to the games, in steps of ten, so the line uses the
    // chart's height instead of hugging the bottom of a 0-100 axis.
    final minY = ((lowest - 5) / 10).floor().clamp(0, 9) * 10.0;
    final fitted = ((highest + 5) / 10).ceil().clamp(1, 10) * 10.0;
    final maxY = fitted < minY + 20 ? minY + 20 : fitted;
    final lastX = (points.length - 1).toDouble();
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        SizedBox(
          key: const Key('stats-accuracy-chart'),
          height: 170,
          child: LineChart(
            LineChartData(
              minX: 0,
              maxX: lastX,
              minY: minY,
              maxY: maxY,
              clipData: const FlClipData.all(),
              borderData: FlBorderData(show: false),
              gridData: FlGridData(
                show: true,
                drawVerticalLine: false,
                horizontalInterval: 10,
                getDrawingHorizontalLine: (_) =>
                    FlLine(color: scheme.outlineVariant, strokeWidth: 1),
              ),
              titlesData: FlTitlesData(
                topTitles: const AxisTitles(sideTitles: SideTitles(showTitles: false)),
                rightTitles: const AxisTitles(sideTitles: SideTitles(showTitles: false)),
                bottomTitles: const AxisTitles(sideTitles: SideTitles(showTitles: false)),
                leftTitles: AxisTitles(
                  sideTitles: SideTitles(
                    showTitles: true,
                    reservedSize: 36,
                    interval: 10,
                    getTitlesWidget: (value, meta) => Padding(
                      padding: const EdgeInsets.only(right: 6),
                      child: Text(
                        '${value.toInt()}',
                        style: theme.textTheme.bodySmall?.copyWith(color: scheme.onSurfaceVariant),
                      ),
                    ),
                  ),
                ),
              ),
              lineTouchData: const LineTouchData(enabled: false),
              lineBarsData: [
                LineChartBarData(
                  spots: [
                    for (final (index, point) in points.indexed)
                      FlSpot(index.toDouble(), point.accuracy),
                  ],
                  barWidth: 0,
                  color: Colors.transparent,
                  dotData: FlDotData(
                    show: true,
                    getDotPainter: (spot, percent, bar, index) => FlDotCirclePainter(
                      radius: 2.4,
                      color: scheme.primary.withValues(alpha: 0.30),
                      strokeWidth: 0,
                    ),
                  ),
                ),
                LineChartBarData(
                  spots: [
                    for (final (index, point) in points.indexed)
                      FlSpot(index.toDouble(), point.average),
                  ],
                  isCurved: true,
                  curveSmoothness: 0.25,
                  preventCurveOverShooting: true,
                  color: scheme.primary,
                  barWidth: 3,
                  dotData: const FlDotData(show: false),
                  belowBarData: BarAreaData(
                    show: true,
                    color: scheme.primary.withValues(alpha: 0.08),
                  ),
                ),
              ],
            ),
          ),
        ),
        const SizedBox(height: 6),
        Text(
          '${_formatPointDate(points.first.endedAt)}  —  ${_formatPointDate(points.last.endedAt)}',
          style: theme.textTheme.bodySmall?.copyWith(color: scheme.onSurfaceVariant),
        ),
      ],
    );
  }
}

class _AccuracySubheading extends StatelessWidget {
  const _AccuracySubheading({required this.label});

  final String label;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return Padding(
      padding: const EdgeInsets.only(bottom: 6),
      child: Text(
        label,
        style: theme.textTheme.labelLarge?.copyWith(fontWeight: FontWeight.w700),
      ),
    );
  }
}

/// A labelled accuracy bar: label and detail, the bar, the percentage.
class _AccuracyBar extends StatelessWidget {
  const _AccuracyBar({
    required this.label,
    required this.accuracy,
    required this.detail,
    super.key,
  });

  final String label;
  final double? accuracy;
  final String detail;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;
    final share = ((accuracy ?? 0) / 100).clamp(0.0, 1.0);
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 4),
      child: Row(
        children: [
          SizedBox(
            width: 112,
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(label, maxLines: 1, overflow: TextOverflow.ellipsis, style: theme.textTheme.bodyMedium),
                Text(
                  detail,
                  maxLines: 1,
                  overflow: TextOverflow.ellipsis,
                  style: theme.textTheme.bodySmall?.copyWith(color: scheme.onSurfaceVariant),
                ),
              ],
            ),
          ),
          const SizedBox(width: 10),
          Expanded(
            child: LayoutBuilder(
              builder: (context, constraints) => Stack(
                children: [
                  // Explicit sizes on both boxes: an unsized box in a Row
                  // collapses to nothing and the bar silently disappears.
                  Container(
                    width: constraints.maxWidth,
                    height: 8,
                    decoration: BoxDecoration(
                      color: scheme.surfaceContainerHighest,
                      borderRadius: BorderRadius.circular(4),
                    ),
                  ),
                  Container(
                    width: constraints.maxWidth * share,
                    height: 8,
                    decoration: BoxDecoration(
                      color: scheme.primary,
                      borderRadius: BorderRadius.circular(4),
                    ),
                  ),
                ],
              ),
            ),
          ),
          const SizedBox(width: 10),
          SizedBox(
            width: 44,
            child: Text(
              accuracy == null ? '–' : '${accuracy!.round()}%',
              textAlign: TextAlign.end,
              style: theme.textTheme.bodyMedium?.copyWith(fontWeight: FontWeight.w700),
            ),
          ),
        ],
      ),
    );
  }
}
