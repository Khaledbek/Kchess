import 'package:flutter/material.dart';

// Section: Presentation of native focus squares, without chess interpretation
class CoachBoardFocus extends StatelessWidget {
  const CoachBoardFocus({
    required this.squares,
    required this.blackAtBottom,
    required this.color,
    super.key,
  });

  final Set<String> squares;
  final bool blackAtBottom;
  final Color color;

  @override
  Widget build(BuildContext context) => IgnorePointer(
    child: CustomPaint(painter: _FocusPainter(squares, blackAtBottom, color)),
  );
}

class _FocusPainter extends CustomPainter {
  _FocusPainter(this.squares, this.blackAtBottom, this.color);

  final Set<String> squares;
  final bool blackAtBottom;
  final Color color;

  @override
  void paint(Canvas canvas, Size size) {
    final cell = size.width / 8;
    if (cell <= 6) return;
    for (final square in squares) {
      if (square.length != 2) continue;
      final file = square.codeUnitAt(0) - 97;
      final rank = square.codeUnitAt(1) - 49;
      if (file < 0 || file > 7 || rank < 0 || rank > 7) continue;
      final x = blackAtBottom ? 7 - file : file;
      final y = blackAtBottom ? rank : 7 - rank;
      final rect = RRect.fromRectAndRadius(
        Rect.fromLTWH(x * cell + 3, y * cell + 3, cell - 6, cell - 6),
        const Radius.circular(6),
      );
      canvas.drawRRect(
        rect,
        Paint()
          ..color = color.withValues(alpha: 0.65)
          ..style = PaintingStyle.stroke
          ..strokeWidth = 5
          ..maskFilter = const MaskFilter.blur(BlurStyle.normal, 5),
      );
      canvas.drawRRect(
        rect,
        Paint()
          ..color = color
          ..style = PaintingStyle.stroke
          ..strokeWidth = 2,
      );
    }
  }

  @override
  bool shouldRepaint(_FocusPainter oldDelegate) =>
      oldDelegate.squares != squares ||
      oldDelegate.blackAtBottom != blackAtBottom ||
      oldDelegate.color != color;
}
