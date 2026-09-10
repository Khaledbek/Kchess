// -----------------------------------------------------------------------------
// Section: Shared board endgame result presentation
// -----------------------------------------------------------------------------

import 'dart:math' as math;

import 'package:flutter/material.dart';

String? boardKingSquareForColor(List<String> pieces, String color) {
  final king = color == 'white' ? 'K' : 'k';
  final index = pieces.indexOf(king);
  if (index < 0) return null;
  final file = String.fromCharCode('a'.codeUnitAt(0) + index % 8);
  final rank = 8 - index ~/ 8;
  return '$file$rank';
}

class BoardEndgameLayer extends StatefulWidget {
  const BoardEndgameLayer({
    required this.visible,
    required this.result,
    required this.checkmate,
    required this.pieces,
    required this.boardSide,
    required this.blackAtBottom,
    this.showCheckmateBadge = true,
    this.useGiveupForLoss = true,
    super.key,
  });

  final bool visible;
  final String? result;
  final bool checkmate;
  final List<String> pieces;
  final double boardSide;
  final bool blackAtBottom;
  final bool showCheckmateBadge;
  final bool useGiveupForLoss;

  @override
  State<BoardEndgameLayer> createState() => _BoardEndgameLayerState();
}

class _BoardEndgameLayerState extends State<BoardEndgameLayer> {
  bool _docked = false;
  bool _presented = false;
  String? _signature;

  String? _currentSignature() {
    final result = widget.result;
    if (result == null || result == '*') return null;
    final whiteKing = boardKingSquareForColor(widget.pieces, 'white');
    final blackKing = boardKingSquareForColor(widget.pieces, 'black');
    return '$result:${widget.checkmate}:$whiteKing:$blackKing';
  }

  @override
  void initState() {
    super.initState();
    _signature = _currentSignature();
  }

  @override
  void didUpdateWidget(covariant BoardEndgameLayer oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (!widget.visible) return;
    final signature = _currentSignature();
    if (signature != _signature) {
      _signature = signature;
      _docked = false;
      _presented = false;
      return;
    }
    if (!oldWidget.visible && _presented) {
      _docked = true;
    }
  }

  void _dock() {
    if (!mounted || _docked) return;
    setState(() {
      _docked = true;
      _presented = true;
    });
  }

  @override
  Widget build(BuildContext context) {
    final result = widget.result;
    if (!widget.visible || result == null || result == '*') {
      return const SizedBox.shrink();
    }
    final whiteKing = boardKingSquareForColor(widget.pieces, 'white');
    final blackKing = boardKingSquareForColor(widget.pieces, 'black');
    if (whiteKing == null && blackKing == null) {
      return const SizedBox.shrink();
    }

    if (_docked || _presented) {
      return IgnorePointer(
        child: Stack(
          children: [
            if (whiteKing != null)
              Positioned.fill(
                child: BoardEndgameKingMarker(
                  square: whiteKing,
                  color: 'white',
                  result: result,
                  checkmate: widget.checkmate,
                  boardSide: widget.boardSide,
                  blackAtBottom: widget.blackAtBottom,
                  showCheckmateBadge: widget.showCheckmateBadge,
                  useGiveupForLoss: widget.useGiveupForLoss,
                ),
              ),
            if (blackKing != null)
              Positioned.fill(
                child: BoardEndgameKingMarker(
                  square: blackKing,
                  color: 'black',
                  result: result,
                  checkmate: widget.checkmate,
                  boardSide: widget.boardSide,
                  blackAtBottom: widget.blackAtBottom,
                  showCheckmateBadge: widget.showCheckmateBadge,
                  useGiveupForLoss: widget.useGiveupForLoss,
                ),
              ),
          ],
        ),
      );
    }

    return BoardEndgameAnimation(
      key: ValueKey('board-endgame-${_signature ?? result}'),
      result: result,
      checkmate: widget.checkmate,
      whiteKingSquare: whiteKing,
      blackKingSquare: blackKing,
      boardSide: widget.boardSide,
      blackAtBottom: widget.blackAtBottom,
      showCheckmateBadge: widget.showCheckmateBadge,
      useGiveupForLoss: widget.useGiveupForLoss,
      onCompleted: _dock,
    );
  }
}

class BoardEndgameAnimation extends StatefulWidget {
  const BoardEndgameAnimation({
    required this.result,
    required this.checkmate,
    required this.whiteKingSquare,
    required this.blackKingSquare,
    required this.boardSide,
    required this.blackAtBottom,
    required this.showCheckmateBadge,
    required this.useGiveupForLoss,
    required this.onCompleted,
    super.key,
  });

  final String result;
  final bool checkmate;
  final String? whiteKingSquare;
  final String? blackKingSquare;
  final double boardSide;
  final bool blackAtBottom;
  final bool showCheckmateBadge;
  final bool useGiveupForLoss;
  final VoidCallback onCompleted;

  @override
  State<BoardEndgameAnimation> createState() => _BoardEndgameAnimationState();
}

class _BoardEndgameAnimationState extends State<BoardEndgameAnimation>
    with SingleTickerProviderStateMixin {
  late final AnimationController _controller;
  bool _completed = false;

  @override
  void initState() {
    super.initState();
    _controller = AnimationController(
      vsync: this,
      duration: const Duration(milliseconds: 1350),
    )..addStatusListener((status) {
        if (status == AnimationStatus.completed) _finish();
      });
    WidgetsBinding.instance.addPostFrameCallback((_) {
      if (mounted) _controller.forward();
    });
  }

  void _finish() {
    if (_completed) return;
    _completed = true;
    widget.onCompleted();
  }

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  Widget _animatedKingMarker({
    required String square,
    required String color,
    required double progress,
  }) {
    final asset = _resultAssetForColor(
      color,
      widget.result,
      widget.checkmate,
      useGiveupForLoss: widget.useGiveupForLoss,
    );
    final fieldColor = _resultFieldColorForColor(color, widget.result);
    if (asset == null || fieldColor == null) return const SizedBox.shrink();

    final squareSide = widget.boardSide / 8;
    final rect = boardSquareRect(
      square,
      widget.boardSide,
      widget.blackAtBottom,
    );

    final fillIn = Curves.easeOutCubic.transform(
      (progress / 0.22).clamp(0.0, 1.0),
    );
    final fillOut = 1 - Curves.easeInCubic.transform(
      ((progress - 0.55) / 0.35).clamp(0.0, 1.0),
    );
    final fillOpacity = math.min(fillIn, fillOut);

    final iconIn = Curves.easeOutBack.transform(
      ((progress - 0.14) / 0.34).clamp(0.0, 1.0),
    );
    final dock = Curves.easeInOutCubic.transform(
      ((progress - 0.55) / 0.45).clamp(0.0, 1.0),
    );
    final largeSize = squareSide * 0.96;
    final smallSize = squareSide * 0.42;
    final size = (largeSize * iconIn) * (1 - dock) + smallSize * dock;
    final largeCenter = rect.center;
    final smallCenter = Offset(
      rect.right - smallSize / 2 - squareSide * 0.035,
      rect.top + smallSize / 2 + squareSide * 0.035,
    );
    final center = Offset.lerp(largeCenter, smallCenter, dock)!;
    final iconOpacity = ((progress - 0.12) / 0.18).clamp(0.0, 1.0);
    final showMate = widget.showCheckmateBadge &&
        _isCheckmatedColor(color, widget.result, widget.checkmate);

    return Stack(
      children: [
        Positioned.fromRect(
          rect: rect,
          child: ColoredBox(
            key: Key('board-result-field-$color'),
            color: fieldColor.withValues(alpha: fillOpacity),
          ),
        ),
        if (size > 0)
          Positioned(
            left: center.dx - size / 2,
            top: center.dy - size / 2,
            width: size,
            height: size,
            child: Opacity(
              opacity: iconOpacity,
              child: Image.asset(
                asset,
                key: Key('board-result-animation-$color'),
                fit: BoxFit.contain,
                errorBuilder: (_, _, _) => const SizedBox.shrink(),
              ),
            ),
          ),
        if (showMate && progress >= 0.58)
          Positioned(
            left: rect.left + squareSide * 0.04,
            top: rect.bottom - squareSide * 0.39,
            child: Opacity(
              opacity: ((progress - 0.58) / 0.16).clamp(0.0, 1.0),
              child: CheckmateBadge(size: squareSide * 0.34),
            ),
          ),
      ],
    );
  }

  @override
  Widget build(BuildContext context) => IgnorePointer(
    child: AnimatedBuilder(
      animation: _controller,
      builder: (context, _) {
        final progress = _controller.value;
        return Stack(
          clipBehavior: Clip.none,
          children: [
            if (widget.whiteKingSquare != null)
              Positioned.fill(
                child: _animatedKingMarker(
                  square: widget.whiteKingSquare!,
                  color: 'white',
                  progress: progress,
                ),
              ),
            if (widget.blackKingSquare != null)
              Positioned.fill(
                child: _animatedKingMarker(
                  square: widget.blackKingSquare!,
                  color: 'black',
                  progress: progress,
                ),
              ),
          ],
        );
      },
    ),
  );
}

class BoardEndgameKingMarker extends StatelessWidget {
  const BoardEndgameKingMarker({
    required this.square,
    required this.color,
    required this.result,
    required this.checkmate,
    required this.boardSide,
    required this.blackAtBottom,
    required this.showCheckmateBadge,
    required this.useGiveupForLoss,
    super.key,
  });

  final String square;
  final String color;
  final String result;
  final bool checkmate;
  final double boardSide;
  final bool blackAtBottom;
  final bool showCheckmateBadge;
  final bool useGiveupForLoss;

  @override
  Widget build(BuildContext context) {
    final asset = _resultAssetForColor(
      color,
      result,
      checkmate,
      useGiveupForLoss: useGiveupForLoss,
    );
    if (asset == null) return const SizedBox.shrink();
    final rect = boardSquareRect(square, boardSide, blackAtBottom);
    final squareSide = boardSide / 8;
    final badgeSize = squareSide * 0.42;
    final showMate = showCheckmateBadge &&
        _isCheckmatedColor(color, result, checkmate);
    return IgnorePointer(
      child: Stack(
        children: [
          Positioned(
            left: rect.right - badgeSize - squareSide * 0.035,
            top: rect.top + squareSide * 0.035,
            width: badgeSize,
            height: badgeSize,
            child: Image.asset(
              asset,
              key: Key('board-result-docked-$color'),
              fit: BoxFit.contain,
              errorBuilder: (_, _, _) => const SizedBox.shrink(),
            ),
          ),
          if (showMate)
            Positioned(
              left: rect.left + squareSide * 0.04,
              top: rect.bottom - squareSide * 0.39,
              child: CheckmateBadge(size: squareSide * 0.34),
            ),
        ],
      ),
    );
  }
}

class CheckmateBadge extends StatelessWidget {
  const CheckmateBadge({required this.size, super.key});

  final double size;

  @override
  Widget build(BuildContext context) => Container(
    key: key ?? const Key('board-checkmate-symbol'),
    width: size,
    height: size,
    alignment: Alignment.center,
    decoration: BoxDecoration(
      color: const Color(0xFF8E1B1B),
      borderRadius: BorderRadius.circular(size * 0.22),
      border: Border.all(color: Colors.white, width: math.max(1, size * 0.07)),
      boxShadow: const [BoxShadow(blurRadius: 2, color: Color(0x55000000))],
    ),
    child: Text(
      '#',
      textDirection: TextDirection.ltr,
      style: TextStyle(
        color: Colors.white,
        fontSize: size * 0.72,
        height: 1,
        fontWeight: FontWeight.w900,
      ),
    ),
  );
}

Rect boardSquareRect(String square, double boardSide, bool blackAtBottom) {
  final file = square.codeUnitAt(0) - 'a'.codeUnitAt(0);
  final rank = int.tryParse(square.substring(1)) ?? 1;
  final column = blackAtBottom ? 7 - file : file;
  final row = blackAtBottom ? rank - 1 : 8 - rank;
  final squareSide = boardSide / 8;
  return Rect.fromLTWH(
    column * squareSide,
    row * squareSide,
    squareSide,
    squareSide,
  );
}

bool _isDrawResult(String result) => result == '1/2-1/2' || result == '½-½';

String? _resultAssetForColor(
  String color,
  String? result,
  bool checkmate, {
  required bool useGiveupForLoss,
}) {
  if (result == null || result == '*') return null;
  if (_isDrawResult(result)) return 'assets/analysis_img/result_draw.png';
  final whiteWon = result == '1-0';
  final colorIsWhite = color == 'white';
  if (whiteWon == colorIsWhite) return 'assets/analysis_img/result_win.png';
  if (checkmate || !useGiveupForLoss) {
    return 'assets/analysis_img/result_loss.png';
  }
  return 'assets/analysis_img/result_giveup.png';
}

Color? _resultFieldColorForColor(String color, String? result) {
  if (result == null || result == '*') return null;
  if (_isDrawResult(result)) return const Color(0xFFF3EBDD);
  final whiteWon = result == '1-0';
  final colorIsWhite = color == 'white';
  return whiteWon == colorIsWhite
      ? const Color(0xFF00D400)
      : const Color(0xFFFF1010);
}

bool _isCheckmatedColor(String color, String? result, bool checkmate) {
  if (!checkmate || result == null || result == '*' || _isDrawResult(result)) {
    return false;
  }
  final whiteWon = result == '1-0';
  return (color == 'white') != whiteWon;
}
