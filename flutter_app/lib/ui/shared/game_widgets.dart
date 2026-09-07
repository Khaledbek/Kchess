part of '../app_root.dart';

class _GameCard extends StatelessWidget {
  const _GameCard({
    required this.game,
    required this.online,
    this.playerIdentity,
    required this.onOpen,
    required this.onToggleFavorite,
    this.onSaveToDownloads,
    this.onMoveFavorite,
    this.onDelete,
  });

  final GameSummary game;
  final bool online;
  final String? playerIdentity;
  final VoidCallback onOpen;
  final VoidCallback onToggleFavorite;
  final VoidCallback? onSaveToDownloads;
  final VoidCallback? onMoveFavorite;
  final VoidCallback? onDelete;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;
    final outcome = AppTheme.outcomeColor(context, game.providerOutcome);
    final accuracy = game.accuracy == null
        ? '—'
        : '${game.accuracy!.toStringAsFixed(1)}%';
    final meta = [
      if (game.event.isNotEmpty) game.event,
      if (game.date.isNotEmpty) game.date,
    ].join('  ·  ');
    final identity = playerIdentity?.trim().toLowerCase() ?? '';
    final isCurrentWhite =
        identity.isNotEmpty && game.whiteName.trim().toLowerCase() == identity;
    final isCurrentBlack =
        identity.isNotEmpty && game.blackName.trim().toLowerCase() == identity;

    return Card(
      child: InkWell(
        key: Key('game-${game.id}'),
        onTap: onOpen,
        child: DecoratedBox(
          decoration: BoxDecoration(
            border: Border(left: BorderSide(color: outcome, width: 4)),
          ),
          child: Padding(
            padding: const EdgeInsets.fromLTRB(14, 12, 6, 12),
            child: Row(
              children: [
                _TimeControlBadge(game: game),
                const SizedBox(width: 14),
                Expanded(
                  child: Column(
                    mainAxisSize: MainAxisSize.min,
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      _PlayerLine(
                        name: game.whiteName,
                        rating: game.whiteRating,
                        isWhitePiece: true,
                        isCurrentPlayer: isCurrentWhite,
                        emphasized: isCurrentWhite,
                      ),
                      const SizedBox(height: 5),
                      _PlayerLine(
                        name: game.blackName,
                        rating: game.blackRating,
                        isWhitePiece: false,
                        isCurrentPlayer: isCurrentBlack,
                        emphasized: isCurrentBlack,
                      ),
                      if (meta.isNotEmpty) ...[
                        const SizedBox(height: 7),
                        Text(
                          meta,
                          maxLines: 1,
                          overflow: TextOverflow.ellipsis,
                          style: theme.textTheme.bodySmall?.copyWith(
                            color: scheme.onSurfaceVariant,
                          ),
                        ),
                      ],
                    ],
                  ),
                ),
                const SizedBox(width: 8),
                Column(
                  mainAxisSize: MainAxisSize.min,
                  crossAxisAlignment: CrossAxisAlignment.end,
                  children: [
                    Container(
                      padding: const EdgeInsets.symmetric(
                        horizontal: 10,
                        vertical: 4,
                      ),
                      decoration: BoxDecoration(
                        color: outcome.withValues(alpha: 0.14),
                        borderRadius: BorderRadius.circular(8),
                      ),
                      child: Text(
                        game.result,
                        style: theme.textTheme.labelLarge?.copyWith(
                          color: outcome,
                          fontWeight: FontWeight.w700,
                        ),
                      ),
                    ),
                    const SizedBox(height: 6),
                    Row(
                      mainAxisSize: MainAxisSize.min,
                      children: [
                        Icon(
                          Icons.insights_rounded,
                          size: 14,
                          color: scheme.onSurfaceVariant,
                        ),
                        const SizedBox(width: 4),
                        Text(
                          accuracy,
                          style: theme.textTheme.labelMedium?.copyWith(
                            color: scheme.onSurface,
                          ),
                        ),
                      ],
                    ),
                  ],
                ),
                Column(
                  mainAxisSize: MainAxisSize.min,
                  children: [
                    IconButton(
                      key: Key('favorite-${game.id}'),
                      tooltip: 'Favorit',
                      visualDensity: VisualDensity.compact,
                      onPressed: onToggleFavorite,
                      icon: Icon(
                        game.favorite ? Icons.favorite : Icons.favorite_border,
                        color: game.favorite
                            ? scheme.error
                            : scheme.onSurfaceVariant,
                      ),
                    ),
                    if (onMoveFavorite != null)
                      IconButton(
                        key: Key('move-favorite-${game.id}'),
                        tooltip: AppLocalizations.of(context)
                            .favoriteMoveToCollection,
                        visualDensity: VisualDensity.compact,
                        onPressed: onMoveFavorite,
                        icon: Icon(
                          Icons.drive_file_move_outline,
                          color: scheme.onSurfaceVariant,
                        ),
                      ),
                    if (online &&
                        game.providerGameId != null &&
                        onSaveToDownloads != null)
                      IconButton(
                        key: Key('download-${game.id}'),
                        tooltip: 'In Downloads speichern',
                        visualDensity: VisualDensity.compact,
                        onPressed: onSaveToDownloads,
                        icon: Icon(
                          Icons.download,
                          color: scheme.onSurfaceVariant,
                        ),
                      )
                    else if (game.providerGameId == null && onDelete != null)
                      IconButton(
                        key: Key('delete-local-${game.id}'),
                        tooltip: 'Lokalen Eintrag löschen',
                        visualDensity: VisualDensity.compact,
                        onPressed: onDelete,
                        icon: Icon(
                          Icons.delete_outline,
                          color: scheme.onSurfaceVariant,
                        ),
                      ),
                  ],
                ),
              ],
            ),
          ),
        ),
      ),
    );
  }
}

/// Rounded, tinted badge that conveys a game's time control at a glance.
class _TimeControlBadge extends StatelessWidget {
  const _TimeControlBadge({required this.game});

  final GameSummary game;

  @override
  Widget build(BuildContext context) {
    final scheme = Theme.of(context).colorScheme;
    final (icon, color) = _style(scheme);
    return Container(
      width: 46,
      height: 46,
      alignment: Alignment.center,
      decoration: BoxDecoration(
        color: color.withValues(alpha: 0.14),
        borderRadius: BorderRadius.circular(13),
      ),
      child: Icon(icon, color: color, size: 22),
    );
  }

  (IconData, Color) _style(ColorScheme scheme) =>
      switch (game.timeControlType) {
        'bullet' => (Icons.bolt, const Color(0xFFE0663B)),
        'blitz' => (Icons.flash_on, AppTheme.warning),
        'rapid' => (Icons.timer_outlined, AppTheme.success),
        'classical' => (Icons.hourglass_bottom, scheme.primary),
        'daily' ||
        'correspondence' => (Icons.calendar_today, const Color(0xFF7C6FF0)),
        _ => (
          game.kind == 'fen'
              ? Icons.grid_on_outlined
              : Icons.description_outlined,
          scheme.onSurfaceVariant,
        ),
      };
}

/// One player row: a hollow (white) or solid (black) piece marker, the name,
/// and an optional rating pill.
class _PlayerLine extends StatelessWidget {
  const _PlayerLine({
    required this.name,
    required this.rating,
    required this.isWhitePiece,
    required this.isCurrentPlayer,
    required this.emphasized,
  });

  final String name;
  final int? rating;
  final bool isWhitePiece;
  final bool isCurrentPlayer;
  final bool emphasized;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;
    return Row(
      children: [
        SizedBox(
          width: 20,
          child: isCurrentPlayer
              ? Text(
                  isWhitePiece ? '♙' : '♟',
                  textAlign: TextAlign.center,
                  style: TextStyle(
                    fontSize: 18,
                    height: 1,
                    color: scheme.onSurface,
                  ),
                )
              : null,
        ),
        const SizedBox(width: 7),
        Flexible(
          child: Text(
            name,
            maxLines: 1,
            overflow: TextOverflow.ellipsis,
            style: emphasized
                ? theme.textTheme.titleSmall?.copyWith(
                    fontWeight: FontWeight.w700,
                  )
                : theme.textTheme.bodyMedium?.copyWith(
                    color: scheme.onSurfaceVariant,
                  ),
          ),
        ),
        if (rating != null) ...[
          const SizedBox(width: 7),
          Container(
            padding: const EdgeInsets.symmetric(horizontal: 6, vertical: 1),
            decoration: BoxDecoration(
              color: scheme.surfaceContainerHigh,
              borderRadius: BorderRadius.circular(6),
            ),
            child: Text(
              '$rating',
              style: theme.textTheme.bodySmall?.copyWith(
                color: scheme.onSurfaceVariant,
              ),
            ),
          ),
        ],
      ],
    );
  }
}

/// A small labelled tag used in the profile header (provider, handle, flair …).
class _ProfileTag extends StatelessWidget {
  const _ProfileTag({required this.icon, required this.label});

  final IconData icon;
  final String label;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 5),
      decoration: BoxDecoration(
        color: scheme.surfaceContainerHigh,
        borderRadius: BorderRadius.circular(9),
      ),
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          Icon(icon, size: 14, color: scheme.onSurfaceVariant),
          const SizedBox(width: 6),
          Text(
            label,
            style: theme.textTheme.bodySmall?.copyWith(color: scheme.onSurface),
          ),
        ],
      ),
    );
  }
}

/// Proportional win / draw / loss ribbon for a performance card.
class _WinLossBar extends StatelessWidget {
  const _WinLossBar({
    required this.wins,
    required this.draws,
    required this.losses,
  });

  final int wins;
  final int draws;
  final int losses;

  @override
  Widget build(BuildContext context) {
    final scheme = Theme.of(context).colorScheme;
    final segments = <Widget>[
      if (wins > 0)
        Expanded(
          flex: wins,
          child: const ColoredBox(color: AppTheme.success),
        ),
      if (draws > 0)
        Expanded(
          flex: draws,
          child: ColoredBox(color: scheme.onSurfaceVariant),
        ),
      if (losses > 0)
        Expanded(
          flex: losses,
          child: ColoredBox(color: scheme.error),
        ),
    ];
    return ClipRRect(
      borderRadius: BorderRadius.circular(6),
      child: SizedBox(
        height: 8,
        child: segments.isEmpty
            ? ColoredBox(color: scheme.surfaceContainerHighest)
            : Row(children: segments),
      ),
    );
  }
}

/// A titled group of settings rendered as a bordered card.
