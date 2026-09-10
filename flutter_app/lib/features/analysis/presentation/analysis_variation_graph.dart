import '../../../shared/models/models.dart';

// -----------------------------------------------------------------------------
// Section: Ephemeral analysis variation graph
// -----------------------------------------------------------------------------

/// One explored sideline move on the analysis board.
///
/// The graph is intentionally UI-session state only. Chess legality, SAN, FEN,
/// positions and engine classifications still come from the native C++ core.
class AnalysisVariationNode {
  AnalysisVariationNode({
    required this.id,
    required this.rootMainLinePly,
    required this.parentId,
    required this.snapshot,
  });

  final int id;
  final int rootMainLinePly;
  final int? parentId;
  VariationAnalysisSnapshot snapshot;
  final List<int> childIds = <int>[];
  int? preferredChildId;
}

/// Keeps every explored sideline branch alive for as long as the analysis
/// screen is open. Nothing in this graph is persisted to the game database.
class AnalysisVariationGraph {
  final Map<int, AnalysisVariationNode> _nodes =
      <int, AnalysisVariationNode>{};
  final Map<int, List<int>> _rootsByMainLinePly = <int, List<int>>{};
  int _nextId = 1;

  int? currentNodeId;

  bool get isEmpty => _nodes.isEmpty;
  bool get isOnVariation => currentNodeId != null;
  AnalysisVariationNode? get current => node(currentNodeId);
  Iterable<AnalysisVariationNode> get nodes => _nodes.values;
  List<int> get rootMainLinePlies {
    final values = _rootsByMainLinePly.keys.toList(growable: false)..sort();
    return values;
  }

  AnalysisVariationNode? node(int? id) => id == null ? null : _nodes[id];

  List<AnalysisVariationNode> rootsAt(int mainLinePly) =>
      (_rootsByMainLinePly[mainLinePly] ?? const <int>[])
          .map((id) => _nodes[id])
          .whereType<AnalysisVariationNode>()
          .toList(growable: false);

  AnalysisVariationNode addOrSelect({
    required int rootMainLinePly,
    required int? parentId,
    required VariationAnalysisSnapshot snapshot,
  }) {
    final siblingIds = parentId == null
        ? _rootsByMainLinePly.putIfAbsent(
            rootMainLinePly,
            () => <int>[],
          )
        : (_nodes[parentId]?.childIds ?? <int>[]);
    for (final siblingId in siblingIds) {
      final sibling = _nodes[siblingId];
      if (sibling != null && sibling.snapshot.playedMove == snapshot.playedMove) {
        sibling.snapshot = snapshot;
        currentNodeId = sibling.id;
        return sibling;
      }
    }

    final node = AnalysisVariationNode(
      id: _nextId++,
      rootMainLinePly: rootMainLinePly,
      parentId: parentId,
      snapshot: snapshot,
    );
    _nodes[node.id] = node;
    if (parentId == null) {
      _rootsByMainLinePly
          .putIfAbsent(rootMainLinePly, () => <int>[])
          .add(node.id);
    } else {
      final parent = _nodes[parentId];
      if (parent != null) {
        parent.childIds.add(node.id);
        parent.preferredChildId ??= node.id;
      }
    }
    currentNodeId = node.id;
    return node;
  }

  void updateSnapshot(int nodeId, VariationAnalysisSnapshot snapshot) {
    final target = _nodes[nodeId];
    if (target != null) target.snapshot = snapshot;
  }

  void pauseRunningSnapshots({int? exceptNodeId}) {
    for (final node in _nodes.values) {
      if (node.id == exceptNodeId || !node.snapshot.isRunning) continue;
      node.snapshot = node.snapshot.copyWith(status: 'paused');
    }
  }

  void selectMainLine() {
    currentNodeId = null;
  }

  bool selectNode(int nodeId) {
    if (!_nodes.containsKey(nodeId)) return false;
    currentNodeId = nodeId;
    return true;
  }

  List<AnalysisVariationNode> pathTo(int? nodeId) {
    final reversed = <AnalysisVariationNode>[];
    var cursor = node(nodeId);
    while (cursor != null) {
      reversed.add(cursor);
      cursor = node(cursor.parentId);
    }
    return reversed.reversed.toList(growable: false);
  }

  List<AnalysisVariationNode> get currentPath => pathTo(currentNodeId);

  /// Route used by previous/next controls for the currently selected branch.
  /// It contains the ancestors plus the preferred descendants, so navigating
  /// backward does not lose the known forward continuation.
  List<AnalysisVariationNode> get currentRoute {
    final currentNode = current;
    if (currentNode == null) return const <AnalysisVariationNode>[];
    final path = pathTo(currentNode.id);
    if (path.isEmpty) return path;

    final route = <AnalysisVariationNode>[...path];
    var cursor = currentNode;
    while (cursor.preferredChildId != null) {
      final child = node(cursor.preferredChildId);
      if (child == null) break;
      route.add(child);
      cursor = child;
    }
    return route;
  }

  int get currentRouteIndex {
    final id = currentNodeId;
    if (id == null) return -1;
    return currentRoute.indexWhere((node) => node.id == id);
  }

  AnalysisVariationNode? get previous {
    final currentNode = current;
    return currentNode == null ? null : node(currentNode.parentId);
  }

  AnalysisVariationNode? get preferredNext {
    final currentNode = current;
    return currentNode == null ? null : node(currentNode.preferredChildId);
  }

  AnalysisVariationNode? get preferredLast {
    var cursor = current;
    if (cursor == null) return null;
    while (true) {
      final childId = cursor!.preferredChildId;
      if (childId == null) break;
      final child = node(childId);
      if (child == null) break;
      cursor = child;
    }
    return cursor;
  }

  /// UI summary entries: one node per complete explored sideline, not one row
  /// per individual move. All intermediate move nodes stay in memory and are
  /// still used by previous/next navigation.
  List<AnalysisVariationNode> terminalNodesAt(int mainLinePly) {
    final terminals = <AnalysisVariationNode>[];
    for (final root in rootsAt(mainLinePly)) {
      _collectTerminalNodes(root, terminals);
    }
    return terminals;
  }

  void _collectTerminalNodes(
    AnalysisVariationNode node,
    List<AnalysisVariationNode> target,
  ) {
    if (node.childIds.isEmpty) {
      target.add(node);
      return;
    }
    for (final childId in node.childIds) {
      final child = this.node(childId);
      if (child != null) _collectTerminalNodes(child, target);
    }
  }
}
