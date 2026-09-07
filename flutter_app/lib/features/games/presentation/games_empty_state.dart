part of '../../../ui/app_root.dart';

class _GamesEmptyState extends StatelessWidget {
  const _GamesEmptyState({
    required this.labels,
    required this.fallbackText,
    required this.hasActiveFilters,
    required this.month,
    required this.onResetFilters,
  });

  final _GameFilterLabels labels;
  final String fallbackText;
  final bool hasActiveFilters;
  final String? month;
  final VoidCallback onResetFilters;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;
    final monthLabel = month == null
        ? null
        : _formatGameMonthLabel(context, month!);
    final title = hasActiveFilters
        ? labels.noMatchingGames
        : monthLabel != null
        ? labels.noGamesForMonth.replaceAll('{month}', monthLabel)
        : fallbackText;
    final subtitle = hasActiveFilters ? labels.noMatchingGamesHelp : null;
    return Center(
      child: ConstrainedBox(
        constraints: const BoxConstraints(maxWidth: 420),
        child: Padding(
          padding: const EdgeInsets.all(32),
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              Container(
                width: 56,
                height: 56,
                alignment: Alignment.center,
                decoration: BoxDecoration(
                  color: scheme.surfaceContainerHigh,
                  borderRadius: BorderRadius.circular(18),
                ),
                child: Icon(
                  hasActiveFilters
                      ? Icons.filter_alt_off_outlined
                      : Icons.event_busy_outlined,
                  color: scheme.onSurfaceVariant,
                ),
              ),
              const SizedBox(height: 16),
              Text(
                title,
                textAlign: TextAlign.center,
                style: theme.textTheme.titleMedium?.copyWith(
                  fontWeight: FontWeight.w700,
                ),
              ),
              if (subtitle != null) ...[
                const SizedBox(height: 6),
                Text(
                  subtitle,
                  textAlign: TextAlign.center,
                  style: theme.textTheme.bodyMedium?.copyWith(
                    color: scheme.onSurfaceVariant,
                  ),
                ),
              ],
              if (hasActiveFilters) ...[
                const SizedBox(height: 18),
                OutlinedButton.icon(
                  onPressed: onResetFilters,
                  icon: const Icon(Icons.restart_alt),
                  label: Text(labels.resetFilters),
                ),
              ],
            ],
          ),
        ),
      ),
    );
  }
}

