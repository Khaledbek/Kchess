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
/// [parent] 0 lists the opening families; any other node lists its variations.
/// Progress is re-read whenever a drill or a variation list is closed, so the
/// depth meters always show what native stored.
class OpeningScenarioList extends StatefulWidget {
  const OpeningScenarioList({
    required this.gateway,
    required this.parent,
    required this.color,
    this.header,
    this.emphasis,
    super.key,
  });

  final CoreGateway gateway;
  final int parent;

  /// The side the user drills: `white` or `black`.
  final String color;

  /// Shown above the cards, e.g. the colour choice.
  final Widget? header;

  /// A scenario to call out — the user's statistical nemesis — by name, with
  /// the line the callout shows.
  final ({String name, String label})? emphasis;

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
      final result = await widget.gateway.practiceCommand({
        'op': 'nodes',
        'parent': widget.parent,
      });
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

    final emphasis = widget.emphasis;
    final emphasised = emphasis == null
        ? null
        : nodes
              .where((node) => node.name.toLowerCase() == emphasis.name.toLowerCase())
              .firstOrNull;

    return ListView(
      key: const Key('opening-scenarios'),
      padding: const EdgeInsets.fromLTRB(16, 4, 16, 32),
      children: [
        ?widget.header,
        if (emphasised != null && emphasis != null) ...[
          _NemesisCallout(label: emphasis.label, onDrill: () => _drill(emphasised)),
          const SizedBox(height: 14),
        ],
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
              emphasised: identical(node, emphasised),
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
    this.emphasised = false,
    super.key,
  });

  final OpeningTreeNode node;
  final VoidCallback onTap;

  /// Opens the scenarios below this one; null when it has none.
  final VoidCallback? onVariations;
  final bool emphasised;

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
          color: emphasised
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

/// The opening the user's games say they struggle with, one tap from a drill.
class _NemesisCallout extends StatelessWidget {
  const _NemesisCallout({required this.label, required this.onDrill});

  final String label;
  final VoidCallback onDrill;

  @override
  Widget build(BuildContext context) => Container(
    key: const Key('opening-scenarios-nemesis'),
    padding: const EdgeInsets.fromLTRB(14, 10, 8, 10),
    decoration: BoxDecoration(
      color: OpeningStudio.danger.withValues(alpha: 0.12),
      borderRadius: BorderRadius.circular(16),
      border: Border.all(color: OpeningStudio.danger.withValues(alpha: 0.5)),
    ),
    child: Row(
      children: [
        const Icon(Icons.warning_amber_rounded, color: OpeningStudio.danger),
        const SizedBox(width: 10),
        Expanded(
          child: Text(
            label,
            style: const TextStyle(
              fontSize: 13.5,
              fontWeight: FontWeight.w600,
              color: OpeningStudio.textPrimary,
            ),
          ),
        ),
        IconButton(
          onPressed: onDrill,
          color: OpeningStudio.danger,
          icon: const Icon(Icons.play_arrow_rounded),
        ),
      ],
    ),
  );
}
