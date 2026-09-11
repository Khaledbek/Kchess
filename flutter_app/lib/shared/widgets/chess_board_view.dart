import 'package:flutter/material.dart';
import 'package:flutter_svg/flutter_svg.dart';

import '../models/models.dart';

/// The 8x8 board itself: squares, coordinates, pieces and drag/drop.
///
/// Deliberately knows nothing about games, engines or exercises — it renders a
/// [BoardPosition] and reports the squares the user touched. Everything a
/// caller wants to draw on top (evaluation arrows, move-classification badges,
/// a solved overlay) arrives through [squareTint] and [squareOverlay], or is
/// stacked over the widget by the caller.
///
/// The analysis board and the training arena share this so a board fix lands in
/// both.
class ChessBoardView extends StatelessWidget {
  const ChessBoardView({
    required this.position,
    required this.onSquareTap,
    required this.onPieceDrop,
    this.blackAtBottom = false,
    this.showCoordinates = true,
    this.interactive = true,
    this.squareTint,
    this.squareOverlay,
    this.moveTargets = const {},
    this.onDragStarted,
    this.onDragEnded,
    super.key,
  });

  /// Board colours, shared so overlays can blend against the real square.
  static const lightSquare = Color(0xFFE8E5DC);
  static const darkSquare = Color(0xFF71867D);

  static const pieceAssets = <String, String>{
    'K': 'assets/analysis_img/piece_white_king.svg',
    'Q': 'assets/analysis_img/piece_white_queen.svg',
    'R': 'assets/analysis_img/piece_white_rook.svg',
    'B': 'assets/analysis_img/piece_white_bishop.svg',
    'N': 'assets/analysis_img/piece_white_knight.svg',
    'P': 'assets/analysis_img/piece_white_pawn.svg',
    'k': 'assets/analysis_img/piece_black_king.svg',
    'q': 'assets/analysis_img/piece_black_queen.svg',
    'r': 'assets/analysis_img/piece_black_rook.svg',
    'b': 'assets/analysis_img/piece_black_bishop.svg',
    'n': 'assets/analysis_img/piece_black_knight.svg',
    'p': 'assets/analysis_img/piece_black_pawn.svg',
  };

  final BoardPosition position;
  final bool blackAtBottom;
  final bool showCoordinates;

  /// When false no piece can be picked up — used while the opponent's scripted
  /// reply is animating and once an exercise is solved.
  final bool interactive;

  /// Colour for a square, given its plain light/dark base. Return the base to
  /// leave it alone.
  final Color Function(String square, Color base)? squareTint;

  /// Extra widget painted inside a square (a badge, a flash ring).
  final Widget? Function(String square, double squareSide)? squareOverlay;

  /// Squares the selected piece can legally move to. Empty targets get a dot,
  /// occupied ones a ring, which is the convention players already know from
  /// every other board.
  final Set<String> moveTargets;

  final ValueChanged<String> onSquareTap;
  final void Function(String source, String target) onPieceDrop;
  final ValueChanged<String>? onDragStarted;
  final VoidCallback? onDragEnded;

  bool _canDragPiece(String piece) {
    if (!interactive || piece.isEmpty) return false;
    final whitePiece = piece == piece.toUpperCase();
    return whitePiece == (position.draggableColor == 'white');
  }

  @override
  Widget build(BuildContext context) {
    final pieces = position.pieces;
    return LayoutBuilder(
      builder: (context, constraints) {
        final squareSide = constraints.maxWidth / 8;
        return GridView.builder(
          physics: const NeverScrollableScrollPhysics(),
          gridDelegate: const SliverGridDelegateWithFixedCrossAxisCount(
            crossAxisCount: 8,
          ),
          itemCount: 64,
          itemBuilder: (context, index) {
            final row = index ~/ 8;
            final column = index % 8;
            final fileIndex = blackAtBottom ? 7 - column : column;
            final rank = blackAtBottom ? row + 1 : 8 - row;
            final square =
                '${String.fromCharCode('a'.codeUnitAt(0) + fileIndex)}$rank';
            final light = (row + column).isEven;
            final baseColor = light ? lightSquare : darkSquare;
            final squareColor =
                squareTint?.call(square, baseColor) ?? baseColor;
            final coordinateColor = light
                ? const Color(0xFF53655E)
                : const Color(0xFFE8E5DC);
            final pieceIndex = (8 - rank) * 8 + fileIndex;
            final piece = pieces[pieceIndex];
            final pieceAsset = pieceAssets[piece];
            final canDrag = pieceAsset != null && _canDragPiece(piece);
            final pieceInset = squareSide * 0.055;

            Widget pieceImage() => Padding(
              padding: EdgeInsets.all(pieceInset),
              child: SvgPicture.asset(
                pieceAsset!,
                fit: BoxFit.contain,
                alignment: Alignment.center,
              ),
            );

            final overlay = squareOverlay?.call(square, squareSide);
            final isTarget = moveTargets.contains(square);

            return DragTarget<String>(
              onWillAcceptWithDetails: (details) => details.data != square,
              onAcceptWithDetails: (details) =>
                  onPieceDrop(details.data, square),
              builder: (context, candidateData, rejectedData) {
                final targetColor = candidateData.isNotEmpty
                    ? Color.alphaBlend(
                        Theme.of(context).colorScheme.secondary
                            .withValues(alpha: 0.34),
                        squareColor,
                      )
                    : squareColor;
                return InkWell(
                  key: Key('board-square-$square'),
                  onTap: () => onSquareTap(square),
                  child: ColoredBox(
                    color: targetColor,
                    child: Stack(
                      fit: StackFit.expand,
                      children: [
                        // Under the piece, so a capture ring frames it
                        // rather than covering it.
                        if (isTarget)
                          Positioned.fill(
                            child: IgnorePointer(
                              child: _MoveTargetMarker(
                                occupied: pieceAsset != null,
                                squareSide: squareSide,
                              ),
                            ),
                          ),
                        if (pieceAsset != null)
                          Positioned.fill(
                            child: canDrag
                                ? Draggable<String>(
                                    data: square,
                                    onDragStarted: () =>
                                        onDragStarted?.call(square),
                                    onDragEnd: (_) => onDragEnded?.call(),
                                    feedback: Material(
                                      color: Colors.transparent,
                                      child: SizedBox.square(
                                        dimension: squareSide,
                                        child: pieceImage(),
                                      ),
                                    ),
                                    childWhenDragging: const SizedBox.expand(),
                                    child: pieceImage(),
                                  )
                                : pieceImage(),
                          ),
                        ?overlay,
                        if (showCoordinates && column == 0)
                          Positioned(
                            left: 3,
                            top: 2,
                            child: Text(
                              '$rank',
                              textDirection: TextDirection.ltr,
                              style: TextStyle(
                                color: coordinateColor,
                                fontSize: 10,
                                fontWeight: FontWeight.w700,
                              ),
                            ),
                          ),
                        if (showCoordinates && row == 7)
                          Positioned(
                            right: 3,
                            bottom: 1,
                            child: Text(
                              String.fromCharCode(
                                'a'.codeUnitAt(0) + fileIndex,
                              ),
                              textDirection: TextDirection.ltr,
                              style: TextStyle(
                                color: coordinateColor,
                                fontSize: 10,
                                fontWeight: FontWeight.w700,
                              ),
                            ),
                          ),
                      ],
                    ),
                  ),
                );
              },
            );
          },
        );
      },
    );
  }
}

/// The dot or ring marking a square the selected piece can reach.
class _MoveTargetMarker extends StatelessWidget {
  const _MoveTargetMarker({required this.occupied, required this.squareSide});

  /// A capture is drawn as a ring around the piece; an empty square as a dot.
  final bool occupied;
  final double squareSide;

  @override
  Widget build(BuildContext context) {
    // Dark enough to read on both the light and the dark square colour, which
    // a theme colour would not manage on the board's own fixed palette.
    const marker = Color(0x66000000);
    if (occupied) {
      return Padding(
        padding: EdgeInsets.all(squareSide * 0.04),
        child: DecoratedBox(
          decoration: BoxDecoration(
            shape: BoxShape.circle,
            border: Border.all(color: marker, width: squareSide * 0.09),
          ),
        ),
      );
    }
    return Center(
      child: SizedBox.square(
        dimension: squareSide * 0.3,
        child: const DecoratedBox(
          decoration: BoxDecoration(color: marker, shape: BoxShape.circle),
        ),
      ),
    );
  }
}
