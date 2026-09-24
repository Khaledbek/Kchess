// -----------------------------------------------------------------------------
// Section: Temporary bot game launched from the analysis board
// -----------------------------------------------------------------------------

import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_svg/flutter_svg.dart';

import '../../../ffi/core_gateway.dart';
import '../../../localization/generated/app_localizations.dart';
import '../../../shared/models/models.dart';
import '../../../ui/shared/board_endgame_presentation.dart';
import '../../../ui/shared/promotion_dialog.dart';

class AnalysisTemporaryBotGameScreen extends StatefulWidget {
  const AnalysisTemporaryBotGameScreen({
    required this.gateway,
    required this.initialPosition,
    required this.playerColor,
    required this.botElo,
    super.key,
  });

  final CoreGateway gateway;
  final BoardPosition initialPosition;
  final String playerColor;
  final int botElo;

  @override
  State<AnalysisTemporaryBotGameScreen> createState() =>
      _AnalysisTemporaryBotGameScreenState();
}

class _AnalysisTemporaryBotGameScreenState
    extends State<AnalysisTemporaryBotGameScreen> {
  late BoardPosition _position;
  String? _selectedSquare;
  String? _lastMoveUci;
  String? _activeBotJobId;
  Object? _error;
  bool _botThinking = false;
  bool _humanMovePending = false;
  bool _gameFinished = false;
  String _gameResult = '*';
  bool _gameCheckmate = false;

  String get _botColor => widget.playerColor == 'white' ? 'black' : 'white';
  bool get _isHumanTurn =>
      !_gameFinished && _position.sideToMove == widget.playerColor;
  bool get _inputLocked =>
      _gameFinished || _botThinking || _humanMovePending || !_isHumanTurn;

  @override
  void initState() {
    super.initState();
    _position = widget.initialPosition;
    if (_position.sideToMove == _botColor) {
      WidgetsBinding.instance.addPostFrameCallback((_) {
        if (mounted) unawaited(_requestBotMove());
      });
    }
  }

  @override
  void dispose() {
    final jobId = _activeBotJobId;
    if (jobId != null) {
      unawaited(widget.gateway.cancelBotMove(jobId));
    }
    super.dispose();
  }

  String? _pieceAtSquare(String square) {
    if (square.length != 2) return null;
    final file = square.codeUnitAt(0) - 'a'.codeUnitAt(0);
    final rank = square.codeUnitAt(1) - '0'.codeUnitAt(0);
    if (file < 0 || file > 7 || rank < 1 || rank > 8) return null;
    final index = (8 - rank) * 8 + file;
    if (index < 0 || index >= _position.pieces.length) return null;
    return _position.pieces[index];
  }

  bool _pieceBelongsToPlayer(String piece) {
    if (piece.isEmpty) return false;
    final isWhite = piece == piece.toUpperCase();
    return isWhite == (widget.playerColor == 'white');
  }

  Future<void> _onSquareTap(String square) async {
    if (_inputLocked) return;
    final source = _selectedSquare;
    if (source == null) {
      final piece = _pieceAtSquare(square) ?? '';
      if (!_pieceBelongsToPlayer(piece)) return;
      setState(() {
        _selectedSquare = square;
        _error = null;
      });
      return;
    }
    if (source == square) {
      setState(() => _selectedSquare = null);
      return;
    }
    final targetPiece = _pieceAtSquare(square) ?? '';
    if (_pieceBelongsToPlayer(targetPiece)) {
      setState(() => _selectedSquare = square);
      return;
    }
    await _playHumanMove(source, square);
  }

  Future<void> _onPieceDrop(String source, String target) async {
    if (_inputLocked || source == target) return;
    await _playHumanMove(source, target);
  }

  Future<void> _playHumanMove(String source, String target) async {
    if (_inputLocked) return;
    try {
      final promotionOptions = await widget.gateway.boardPromotionOptions(
        fen: _position.fen,
        source: source,
        target: target,
      );
      var resolvedTarget = target;
      if (promotionOptions.isNotEmpty) {
        final promotion = await showPromotionChoiceDialog(
          context: context,
          options: promotionOptions,
          sideToMove: _position.sideToMove,
        );
        if (!mounted || promotion == null) {
          if (mounted) setState(() => _selectedSquare = null);
          return;
        }
        resolvedTarget = '$target$promotion';
      }
      if (!mounted) return;
      setState(() {
        _humanMovePending = true;
        _selectedSquare = null;
        _error = null;
      });
      final resolved = await widget.gateway.resolveFreeBoardMove(
        fen: _position.fen,
        source: source,
        target: resolvedTarget,
      );
      if (!mounted) return;
      final terminal = resolved.terminal;
      setState(() {
        _position = resolved.positionAfter;
        _lastMoveUci = resolved.uci;
        _humanMovePending = false;
        _gameFinished = terminal;
        _gameResult = resolved.result;
        _gameCheckmate = resolved.checkmate;
      });
      if (!terminal) await _requestBotMove();
    } catch (error) {
      if (!mounted) return;
      setState(() {
        _humanMovePending = false;
        _error = error;
      });
    }
  }

  Future<void> _requestBotMove() async {
    if (!mounted || _gameFinished || _position.sideToMove != _botColor) return;
    final requestedFen = _position.fen;
    setState(() {
      _botThinking = true;
      _error = null;
    });
    try {
      var snapshot = await widget.gateway.startBotMove(
        fen: requestedFen,
        requestedElo: widget.botElo,
      );
      _activeBotJobId = snapshot.jobId;
      while (mounted && snapshot.isRunning) {
        await Future<void>.delayed(const Duration(milliseconds: 120));
        if (!mounted) return;
        snapshot = await widget.gateway.botMoveStatus(snapshot.jobId);
      }
      if (!mounted) return;
      _activeBotJobId = null;
      if (snapshot.isFailed) {
        throw CoreGatewayException(
          snapshot.error ?? AppLocalizations.of(context).botMoveFailed,
        );
      }
      if (_position.fen != requestedFen) {
        setState(() => _botThinking = false);
        return;
      }
      final positionAfter = snapshot.positionAfter;
      if (!snapshot.isComplete || snapshot.move.isEmpty || positionAfter == null) {
        setState(() {
          _botThinking = false;
          _gameFinished = snapshot.terminal || snapshot.isComplete;
          _gameResult = snapshot.result;
          _gameCheckmate = snapshot.checkmate;
        });
        return;
      }
      setState(() {
        _position = positionAfter;
        _lastMoveUci = snapshot.move;
        _botThinking = false;
        _gameFinished = snapshot.terminal;
        _gameResult = snapshot.result;
        _gameCheckmate = snapshot.checkmate;
      });
    } catch (error) {
      if (!mounted) return;
      _activeBotJobId = null;
      setState(() {
        _botThinking = false;
        _error = error;
      });
    }
  }

  String _statusText(AppLocalizations strings) {
    if (_gameFinished) return strings.botGameFinished;
    if (_botThinking) return strings.botThinking;
    if (_humanMovePending) return strings.botApplyingMove;
    return _isHumanTurn ? strings.botYourTurn : strings.botWaiting;
  }

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final theme = Theme.of(context);
    final playerLabel = widget.playerColor == 'white'
        ? strings.whitePlayer
        : strings.blackPlayer;

    return Scaffold(
      appBar: AppBar(
        title: Text(strings.continueAgainstBot),
        actions: [
          IconButton(
            key: const Key('temporary-bot-close'),
            tooltip: strings.close,
            onPressed: () => Navigator.of(context).pop(),
            icon: const Icon(Icons.close_rounded),
          ),
        ],
      ),
      body: SafeArea(
        child: LayoutBuilder(
          builder: (context, constraints) {
            final maxBoard = constraints.maxWidth >= 760
                ? constraints.maxHeight - 48
                : constraints.maxWidth - 24;
            final boardSide = maxBoard.clamp(260.0, 720.0).toDouble();
            final info = Column(
              mainAxisSize: MainAxisSize.min,
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                Card(
                  child: Padding(
                    padding: const EdgeInsets.all(16),
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Text(
                          _statusText(strings),
                          key: const Key('temporary-bot-status'),
                          style: theme.textTheme.titleMedium?.copyWith(
                            fontWeight: FontWeight.w800,
                          ),
                        ),
                        const SizedBox(height: 8),
                        Text('${strings.botYou}: $playerLabel'),
                        Text('${strings.stockfish18} · ${strings.botElo} ${widget.botElo}'),
                        const SizedBox(height: 10),
                        Text(
                          strings.temporaryBotGameNotSaved,
                          style: theme.textTheme.bodySmall,
                        ),
                      ],
                    ),
                  ),
                ),
                if (_error != null) ...[
                  const SizedBox(height: 10),
                  Text(
                    _error.toString(),
                    style: TextStyle(color: theme.colorScheme.error),
                  ),
                ],
                const SizedBox(height: 14),
                OutlinedButton.icon(
                  key: const Key('temporary-bot-abort'),
                  onPressed: () => Navigator.of(context).pop(),
                  icon: const Icon(Icons.stop_circle_outlined),
                  label: Text(strings.cancelAction),
                ),
              ],
            );

            final board = SizedBox.square(
              dimension: boardSide,
              child: _TemporaryBotBoard(
                position: _position,
                playerColor: widget.playerColor,
                selectedSquare: _selectedSquare,
                lastMoveUci: _lastMoveUci,
                endgameResult: _gameResult,
                endgameCheckmate: _gameCheckmate,
                showEndgame: _gameFinished,
                enabled: !_inputLocked,
                onSquareTap: _onSquareTap,
                onPieceDrop: _onPieceDrop,
              ),
            );

            if (constraints.maxWidth >= 760) {
              return Padding(
                padding: const EdgeInsets.all(20),
                child: Row(
                  crossAxisAlignment: CrossAxisAlignment.center,
                  children: [
                    Expanded(child: Center(child: board)),
                    const SizedBox(width: 20),
                    SizedBox(width: 300, child: info),
                  ],
                ),
              );
            }

            return ListView(
              padding: const EdgeInsets.fromLTRB(12, 12, 12, 24),
              children: [
                Center(child: board),
                const SizedBox(height: 14),
                info,
              ],
            );
          },
        ),
      ),
    );
  }
}

class _TemporaryBotBoard extends StatelessWidget {
  const _TemporaryBotBoard({
    required this.position,
    required this.playerColor,
    required this.selectedSquare,
    required this.lastMoveUci,
    required this.endgameResult,
    required this.endgameCheckmate,
    required this.showEndgame,
    required this.enabled,
    required this.onSquareTap,
    required this.onPieceDrop,
  });

  final BoardPosition position;
  final String playerColor;
  final String? selectedSquare;
  final String? lastMoveUci;
  final String endgameResult;
  final bool endgameCheckmate;
  final bool showEndgame;
  final bool enabled;
  final ValueChanged<String> onSquareTap;
  final void Function(String source, String target) onPieceDrop;

  static const _pieceAssets = <String, String>{
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

  int _boardIndex(int displayIndex) =>
      playerColor == 'black' ? 63 - displayIndex : displayIndex;

  String _squareName(int boardIndex) {
    final file = String.fromCharCode('a'.codeUnitAt(0) + boardIndex % 8);
    final rank = 8 - boardIndex ~/ 8;
    return '$file$rank';
  }

  bool _isLastMoveSquare(String square) {
    final move = lastMoveUci;
    if (move == null || move.length < 4) return false;
    return move.substring(0, 2) == square || move.substring(2, 4) == square;
  }

  bool _canDrag(String piece) {
    if (!enabled || piece.isEmpty) return false;
    final isWhite = piece == piece.toUpperCase();
    return isWhite == (playerColor == 'white');
  }

  @override
  Widget build(BuildContext context) {
    final scheme = Theme.of(context).colorScheme;
    final endgameVisible =
        showEndgame && endgameResult.isNotEmpty && endgameResult != '*';
    final whiteKingSquare = boardKingSquareForColor(position.pieces, 'white');
    final blackKingSquare = boardKingSquareForColor(position.pieces, 'black');

    return DecoratedBox(
      decoration: BoxDecoration(
        border: Border.all(color: scheme.outlineVariant),
        borderRadius: BorderRadius.circular(10),
        boxShadow: const [
          BoxShadow(
            blurRadius: 8,
            offset: Offset(0, 2),
            color: Color(0x24000000),
          ),
        ],
      ),
      child: ClipRRect(
        borderRadius: BorderRadius.circular(9),
        child: LayoutBuilder(
          builder: (context, constraints) {
            final boardSide = constraints.maxWidth;
            return Stack(
              fit: StackFit.expand,
              children: [
                GridView.builder(
                  physics: const NeverScrollableScrollPhysics(),
                  gridDelegate:
                      const SliverGridDelegateWithFixedCrossAxisCount(
                        crossAxisCount: 8,
                      ),
                  itemCount: 64,
                  itemBuilder: (context, displayIndex) {
                    final boardIndex = _boardIndex(displayIndex);
                    final square = _squareName(boardIndex);
                    final piece = boardIndex < position.pieces.length
                        ? position.pieces[boardIndex]
                        : '';
                    final row = displayIndex ~/ 8;
                    final column = displayIndex % 8;
                    final light =
                        ((boardIndex ~/ 8) + (boardIndex % 8)).isEven;
                    final selected = selectedSquare == square;
                    final lastMove = _isLastMoveSquare(square);
                    final endgameKingDestination =
                        endgameVisible &&
                        lastMoveUci != null &&
                        lastMoveUci!.length >= 4 &&
                        lastMoveUci!.substring(2, 4) == square &&
                        (square == whiteKingSquare ||
                            square == blackKingSquare);
                    final base = light
                        ? const Color(0xFFD4D1C9)
                        : const Color(0xFF60736C);
                    final squareColor = endgameKingDestination
                        ? base
                        : selected
                        ? scheme.primaryContainer.withValues(alpha: 0.90)
                        : lastMove
                        ? const Color(0xFF82A9C5)
                        : base;

                    Widget pieceWidget() {
                      final asset = _pieceAssets[piece];
                      if (asset == null) return const SizedBox.shrink();
                      return Padding(
                        padding: const EdgeInsets.all(5),
                        child: SvgPicture.asset(asset, fit: BoxFit.contain),
                      );
                    }

                    final content = _canDrag(piece)
                        ? Draggable<String>(
                            data: square,
                            feedback: SizedBox.square(
                              dimension: 56,
                              child: pieceWidget(),
                            ),
                            childWhenDragging: const SizedBox.shrink(),
                            child: pieceWidget(),
                          )
                        : pieceWidget();

                    return DragTarget<String>(
                      onWillAcceptWithDetails: (_) => enabled,
                      onAcceptWithDetails: (details) =>
                          onPieceDrop(details.data, square),
                      builder: (context, candidates, rejected) =>
                          GestureDetector(
                            key: Key('temporary-bot-square-$square'),
                            behavior: HitTestBehavior.opaque,
                            onTap: enabled ? () => onSquareTap(square) : null,
                            child: ColoredBox(
                              color: candidates.isNotEmpty
                                  ? scheme.primaryContainer
                                  : squareColor,
                              child: Stack(
                                fit: StackFit.expand,
                                children: [
                                  Center(child: content),
                                  if (column == 0)
                                    Positioned(
                                      left: 3,
                                      top: 2,
                                      child: Text(
                                        square.substring(1),
                                        style: TextStyle(
                                          fontSize: 10,
                                          fontWeight: FontWeight.w700,
                                          color: light
                                              ? const Color(0xFF6D533D)
                                              : const Color(0xFFD9D6CE),
                                        ),
                                      ),
                                    ),
                                  if (row == 7)
                                    Positioned(
                                      right: 3,
                                      bottom: 1,
                                      child: Text(
                                        square.substring(0, 1),
                                        style: TextStyle(
                                          fontSize: 10,
                                          fontWeight: FontWeight.w700,
                                          color: light
                                              ? const Color(0xFF6D533D)
                                              : const Color(0xFFD9D6CE),
                                        ),
                                      ),
                                    ),
                                ],
                              ),
                            ),
                          ),
                    );
                  },
                ),
                BoardEndgameLayer(
                  visible: endgameVisible,
                  result: endgameResult,
                  checkmate: endgameCheckmate,
                  pieces: position.pieces,
                  boardSide: boardSide,
                  blackAtBottom: playerColor == 'black',
                  useGiveupForLoss: false,
                ),
              ],
            );
          },
        ),
      ),
    );
  }
}

