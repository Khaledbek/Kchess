import 'dart:async';
import 'package:flutter/material.dart';

import '../../../../ffi/core_gateway.dart';
import '../../../../shared/models/models.dart';
import '../../models/practice_models.dart';
import '../practice_player.dart';
import 'opening_cards.dart';

class OpeningFamilyHubScreen extends StatefulWidget {
  const OpeningFamilyHubScreen({
    required this.gateway,
    required this.rootNode,
    required this.color,
    this.familyStats,
    super.key,
  });

  final CoreGateway gateway;
  final OpeningTreeNode rootNode;
  final String color;
  final OpeningFamily? familyStats;

  @override
  State<OpeningFamilyHubScreen> createState() => _OpeningFamilyHubScreenState();
}

class _OpeningFamilyHubScreenState extends State<OpeningFamilyHubScreen> {
  List<OpeningTreeNode>? _children;
  bool _error = false;
  bool _busy = false;

  @override
  void initState() {
    super.initState();
    _loadChildren();
  }

  Future<void> _loadChildren() async {
    try {
      final result = await widget.gateway.practiceCommand({
        'op': 'nodes',
        'parent': widget.rootNode.id,
      });
      final nodes = (result! as List<Object?>)
          .cast<Map<String, Object?>>()
          .map(OpeningTreeNode.fromJson)
          .toList();
      
      // Sort by childCount descending to find the main lines (80/20 rule)
      nodes.sort((a, b) => b.childCount.compareTo(a.childCount));
      
      if (mounted) {
        setState(() => _children = nodes);
      }
    } catch (_) {
      if (mounted) {
        setState(() => _error = true);
      }
    }
  }

  Future<void> _open(int id, String name) async {
    if (_busy) return;
    setState(() => _busy = true);
    await Navigator.of(context).push(
      MaterialPageRoute<void>(
        builder: (_) => PracticePlayer(
          gateway: widget.gateway,
          title: name,
          request: {'kind': 'opening', 'id': id, 'color': widget.color},
        ),
      ),
    );
    if (!mounted) return;
    setState(() => _busy = false);
    // Reload children to update progress
    _loadChildren();
  }

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final colorScheme = theme.colorScheme;
    
    return Scaffold(
      appBar: AppBar(
        title: Text(widget.rootNode.name),
        actions: [
          Padding(
            padding: const EdgeInsets.only(right: 8),
            child: FilledButton.icon(
              icon: const Icon(Icons.fitness_center, size: 18),
              label: const Text('Trainieren'),
              onPressed: () => _open(widget.rootNode.openingId, widget.rootNode.name),
            ),
          ),
        ],
      ),
      body: _buildBody(theme, colorScheme),
    );
  }

  Widget _buildBody(ThemeData theme, ColorScheme colorScheme) {
    if (_error) {
      return Center(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            const Text('Laden fehlgeschlagen'),
            const SizedBox(height: 16),
            FilledButton(
              onPressed: () {
                setState(() {
                  _error = false;
                  _children = null;
                });
                _loadChildren();
              },
              child: const Text('Erneut versuchen'),
            ),
          ],
        ),
      );
    }

    final children = _children;
    if (children == null) {
      return const Center(child: CircularProgressIndicator());
    }

    if (children.isEmpty) {
      return const Center(child: Text('Keine Varianten gefunden.'));
    }

    // Split 80/20 (Top 4 variations are main lines, rest are sidelines)
    final mainLines = children.take(4).toList();
    final sideLines = children.skip(4).toList();

    return CustomScrollView(
      slivers: [
        if (widget.familyStats != null && widget.familyStats!.tally.games > 0)
          SliverToBoxAdapter(
            child: Padding(
              padding: const EdgeInsets.fromLTRB(16, 16, 16, 0),
              child: Card(
                elevation: 0,
                color: colorScheme.surfaceContainerHighest.withValues(alpha: 0.3),
                shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(16)),
                child: Padding(
                  padding: const EdgeInsets.all(20),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Row(
                        children: [
                          Icon(Icons.analytics_outlined, color: colorScheme.primary),
                          const SizedBox(width: 8),
                          Text(
                            'Deine Statistik',
                            style: theme.textTheme.titleMedium?.copyWith(fontWeight: FontWeight.bold),
                          ),
                        ],
                      ),
                      const SizedBox(height: 16),
                      _buildStatsBar(context, widget.familyStats!.tally),
                    ],
                  ),
                ),
              ),
            ),
          ),
          
        SliverToBoxAdapter(
          child: Padding(
            padding: const EdgeInsets.fromLTRB(16, 24, 16, 12),
            child: Text(
              'Hauptvarianten',
              style: theme.textTheme.titleLarge?.copyWith(fontWeight: FontWeight.bold),
            ),
          ),
        ),
        SliverPadding(
          padding: const EdgeInsets.symmetric(horizontal: 16),
          sliver: SliverList(
            delegate: SliverChildBuilderDelegate(
              (context, index) => Padding(
                padding: const EdgeInsets.only(bottom: 12),
                child: OpeningVariationCard(
                  node: mainLines[index],
                  isMainLine: true,
                  onTap: () => _open(mainLines[index].openingId, mainLines[index].name),
                ),
              ),
              childCount: mainLines.length,
            ),
          ),
        ),
        if (sideLines.isNotEmpty) ...[
          SliverToBoxAdapter(
            child: Padding(
              padding: const EdgeInsets.fromLTRB(16, 32, 16, 12),
              child: Text(
                'Nebenvarianten & Gambits',
                style: theme.textTheme.titleLarge?.copyWith(fontWeight: FontWeight.bold),
              ),
            ),
          ),
          SliverPadding(
            padding: const EdgeInsets.symmetric(horizontal: 16),
            sliver: SliverList(
              delegate: SliverChildBuilderDelegate(
                (context, index) => Card(
                  elevation: 0,
                  color: colorScheme.surfaceContainerHighest.withValues(alpha: 0.3),
                  margin: const EdgeInsets.only(bottom: 4),
                  shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
                  child: OpeningVariationCard(
                    node: sideLines[index],
                    isMainLine: false,
                    onTap: () => _open(sideLines[index].openingId, sideLines[index].name),
                  ),
                ),
                childCount: sideLines.length,
              ),
            ),
          ),
        ],
        const SliverToBoxAdapter(child: SizedBox(height: 48)),
      ],
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
            Text('${(winPct * 100).round()}% Win', style: theme.textTheme.labelMedium?.copyWith(color: Colors.green, fontWeight: FontWeight.bold)),
            Text('${(drawPct * 100).round()}% Draw', style: theme.textTheme.labelMedium?.copyWith(color: Colors.grey, fontWeight: FontWeight.bold)),
            Text('${(lossPct * 100).round()}% Loss', style: theme.textTheme.labelMedium?.copyWith(color: Colors.red, fontWeight: FontWeight.bold)),
          ],
        ),
        const SizedBox(height: 8),
        SizedBox(
          height: 10,
          child: ClipRRect(
            borderRadius: BorderRadius.circular(5),
            child: Row(
              children: [
                if (tally.wins > 0) Expanded(flex: tally.wins, child: Container(color: Colors.green)),
                if (tally.draws > 0) Expanded(flex: tally.draws, child: Container(color: Colors.grey)),
                if (tally.losses > 0) Expanded(flex: tally.losses, child: Container(color: Colors.red)),
              ],
            ),
          ),
        ),
        const SizedBox(height: 8),
        Text(
          ' Partien gespielt',
          textAlign: TextAlign.center,
          style: theme.textTheme.bodySmall?.copyWith(color: theme.colorScheme.onSurfaceVariant),
        ),
      ],
    );
  }
}
