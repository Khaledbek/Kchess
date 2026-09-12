import 'package:flutter/material.dart';

import '../../../../shared/models/models.dart';
import '../../../../shared/widgets/chess_board_view.dart';
import '../../models/practice_models.dart';

class OpeningRootCard extends StatelessWidget {
  const OpeningRootCard({
    required this.node,
    required this.onTap,
    this.familyStats,
    this.isNemesis = false,
    super.key,
  });

  final OpeningTreeNode node;
  final VoidCallback onTap;
  final bool isNemesis;
  final OpeningFamily? familyStats;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final colorScheme = theme.colorScheme;
    
    // Determine mastery visually
    final isMastered = node.progress.isMastered;
    final progressColor = isMastered ? Colors.green : colorScheme.primary;

    return Card(
      clipBehavior: Clip.antiAlias,
      elevation: isNemesis ? 4 : 2,
      shape: RoundedRectangleBorder(
        side: isNemesis 
            ? BorderSide(color: colorScheme.error, width: 2) 
            : BorderSide.none,
        borderRadius: BorderRadius.circular(16),
      ),
      child: InkWell(
        onTap: onTap,
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Expanded(
              child: Stack(
                children: [
                  Positioned.fill(
                    child: IgnorePointer(
                      child: ChessBoardView(
                        position: node.position,
                        onSquareTap: (_) {},
                        onPieceDrop: (_, _) {},
                        interactive: false,
                        showCoordinates: false,
                      ),
                    ),
                  ),
                  if (isNemesis)
                    Positioned(
                      top: 8,
                      right: 8,
                      child: Container(
                        padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 4),
                        decoration: BoxDecoration(
                          color: colorScheme.error,
                          borderRadius: BorderRadius.circular(8),
                        ),
                        child: Row(
                          mainAxisSize: MainAxisSize.min,
                          children: [
                            Icon(Icons.warning_amber_rounded, size: 14, color: colorScheme.onError),
                            const SizedBox(width: 4),
                            Text(
                              'Schwachstelle',
                              style: theme.textTheme.labelSmall?.copyWith(
                                color: colorScheme.onError,
                                fontWeight: FontWeight.bold,
                              ),
                            ),
                          ],
                        ),
                      ),
                    ),
                ],
              ),
            ),
            Container(
              padding: const EdgeInsets.all(12),
              color: colorScheme.surfaceContainerHighest.withValues(alpha: 0.5),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Row(
                    children: [
                      Expanded(
                        child: Text(
                          node.name,
                          style: theme.textTheme.titleMedium?.copyWith(
                            fontWeight: FontWeight.bold,
                          ),
                          maxLines: 1,
                          overflow: TextOverflow.ellipsis,
                        ),
                      ),
                      if (node.eco != null) ...[
                        const SizedBox(width: 4),
                        Container(
                          padding: const EdgeInsets.symmetric(horizontal: 6, vertical: 2),
                          decoration: BoxDecoration(
                            color: colorScheme.secondaryContainer,
                            borderRadius: BorderRadius.circular(4),
                          ),
                          child: Text(
                            node.eco!,
                            style: theme.textTheme.labelSmall?.copyWith(
                              color: colorScheme.onSecondaryContainer,
                              fontWeight: FontWeight.bold,
                            ),
                          ),
                        ),
                      ],
                    ],
                  ),
                  const SizedBox(height: 8),
                  
                  if (familyStats != null && familyStats!.tally.games > 0) ...[
                    _buildStatsBar(context, familyStats!.tally),
                    const SizedBox(height: 8),
                  ],

                  Row(
                    children: [
                      Icon(Icons.alt_route, size: 16, color: colorScheme.onSurfaceVariant),
                      const SizedBox(width: 4),
                      Text(
                        ' Variations',
                        style: theme.textTheme.bodySmall?.copyWith(
                          color: colorScheme.onSurfaceVariant,
                        ),
                      ),
                      const Spacer(),
                      if (isMastered)
                        const Icon(Icons.check_circle, size: 16, color: Colors.green)
                      else
                        Text(
                          '/',
                          style: theme.textTheme.bodySmall?.copyWith(
                            color: progressColor,
                            fontWeight: FontWeight.bold,
                          ),
                        ),
                    ],
                  ),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildStatsBar(BuildContext context, StatTally tally) {
    final theme = Theme.of(context);
    final total = tally.games;
    final winPct = tally.wins / total;
    final drawPct = tally.draws / total;
    final lossPct = tally.losses / total;

    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        Row(
          mainAxisAlignment: MainAxisAlignment.spaceBetween,
          children: [
            Text('${(winPct * 100).round()}% Win', style: theme.textTheme.labelSmall?.copyWith(color: Colors.green)),
            Text('${(drawPct * 100).round()}% Draw', style: theme.textTheme.labelSmall?.copyWith(color: Colors.grey)),
            Text('${(lossPct * 100).round()}% Loss', style: theme.textTheme.labelSmall?.copyWith(color: Colors.red)),
          ],
        ),
        const SizedBox(height: 4),
        SizedBox(
          height: 6,
          child: ClipRRect(
            borderRadius: BorderRadius.circular(3),
            child: Row(
              children: [
                if (tally.wins > 0) Expanded(flex: tally.wins, child: Container(color: Colors.green)),
                if (tally.draws > 0) Expanded(flex: tally.draws, child: Container(color: Colors.grey)),
                if (tally.losses > 0) Expanded(flex: tally.losses, child: Container(color: Colors.red)),
              ],
            ),
          ),
        ),
      ],
    );
  }
}

class OpeningVariationCard extends StatelessWidget {
  const OpeningVariationCard({
    required this.node,
    required this.onTap,
    this.isMainLine = false,
    super.key,
  });

  final OpeningTreeNode node;
  final VoidCallback onTap;
  final bool isMainLine;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final colorScheme = theme.colorScheme;
    final isMastered = node.progress.isMastered;

    if (!isMainLine) {
      // Sideline: List style
      return ListTile(
        onTap: onTap,
        contentPadding: const EdgeInsets.symmetric(horizontal: 16, vertical: 4),
        title: Text(node.name, style: const TextStyle(fontWeight: FontWeight.w500)),
        subtitle: Text(
          node.eco != null ? 'ECO  •  Lines' : ' Lines',
        ),
        trailing: isMastered 
            ? const Icon(Icons.check_circle, color: Colors.green)
            : Text(
                '/',
                style: TextStyle(color: colorScheme.primary, fontWeight: FontWeight.bold),
              ),
      );
    }

    // Main line: Card style
    return Card(
      clipBehavior: Clip.antiAlias,
      child: InkWell(
        onTap: onTap,
        child: Row(
          children: [
            SizedBox(
              width: 100,
              height: 100,
              child: IgnorePointer(
                child: ChessBoardView(
                  position: node.position,
                  onSquareTap: (_) {},
                  onPieceDrop: (_, _) {},
                  interactive: false,
                  showCoordinates: false,
                ),
              ),
            ),
            Expanded(
              child: Padding(
                padding: const EdgeInsets.all(12),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(
                      node.name,
                      style: theme.textTheme.titleMedium?.copyWith(fontWeight: FontWeight.bold),
                      maxLines: 2,
                      overflow: TextOverflow.ellipsis,
                    ),
                    const SizedBox(height: 8),
                    Row(
                      children: [
                        if (node.eco != null) ...[
                          Text(node.eco!, style: theme.textTheme.labelMedium),
                          const SizedBox(width: 8),
                        ],
                        const Icon(Icons.alt_route, size: 14),
                        const SizedBox(width: 4),
                        Text(''),
                        const Spacer(),
                        if (isMastered)
                          const Icon(Icons.check_circle, size: 16, color: Colors.green)
                        else
                          Text(
                            '/',
                            style: TextStyle(color: colorScheme.primary, fontWeight: FontWeight.bold),
                          ),
                      ],
                    ),
                  ],
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}
