import 'package:flutter/material.dart';

// -----------------------------------------------------------------------------
// Section: Isolated best-move and threat arrow presentation
// -----------------------------------------------------------------------------

/// Keeps unchanged arrows out of board/result-animation repaint passes.
/// Native analysis remains responsible for selecting the move.
class AnalysisMoveArrow extends StatelessWidget {
  const AnalysisMoveArrow({
    required this.move,
    required this.color,
    required this.blackAtBottom,
    required this.paintKey,
    this.thickness = 0.17,
    this.glow = 0,
    super.key,
  });

  final String move;
  final Color color;
  final bool blackAtBottom;
  final Key paintKey;

  /// Shaft width as a fraction of one square.
  final double thickness;

  /// Neon halo behind the arrow, as a fraction of one square. 0 draws none.
  final double glow;

  @override
  Widget build(BuildContext context) => IgnorePointer(
    child: RepaintBoundary(
      child: CustomPaint(
        key: paintKey,
        painter: _ArrowPainter(
          move,
          color,
          blackAtBottom: blackAtBottom,
          thickness: thickness,
          glow: glow,
        ),
      ),
    ),
  );
}

class _ArrowPainter extends CustomPainter {
  const _ArrowPainter(
    this.move,
    this.color, {
    required this.blackAtBottom,
    required this.thickness,
    required this.glow,
  });

  final String move;
  final Color color;
  final bool blackAtBottom;
  final double thickness;
  final double glow;

  Offset? _squareCenter(int offset, double square) {
    final file = move.codeUnitAt(offset) - 97;
    final rank = move.codeUnitAt(offset + 1) - 48;
    if (file < 0 || file > 7 || rank < 1 || rank > 8) {
      return null;
    }
    if (blackAtBottom) {
      return Offset((7 - file + 0.5) * square, (rank - 1 + 0.5) * square);
    }
    return Offset((file + 0.5) * square, (8 - rank + 0.5) * square);
  }

  @override
  void paint(Canvas canvas, Size size) {
    if (move.length < 4) return;
    final square = size.width / 8;
    final start = _squareCenter(0, square);
    final end = _squareCenter(2, square);
    if (start == null || end == null) return;
    final vector = end - start;
    final length = vector.distance;
    if (length == 0) return;
    final unit = vector / length;
    // Head geometry follows the shaft so a thicker arrow keeps its shape.
    final head = thickness / 0.17;
    final base = end - unit * square * 0.34 * head;
    final perpendicular = Offset(-unit.dy, unit.dx);
    final paint = Paint()
      ..color = color
      ..strokeWidth = square * thickness
      ..strokeCap = StrokeCap.round;
    if (glow > 0) {
      final halo = Paint()
        ..color = color.withValues(alpha: 0.55)
        ..strokeWidth = square * thickness
        ..strokeCap = StrokeCap.round
        ..maskFilter = MaskFilter.blur(BlurStyle.normal, square * glow);
      canvas.drawLine(start, end, halo);
    }
    canvas.drawLine(start, base, paint);
    final path = Path()
      ..moveTo(end.dx, end.dy)
      ..lineTo(
        base.dx + perpendicular.dx * square * 0.24 * head,
        base.dy + perpendicular.dy * square * 0.24 * head,
      )
      ..lineTo(
        base.dx - perpendicular.dx * square * 0.24 * head,
        base.dy - perpendicular.dy * square * 0.24 * head,
      )
      ..close();
    canvas.drawPath(path, paint..style = PaintingStyle.fill);
  }

  @override
  bool shouldRepaint(covariant _ArrowPainter oldDelegate) =>
      oldDelegate.move != move ||
      oldDelegate.color != color ||
      oldDelegate.blackAtBottom != blackAtBottom ||
      oldDelegate.thickness != thickness ||
      oldDelegate.glow != glow;
}
