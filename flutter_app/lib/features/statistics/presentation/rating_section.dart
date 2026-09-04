part of '../../../ui/app_root.dart';

/// A single (timestamp, rating) sample derived from a game — pure per-row
/// display selection, no aggregation.
class _RatingPoint {
  const _RatingPoint({required this.endedAt, required this.rating});
  final int endedAt;
  final int rating;
}

/// One rating line for a time-control category (Blitz, Bullet, Rapid, Daily …),
/// sorted oldest → newest with bad points already filtered out.
class _RatingSeries {
  const _RatingSeries({
    required this.timeControl,
    required this.color,
    required this.points,
  });

  final String timeControl;
  final Color color;
  final List<_RatingPoint> points;

  int get current => points.last.rating;
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

  final Future<List<GameSummary>> future;
  final String timeControl;
  final VoidCallback onRetry;

  /// Fraction of a series' median rating below which a point is treated as bad
  /// data (chess.com occasionally records unrated/variant games at a wildly
  /// different scale). Applied per series so each scale is judged on its own.
  static const _outlierFloorFactor = 0.6;

  List<_RatingPoint> _hygiene(List<_RatingPoint> raw) {
    List<_RatingPoint> points = raw;
    if (raw.length >= 3) {
      final ratings = raw.map((p) => p.rating).toList()..sort();
      final median = ratings[ratings.length ~/ 2];
      final floor = (median * _outlierFloorFactor).round();
      final cleaned = raw.where((p) => p.rating >= floor).toList();
      if (cleaned.length >= 2) points = cleaned;
    }
    points.sort((a, b) => a.endedAt.compareTo(b.endedAt));
    return points;
  }

  List<_RatingSeries> _buildSeries(BuildContext context, List<GameSummary> games) {
    final byTimeControl = <String, List<_RatingPoint>>{};
    for (final game in games) {
      final rating = _statProfileRating(game);
      if (rating == null || rating <= 0 || game.endedAt <= 0) continue;
      (byTimeControl[game.timeControlType] ??= []).add(
        _RatingPoint(endedAt: game.endedAt, rating: rating),
      );
    }

    // Stable order: the four named controls first, then any extras.
    const order = [
      'bullet',
      'blitz',
      'rapid',
      'daily',
      'classical',
      'correspondence',
    ];
    final keys = <String>[
      ...order.where(byTimeControl.containsKey),
      ...byTimeControl.keys.where((k) => !order.contains(k)),
    ];

    final scheme = Theme.of(context).colorScheme;
    final series = <_RatingSeries>[];
    for (final tc in keys) {
      final points = _hygiene(byTimeControl[tc]!);
      if (points.isEmpty) continue;
      series.add(
        _RatingSeries(
          timeControl: tc,
          color: _kRatingSeriesColors[tc] ?? scheme.onSurfaceVariant,
          points: points,
        ),
      );
    }
    return series;
  }

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
            FutureBuilder<List<GameSummary>>(
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
                label: _timeControlLabel(s.timeControl),
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
      '${_timeControlLabel(series.timeControl)} · ${point.rating}\n'
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
  switch (Localizations.localeOf(context).languageCode) {
    case 'ar':
      return const _RatingText(
        title: 'تطوّر التصنيف',
        empty: 'لا توجد بيانات تصنيف كافية لرسم منحنى.',
        error: 'تعذّر تحميل بيانات التصنيف.',
        retry: 'إعادة المحاولة',
      );
    case 'en':
      return const _RatingText(
        title: 'Rating trend',
        empty: 'Not enough rating data to draw a trend.',
        error: 'Could not load rating data.',
        retry: 'Retry',
      );
    default:
      return const _RatingText(
        title: 'Rating-Verlauf',
        empty: 'Nicht genügend Rating-Daten für einen Verlauf.',
        error: 'Rating-Daten konnten nicht geladen werden.',
        retry: 'Erneut versuchen',
      );
  }
}
