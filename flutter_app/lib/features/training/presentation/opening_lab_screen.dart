import 'dart:async';

import 'package:flutter/material.dart';

import '../../../ffi/core_gateway.dart';
import '../../../localization/generated/app_localizations.dart';
import '../../../shared/models/models.dart';
import '../models/opening_training_request.dart';
import '../models/practice_models.dart';
import 'practice_player.dart';
import 'opening/opening_cards.dart';
import 'opening/opening_family_hub.dart';

class OpeningLabScreen extends StatefulWidget {
  const OpeningLabScreen({required this.gateway, this.request, super.key});
  final CoreGateway gateway;
  final OpeningTrainingRequest? request;
  @override
  State<OpeningLabScreen> createState() => _OpeningLabScreenState();
}

class _OpeningLabScreenState extends State<OpeningLabScreen> {
  String _color = 'white';
  bool _busy = false;
  
  List<OpeningTreeNode>? _roots;
  OpeningsStats? _stats;
  bool _error = false;

  @override
  void initState() {
    super.initState();
    if (widget.request?.color == 'black') _color = 'black';
    WidgetsBinding.instance.addPostFrameCallback((_) {
      unawaited(_openRequest());
      unawaited(_loadData());
    });
  }

  Future<void> _loadData() async {
    try {
      final rootsResult = await widget.gateway.practiceCommand({
        'op': 'nodes',
        'parent': 0,
      });
      final roots = (rootsResult! as List<Object?>)
          .cast<Map<String, Object?>>()
          .map(OpeningTreeNode.fromJson)
          .toList();
          
      final stats = await widget.gateway.openingsStats();
      
      if (mounted) {
        setState(() {
          _roots = roots;
          _stats = stats;
        });
      }
    } catch (_) {
      if (mounted) {
        setState(() => _error = true);
      }
    }
  }

  Future<void> _openRequest() async {
    final request = widget.request;
    if (request == null) return;
    try {
      final match = (await widget.gateway.practiceCommand({
        'op': 'match',
        'eco': request.eco,
        'name': request.openingName,
      }))! as Map<String, Object?>;
      final id = match['id']! as int;
      if (!mounted) return;
      if (id > 0) {
        await _openDirect(id, request.openingName);
      } else {
        _showError();
      }
    } catch (_) {
      if (mounted) _showError();
    }
  }

  void _showError() {
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(
        content: Text(AppLocalizations.of(context).trainingOpeningTreeLoadFailed),
      ),
    );
  }

  Future<void> _openDirect(int id, String name) async {
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
    setState(() => _busy = false);
    _loadData(); // reload progress
  }
  
  void _openHub(OpeningTreeNode node, OpeningFamily? familyStats) {
    Navigator.of(context).push(
      MaterialPageRoute<void>(
        builder: (_) => OpeningFamilyHubScreen(
          gateway: widget.gateway,
          rootNode: node,
          color: _color,
          familyStats: familyStats,
        ),
      ),
    ).then((_) {
      if (mounted) _loadData();
    });
  }

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final theme = Theme.of(context);
    final colorScheme = theme.colorScheme;
    
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
      body: _buildBody(theme, colorScheme, strings),
    );
  }
  
  Widget _buildBody(ThemeData theme, ColorScheme colorScheme, AppLocalizations strings) {
    if (_error) {
      return Center(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Text(strings.trainingOpeningTreeLoadFailed),
            const SizedBox(height: 16),
            FilledButton(
              onPressed: () {
                setState(() => _error = false);
                _loadData();
              },
              child: Text(strings.trainingRestart),
            ),
          ],
        ),
      );
    }

    final roots = _roots;
    if (roots == null) {
      return const Center(child: CircularProgressIndicator());
    }

    final nemesis = _stats?.nemesis;
    
    return CustomScrollView(
      slivers: [
        if (nemesis != null)
          SliverToBoxAdapter(
            child: Padding(
              padding: const EdgeInsets.fromLTRB(16, 16, 16, 8),
              child: Container(
                decoration: BoxDecoration(
                  gradient: LinearGradient(
                    colors: [
                      colorScheme.errorContainer.withValues(alpha: 0.9),
                      colorScheme.errorContainer.withValues(alpha: 0.5),
                    ],
                    begin: Alignment.topLeft,
                    end: Alignment.bottomRight,
                  ),
                  border: Border.all(color: colorScheme.error.withValues(alpha: 0.5), width: 1),
                  borderRadius: BorderRadius.circular(16),
                  boxShadow: [
                    BoxShadow(
                      color: colorScheme.error.withValues(alpha: 0.1),
                      blurRadius: 10,
                      offset: const Offset(0, 4),
                    ),
                  ],
                ),
                padding: const EdgeInsets.all(20),
                child: Row(
                  children: [
                    Container(
                      padding: const EdgeInsets.all(10),
                      decoration: BoxDecoration(
                        color: colorScheme.error.withValues(alpha: 0.1),
                        shape: BoxShape.circle,
                      ),
                      child: Icon(Icons.warning_amber_rounded, color: colorScheme.error, size: 32),
                    ),
                    const SizedBox(width: 16),
                    Expanded(
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          Text(
                            'Schwachstelle',
                            style: theme.textTheme.labelMedium?.copyWith(
                              color: colorScheme.error,
                              fontWeight: FontWeight.bold,
                              letterSpacing: 1.2,
                            ),
                          ),
                          const SizedBox(height: 4),
                          Text(
                            nemesis.familyName,
                            style: theme.textTheme.titleLarge?.copyWith(
                              color: colorScheme.onErrorContainer,
                              fontWeight: FontWeight.bold,
                            ),
                          ),
                        ],
                      ),
                    ),
                    FilledButton.icon(
                      style: FilledButton.styleFrom(
                        backgroundColor: colorScheme.error,
                        foregroundColor: colorScheme.onError,
                        padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 12),
                        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
                      ),
                      icon: const Icon(Icons.fitness_center, size: 20),
                      label: const Text('Trainieren', style: TextStyle(fontWeight: FontWeight.bold)),
                      onPressed: () {
                        // find the root node matching nemesis
                        final match = roots.firstWhere(
                          (r) => r.name.toLowerCase() == nemesis.familyName.toLowerCase(),
                          orElse: () => roots.first,
                        );
                        final familyList = _stats?.families.where((f) => f.familyName.toLowerCase() == match.name.toLowerCase()).toList();
                        final fStats = (familyList != null && familyList.isNotEmpty) ? familyList.first : null;
                        _openHub(match, fStats);
                      },
                    ),
                  ],
                ),
              ),
            ),
          ),
          
        SliverPadding(
          padding: const EdgeInsets.all(16),
          sliver: SliverGrid(
            gridDelegate: const SliverGridDelegateWithMaxCrossAxisExtent(
              maxCrossAxisExtent: 320,
              mainAxisSpacing: 16,
              crossAxisSpacing: 16,
              childAspectRatio: 0.75,
            ),
            delegate: SliverChildBuilderDelegate(
              (context, index) {
                final node = roots[index];
                final isNemesis = nemesis != null && 
                    node.name.toLowerCase() == nemesis.familyName.toLowerCase();
                    
                final familyList = _stats?.families.where((f) => f.familyName.toLowerCase() == node.name.toLowerCase()).toList();
                final familyStats = (familyList != null && familyList.isNotEmpty) ? familyList.first : null;
                    
                return OpeningRootCard(
                  node: node,
                  isNemesis: isNemesis,
                  familyStats: familyStats,
                  onTap: () => _openHub(node, familyStats),
                );
              },
              childCount: roots.length,
            ),
          ),
        ),
      ],
    );
  }
}
