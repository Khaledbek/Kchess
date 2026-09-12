// -----------------------------------------------------------------------------
// Section: rating section presentation
// -----------------------------------------------------------------------------

part of '../../../ui/app_root.dart';

/// One rating line for a time-control category (Blitz, Bullet, Rapid, Daily …),
/// sorted oldest → newest with bad points already filtered out.
class _RatingSeries {
  const _RatingSeries({
    required this.timeControl,
    required this.color,
    required this.points,
    required this.current,
  });

  final String timeControl;
  final Color color;
  final List<StatisticsRatingPoint> points;

  final int current;
}

/// Distinct colours per time control so the merged "Alle" view reads as several
/// separate lines instead of one jagged line across incompatible scales.
const _kRatingSeriesColors = <String, Color>{
  'blitz': Color(0xFF3B82F6), // electric blue
  'bullet': Color(0xFFF59E0B), // amber
  'rapid': Color(0xFF10B981), // emerald green
  'daily': Color(0xFF8B5CF6), // purple
};

/// Rating-over-time line chart for the active profile. Under "Alle" it draws one
/// line per time control; a specific top-filter (or a legend tap) isolates one.
class _RatingTrendCard extends StatelessWidget {
  const _RatingTrendCard({
    required this.future,
    required this.timeControl,
    required this.onRetry,
  });

  final Future<StatisticsTimeline> future;
  final String timeControl;
  final VoidCallback onRetry;

  List<_RatingSeries> _buildSeries(
    BuildContext context,
    StatisticsTimeline timeline,
  ) => [
    for (final series in timeline.ratingSeries)
      _RatingSeries(
        timeControl: series.timeControl,
        color:
            _kRatingSeriesColors[series.timeControl] ??
            Theme.of(context).colorScheme.onSurfaceVariant,
        points: series.points,
        current: series.currentRating,
      ),
  ];

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final labels = _ratingText(context);
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(18),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                Icon(Icons.show_chart, color: theme.colorScheme.primary),
                const SizedBox(width: 8),
                Expanded(
                  child: Text(
                    labels.title,
                    style: theme.textTheme.titleMedium?.copyWith(
                      fontWeight: FontWeight.w800,
                    ),
                  ),
                ),
                if (timeControl != 'all')
                  _FilterPill(
                    label: _statsLabels(context).timeControl(timeControl),
                  ),
              ],
            ),
            const SizedBox(height: 12),
            FutureBuilder<StatisticsTimeline>(
              future: future,
              builder: (context, snapshot) {
                if (snapshot.connectionState != ConnectionState.done) {
                  return const SizedBox(
                    height: 220,
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
                final series = _buildSeries(context, snapshot.data!);
                if (series.isEmpty) {
                  return _OverviewMessage(
                    icon: Icons.stacked_line_chart_outlined,
                    text: labels.empty,
                  );
                }
                return _RatingTrendChart(series: series);
              },
            ),
          ],
        ),
      ),
    );
  }
}

class _RatingTrendChart extends StatefulWidget {
  const _RatingTrendChart({required this.series});

  final List<_RatingSeries> series;

  @override
  State<_RatingTrendChart> createState() => _RatingTrendChartState();
}

class _RatingTrendChartState extends State<_RatingTrendChart> {
  String? _isolated; // when set, only this time control's line is drawn

  @override
  void didUpdateWidget(_RatingTrendChart oldWidget) {
    super.didUpdateWidget(oldWidget);
    // Drop an isolation that no longer matches the (re-filtered) data.
    if (_isolated != null &&
        !widget.series.any((s) => s.timeControl == _isolated)) {
      _isolated = null;
    }
  }

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;

    final visible = _isolated == null
        ? widget.series
        : widget.series.where((s) => s.timeControl == _isolated).toList();

    // Combined bounds over the visible series.
    var minRating = visible.first.points.first.rating;
    var maxRating = minRating;
    var minX = visible.first.points.first.endedAt;
    var maxX = minX;
    for (final s in visible) {
      for (final p in s.points) {
        if (p.rating < minRating) minRating = p.rating;
        if (p.rating > maxRating) maxRating = p.rating;
        if (p.endedAt < minX) minX = p.endedAt;
        if (p.endedAt > maxX) maxX = p.endedAt;
      }
    }
    final span = (maxRating - minRating).clamp(20, 100000);
    final pad = (span * 0.15).ceil().clamp(10, 100);
    final minY = ((minRating - pad) / 10).floor() * 10.0;
    final maxY = ((maxRating + pad) / 10).ceil() * 10.0;
    final labelInterval = (((maxY - minY) / 4) / 10).ceil() * 10.0;
    final minXd = minX.toDouble();
    final maxXd = (maxX == minX ? minX + 1 : maxX).toDouble();

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        // Inline legend: one entry per series with its current rating; tapping
        // isolates that line.
        Wrap(
          spacing: 14,
          runSpacing: 6,
          children: [
            for (final s in widget.series)
              _RatingLegendChip(
                label: _timeControlLabel(context, s.timeControl),
                rating: s.current,
                color: s.color,
                active: _isolated == null || _isolated == s.timeControl,
                onTap: () => setState(
                  () => _isolated = _isolated == s.timeControl
                      ? null
                      : s.timeControl,
                ),
              ),
          ],
        ),
        const SizedBox(height: 16),
        SizedBox(
          height: 200,
          child: LineChart(
            LineChartData(
              minY: minY,
              maxY: maxY,
              minX: minXd,
              maxX: maxXd,
              clipData: FlClipData.all(),
              gridData: FlGridData(
                show: true,
                drawVerticalLine: false,
                horizontalInterval: labelInterval,
                getDrawingHorizontalLine: (_) =>
                    FlLine(color: scheme.outlineVariant, strokeWidth: 1),
              ),
              borderData: FlBorderData(show: false),
              titlesData: FlTitlesData(
                topTitles: const AxisTitles(
                  sideTitles: SideTitles(showTitles: false),
                ),
                rightTitles: const AxisTitles(
                  sideTitles: SideTitles(showTitles: false),
                ),
                bottomTitles: const AxisTitles(
                  sideTitles: SideTitles(showTitles: false),
                ),
                leftTitles: AxisTitles(
                  sideTitles: SideTitles(
                    showTitles: true,
                    reservedSize: 40,
                    interval: labelInterval,
                    getTitlesWidget: (value, meta) {
                      if (value < minY || value > maxY) {
                        return const SizedBox.shrink();
                      }
                      return Padding(
                        padding: const EdgeInsets.only(right: 6),
                        child: Text(
                          value.toInt().toString(),
                          style: theme.textTheme.bodySmall?.copyWith(
                            color: scheme.onSurfaceVariant,
                          ),
                        ),
                      );
                    },
                  ),
                ),
              ),
              lineTouchData: LineTouchData(
                touchTooltipData: LineTouchTooltipData(
                  getTooltipColor: (_) => scheme.inverseSurface,
                  getTooltipItems: (touched) => [
                    for (final spot in touched)
                      _tooltipItem(context, visible, spot),
                  ],
                ),
              ),
              lineBarsData: [
                for (final s in visible)
                  LineChartBarData(
                    spots: [
                      for (final p in s.points)
                        FlSpot(p.endedAt.toDouble(), p.rating.toDouble()),
                    ],
                    isCurved: true,
                    curveSmoothness: 0.2,
                    preventCurveOverShooting: true,
                    color: s.color,
                    barWidth: 2.5,
                    dotData: FlDotData(
                      show: true,
                      getDotPainter: (spot, percent, bar, index) =>
                          FlDotCirclePainter(
                            radius: 2.8,
                            color: s.color,
                            strokeWidth: 0,
                          ),
                    ),
                    // A single visible line gets a faint fill; multiple lines
                    // stay clean with no overlapping areas.
                    belowBarData: BarAreaData(
                      show: visible.length == 1,
                      color: s.color.withValues(alpha: 0.10),
                    ),
                  ),
              ],
            ),
          ),
        ),
        const SizedBox(height: 8),
        Text(
          '${_formatPointDate(minX)}  —  ${_formatPointDate(maxX)}',
          style: theme.textTheme.bodySmall?.copyWith(
            color: scheme.onSurfaceVariant,
          ),
        ),
      ],
    );
  }

  LineTooltipItem _tooltipItem(
    BuildContext context,
    List<_RatingSeries> visible,
    LineBarSpot spot,
  ) {
    final scheme = Theme.of(context).colorScheme;
    final series = visible[spot.barIndex];
    final point = series.points[spot.spotIndex];
    return LineTooltipItem(
      '${_timeControlLabel(context, series.timeControl)} · ${point.rating}\n'
      '${_formatPointDate(point.endedAt)}',
      TextStyle(
        color: scheme.onInverseSurface,
        fontWeight: FontWeight.w600,
        fontSize: 12,
      ),
    );
  }
}

class _RatingLegendChip extends StatelessWidget {
  const _RatingLegendChip({
    required this.label,
    required this.rating,
    required this.color,
    required this.active,
    required this.onTap,
  });

  final String label;
  final int rating;
  final Color color;
  final bool active;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return InkWell(
      onTap: onTap,
      borderRadius: BorderRadius.circular(8),
      child: Opacity(
        opacity: active ? 1 : 0.4,
        child: Padding(
          padding: const EdgeInsets.symmetric(horizontal: 2, vertical: 2),
          child: Row(
            mainAxisSize: MainAxisSize.min,
            children: [
              Container(
                width: 10,
                height: 10,
                decoration: BoxDecoration(color: color, shape: BoxShape.circle),
              ),
              const SizedBox(width: 6),
              Text(
                '$label: $rating',
                style: theme.textTheme.bodySmall?.copyWith(
                  fontWeight: FontWeight.w600,
                  color: theme.colorScheme.onSurface,
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }
}

String _formatPointDate(int endedAtSeconds) {
  final date = DateTime.fromMillisecondsSinceEpoch(
    endedAtSeconds * 1000,
    isUtc: true,
  ).toLocal();
  String two(int value) => value.toString().padLeft(2, '0');
  return '${date.year}-${two(date.month)}-${two(date.day)}';
}

class _RatingText {
  const _RatingText({
    required this.title,
    required this.empty,
    required this.error,
    required this.retry,
  });

  final String title;
  final String empty;
  final String error;
  final String retry;
}

_RatingText _ratingText(BuildContext context) {
  final strings = AppLocalizations.of(context);
  return _RatingText(
    title: strings.statsRatingTitle,
    empty: strings.statsRatingEmpty,
    error: strings.statsRatingError,
    retry: strings.statsRatingRetry,
  );
}
