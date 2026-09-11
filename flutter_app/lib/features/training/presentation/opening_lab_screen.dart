// -----------------------------------------------------------------------------
// Section: Opening selection and navigation; rules remain native
// -----------------------------------------------------------------------------
import 'dart:async';

import 'package:flutter/material.dart';

import '../../../ffi/core_gateway.dart';
import '../../../localization/generated/app_localizations.dart';
import '../models/opening_training_request.dart';
import '../models/practice_models.dart';
import 'practice_player.dart';
import 'tree/opening_tree_controller.dart';
import 'tree/skill_tree_canvas.dart';

class OpeningLabScreen extends StatefulWidget {
  const OpeningLabScreen({required this.gateway, this.request, super.key});
  final CoreGateway gateway;
  final OpeningTrainingRequest? request;
  @override
  State<OpeningLabScreen> createState() => _OpeningLabScreenState();
}

class _OpeningLabScreenState extends State<OpeningLabScreen> {
  late OpeningTreeController _tree;
  String _color = 'white';
  bool _busy = false;
  @override
  void initState() {
    super.initState();
    _tree = OpeningTreeController(gateway: widget.gateway);
    if (widget.request?.color == 'black') _color = 'black';
    WidgetsBinding.instance.addPostFrameCallback(
      (_) => unawaited(_openRequest()),
    );
  }

  @override
  void dispose() {
    _tree.dispose();
    super.dispose();
  }

  Future<void> _openRequest() async {
    final request = widget.request;
    if (request == null) return;
    try {
      final match =
          (await widget.gateway.practiceCommand({
                'op': 'match',
                'eco': request.eco,
                'name': request.openingName,
              }))!
              as Map<String, Object?>;
      final id = match['id']! as int;
      if (!mounted) return;
      if (id > 0) {
        await _open(id, request.openingName);
      } else {
        _error();
      }
    } catch (_) {
      if (mounted) _error();
    }
  }

  void _error() {
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(
        content: Text(
          AppLocalizations.of(context).trainingOpeningTreeLoadFailed,
        ),
      ),
    );
  }

  Future<void> _open(int id, String name) async {
    if (_busy) return;
    setState(() => _busy = true);
    await Navigator.of(context).push(
      MaterialPageRoute<void>(
        builder: (_) => PracticePlayer(
          gateway: widget.gateway,
          title: name,
          request: {'kind': 'opening', 'id': id, 'color': _color},
        ),
      ),
    );
    if (!mounted) return;
    _tree.dispose();
    setState(() {
      _tree = OpeningTreeController(gateway: widget.gateway);
      _busy = false;
    });
  }

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    return Scaffold(
      appBar: AppBar(
        title: Text(strings.trainingOpeningTitle),
        actions: [
          Padding(
            padding: const EdgeInsets.only(right: 12),
            child: SegmentedButton<String>(
              segments: [
                ButtonSegment(
                  value: 'white',
                  label: Text(strings.statsCompareColorWhite),
                ),
                ButtonSegment(
                  value: 'black',
                  label: Text(strings.statsCompareColorBlack),
                ),
              ],
              selected: {_color},
              onSelectionChanged: (selection) =>
                  setState(() => _color = selection.first),
            ),
          ),
        ],
      ),
      body: ListenableBuilder(
        listenable: _tree,
        builder: (context, _) {
          if (_tree.loadError) {
            return Center(
              child: Column(
                mainAxisSize: MainAxisSize.min,
                children: [
                  Text(strings.trainingOpeningTreeLoadFailed),
                  TextButton(
                    onPressed: _tree.loadRoots,
                    child: Text(strings.trainingRestart),
                  ),
                ],
              ),
            );
          }
          return SkillTreeCanvas(
            controller: _tree,
            onNodeSelected: (OpeningTreeNode node) =>
                unawaited(_open(node.openingId, node.name)),
          );
        },
      ),
    );
  }
}
