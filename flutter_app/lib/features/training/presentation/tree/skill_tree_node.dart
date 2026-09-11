import 'package:flutter/material.dart';

import '../../../../localization/generated/app_localizations.dart';
import '../../../../shared/models/models.dart';
import '../../../../shared/theme/app_theme.dart';
import '../../../../shared/widgets/chess_board_view.dart';
import '../../models/models.dart';
import 'opening_tree_controller.dart';

/// How far the user has got with a line, as the card paints it.
///
/// Three earned states rather than a lock: the tree doubles as the app's
/// opening reference, and a player who already knows the Najdorf should not
/// have to grind the Sicilian first to look it up.
enum NodeProgressState {
  /// Never attempted.
  fresh,

  /// Attempted, not yet three clean runs in a row.
  learning,

  /// Mastered — the badge stays even after a later slip.
  mastered;

  static NodeProgressState of(ExerciseProgress progress) {
    if (progress.isMastered) return NodeProgressState.mastered;
    if (progress.attemptCount > 0) return NodeProgressState.learning;
    return NodeProgressState.fresh;
  }
}

/// One card on the skill tree: the position the line reaches, what it is
/// called, how far the user has got, and the two things they can do with it.
///
/// Sized to [TreeMetrics] rather than to its content, because the canvas places
/// every card before it builds any of them.
class SkillTreeNodeWidget extends StatelessWidget {
  const SkillTreeNodeWidget({
    required this.node,
    required this.progress,
    required this.onToggle,
    required this.onTrain,
    this.fen,
    super.key,
  });

  /// Side of the square board thumbnail.
  static const previewSide = 84.0;

  final TreeLayoutNode node;
  final String? fen;
  final ExerciseProgress progress;
  final VoidCallback onToggle;
  final VoidCallback onTrain;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;
    final state = NodeProgressState.of(progress);
    final accent = _accentFor(state, scheme);

    return SizedBox(
      width: TreeMetrics.nodeWidth,
      height: TreeMetrics.nodeHeight,
      child: Material(
        color: scheme.surfaceContainerHigh,
        clipBehavior: Clip.antiAlias,
        elevation: node.expanded ? 4 : 1,
        shadowColor: accent.withValues(alpha: 0.45),
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(16),
          side: BorderSide(
            color: state == NodeProgressState.fresh && !node.expanded
                ? scheme.outlineVariant
                : accent,
            width: node.expanded ? 2 : 1,
          ),
        ),
        child: Padding(
          padding: const EdgeInsets.all(10),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              SizedBox(
                height: previewSide,
                child: Row(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    _preview(context),
                    const SizedBox(width: 10),
                    Expanded(child: _summary(context, strings, theme, accent, state)),
                  ],
                ),
              ),
              const SizedBox(height: 8),
              _actions(context, strings),
            ],
          ),
        ),
      ),
    );
  }

  static Color _accentFor(NodeProgressState state, ColorScheme scheme) =>
      switch (state) {
        NodeProgressState.mastered => AppTheme.success,
        NodeProgressState.learning => AppTheme.brand,
        NodeProgressState.fresh => scheme.outline,
      };

  /// The position the line reaches, as a square thumbnail.
  ///
  /// Square and unscaled rather than stretched across the card, because a
  /// cropped or squashed board is worse than no board at all.
  Widget _preview(BuildContext context) {
    final scheme = Theme.of(context).colorScheme;
    final resolved = fen;

    return ClipRRect(
      borderRadius: BorderRadius.circular(8),
      child: SizedBox(
        width: previewSide,
        height: previewSide,
        child: resolved == null
            // Still replaying the line: a plate rather than a spinner, so a
            // screen full of cards does not strobe as they resolve.
            ? ColoredBox(color: scheme.surfaceContainerHighest)
            : IgnorePointer(
                child: ChessBoardView(
                  position: _previewPosition(resolved),
                  interactive: false,
                  showCoordinates: false,
                  onSquareTap: _ignoreSquare,
                  onPieceDrop: _ignoreDrop,
                ),
              ),
      ),
    );
  }

  Widget _summary(
    BuildContext context,
    AppLocalizations strings,
    ThemeData theme,
    Color accent,
    NodeProgressState state,
  ) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Row(
          children: [
            if ((node.data.eco ?? '').isNotEmpty) _ecoBadge(theme, node.data.eco!),
            const Spacer(),
            if (node.hasChildren) _branchCount(theme),
          ],
        ),
        const SizedBox(height: 4),
        Expanded(
          child: Text(
            node.data.name,
            style: theme.textTheme.titleSmall?.copyWith(
              fontWeight: FontWeight.w700,
              height: 1.15,
            ),
            maxLines: 2,
            overflow: TextOverflow.ellipsis,
          ),
        ),
        _masteryDots(theme, accent, state),
      ],
    );
  }

  Widget _ecoBadge(ThemeData theme, String eco) => DecoratedBox(
    decoration: BoxDecoration(
      color: theme.colorScheme.primaryContainer,
      borderRadius: BorderRadius.circular(6),
    ),
    child: Padding(
      padding: const EdgeInsets.symmetric(horizontal: 6, vertical: 2),
      child: Text(
        eco,
        style: theme.textTheme.labelSmall?.copyWith(
          color: theme.colorScheme.onPrimaryContainer,
          fontWeight: FontWeight.w800,
          letterSpacing: 0.4,
        ),
      ),
    ),
  );

  Widget _branchCount(ThemeData theme) => Row(
    mainAxisSize: MainAxisSize.min,
    children: [
      Icon(
        Icons.account_tree_outlined,
        size: 12,
        color: theme.colorScheme.onSurfaceVariant,
      ),
      const SizedBox(width: 2),
      Text(
        '${node.childCount}',
        style: theme.textTheme.labelSmall?.copyWith(
          color: theme.colorScheme.onSurfaceVariant,
          fontWeight: FontWeight.w700,
        ),
      ),
    ],
  );

  /// One dot per clean repeat mastery needs, filled as the streak grows.
  Widget _masteryDots(ThemeData theme, Color accent, NodeProgressState state) {
    final filled = state == NodeProgressState.mastered
        ? ExerciseProgress.masteryThreshold
        : progress.successStreak;

    return Row(
      children: [
        for (var i = 0; i < ExerciseProgress.masteryThreshold; i++)
          Padding(
            padding: EdgeInsets.only(right: 4, left: i == 0 ? 0 : 0),
            child: Container(
              width: 7,
              height: 7,
              decoration: BoxDecoration(
                shape: BoxShape.circle,
                color: i < filled ? accent : theme.colorScheme.outlineVariant,
              ),
            ),
          ),
        if (state == NodeProgressState.mastered)
          Icon(Icons.verified_rounded, size: 14, color: accent),
      ],
    );
  }

  /// Train on the left, unfold on the right — both always reachable.
  ///
  /// The train button used to be hidden on any card with variations, which put
  /// every major opening out of reach: a card is a line *and* a branch.
  Widget _actions(BuildContext context, AppLocalizations strings) {
    return SizedBox(
      height: 36,
      child: Row(
        children: [
          Expanded(
            child: FilledButton(
              key: Key('opening-tree-train-${node.data.id}'),
              onPressed: onTrain,
              style: FilledButton.styleFrom(
                padding: const EdgeInsets.symmetric(horizontal: 8),
                visualDensity: VisualDensity.compact,
                textStyle: Theme.of(context).textTheme.labelLarge,
              ),
              child: Text(
                strings.trainingOpeningLabStart,
                maxLines: 1,
                overflow: TextOverflow.ellipsis,
              ),
            ),
          ),
          if (node.hasChildren) ...[
            const SizedBox(width: 8),
            _expandButton(context, strings),
          ],
        ],
      ),
    );
  }

  Widget _expandButton(BuildContext context, AppLocalizations strings) {
    if (node.isLoadingChildren) {
      return const SizedBox(
        width: 36,
        height: 36,
        child: Center(
          child: SizedBox(
            width: 16,
            height: 16,
            child: CircularProgressIndicator(strokeWidth: 2),
          ),
        ),
      );
    }

    final scheme = Theme.of(context).colorScheme;
    final label = node.expanded
        ? strings.trainingOpeningTreeCollapse
        : strings.trainingOpeningTreeExpand;

    // A 36pt square: the smallest comfortable touch target, and the reason the
    // fold control is a button now rather than a strip of tappable text.
    return SizedBox(
      width: 36,
      height: 36,
      child: IconButton.filledTonal(
        key: Key('opening-tree-expand-${node.data.id}'),
        onPressed: onToggle,
        padding: EdgeInsets.zero,
        iconSize: 20,
        visualDensity: VisualDensity.compact,
        tooltip: '$label · ${strings.trainingOpeningTreeVariations(node.childCount)}',
        style: IconButton.styleFrom(
          backgroundColor: node.expanded
              ? scheme.secondaryContainer
              : scheme.surfaceContainerHighest,
        ),
        icon: Icon(node.expanded ? Icons.unfold_less_rounded : Icons.hub_outlined),
      ),
    );
  }

  static void _ignoreSquare(String square) {}

  static void _ignoreDrop(String source, String target) {}

  /// The board half of a FEN, as the shared board view wants it.
  ///
  /// Only the placement field matters for a static preview, so nothing here
  /// tries to recover castling rights or the clocks.
  static BoardPosition _previewPosition(String fen) {
    final fields = fen.split(' ');
    final placement = fields.isEmpty ? '' : fields.first;
    final pieces = List.filled(64, '');

    var rank = 7;
    var file = 0;
    for (final char in placement.split('')) {
      if (char == '/') {
        rank--;
        file = 0;
        continue;
      }
      final empty = int.tryParse(char);
      if (empty != null) {
        file += empty;
        continue;
      }
      if (rank >= 0 && rank <= 7 && file >= 0 && file <= 7) {
        pieces[rank * 8 + file] = char;
      }
      file++;
    }

    return BoardPosition(
      fen: fen,
      pieces: pieces,
      sideToMove: fields.length > 1 && fields[1] == 'b' ? 'black' : 'white',
      draggableColor: 'white',
      fullmoveNumber: 1,
      inCheck: false,
      legalMoveCount: null,
      status: BoardStatus.playable,
      insufficientMaterial: false,
    );
  }
}
