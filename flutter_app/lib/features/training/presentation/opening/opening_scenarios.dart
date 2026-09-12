// -----------------------------------------------------------------------------
// Section: Opening scenarios — the dashboard every opening drill starts from
// -----------------------------------------------------------------------------

import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import '../../../../ffi/core_gateway.dart';
import '../../../../localization/generated/app_localizations.dart';
import '../../../../shared/widgets/chess_board_view.dart';
import '../../models/practice_models.dart';
import 'opening_family_hub.dart';
import 'opening_trainer_panels.dart';
import 'opening_trainer_screen.dart';

/// Studio-themed page for a list of opening scenarios.
class OpeningScenarioPage extends StatelessWidget {
  const OpeningScenarioPage({
    required this.title,
    required this.body,
    this.actions = const [],
    super.key,
  });

  final String title;
  final Widget body;
  final List<Widget> actions;

  @override
  Widget build(BuildContext context) => Theme(
    data: openingStudioTheme(),
    child: OpeningStudioBackdrop(
      child: Scaffold(
        backgroundColor: Colors.transparent,
        appBar: AppBar(
          systemOverlayStyle: SystemUiOverlayStyle.light,
          backgroundColor: Colors.transparent,
          surfaceTintColor: Colors.transparent,
          foregroundColor: OpeningStudio.textPrimary,
          centerTitle: true,
          title: Text(
            title.toUpperCase(),
            overflow: TextOverflow.ellipsis,
            style: const TextStyle(
              fontSize: 16,
              fontWeight: FontWeight.w800,
              letterSpacing: 1.2,
            ),
          ),
          actions: actions,
        ),
        body: SafeArea(top: false, child: body),
      ),
    ),
  );
}

/// The scenarios under one catalogue node, each a card that starts its drill.
///
/// A null [parent] lists every named opening family; a node id lists the
/// variations below it.
/// Progress is re-read whenever a drill or a variation list is closed, so the
/// depth meters always show what native stored.
class OpeningScenarioList extends StatefulWidget {
  const OpeningScenarioList({
    required this.gateway,
    required this.parent,
    required this.color,
    this.header,
    this.weakFamilies = const {},
    super.key,
  });

  final CoreGateway gateway;
  final int? parent;

  /// The side the user drills: `white` or `black`.
  final String color;

  /// Shown above the cards, e.g. the colour choice.
  final Widget? header;

  /// Lower-cased names of the openings the statistics flagged for this colour;
  /// their cards carry a weak-spot badge.
  final Set<String> weakFamilies;

  @override
  State<OpeningScenarioList> createState() => _OpeningScenarioListState();
}

class _OpeningScenarioListState extends State<OpeningScenarioList> {
  List<OpeningTreeNode>? _nodes;
  bool _error = false;
  bool _busy = false;

  @override
  void initState() {
    super.initState();
    unawaited(_load());
  }

  Future<void> _load() async {
    try {
      final parent = widget.parent;
      final result = await widget.gateway.practiceCommand(
        parent == null ? {'op': 'families'} : {'op': 'nodes', 'parent': parent},
      );
      final nodes = (result! as List<Object?>)
          .cast<Map<String, Object?>>()
          .map(OpeningTreeNode.fromJson)
          .toList();
      if (mounted) {
        setState(() {
          _nodes = nodes;
          _error = false;
        });
      }
    } catch (_) {
      if (mounted) setState(() => _error = true);
    }
  }

  Future<void> _drill(OpeningTreeNode node) async {
    if (_busy) return;
    setState(() => _busy = true);
    await Navigator.of(context).push(
      MaterialPageRoute<void>(
        builder: (_) => OpeningTrainerScreen(
          gateway: widget.gateway,
          title: node.name,
          request: {'kind': 'opening', 'id': node.openingId, 'color': widget.color},
        ),
      ),
    );
    if (!mounted) return;
    setState(() => _busy = false);
    unawaited(_load());
  }

  Future<void> _variations(OpeningTreeNode node) async {
    await Navigator.of(context).push(
      MaterialPageRoute<void>(
        builder: (_) => OpeningFamilyHubScreen(
          gateway: widget.gateway,
          rootNode: node,
          color: widget.color,
        ),
      ),
    );
    if (mounted) unawaited(_load());
  }

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    if (_error) {
      return Center(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Text(
              strings.trainingOpeningTreeLoadFailed,
              style: const TextStyle(color: OpeningStudio.textPrimary),
            ),
            const SizedBox(height: 16),
            FilledButton(
              onPressed: () {
                setState(() => _error = false);
                unawaited(_load());
              },
              child: Text(strings.trainingRestart),
            ),
          ],
        ),
      );
    }
    final nodes = _nodes;
    if (nodes == null) return const Center(child: CircularProgressIndicator());

    return ListView(
      key: const Key('opening-scenarios'),
      padding: const EdgeInsets.fromLTRB(16, 4, 16, 32),
      children: [
        ?widget.header,
        if (nodes.isEmpty)
          Padding(
            padding: const EdgeInsets.symmetric(vertical: 48),
            child: Text(
              strings.trainingOpeningTreeEmpty,
              textAlign: TextAlign.center,
              style: const TextStyle(color: OpeningStudio.textMuted),
            ),
          ),
        for (final node in nodes)
          Padding(
            padding: const EdgeInsets.only(bottom: 14),
            child: OpeningScenarioCard(
              node: node,
              weakSpot: widget.weakFamilies.contains(node.name.toLowerCase()),
              onTap: () => _drill(node),
              onVariations: node.childCount > 0 ? () => _variations(node) : null,
            ),
          ),
      ],
    );
  }
}

/// One opening scenario: its position, its setup line and how deep the user
/// has drilled it. Deliberately nothing else — no variation graph, no stats.
class OpeningScenarioCard extends StatelessWidget {
  const OpeningScenarioCard({
    required this.node,
    required this.onTap,
    this.onVariations,
    this.weakSpot = false,
    super.key,
  });

  final OpeningTreeNode node;
  final VoidCallback onTap;

  /// Opens the scenarios below this one; null when it has none.
  final VoidCallback? onVariations;

  /// The statistics say the user keeps losing or misplaying this opening.
  final bool weakSpot;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final target = node.targetDepth < 1 ? 1 : node.targetDepth;
    final depth = node.progress.bestDepth.clamp(0, target);
    final description = [
      if (node.eco != null && node.eco!.isNotEmpty) node.eco!,
      if (node.moves.isNotEmpty) formatOpeningLine(node.moves),
    ].join('  ·  ');

    return Material(
      key: ValueKey('opening-scenario-${node.id}'),
      color: OpeningStudio.sheet,
      shape: RoundedRectangleBorder(
        borderRadius: BorderRadius.circular(20),
        side: BorderSide(
          color: weakSpot
              ? OpeningStudio.danger.withValues(alpha: 0.7)
              : OpeningStudio.hairline,
        ),
      ),
      clipBehavior: Clip.antiAlias,
      child: InkWell(
        onTap: onTap,
        child: Padding(
          padding: const EdgeInsets.all(14),
          child: Row(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              SizedBox.square(
                dimension: 104,
                child: ClipRRect(
                  borderRadius: BorderRadius.circular(10),
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
              ),
              const SizedBox(width: 14),
              Expanded(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(
                      node.name,
                      maxLines: 2,
                      overflow: TextOverflow.ellipsis,
                      style: const TextStyle(
                        fontSize: 18,
                        height: 1.2,
                        fontWeight: FontWeight.w800,
                        color: OpeningStudio.textPrimary,
                      ),
                    ),
                    if (weakSpot) ...[
                      const SizedBox(height: 4),
                      Container(
                        key: const Key('opening-scenario-weak-spot'),
                        padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 2),
                        decoration: BoxDecoration(
                          color: OpeningStudio.danger.withValues(alpha: 0.16),
                          borderRadius: BorderRadius.circular(8),
                        ),
                        child: Row(
                          mainAxisSize: MainAxisSize.min,
                          children: [
                            const Icon(
                              Icons.warning_amber_rounded,
                              size: 13,
                              color: OpeningStudio.danger,
                            ),
                            const SizedBox(width: 4),
                            Text(
                              strings.trainingWeakSpotBadge,
                              style: const TextStyle(
                                fontSize: 11.5,
                                fontWeight: FontWeight.w700,
                                color: OpeningStudio.danger,
                              ),
                            ),
                          ],
                        ),
                      ),
                    ],
                    if (description.isNotEmpty) ...[
                      const SizedBox(height: 4),
                      Text(
                        description,
                        maxLines: 2,
                        overflow: TextOverflow.ellipsis,
                        textDirection: TextDirection.ltr,
                        style: const TextStyle(
                          fontSize: 13,
                          height: 1.3,
                          color: OpeningStudio.textMuted,
                        ),
                      ),
                    ],
                    const SizedBox(height: 10),
                    Container(
                      key: const Key('opening-scenario-depth'),
                      padding: const EdgeInsets.fromLTRB(10, 8, 10, 10),
                      decoration: BoxDecoration(
                        color: OpeningStudio.raised,
                        borderRadius: BorderRadius.circular(12),
                        border: Border.all(color: OpeningStudio.hairline),
                      ),
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          Row(
                            children: [
                              Expanded(
                                child: Text(
                                  strings
                                      .trainingOpeningScenarioDepth(depth, target)
                                      .toUpperCase(),
                                  maxLines: 1,
                                  overflow: TextOverflow.ellipsis,
                                  style: const TextStyle(
                                    fontSize: 11,
                                    fontWeight: FontWeight.w700,
                                    letterSpacing: 0.6,
                                    color: OpeningStudio.textPrimary,
                                  ),
                                ),
                              ),
                              if (node.progress.isMastered)
                                const Icon(
                                  Icons.verified_rounded,
                                  size: 15,
                                  color: OpeningStudio.mastery,
                                ),
                            ],
                          ),
                          const SizedBox(height: 7),
                          OpeningSegmentedMeter(
                            done: depth,
                            total: target,
                            color: OpeningStudio.mastery,
                            height: 6,
                          ),
                        ],
                      ),
                    ),
                    if (onVariations != null)
                      Align(
                        alignment: AlignmentDirectional.centerEnd,
                        child: InkWell(
                          key: ValueKey('opening-scenario-variations-${node.id}'),
                          onTap: onVariations,
                          borderRadius: BorderRadius.circular(8),
                          child: Padding(
                            padding: const EdgeInsetsDirectional.fromSTEB(6, 10, 0, 2),
                            child: Row(
                              mainAxisSize: MainAxisSize.min,
                              children: [
                                Text(
                                  strings.trainingOpeningTreeVariations(node.childCount),
                                  style: const TextStyle(
                                    fontSize: 13,
                                    fontWeight: FontWeight.w700,
                                    color: OpeningStudio.accent,
                                  ),
                                ),
                                const Icon(
                                  Icons.chevron_right_rounded,
                                  size: 18,
                                  color: OpeningStudio.accent,
                                ),
                              ],
                            ),
                          ),
                        ),
                      ),
                  ],
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }
}
