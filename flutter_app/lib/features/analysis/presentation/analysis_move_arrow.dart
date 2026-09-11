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
    super.key,
  });

  final String move;
  final Color color;
  final bool blackAtBottom;
  final Key paintKey;

  @override
  Widget build(BuildContext context) => IgnorePointer(
    child: RepaintBoundary(
      child: CustomPaint(
        key: paintKey,
        painter: _ArrowPainter(move, color, blackAtBottom: blackAtBottom),
      ),
    ),
  );
}

class _ArrowPainter extends CustomPainter {
  const _ArrowPainter(this.move, this.color, {required this.blackAtBottom});

  final String move;
  final Color color;
  final bool blackAtBottom;

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
    final base = end - unit * square * 0.34;
    final perpendicular = Offset(-unit.dy, unit.dx);
    final paint = Paint()
      ..color = color
      ..strokeWidth = square * 0.17
      ..strokeCap = StrokeCap.round;
    canvas.drawLine(start, base, paint);
    final path = Path()
      ..moveTo(end.dx, end.dy)
      ..lineTo(
        base.dx + perpendicular.dx * square * 0.24,
        base.dy + perpendicular.dy * square * 0.24,
      )
      ..lineTo(
        base.dx - perpendicular.dx * square * 0.24,
        base.dy - perpendicular.dy * square * 0.24,
      )
      ..close();
    canvas.drawPath(path, paint..style = PaintingStyle.fill);
  }

  @override
  bool shouldRepaint(covariant _ArrowPainter oldDelegate) =>
      oldDelegate.move != move ||
      oldDelegate.color != color ||
      oldDelegate.blackAtBottom != blackAtBottom;
}
