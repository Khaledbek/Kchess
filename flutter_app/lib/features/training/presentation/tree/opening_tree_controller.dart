// -----------------------------------------------------------------------------
// Section: Opening tree view-state and layout
// -----------------------------------------------------------------------------
import 'dart:async';

import 'package:flutter/widgets.dart';

import '../../../../ffi/core_gateway.dart';
import '../../models/practice_models.dart';

/// Fixed geometry for the skill tree.
///
/// The cards are a fixed size on purpose: the layout pass has to know where a
/// card ends before it is built, and the connector curves have to meet its
/// edge exactly. A card that sized itself to its name would make both guesses.
class TreeMetrics {
  const TreeMetrics._();

  static const nodeWidth = 260.0;
  static const nodeHeight = 148.0;

  /// Gap between a card's right edge and its children's left edge.
  static const columnGap = 72.0;

  /// Gap between two stacked cards.
  static const rowGap = 20.0;

  /// Breathing room around the whole tree, so an edge card is never flush
  /// against the viewport when the canvas is fitted.
  static const padding = 48.0;

  static const columnStride = nodeWidth + columnGap;
  static const rowStride = nodeHeight + rowGap;
}

/// One card in the laid-out tree.
///
/// [x] is the card's left edge and [y] its vertical *centre* — centres are
/// what the connector curves and the tidy-tree parent placement both work in.
class TreeLayoutNode {
  TreeLayoutNode(this.data, {this.depth = 0});

  OpeningTreeNode data;
  final int depth;

  double x = 0;
  double y = 0;
  List<TreeLayoutNode> children = [];
  bool expanded = false;
  bool isLoadingChildren = false;

  /// Whether this node has variations to unfold.
  bool get hasChildren => data.childCount > 0;

  Rect get rect => Rect.fromLTWH(
    x,
    y - TreeMetrics.nodeHeight / 2,
    TreeMetrics.nodeWidth,
    TreeMetrics.nodeHeight,
  );
}

/// Loads the opening tree one level at a time and keeps it laid out.
///
/// The tree is far too large to hold at once — roots come up front, children
/// only when their parent is unfolded — so both the layout and the board
/// previews are recomputed incrementally as levels arrive.
class OpeningTreeController extends ChangeNotifier {
  OpeningTreeController({required this.gateway}) {
    unawaited(loadRoots());
  }
  final CoreGateway gateway;
  Future<List<OpeningTreeNode>> _nodes(int parent) async {
    final result = await gateway.practiceCommand({
      'op': 'nodes',
      'parent': parent,
    });
    return (result! as List<Object?>)
        .cast<Map<String, Object?>>()
        .map(OpeningTreeNode.fromJson)
        .toList(growable: false);
  }

  List<TreeLayoutNode> rootNodes = [];
  bool isLoadingRoots = true;
  bool loadError = false;

  double totalWidth = 0;
  double totalHeight = 0;

  /// Node the canvas should bring into view, consumed by [takeFocusRequest].
  TreeLayoutNode? _focusRequest;

  bool _disposed = false;

  @override
  void dispose() {
    _disposed = true;
    super.dispose();
  }

  @override
  void notifyListeners() {
    if (_disposed) return;
    super.notifyListeners();
  }

  Future<void> loadRoots() async {
    loadError = false;
    isLoadingRoots = true;
    notifyListeners();
    try {
      final roots = await _nodes(0);
      if (_disposed) return;
      _adoptRoots(roots);
    } catch (_) {
      loadError = true;
      isLoadingRoots = false;
      notifyListeners();
    }
  }

  void _adoptRoots(List<OpeningTreeNode> roots) {
    rootNodes = [for (final root in roots) TreeLayoutNode(root)];
    isLoadingRoots = false;
    _computeLayout();
    // No focus request here: the canvas frames the first root itself once it
    // knows its own size, and a request raced against that would fight it.
    //
  }

  /// Unfolds [node], loading its variations on first open.
  Future<void> toggleExpansion(TreeLayoutNode node) async {
    if (node.expanded) {
      node.expanded = false;
      _computeLayout();
      return;
    }
    if (!node.hasChildren) return;

    node.expanded = true;

    // Reopening during an outstanding fetch reuses that request.
    if (node.isLoadingChildren) {
      _focusRequest = node;
      _computeLayout();
      return;
    }

    if (node.children.isEmpty) {
      node.isLoadingChildren = true;
      _computeLayout();

      try {
        final children = await _nodes(node.data.id);
        if (_disposed) return;
        node.children = [
          for (final child in children)
            TreeLayoutNode(child, depth: node.depth + 1),
        ];
      } catch (_) {
        node.expanded = false;
        loadError = true;
      } finally {
        node.isLoadingChildren = false;
      }
    }

    _focusRequest = node;
    _computeLayout();
  }

  /// Collapses everything back to the roots.
  void collapseAll() {
    void collapse(TreeLayoutNode node) {
      node.expanded = false;
      for (final child in node.children) {
        collapse(child);
      }
    }

    for (final root in rootNodes) {
      collapse(root);
    }
    if (rootNodes.isNotEmpty) _focusRequest = rootNodes.first;
    _computeLayout();
  }

  /// Node the canvas should scroll to, cleared as it is read so a rebuild for
  /// an unrelated reason does not yank the viewport back.
  TreeLayoutNode? takeFocusRequest() {
    final request = _focusRequest;
    _focusRequest = null;
    return request;
  }

  /// Every node currently on the canvas, parents before children.
  Iterable<TreeLayoutNode> get visibleNodes sync* {
    Iterable<TreeLayoutNode> walk(List<TreeLayoutNode> nodes) sync* {
      for (final node in nodes) {
        yield node;
        if (node.expanded) yield* walk(node.children);
      }
    }

    yield* walk(rootNodes);
  }

  // --- Layout ------------------------------------------------------------

  /// Places every visible card: one column per depth, children stacked in
  /// order, and a parent centred on the block its children occupy.
  void _computeLayout() {
    var cursorY = TreeMetrics.padding + TreeMetrics.nodeHeight / 2;
    var maxX = 0.0;
    var maxY = 0.0;

    void place(TreeLayoutNode node) {
      node.x = TreeMetrics.padding + node.depth * TreeMetrics.columnStride;
      if (node.x > maxX) maxX = node.x;

      if (!node.expanded || node.children.isEmpty) {
        node.y = cursorY;
        cursorY += TreeMetrics.rowStride;
      } else {
        final first = cursorY;
        for (final child in node.children) {
          place(child);
        }
        final last = cursorY - TreeMetrics.rowStride;
        node.y = (first + last) / 2;
      }
      if (node.y > maxY) maxY = node.y;
    }

    for (final root in rootNodes) {
      place(root);
    }

    totalWidth = maxX + TreeMetrics.nodeWidth + TreeMetrics.padding;
    totalHeight = maxY + TreeMetrics.nodeHeight / 2 + TreeMetrics.padding;
    notifyListeners();
  }
}
