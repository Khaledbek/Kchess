part of '../../../ui/app_root.dart';

/// A single (timestamp, rating) sample derived from a game — pure per-row
/// display selection, no aggregation.
class _RatingPoint {
  const _RatingPoint({required this.endedAt, required this.rating});
  final int endedAt;
  final int rating;
}

/// Rating-over-time line chart for the active profile, built from the ratings
/// the profile carried in each (filtered) game. Falls back to a hint when there
/// is not enough rating data (e.g. local PGN libraries without ratings).
class _RatingTrendCard extends StatelessWidget {
  const _RatingTrendCard({
    required this.future,
    required this.timeControl,
    required this.onRetry,
  });

  final Future<List<GameSummary>> future;
  final String timeControl;
  final VoidCallback onRetry;

  /// Fraction of the median rating below which a point is treated as bad data.
  /// Chess.com occasionally records unrated/variant games at a wildly different
  /// scale (e.g. a 70 or 178 among 400s); those plunges skew the axis and the
  /// "Tiefststand", so they are dropped before plotting.
  static const _outlierFloorFactor = 0.6;

  List<_RatingPoint> _points(List<GameSummary> games) {
    final raw = <_RatingPoint>[];
    for (final game in games) {
      final rating = _statProfileRating(game);
      // Skip unrated / zero / null ratings and games without a timestamp.
      if (rating == null || rating <= 0 || game.endedAt <= 0) continue;
      raw.add(_RatingPoint(endedAt: game.endedAt, rating: rating));
    }

    List<_RatingPoint> points = raw;
    if (raw.length >= 3) {
      // Robust gate around the median (resistant to a handful of bad points).
      final ratings = raw.map((p) => p.rating).toList()..sort();
      final median = ratings[ratings.length ~/ 2];
      final floor = (median * _outlierFloorFactor).round();
      final cleaned = raw.where((p) => p.rating >= floor).toList();
      // Only apply the filter if it still leaves a usable trend.
      if (cleaned.length >= 2) points = cleaned;
    }

    // queryGames returns newest-first; a trend reads oldest → newest.
    points.sort((a, b) => a.endedAt.compareTo(b.endedAt));
    return points;
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
            const SizedBox(height: 16),
            FutureBuilder<List<GameSummary>>(
              future: future,
              builder: (context, snapshot) {
                if (snapshot.connectionState != ConnectionState.done) {
                  return const SizedBox(
                    height: 200,
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
                final points = _points(snapshot.data!);
                if (points.length < 2) {
                  return _OverviewMessage(
                    icon: Icons.stacked_line_chart_outlined,
                    text: labels.empty,
                  );
                }
                return _RatingTrendChart(points: points, labels: labels);
              },
            ),
          ],
        ),
      ),
    );
  }
}

class _RatingTrendChart extends StatelessWidget {
  const _RatingTrendChart({required this.points, required this.labels});

  final List<_RatingPoint> points;
  final _RatingText labels;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;

    var minRating = points.first.rating;
    var maxRating = points.first.rating;
    for (final point in points) {
      minRating = point.rating < minRating ? point.rating : minRating;
      maxRating = point.rating > maxRating ? point.rating : maxRating;
    }
    final current = points.last.rating;
    // Pad the axis so the line never touches the frame; round to tidy tick
    // boundaries so left-axis labels read as round numbers.
    final span = (maxRating - minRating).clamp(20, 100000);
    final pad = (span * 0.15).ceil().clamp(10, 100);
    final minY = ((minRating - pad) / 10).floor() * 10.0;
    final maxY = ((maxRating + pad) / 10).ceil() * 10.0;
    final labelInterval = (((maxY - minY) / 4) / 10).ceil() * 10.0;

    final spots = <FlSpot>[
      for (var i = 0; i < points.length; i++)
        FlSpot(i.toDouble(), points[i].rating.toDouble()),
    ];

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Wrap(
          spacing: 20,
          runSpacing: 8,
          children: [
            _RatingStat(label: labels.current, value: '$current'),
            _RatingStat(
              label: labels.peak,
              value: '$maxRating',
              color: AppTheme.success,
            ),
            _RatingStat(
              label: labels.low,
              value: '$minRating',
              color: scheme.error,
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
              minX: 0,
              maxX: (points.length - 1).toDouble(),
              clipData: FlClipData.all(),
              gridData: FlGridData(
                show: true,
                drawVerticalLine: false,
                horizontalInterval: labelInterval,
                getDrawingHorizontalLine: (_) => FlLine(
                  color: scheme.outlineVariant,
                  strokeWidth: 1,
                ),
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
                      LineTooltipItem(
                        '${spot.y.toInt()}\n${_formatPointDate(points[spot.x.toInt()].endedAt)}',
                        theme.textTheme.bodySmall?.copyWith(
                              color: scheme.onInverseSurface,
                              fontWeight: FontWeight.w600,
                            ) ??
                            const TextStyle(),
                      ),
                  ],
                ),
              ),
              lineBarsData: [
                LineChartBarData(
                  spots: spots,
                  isCurved: true,
                  curveSmoothness: 0.2,
                  preventCurveOverShooting: true,
                  color: scheme.primary,
                  barWidth: 2.5,
                  dotData: FlDotData(show: points.length <= 30),
                  belowBarData: BarAreaData(
                    show: true,
                    color: scheme.primary.withValues(alpha: 0.12),
                  ),
                ),
              ],
            ),
          ),
        ),
        const SizedBox(height: 8),
        Text(
          '${_formatPointDate(points.first.endedAt)}  —  ${_formatPointDate(points.last.endedAt)}',
          style: theme.textTheme.bodySmall?.copyWith(
            color: scheme.onSurfaceVariant,
          ),
        ),
      ],
    );
  }
}

class _RatingStat extends StatelessWidget {
  const _RatingStat({required this.label, required this.value, this.color});

  final String label;
  final String value;
  final Color? color;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      mainAxisSize: MainAxisSize.min,
      children: [
        Text(
          value,
          style: theme.textTheme.titleLarge?.copyWith(
            fontWeight: FontWeight.w800,
            color: color,
          ),
        ),
        Text(
          label,
          style: theme.textTheme.bodySmall?.copyWith(
            color: theme.colorScheme.onSurfaceVariant,
          ),
        ),
      ],
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
    required this.current,
    required this.peak,
    required this.low,
    required this.empty,
    required this.error,
    required this.retry,
  });

  final String title;
  final String current;
  final String peak;
  final String low;
  final String empty;
  final String error;
  final String retry;
}

_RatingText _ratingText(BuildContext context) {
  switch (Localizations.localeOf(context).languageCode) {
    case 'ar':
      return const _RatingText(
        title: 'تطوّر التصنيف',
        current: 'الحالي',
        peak: 'الأعلى',
        low: 'الأدنى',
        empty: 'لا توجد بيانات تصنيف كافية لرسم منحنى.',
        error: 'تعذّر تحميل بيانات التصنيف.',
        retry: 'إعادة المحاولة',
      );
    case 'en':
      return const _RatingText(
        title: 'Rating trend',
        current: 'Current',
        peak: 'Peak',
        low: 'Low',
        empty: 'Not enough rating data to draw a trend.',
        error: 'Could not load rating data.',
        retry: 'Retry',
      );
    default:
      return const _RatingText(
        title: 'Rating-Verlauf',
        current: 'Aktuell',
        peak: 'Höchststand',
        low: 'Tiefststand',
        empty: 'Nicht genügend Rating-Daten für einen Verlauf.',
        error: 'Rating-Daten konnten nicht geladen werden.',
        retry: 'Erneut versuchen',
      );
  }
}
