import 'package:flutter/material.dart';

import '../../../../localization/generated/app_localizations.dart';
import '../../../../services/training_progress_service.dart';
import '../../data/opening_database.dart';
import '../../models/models.dart';
import 'opening_tree_controller.dart';
import 'skill_tree_node.dart';

/// The pannable skill tree.
///
/// Owns the viewport: the tree canvas is thousands of points wide, so the user
/// must never be dropped into an empty corner of it. The first root is brought
/// into view on open, and unfolding a card slides its new children into sight
/// rather than leaving them somewhere off to the right.
class SkillTreeCanvas extends StatefulWidget {
  const SkillTreeCanvas({
    required this.controller,
    required this.progress,
    required this.onNodeSelected,
    super.key,
  });

  final OpeningTreeController controller;
  final TrainingProgressService progress;
  final ValueChanged<OpeningTreeNode> onNodeSelected;

  @override
  State<SkillTreeCanvas> createState() => _SkillTreeCanvasState();
}

class _SkillTreeCanvasState extends State<SkillTreeCanvas>
    with SingleTickerProviderStateMixin {
  static const _minScale = 0.25;
  static const _maxScale = 1.6;
  static const _initialScale = 0.85;

  final _viewport = TransformationController();
  late final AnimationController _animation = AnimationController(
    vsync: this,
    duration: const Duration(milliseconds: 320),
  );
  Animation<Matrix4>? _glide;

  Size _viewportSize = Size.zero;

  @override
  void initState() {
    super.initState();
    _animation.addListener(() {
      final glide = _glide;
      if (glide != null) _viewport.value = glide.value;
    });
    // Panning changes which cards are on screen, and only those get built.
    _viewport.addListener(_onViewportMoved);
    widget.controller.addListener(_onTreeChanged);
  }

  @override
  void didUpdateWidget(covariant SkillTreeCanvas oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.controller != widget.controller) {
      oldWidget.controller.removeListener(_onTreeChanged);
      widget.controller.addListener(_onTreeChanged);
    }
  }

  @override
  void dispose() {
    widget.controller.removeListener(_onTreeChanged);
    _viewport.removeListener(_onViewportMoved);
    _animation.dispose();
    _viewport.dispose();
    super.dispose();
  }

  void _onViewportMoved() {
    if (mounted) setState(() {});
  }

  /// The slice of the canvas currently on screen, with a card's worth of
  /// margin so nothing pops in at the edge mid-pan.
  ///
  /// One parent in the real database has 85 variations; building a live
  /// chessboard for each of them off-screen is what makes the canvas stutter.
  Rect get _visibleScene {
    if (_viewportSize.isEmpty) return Rect.largest;
    return Rect.fromPoints(
      _viewport.toScene(Offset.zero),
      _viewport.toScene(_viewportSize.bottomRight(Offset.zero)),
    ).inflate(TreeMetrics.nodeWidth);
  }

  /// Honours a focus request as soon as the frame that laid the node out is up.
  void _onTreeChanged() {
    final node = widget.controller.takeFocusRequest();
    if (node == null) return;
    WidgetsBinding.instance.addPostFrameCallback((_) {
      if (mounted) _focusOn(node);
    });
  }

  double get _scale => _viewport.value.getMaxScaleOnAxis();

  /// Slides [node] to the left third of the viewport, where its children will
  /// appear as it unfolds.
  void _focusOn(TreeLayoutNode node, {double? scale}) {
    if (_viewportSize.isEmpty) return;

    final target = (scale ?? _scale).clamp(_minScale, _maxScale);
    final anchor = Offset(
      node.rect.left - TreeMetrics.padding,
      node.rect.center.dy - _viewportSize.height / (2 * target),
    );
    _glideTo(
      Matrix4.identity()
        ..scaleByDouble(target, target, 1, 1)
        ..translateByDouble(-anchor.dx, -anchor.dy, 0, 1),
    );
  }

  void _glideTo(Matrix4 destination) {
    _glide = Matrix4Tween(begin: _viewport.value, end: destination).animate(
      CurvedAnimation(parent: _animation, curve: Curves.easeOutCubic),
    );
    _animation.forward(from: 0);
  }

  void _zoomBy(double factor) {
    final controller = widget.controller;
    if (controller.rootNodes.isEmpty || _viewportSize.isEmpty) return;

    final target = (_scale * factor).clamp(_minScale, _maxScale);
    // Zoom about the middle of the viewport, so the card the user is reading
    // stays the card they are reading.
    final centre = _viewport.toScene(_viewportSize.center(Offset.zero));
    _glideTo(
      Matrix4.identity()
        ..scaleByDouble(target, target, 1, 1)
        ..translateByDouble(
          -(centre.dx - _viewportSize.width / (2 * target)),
          -(centre.dy - _viewportSize.height / (2 * target)),
          0,
          1,
        ),
    );
  }

  /// Scales the whole tree down until it fits, then centres it.
  void _fit() {
    final controller = widget.controller;
    if (_viewportSize.isEmpty || controller.totalWidth <= 0) return;

    final target =
        (_viewportSize.width / controller.totalWidth)
            .clamp(_minScale, _maxScale)
            .toDouble();
    final scaled = Size(
      controller.totalWidth * target,
      controller.totalHeight * target,
    );
    _glideTo(
      Matrix4.identity()
        ..translateByDouble(
          (_viewportSize.width - scaled.width) / 2,
          scaled.height < _viewportSize.height
              ? (_viewportSize.height - scaled.height) / 2
              : 0.0,
          0,
          1,
        )
        ..scaleByDouble(target, target, 1, 1),
    );
  }

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final controller = widget.controller;

    if (controller.isLoadingRoots) {
      return const Center(child: CircularProgressIndicator());
    }
    if (controller.rootNodes.isEmpty) {
      return _Empty(message: strings.trainingOpeningTreeEmpty);
    }

    return LayoutBuilder(
      builder: (context, constraints) {
        final size = constraints.biggest;
        if (size != _viewportSize) {
          final first = _viewportSize.isEmpty;
          _viewportSize = size;
          if (first) {
            WidgetsBinding.instance.addPostFrameCallback((_) {
              if (mounted && controller.rootNodes.isNotEmpty) {
                _focusOn(controller.rootNodes.first, scale: _initialScale);
              }
            });
          }
        }

        return Stack(
          children: [
            Positioned.fill(child: _canvas(context)),
            Positioned(
              right: 12,
              bottom: 12,
              child: _ViewportControls(
                onZoomIn: () => _zoomBy(1.25),
                onZoomOut: () => _zoomBy(0.8),
                onFit: _fit,
                onHome: controller.collapseAll,
              ),
            ),
            // The hint explains the two things a card does; once a card has
            // been unfolded it has been read, and it is just covering the tree.
            if (!controller.rootNodes.any((node) => node.expanded))
              Positioned(
                left: 12,
                top: 12,
                right: 76,
                child: _Hint(text: strings.trainingOpeningTreeHint),
              ),
          ],
        );
      },
    );
  }

  Widget _canvas(BuildContext context) {
    final controller = widget.controller;
    final visible = _visibleScene;

    return InteractiveViewer(
      transformationController: _viewport,
      constrained: false,
      // Half a viewport of slack all round: enough to drag an edge card into
      // the middle, not so much that the tree can be flung into the void.
      boundaryMargin: EdgeInsets.symmetric(
        horizontal: _viewportSize.width / 2,
        vertical: _viewportSize.height / 2,
      ),
      minScale: _minScale,
      maxScale: _maxScale,
      // Heavier than the default fling friction: a flick should coast a card
      // or two, not launch the tree off into the boundary margin.
      interactionEndFrictionCoefficient: 0.0001,
      child: SizedBox(
        width: controller.totalWidth,
        height: controller.totalHeight,
        child: Stack(
          children: [
            Positioned.fill(
              child: CustomPaint(
                painter: _ConnectorPainter(
                  nodes: controller.visibleNodes.toList(),
                  color: Theme.of(context).colorScheme.outlineVariant,
                ),
              ),
            ),
            for (final node in controller.visibleNodes)
              if (visible.overlaps(node.rect)) _positioned(node),
          ],
        ),
      ),
    );
  }

  Widget _positioned(TreeLayoutNode node) {
    final key = node.data.progressKey;
    final progress = key == null
        ? const ExerciseProgress(exerciseId: '')
        : widget.progress.progressFor(key);

    return Positioned(
      key: ValueKey(node.data.id),
      left: node.rect.left,
      top: node.rect.top,
      width: TreeMetrics.nodeWidth,
      height: TreeMetrics.nodeHeight,
      child: SkillTreeNodeWidget(
        node: node,
        fen: widget.controller.fenFor(node),
        progress: progress,
        onToggle: () => widget.controller.toggleExpansion(node),
        onTrain: () => widget.onNodeSelected(node.data),
      ),
    );
  }
}

/// Draws the curve from each unfolded card's right edge to each child's left.
class _ConnectorPainter extends CustomPainter {
  const _ConnectorPainter({required this.nodes, required this.color});

  final List<TreeLayoutNode> nodes;
  final Color color;

  @override
  void paint(Canvas canvas, Size size) {
    final stroke = Paint()
      ..color = color
      ..strokeWidth = 2
      ..style = PaintingStyle.stroke;
    final dot = Paint()..color = color;

    for (final node in nodes) {
      if (!node.expanded || node.children.isEmpty) continue;

      final start = Offset(node.rect.right, node.rect.center.dy);
      canvas.drawCircle(start, 3, dot);

      for (final child in node.children) {
        final end = Offset(child.rect.left, child.rect.center.dy);
        final bend = (end.dx - start.dx) / 2;
        canvas.drawPath(
          Path()
            ..moveTo(start.dx, start.dy)
            ..cubicTo(
              start.dx + bend,
              start.dy,
              end.dx - bend,
              end.dy,
              end.dx,
              end.dy,
            ),
          stroke,
        );
      }
    }
  }

  @override
  bool shouldRepaint(covariant _ConnectorPainter oldDelegate) =>
      oldDelegate.color != color || oldDelegate.nodes != nodes;
}

class _ViewportControls extends StatelessWidget {
  const _ViewportControls({
    required this.onZoomIn,
    required this.onZoomOut,
    required this.onFit,
    required this.onHome,
  });

  final VoidCallback onZoomIn;
  final VoidCallback onZoomOut;
  final VoidCallback onFit;
  final VoidCallback onHome;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);

    return Card(
      elevation: 3,
      margin: EdgeInsets.zero,
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(24)),
      child: Padding(
        padding: const EdgeInsets.symmetric(vertical: 4),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            IconButton(
              key: const Key('opening-tree-zoom-in'),
              tooltip: strings.trainingOpeningTreeZoomIn,
              onPressed: onZoomIn,
              icon: const Icon(Icons.add),
            ),
            IconButton(
              key: const Key('opening-tree-zoom-out'),
              tooltip: strings.trainingOpeningTreeZoomOut,
              onPressed: onZoomOut,
              icon: const Icon(Icons.remove),
            ),
            IconButton(
              key: const Key('opening-tree-fit'),
              tooltip: strings.trainingOpeningTreeFit,
              onPressed: onFit,
              icon: const Icon(Icons.fit_screen_outlined),
            ),
            IconButton(
              key: const Key('opening-tree-home'),
              tooltip: strings.trainingOpeningLabBackToOverview,
              onPressed: onHome,
              icon: const Icon(Icons.restart_alt),
            ),
          ],
        ),
      ),
    );
  }
}

class _Hint extends StatelessWidget {
  const _Hint({required this.text});

  final String text;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);

    return IgnorePointer(
      child: DecoratedBox(
        decoration: BoxDecoration(
          color: theme.colorScheme.surface.withValues(alpha: 0.85),
          borderRadius: BorderRadius.circular(10),
        ),
        child: Padding(
          padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
          child: Text(
            text,
            style: theme.textTheme.bodySmall?.copyWith(
              color: theme.colorScheme.onSurfaceVariant,
            ),
            maxLines: 2,
            overflow: TextOverflow.ellipsis,
          ),
        ),
      ),
    );
  }
}

class _Empty extends StatelessWidget {
  const _Empty({required this.message});

  final String message;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);

    return Center(
      child: Padding(
        padding: const EdgeInsets.all(32),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Icon(
              Icons.account_tree_outlined,
              size: 48,
              color: theme.colorScheme.outline,
            ),
            const SizedBox(height: 12),
            Text(
              message,
              textAlign: TextAlign.center,
              style: theme.textTheme.bodyMedium?.copyWith(
                color: theme.colorScheme.onSurfaceVariant,
              ),
            ),
          ],
        ),
      ),
    );
  }
}
