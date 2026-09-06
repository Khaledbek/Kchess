part of '../../../ui/app_root.dart';

/// "Aktuelle Form": a strip of tappable result chips for the most recent games,
/// each with a tooltip (opponent, rating, result, time control, date) and a tap
/// target that opens the game in the analysis workflow.
class _RecentFormCard extends StatelessWidget {
  const _RecentFormCard({
    required this.controller,
    required this.future,
    required this.timeControl,
    required this.onRetry,
  });

  final AppController controller;
  final Future<List<GameSummary>> future;
  final String timeControl;
  final VoidCallback onRetry;

  static const _maxChips = 15;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final labels = _formText(context);
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(18),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                Icon(Icons.timeline_outlined, color: theme.colorScheme.primary),
                const SizedBox(width: 8),
                Flexible(
                  child: Text(
                    labels.title,
                    maxLines: 1,
                    overflow: TextOverflow.ellipsis,
                    style: theme.textTheme.titleMedium?.copyWith(
                      fontWeight: FontWeight.w800,
                    ),
                  ),
                ),
                // Active win/loss streak, read from the same recent games as the
                // strip below (newest first). Hidden until the data resolves.
                FutureBuilder<List<GameSummary>>(
                  future: future,
                  builder: (context, snapshot) {
                    if (!snapshot.hasData) return const SizedBox.shrink();
                    final streak = _currentStreak(snapshot.data!);
                    if (streak == null) return const SizedBox.shrink();
                    return Padding(
                      padding: const EdgeInsets.only(left: 8),
                      child: _StreakBadge(
                        count: streak.length,
                        outcome: streak.outcome,
                      ),
                    );
                  },
                ),
                if (timeControl != 'all') ...[
                  const Spacer(),
                  _FilterPill(
                    label: _statsLabels(context).timeControl(timeControl),
                  ),
                ],
              ],
            ),
            const SizedBox(height: 16),
            FutureBuilder<List<GameSummary>>(
              future: future,
              builder: (context, snapshot) {
                if (snapshot.connectionState != ConnectionState.done) {
                  return const SizedBox(
                    height: 90,
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
                final games = snapshot.data!
                    .where((game) => _statGameOutcome(game) != 'unknown')
                    .take(_maxChips)
                    .toList(growable: false);
                if (games.isEmpty) {
                  return _OverviewMessage(
                    icon: Icons.sports_esports_outlined,
                    text: labels.empty,
                  );
                }
                return Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Wrap(
                      spacing: 8,
                      runSpacing: 8,
                      children: [
                        for (final game in games)
                          Tooltip(
                            richMessage: _formTooltip(context, game, labels),
                            child: _StatResultChip(
                              outcome: _statGameOutcome(game),
                              onTap: () => _openAnalysisAndRefreshSettings(
                                context: context,
                                controller: controller,
                                game: game,
                              ),
                            ),
                          ),
                      ],
                    ),
                    const SizedBox(height: 12),
                    Text(
                      labels.hint,
                      style: theme.textTheme.bodySmall?.copyWith(
                        color: theme.colorScheme.onSurfaceVariant,
                      ),
                    ),
                  ],
                );
              },
            ),
          ],
        ),
      ),
    );
  }

  TextSpan _formTooltip(
    BuildContext context,
    GameSummary game,
    _FormText labels,
  ) {
    final theme = Theme.of(context);
    final onInverse = theme.colorScheme.onInverseSurface;
    final opponent = _statOpponentName(game);
    final opponentRating = _statOpponentRating(game);
    final meta = <String>[
      if (game.timeControlType != 'unknown')
        _timeControlLabel(game.timeControlType),
      if (game.date.isNotEmpty) game.date,
    ].join('  ·  ');
    return TextSpan(
      style: theme.textTheme.bodySmall?.copyWith(color: onInverse),
      children: [
        TextSpan(
          text: '${labels.versus} $opponent',
          style: const TextStyle(fontWeight: FontWeight.w700),
        ),
        if (opponentRating != null) TextSpan(text: '  ($opponentRating)'),
        TextSpan(text: '\n${game.result}'),
        if (meta.isNotEmpty)
          TextSpan(
            text: '\n$meta',
            style: TextStyle(color: onInverse.withValues(alpha: 0.8)),
          ),
      ],
    );
  }
}

/// The current run of identical results counting back from the most recent
/// game, as (outcome, length). Only win or loss runs of two or more are
/// reported; a leading draw, a lone result, or no games yields null. Games
/// whose outcome can't be attributed are skipped rather than breaking the run.
({String outcome, int length})? _currentStreak(List<GameSummary> games) {
  String? outcome;
  var length = 0;
  for (final game in games) {
    final result = _statGameOutcome(game);
    if (result == 'unknown') continue;
    if (outcome == null) {
      outcome = result;
      length = 1;
    } else if (result == outcome) {
      length++;
    } else {
      break;
    }
  }
  if (length < 2 || (outcome != 'win' && outcome != 'loss')) return null;
  return (outcome: outcome!, length: length);
}

/// Compact "🔥 3 Siege" / "⚠️ 2 Niederlagen" chip summarising the active streak:
/// green tint for a winning run, muted red tint for a losing one.
class _StreakBadge extends StatelessWidget {
  const _StreakBadge({required this.count, required this.outcome});

  final int count;
  final String outcome; // 'win' | 'loss'

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final labels = _statsLabels(context);
    final isWin = outcome == 'win';
    final color = isWin ? AppTheme.success : theme.colorScheme.error;
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 3),
      decoration: BoxDecoration(
        color: color.withValues(alpha: 0.14),
        borderRadius: BorderRadius.circular(999),
        border: Border.all(color: color.withValues(alpha: 0.40)),
      ),
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          Text(isWin ? '🔥' : '⚠️', style: const TextStyle(fontSize: 12)),
          const SizedBox(width: 4),
          Text(
            '$count ${isWin ? labels.wins : labels.losses}',
            style: theme.textTheme.labelSmall?.copyWith(
              color: color,
              fontWeight: FontWeight.w800,
            ),
          ),
        ],
      ),
    );
  }
}

class _FormText {
  const _FormText({
    required this.title,
    required this.hint,
    required this.versus,
    required this.empty,
    required this.error,
    required this.retry,
  });

  final String title;
  final String hint;
  final String versus;
  final String empty;
  final String error;
  final String retry;
}

_FormText _formText(BuildContext context) {
  switch (Localizations.localeOf(context).languageCode) {
    case 'ar':
      return const _FormText(
        title: 'الأداء الأخير',
        hint: 'اضغط على نتيجة لفتح المباراة.',
        versus: 'ضد',
        empty: 'لا توجد مباريات حديثة لعرضها.',
        error: 'تعذّر تحميل المباريات الأخيرة.',
        retry: 'إعادة المحاولة',
      );
    case 'en':
      return const _FormText(
        title: 'Recent form',
        hint: 'Tap a result to open the game.',
        versus: 'vs',
        empty: 'No recent games to show.',
        error: 'Could not load recent games.',
        retry: 'Retry',
      );
    default:
      return const _FormText(
        title: 'Aktuelle Form',
        hint: 'Tippe auf ein Ergebnis, um die Partie zu öffnen.',
        versus: 'gegen',
        empty: 'Keine aktuellen Partien vorhanden.',
        error: 'Aktuelle Partien konnten nicht geladen werden.',
        retry: 'Erneut versuchen',
      );
  }
}
