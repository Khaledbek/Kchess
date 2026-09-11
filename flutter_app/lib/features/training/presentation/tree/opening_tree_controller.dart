import 'dart:async';

import 'package:flutter/widgets.dart';

import '../../../../ffi/core_gateway.dart';
import '../../data/opening_database.dart';
import 'opening_fen_resolver.dart';

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
  ///
  /// Falls back to the loaded children while the count is still unknown, so a
  /// tree written by an older schema still behaves.
  bool get hasChildren => (data.childCount ?? children.length) > 0;

  int get childCount => data.childCount ?? children.length;

  Rect get rect =>
      Rect.fromLTWH(x, y - TreeMetrics.nodeHeight / 2, TreeMetrics.nodeWidth, TreeMetrics.nodeHeight);
}

/// Loads the opening tree one level at a time and keeps it laid out.
///
/// The tree is far too large to hold at once — roots come up front, children
/// only when their parent is unfolded — so both the layout and the board
/// previews are recomputed incrementally as levels arrive.
typedef ChildNodeLoader = Future<List<OpeningTreeNode>> Function(int parentId);

class OpeningTreeController extends ChangeNotifier {
  OpeningTreeController._(this._resolver, this._childLoader);

  factory OpeningTreeController({
    required CoreGateway gateway,
    OpeningFenResolver? resolver,
  }) {
    final controller = OpeningTreeController._(
      resolver ?? OpeningFenResolver(gateway: gateway),
      null,
    );
    unawaited(controller.loadRoots());
    return controller;
  }

  /// Builds a controller over nodes supplied directly, for tests and previews
  /// that must not touch the bundled database.
  @visibleForTesting
  factory OpeningTreeController.withRoots({
    required OpeningFenResolver resolver,
    required List<OpeningTreeNode> roots,
    ChildNodeLoader? childLoader,
  }) {
    final controller = OpeningTreeController._(resolver, childLoader);
    controller._adoptRoots(roots);
    return controller;
  }

  final OpeningFenResolver _resolver;
  final ChildNodeLoader? _childLoader;

  List<TreeLayoutNode> rootNodes = [];
  bool isLoadingRoots = true;

  final Map<String, String> _fenByPgn = {};

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
    final roots = await OpeningDatabase.getRootNodes();
    if (_disposed) return;
    _adoptRoots(roots);
  }

  void _adoptRoots(List<OpeningTreeNode> roots) {
    rootNodes = [
      for (final root in roots) _register(TreeLayoutNode(root)),
    ];
    isLoadingRoots = false;
    _computeLayout();
    // No focus request here: the canvas frames the first root itself once it
    // knows its own size, and a request raced against that would fight it.
    //
    // The bug this replaces: roots were never queued for replay at all, so
    // every card on the first screen the user sees rendered as an empty box.
    unawaited(_resolveFens(rootNodes));
  }

  TreeLayoutNode _register(TreeLayoutNode node) {
    final fen = node.data.fen;
    if (fen != null && fen.isNotEmpty) _fenByPgn[node.data.pgn] = fen;
    return node;
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

    if (node.children.isEmpty) {
      node.isLoadingChildren = true;
      _computeLayout();

      final loader = _childLoader ?? OpeningDatabase.getChildNodes;
      final children = await loader(node.data.id);
      if (_disposed) return;

      node.children = [
        for (final child in children)
          _register(TreeLayoutNode(child, depth: node.depth + 1)),
      ];
      node.isLoadingChildren = false;
    }

    _focusRequest = node;
    _computeLayout();
    await _resolveFens(node.children);
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

  /// The board preview for [node], or null while it is still being replayed.
  String? fenFor(TreeLayoutNode node) {
    final fen = _fenByPgn[node.data.pgn];
    return (fen == null || fen.isEmpty) ? null : fen;
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

  // --- Board previews ----------------------------------------------------

  /// Replays [nodes] into board previews, nearest first.
  ///
  /// Sequential rather than parallel: the resolver's prefix cache only pays off
  /// when a sibling can reuse what the one before it worked out, and a burst of
  /// concurrent replays would race past it and re-walk the same opening moves.
  Future<void> _resolveFens(List<TreeLayoutNode> nodes) async {
    for (final node in nodes) {
      if (_disposed) return;
      if (_fenByPgn.containsKey(node.data.pgn)) continue;

      String? fen;
      try {
        fen = await _resolver.resolve(node.data.pgn);
      } catch (_) {
        fen = null;
      }
      if (_disposed || fen == null) continue;

      _fenByPgn[node.data.pgn] = fen;
      node.data = node.data.copyWith(fen: fen);
      unawaited(OpeningDatabase.updateNodeFen(node.data.id, fen));
      notifyListeners();
    }
  }
}
