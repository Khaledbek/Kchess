import 'package:flutter/material.dart';

import '../../../localization/generated/app_localizations.dart';
import '../../../shared/models/models.dart';
import '../models/coach_ui_models.dart';
import 'coach_board_controls.dart';
import 'coach_board_panel.dart';
import 'coach_input_bar.dart';
import 'coach_output_panel.dart';

// -----------------------------------------------------------------------------
// Coach screen
// -----------------------------------------------------------------------------

class CoachScreen extends StatefulWidget {
  const CoachScreen({
    this.position = BoardPosition.empty,
    this.blackAtBottom = false,
    this.playerColor = 'white',
    this.messages = const <CoachUiMessage>[],
    this.isLoading = false,
    this.errorMessage,
    this.depth = CoachResponseDepth.balanced,
    this.onSubmit,
    this.onShowBoard,
    this.onCompare,
    this.onQuiz,
    this.onHintRequested,
    this.onDepthChanged,
    this.onFenRequested,
    this.onPgnRequested,
    this.onPlayerColorChanged,
    this.onBoardSquareTap,
    this.onBoardPieceDrop,
    this.selectedSquare,
    this.hintSourceSquares = const <String>{},
    this.hintTargetSquares = const <String>{},
    this.arrowMoves = const <String>[],
    this.evaluationLine,
    this.classification,
    this.classificationSquare,
    this.boardBusy = false,
    this.hintEnabled = true,
    this.hintBusy = false,
    this.showPgnControls = false,
    this.onFirstMove,
    this.onPreviousMove,
    this.onNextMove,
    this.onLastMove,
    this.showAppBar = true,
    this.onExit,
    super.key,
  });

  final BoardPosition position;
  final bool blackAtBottom;
  final String playerColor;
  final List<CoachUiMessage> messages;
  final bool isLoading;
  final String? errorMessage;
  final CoachResponseDepth depth;
  final ValueChanged<String>? onSubmit;
  final ValueChanged<CoachUiMessage>? onShowBoard;
  final VoidCallback? onCompare;
  final VoidCallback? onQuiz;
  final VoidCallback? onHintRequested;
  final ValueChanged<CoachResponseDepth>? onDepthChanged;
  final VoidCallback? onFenRequested;
  final VoidCallback? onPgnRequested;
  final ValueChanged<String>? onPlayerColorChanged;
  final ValueChanged<String>? onBoardSquareTap;
  final void Function(String source, String target)? onBoardPieceDrop;
  final String? selectedSquare;
  final Set<String> hintSourceSquares;
  final Set<String> hintTargetSquares;
  final List<String> arrowMoves;
  final EngineLine? evaluationLine;
  final MoveClassification? classification;
  final String? classificationSquare;
  final bool boardBusy;
  final bool hintEnabled;
  final bool hintBusy;
  final bool showPgnControls;
  final VoidCallback? onFirstMove;
  final VoidCallback? onPreviousMove;
  final VoidCallback? onNextMove;
  final VoidCallback? onLastMove;
  final bool showAppBar;
  final VoidCallback? onExit;

  @override
  State<CoachScreen> createState() => _CoachScreenState();
}

class _CoachScreenState extends State<CoachScreen> {
  late bool _blackAtBottom;

  @override
  void initState() {
    super.initState();
    _blackAtBottom = widget.blackAtBottom;
  }

  @override
  void didUpdateWidget(CoachScreen oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.blackAtBottom != widget.blackAtBottom) {
      _blackAtBottom = widget.blackAtBottom;
    }
  }

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final showBar = widget.showAppBar || widget.onExit != null;

    return Scaffold(
      appBar: showBar
          ? AppBar(
              leading: widget.onExit == null
                  ? null
                  : IconButton(
                      onPressed: widget.onExit,
                      icon: const Icon(Icons.arrow_back_rounded),
                    ),
              title: Text(strings.coach),
            )
          : null,
      body: LayoutBuilder(
        builder: (context, constraints) {
          final board = CoachBoardPanel(
            position: widget.position,
            blackAtBottom: _blackAtBottom,
            playerColor: widget.playerColor,
            onRotate: () => setState(() => _blackAtBottom = !_blackAtBottom),
            onPlayerColorChanged: widget.onPlayerColorChanged ?? (_) {},
            interactive: !widget.boardBusy && widget.onBoardPieceDrop != null,
            selectedSquare: widget.selectedSquare,
            hintSourceSquares: widget.hintSourceSquares,
            hintTargetSquares: widget.hintTargetSquares,
            arrowMoves: widget.arrowMoves,
            evaluationLine: widget.evaluationLine,
            classification: widget.classification,
            classificationSquare: widget.classificationSquare,
            onSquareTap: widget.onBoardSquareTap ?? (_) {},
            onPieceDrop: widget.onBoardPieceDrop ?? (_, _) {},
          );
          final boardColumn = Column(
            children: [
              Expanded(child: board),
              if (widget.showPgnControls)
                CoachBoardControls(
                  onFirst: widget.onFirstMove,
                  onPrevious: widget.onPreviousMove,
                  onNext: widget.onNextMove,
                  onLast: widget.onLastMove,
                ),
            ],
          );
          final trainer = Column(
            children: [
              Expanded(
                child: CoachOutputPanel(
                  messages: widget.messages,
                  isLoading: widget.isLoading,
                  errorMessage: widget.errorMessage,
                  onShowBoard: widget.onShowBoard,
                ),
              ),
              const Divider(height: 1),
              Padding(
                padding: const EdgeInsets.symmetric(
                  horizontal: 12,
                  vertical: 4,
                ),
                child: Wrap(
                  spacing: 8,
                  children: [
                    TextButton.icon(
                      onPressed: widget.isLoading || widget.hintBusy
                          ? null
                          : widget.onCompare,
                      icon: const Icon(Icons.compare_arrows, size: 18),
                      label: Text(
                        AppLocalizations.of(context).coachCompareMoves,
                      ),
                    ),
                    TextButton.icon(
                      onPressed: widget.isLoading || widget.hintBusy
                          ? null
                          : widget.onQuiz,
                      icon: const Icon(Icons.psychology_outlined, size: 18),
                      label: Text(AppLocalizations.of(context).coachChallenge),
                    ),
                  ],
                ),
              ),
              CoachInputBar(
                depth: widget.depth,
                isLoading: widget.isLoading,
                onSubmit: widget.onSubmit,
                onHintRequested: widget.onHintRequested,
                onDepthChanged: widget.onDepthChanged,
                onFenRequested: widget.onFenRequested,
                onPgnRequested: widget.onPgnRequested,
                hintEnabled: widget.hintEnabled,
                hintBusy: widget.hintBusy,
              ),
            ],
          );

          if (constraints.maxWidth >= 900) {
            return Row(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                Flexible(flex: 68, child: SizedBox.expand(child: boardColumn)),
                const VerticalDivider(width: 1),
                Flexible(flex: 32, child: trainer),
              ],
            );
          }

          final boardHeight = constraints.maxHeight * 0.58;
          return Column(
            children: [
              SizedBox(
                height: boardHeight,
                width: double.infinity,
                child: boardColumn,
              ),
              const Divider(height: 1),
              Expanded(child: trainer),
            ],
          );
        },
      ),
    );
  }
}
