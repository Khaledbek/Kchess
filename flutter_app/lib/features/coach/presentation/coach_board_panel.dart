import 'dart:math' as math;

import 'package:flutter/material.dart';

import '../../../localization/generated/app_localizations.dart';
import '../../../shared/models/models.dart';
import '../../../shared/widgets/chess_board_view.dart';
import '../../../shared/widgets/evaluation_bar.dart';
import '../../analysis/presentation/analysis_move_arrow.dart';
import 'coach_board_focus.dart';

// -----------------------------------------------------------------------------
// Coach board panel
// -----------------------------------------------------------------------------

class CoachBoardPanel extends StatelessWidget {
  const CoachBoardPanel({
    required this.position,
    required this.blackAtBottom,
    required this.playerColor,
    required this.onRotate,
    required this.onPlayerColorChanged,
    required this.onSquareTap,
    required this.onPieceDrop,
    this.selectedSquare,
    this.hintSourceSquares = const <String>{},
    this.hintTargetSquares = const <String>{},
    this.arrowMoves = const <String>[],
    this.evaluationLine,
    this.classification,
    this.classificationSquare,
    this.interactive = true,
    super.key,
  });

  final BoardPosition position;
  final bool blackAtBottom;
  final String playerColor;
  final VoidCallback onRotate;
  final ValueChanged<String> onPlayerColorChanged;
  final ValueChanged<String> onSquareTap;
  final void Function(String source, String target) onPieceDrop;
  final String? selectedSquare;
  final Set<String> hintSourceSquares;
  final Set<String> hintTargetSquares;
  final List<String> arrowMoves;
  final EngineLine? evaluationLine;
  final MoveClassification? classification;
  final String? classificationSquare;
  final bool interactive;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final theme = Theme.of(context);

    return ColoredBox(
      color: theme.colorScheme.surface,
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Padding(
            padding: const EdgeInsets.fromLTRB(16, 8, 8, 4),
            child: Row(
              children: [
                Expanded(
                  child: Text(
                    strings.coachBoardContext,
                    style: theme.textTheme.titleMedium,
                  ),
                ),
                SegmentedButton<String>(
                  segments: [
                    ButtonSegment(
                      value: 'white',
                      label: Text(strings.coachPlayerWhite),
                    ),
                    ButtonSegment(
                      value: 'black',
                      label: Text(strings.coachPlayerBlack),
                    ),
                  ],
                  selected: <String>{playerColor},
                  showSelectedIcon: false,
                  onSelectionChanged: (selection) {
                    if (selection.isNotEmpty) {
                      onPlayerColorChanged(selection.first);
                    }
                  },
                ),
                IconButton(
                  key: const Key('coach-rotate-board'),
                  onPressed: onRotate,
                  tooltip: strings.rotateBoard,
                  icon: const Icon(Icons.rotate_90_degrees_ccw_outlined),
                ),
              ],
            ),
          ),
          Expanded(
            child: LayoutBuilder(
              builder: (context, constraints) {
                const evalHeight = 22.0;
                const evalGap = 6.0;
                final side = math
                    .max(
                      0.0,
                      math.min(
                        constraints.maxWidth - 24,
                        constraints.maxHeight - evalHeight - evalGap - 12,
                      ),
                    )
                    .toDouble();
                return Center(
                  child: SizedBox(
                    width: side,
                    height: side + evalHeight + evalGap,
                    child: Column(
                      children: [
                        SizedBox(
                          height: evalHeight,
                          width: side,
                          child: EvaluationBar(line: evaluationLine),
                        ),
                        const SizedBox(height: evalGap),
                        SizedBox.square(
                          dimension: side,
                          child: Stack(
                            fit: StackFit.expand,
                            children: [
                              ChessBoardView(
                                position: position,
                                blackAtBottom: blackAtBottom,
                                interactive: interactive,
                                onSquareTap: onSquareTap,
                                onPieceDrop: onPieceDrop,
                                squareTint: (square, base) {
                                  if (hintTargetSquares.contains(square)) {
                                    return Color.alphaBlend(
                                      theme.colorScheme.tertiary.withValues(
                                        alpha: 0.42,
                                      ),
                                      base,
                                    );
                                  }
                                  if (hintSourceSquares.contains(square)) {
                                    return Color.alphaBlend(
                                      theme.colorScheme.primary.withValues(
                                        alpha: 0.38,
                                      ),
                                      base,
                                    );
                                  }
                                  if (square == selectedSquare) {
                                    return Color.alphaBlend(
                                      theme.colorScheme.secondary.withValues(
                                        alpha: 0.36,
                                      ),
                                      base,
                                    );
                                  }
                                  return base;
                                },
                              ),
                              CoachBoardFocus(
                                squares: {
                                  ...hintSourceSquares,
                                  ...hintTargetSquares,
                                },
                                blackAtBottom: blackAtBottom,
                                color: theme.colorScheme.primary,
                              ),
                              for (
                                var index = 0;
                                index < math.min(2, arrowMoves.length);
                                index++
                              )
                                if (arrowMoves[index].length >= 4)
                                  AnalysisMoveArrow(
                                    paintKey: Key('coach-best-arrow-$index'),
                                    move: arrowMoves[index],
                                    color: index == 0
                                        ? theme.colorScheme.tertiary
                                        : theme.colorScheme.primary,
                                    blackAtBottom: blackAtBottom,
                                  ),
                              if (classification != null &&
                                  classification !=
                                      MoveClassification.unknown &&
                                  classificationSquare != null)
                                _ClassificationBadge(
                                  square: classificationSquare!,
                                  classification: classification!,
                                  boardSide: side,
                                  blackAtBottom: blackAtBottom,
                                ),
                            ],
                          ),
                        ),
                      ],
                    ),
                  ),
                );
              },
            ),
          ),
        ],
      ),
    );
  }
}

// -----------------------------------------------------------------------------
// Classification overlay
// -----------------------------------------------------------------------------

class _ClassificationBadge extends StatelessWidget {
  const _ClassificationBadge({
    required this.square,
    required this.classification,
    required this.boardSide,
    required this.blackAtBottom,
  });

  final String square;
  final MoveClassification classification;
  final double boardSide;
  final bool blackAtBottom;

  @override
  Widget build(BuildContext context) {
    if (square.length < 2) return const SizedBox.shrink();
    final file = square.codeUnitAt(0) - 'a'.codeUnitAt(0);
    final rank = int.tryParse(square.substring(1, 2));
    if (file < 0 || file > 7 || rank == null || rank < 1 || rank > 8) {
      return const SizedBox.shrink();
    }
    final column = blackAtBottom ? 7 - file : file;
    final row = blackAtBottom ? rank - 1 : 8 - rank;
    final squareSide = boardSide / 8;
    final badgeSide = squareSide * 0.42;
    final asset = _classificationAsset(classification);
    if (asset == null) return const SizedBox.shrink();
    return Positioned(
      left: column * squareSide + squareSide - badgeSide - 2,
      top: row * squareSide + 2,
      width: badgeSide,
      height: badgeSide,
      child: IgnorePointer(child: Image.asset(asset, fit: BoxFit.contain)),
    );
  }
}

String? _classificationAsset(MoveClassification classification) =>
    switch (classification) {
      MoveClassification.theory => 'assets/analysis_img/move_book.png',
      MoveClassification.forced => 'assets/analysis_img/move_force.png',
      MoveClassification.brilliant => 'assets/analysis_img/move_brilliant.png',
      MoveClassification.critical => 'assets/analysis_img/move_great.png',
      MoveClassification.best => 'assets/analysis_img/move_best.png',
      MoveClassification.excellent => 'assets/analysis_img/move_excellent.png',
      MoveClassification.good => 'assets/analysis_img/move_okay.png',
      MoveClassification.okay => 'assets/analysis_img/move_okay.png',
      MoveClassification.miss => 'assets/analysis_img/move_miss.png',
      MoveClassification.mistake => 'assets/analysis_img/move_mistake.png',
      MoveClassification.blunder => 'assets/analysis_img/move_blunder.png',
      MoveClassification.unknown => null,
    };
