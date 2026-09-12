part of '../../../ui/app_root.dart';

// -----------------------------------------------------------------------------
// Section: Ephemeral local bot game state
// -----------------------------------------------------------------------------

class BotGameScreen extends StatefulWidget {
  const BotGameScreen({
    required this.gateway,
    required this.botElo,
    this.gameId,
    super.key,
  });

  final CoreGateway gateway;
  final int botElo;
  final String? gameId;

  @override
  State<BotGameScreen> createState() => _BotGameScreenState();
}

class _BotGameScreenState extends State<BotGameScreen> {
  BotGameSession? _start;
  final List<BoardPosition> _positions = [];
  final List<_BotPlayedMove> _moves = [];
  int _viewPly = 0;
  String? _selectedSquare;
  String? _activeBotJobId;
  String? _activeHintJobId;
  String? _activeEvaluationJobId;
  String? _hintMoveUci;
  EngineLine? _evaluationLine;
  String? _evaluationFen;
  Object? _error;
  bool _loading = true;
  bool _botThinking = false;
  bool _humanMovePending = false;
  bool _hintThinking = false;
  bool _evaluationThinking = false;
  bool _gameFinished = false;
  int _hintsUsed = 0;

  @override
  void initState() {
    super.initState();
    unawaited(_loadGame());
  }

  @override
  void dispose() {
    final jobId = _activeBotJobId;
    if (jobId != null) {
      unawaited(widget.gateway.cancelBotMove(jobId));
    }
    final hintJobId = _activeHintJobId;
    if (hintJobId != null) {
      unawaited(widget.gateway.cancelBotMove(hintJobId));
    }
    final evaluationJobId = _activeEvaluationJobId;
    if (evaluationJobId != null) {
      unawaited(widget.gateway.cancelBotMove(evaluationJobId));
    }
    super.dispose();
  }

  Future<void> _loadGame() async {
    try {
      final start = widget.gameId == null
          ? await widget.gateway.createBotGame(widget.botElo)
          : await widget.gateway.botGame(widget.gameId!);
      if (!mounted) return;
      setState(() {
        _start = start;
        _positions
          ..clear()
          ..addAll(start.positions.isEmpty ? [start.position] : start.positions);
        _moves
          ..clear()
          ..addAll(
            start.moves.map(
              (move) => _BotPlayedMove(uci: move.uci, san: move.san),
            ),
          );
        _viewPly = _positions.length - 1;
        _selectedSquare = null;
        _hintMoveUci = null;
        _hintsUsed = 0;
        _hintThinking = false;
        _evaluationThinking = false;
        _evaluationLine = null;
        _evaluationFen = null;
        _gameFinished = !start.isActive;
        _error = null;
        _loading = false;
      });
      if (start.isActive && start.position.sideToMove == start.botColor) {
        await _requestBotMove();
      } else if (start.isActive && start.showEvaluationBar) {
        unawaited(_refreshEvaluation());
      }
    } catch (error) {
      if (!mounted) return;
      setState(() {
        _error = error;
        _loading = false;
      });
    }
  }

  BoardPosition get _livePosition => _positions.last;
  int get _effectiveBotElo => _start?.botElo ?? widget.botElo;
  BoardPosition get _displayPosition => _positions[_viewPly];
  bool get _atLivePosition => _viewPly == _positions.length - 1;
  bool get _inputLocked =>
      _loading ||
      _botThinking ||
      _humanMovePending ||
      _hintThinking ||
      _gameFinished;

  bool get _showEvaluationBar => _start?.showEvaluationBar ?? false;

  bool get _isHumanTurn {
    final start = _start;
    if (start == null || _positions.isEmpty) return false;
    return _displayPosition.sideToMove == start.playerColor;
  }

  String? get _displayLastMoveUci {
    if (_viewPly <= 0 || _viewPly > _moves.length) return null;
    return _moves[_viewPly - 1].uci;
  }

  String? _pieceAtSquare(String square) {
    if (square.length != 2 || _positions.isEmpty) return null;
    final file = square.codeUnitAt(0) - 'a'.codeUnitAt(0);
    final rank = square.codeUnitAt(1) - '0'.codeUnitAt(0);
    if (file < 0 || file > 7 || rank < 1 || rank > 8) return null;
    final index = (8 - rank) * 8 + file;
    final pieces = _displayPosition.pieces;
    if (index < 0 || index >= pieces.length) return null;
    return pieces[index];
  }

  bool _pieceBelongsToPlayer(String piece) {
    final start = _start;
    if (start == null || piece.isEmpty) return false;
    final isWhite = piece == piece.toUpperCase();
    return isWhite == (start.playerColor == 'white');
  }

  Future<void> _onSquareTap(String square) async {
    if (_inputLocked || !_isHumanTurn) return;
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
    if (_inputLocked || !_isHumanTurn || source == target) return;
    await _playHumanMove(source, target);
  }

  Future<void> _playHumanMove(String source, String target) async {
    if (_inputLocked || !_isHumanTurn) return;
    final branchPosition = _displayPosition;
    try {
      final promotionOptions = await widget.gateway.boardPromotionOptions(
        fen: branchPosition.fen,
        source: source,
        target: target,
      );
      var moveUci = '$source$target';
      if (promotionOptions.isNotEmpty) {
        final promotion = await showPromotionChoiceDialog(
          context: context,
          options: promotionOptions,
          sideToMove: branchPosition.sideToMove,
        );
        if (!mounted || promotion == null) {
          if (mounted) setState(() => _selectedSquare = null);
          return;
        }
        moveUci += promotion;
      }
      await _cancelEvaluation();
      if (!mounted) return;
      setState(() {
        _humanMovePending = true;
        _selectedSquare = null;
        _error = null;
      });
      final start = _start!;
      final branchPly = _viewPly;
      final branchFen = branchPosition.fen;
      await widget.gateway.replaceBotGameContinuation(
        gameId: start.gameId,
        basePly: branchPly,
        expectedFenBefore: branchFen,
        uci: moveUci,
      );
      final updatedSession = await widget.gateway.botGame(start.gameId);
      if (!mounted) return;
      setState(() {
        _start = updatedSession;
        _positions
          ..clear()
          ..addAll(
            updatedSession.positions.isEmpty
                ? [updatedSession.position]
                : updatedSession.positions,
          );
        _moves
          ..clear()
          ..addAll(
            updatedSession.moves.map(
              (move) => _BotPlayedMove(uci: move.uci, san: move.san),
            ),
          );
        _viewPly = _positions.length - 1;
        _humanMovePending = false;
        _gameFinished = !updatedSession.isActive;
        _hintMoveUci = null;
        _hintsUsed = 0;
        _hintThinking = false;
      });
      if (updatedSession.isActive) await _requestBotMove();
    } catch (error) {
      if (!mounted) return;
      setState(() {
        _humanMovePending = false;
        _error = error;
      });
    }
  }

  Future<void> _requestHint() async {
    final start = _start;
    if (!mounted ||
        start == null ||
        _gameFinished ||
        !_atLivePosition ||
        !_isHumanTurn ||
        _botThinking ||
        _humanMovePending ||
        _hintThinking ||
        _hintsUsed >= 2) {
      return;
    }

    final cached = _hintMoveUci;
    if (cached != null && cached.length >= 4) {
      setState(() => _hintsUsed += 1);
      return;
    }

    final evaluatedMove = _evaluationFen == _livePosition.fen
        ? _evaluationLine?.bestMove
        : null;
    if (evaluatedMove != null && evaluatedMove.length >= 4) {
      setState(() {
        _hintMoveUci = evaluatedMove;
        _hintsUsed = 1;
      });
      return;
    }
    if (_evaluationThinking) return;

    final requestedFen = _livePosition.fen;
    setState(() {
      _hintThinking = true;
      _error = null;
    });

    try {
      var snapshot = await widget.gateway.startBotMove(
        fen: requestedFen,
        requestedElo: 3200,
      );
      _activeHintJobId = snapshot.jobId;
      while (mounted && snapshot.isRunning) {
        await Future<void>.delayed(const Duration(milliseconds: 120));
        if (!mounted) return;
        snapshot = await widget.gateway.botMoveStatus(snapshot.jobId);
      }
      if (!mounted) return;
      _activeHintJobId = null;

      if (snapshot.isFailed) {
        throw CoreGatewayException(
          snapshot.error ?? AppLocalizations.of(context).botHintFailed,
        );
      }

      // A hint belongs to one exact player position. Never surface a stale
      // engine result after the live board has changed.
      if (_livePosition.fen != requestedFen || !_isHumanTurn) {
        setState(() => _hintThinking = false);
        return;
      }

      final move = snapshot.bestMove.isNotEmpty
          ? snapshot.bestMove
          : snapshot.move;
      if (!snapshot.isComplete || move.length < 4) {
        throw CoreGatewayException(AppLocalizations.of(context).botHintFailed);
      }

      setState(() {
        _hintMoveUci = move;
        _hintsUsed = 1;
        _hintThinking = false;
      });
    } catch (error) {
      if (!mounted) return;
      _activeHintJobId = null;
      setState(() {
        _hintThinking = false;
        _error = error;
      });
    }
  }

  Future<void> _requestBotMove() async {
    final start = _start;
    if (!mounted || start == null || _gameFinished) return;
    if (_livePosition.sideToMove != start.botColor) return;
    setState(() {
      _botThinking = true;
      _error = null;
    });
    try {
      var snapshot = await widget.gateway.startBotMove(
        fen: _livePosition.fen,
        requestedElo: _effectiveBotElo,
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
      if (snapshot.isComplete && snapshot.move.isNotEmpty) {
        final start = _start!;
        final expectedFen = _livePosition.fen;
        final resolved = await widget.gateway.recordBotGameMove(
          gameId: start.gameId,
          expectedFenBefore: expectedFen,
          uci: snapshot.move,
        );
        final updatedSession = await widget.gateway.botGame(start.gameId);
        if (!mounted) return;
        setState(() {
          _start = updatedSession;
          _moves.add(
            _BotPlayedMove(
              uci: resolved.uci,
              san: resolved.san,
            ),
          );
          _positions.add(resolved.positionAfter);
          _viewPly = _positions.length - 1;
          _botThinking = false;
          _gameFinished = !updatedSession.isActive;
          if (updatedSession.showEvaluationBar &&
              snapshot.postMoveEvaluationFen == resolved.fenAfter &&
              (snapshot.postMoveEvaluationCp != null ||
                  snapshot.postMoveMateIn != null ||
                  snapshot.postMoveWdl != null)) {
            _evaluationLine = EngineLine(
              rank: 1,
              depth: 0,
              nodes: 0,
              moves: snapshot.move.isEmpty ? const [] : [snapshot.move],
              evaluationCp: snapshot.postMoveEvaluationCp,
              mateIn: snapshot.postMoveMateIn,
              wdl: snapshot.postMoveWdl,
            );
            _evaluationFen = resolved.fenAfter;
          }
        });
        return;
      }
      setState(() {
        _botThinking = false;
        _gameFinished = snapshot.isComplete;
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

  Future<void> _cancelActiveJobs() async {
    final botJobId = _activeBotJobId;
    _activeBotJobId = null;
    if (botJobId != null) {
      try {
        await widget.gateway.cancelBotMove(botJobId);
      } catch (_) {}
    }
    final hintJobId = _activeHintJobId;
    _activeHintJobId = null;
    if (hintJobId != null) {
      try {
        await widget.gateway.cancelBotMove(hintJobId);
      } catch (_) {}
    }
    await _cancelEvaluation();
  }

  Future<void> _cancelEvaluation() async {
    final jobId = _activeEvaluationJobId;
    _activeEvaluationJobId = null;
    if (jobId != null) {
      try {
        await widget.gateway.cancelBotMove(jobId);
        for (var attempt = 0; attempt < 40; attempt++) {
          final snapshot = await widget.gateway.botMoveStatus(jobId);
          if (!snapshot.isRunning && snapshot.status != 'cancelling') break;
          await Future<void>.delayed(const Duration(milliseconds: 25));
        }
      } catch (_) {}
    }
    if (mounted && _evaluationThinking) {
      setState(() => _evaluationThinking = false);
    }
  }

  Future<void> _refreshEvaluation() async {
    final start = _start;
    if (!mounted || start == null || !start.showEvaluationBar ||
        _gameFinished || !_atLivePosition || !_isHumanTurn ||
        _botThinking || _humanMovePending || _hintThinking ||
        _evaluationThinking) {
      return;
    }
    final requestedFen = _livePosition.fen;
    setState(() => _evaluationThinking = true);
    try {
      var snapshot = await widget.gateway.startBotMove(
        fen: requestedFen,
        requestedElo: 3200,
      );
      _activeEvaluationJobId = snapshot.jobId;
      while (mounted && snapshot.isRunning) {
        await Future<void>.delayed(const Duration(milliseconds: 120));
        if (!mounted) return;
        snapshot = await widget.gateway.botMoveStatus(snapshot.jobId);
      }
      if (!mounted) return;
      _activeEvaluationJobId = null;
      if (!snapshot.isComplete || snapshot.evaluationFen != requestedFen ||
          _livePosition.fen != requestedFen || !_showEvaluationBar) {
        setState(() => _evaluationThinking = false);
        return;
      }
      setState(() {
        _evaluationLine = EngineLine(
          rank: 1,
          depth: 0,
          nodes: 0,
          moves: snapshot.bestMove.isEmpty ? const [] : [snapshot.bestMove],
          evaluationCp: snapshot.evaluationCp,
          mateIn: snapshot.mateIn,
          wdl: snapshot.wdl,
        );
        _evaluationFen = requestedFen;
        _evaluationThinking = false;
      });
    } catch (error) {
      if (!mounted) return;
      _activeEvaluationJobId = null;
      setState(() {
        _evaluationThinking = false;
        _error = error;
      });
    }
  }

  Future<void> _setShowEvaluationBar(bool enabled) async {
    final start = _start;
    if (start == null || start.showEvaluationBar == enabled) return;
    if (!enabled) await _cancelEvaluation();
    try {
      await widget.gateway.setBotGameShowEvaluationBar(start.gameId, enabled);
      final updated = await widget.gateway.botGame(start.gameId);
      if (!mounted) return;
      setState(() {
        _start = updated;
        if (!enabled) {
          _evaluationLine = null;
          _evaluationFen = null;
        }
      });
      if (enabled) unawaited(_refreshEvaluation());
    } catch (error) {
      if (!mounted) return;
      setState(() => _error = error);
    }
  }

  Future<void> _openGameSettings() async {
    final start = _start;
    if (start == null) return;
    await showModalBottomSheet<void>(
      context: context,
      showDragHandle: true,
      builder: (context) => _BotGameSettingsSheet(
        showEvaluationBar: _showEvaluationBar,
        onShowEvaluationBarChanged: (value) async {
          await _setShowEvaluationBar(value);
          if (context.mounted) Navigator.of(context).pop();
        },
      ),
    );
  }

  Future<void> _resignGame() async {
    final start = _start;
    if (start == null || _gameFinished) return;
    final strings = AppLocalizations.of(context);
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        title: Text(strings.statsTerminationResignation),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(context).pop(false),
            child: Text(strings.close),
          ),
          FilledButton(
            onPressed: () => Navigator.of(context).pop(true),
            child: Text(strings.statsTerminationResignation),
          ),
        ],
      ),
    );
    if (confirmed != true || !mounted) return;
    await _cancelActiveJobs();
    try {
      await widget.gateway.resignBotGame(start.gameId);
      final updatedSession = await widget.gateway.botGame(start.gameId);
      if (!mounted) return;
      setState(() {
        _start = updatedSession;
        _gameFinished = true;
        _botThinking = false;
        _hintThinking = false;
        _humanMovePending = false;
        _selectedSquare = null;
        _error = null;
      });
    } catch (error) {
      if (!mounted) return;
      setState(() => _error = error);
    }
  }

  Future<void> _abortGame() async {
    final start = _start;
    if (start == null) return;
    final strings = AppLocalizations.of(context);
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        title: Text(strings.cancelAction),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(context).pop(false),
            child: Text(strings.close),
          ),
          FilledButton(
            onPressed: () => Navigator.of(context).pop(true),
            child: Text(strings.cancelAction),
          ),
        ],
      ),
    );
    if (confirmed != true || !mounted) return;
    await _cancelActiveJobs();
    try {
      await widget.gateway.abortBotGame(start.gameId);
      if (!mounted) return;
      Navigator.of(context).pop();
    } catch (error) {
      if (!mounted) return;
      setState(() => _error = error);
    }
  }

  void _goToPly(int ply) {
    if (_positions.isEmpty) return;
    final next = ply.clamp(0, _positions.length - 1).toInt();
    if (next == _viewPly) return;
    setState(() {
      _viewPly = next;
      _selectedSquare = null;
    });
  }

  String _statusText(AppLocalizations strings) {
    if (_loading) return strings.botGameLoading;
    if (!_atLivePosition) return strings.botViewingHistory;
    if (_gameFinished) return strings.botGameFinished;
    if (_botThinking) return strings.botThinking;
    if (_humanMovePending) return strings.botApplyingMove;
    if (_isHumanTurn) return strings.botYourTurn;
    return strings.botWaiting;
  }

  String? get _visibleHintSource {
    final move = _hintMoveUci;
    if (!_atLivePosition || _hintsUsed < 1 || move == null || move.length < 4) {
      return null;
    }
    return move.substring(0, 2);
  }

  String? get _visibleHintTarget {
    final move = _hintMoveUci;
    if (!_atLivePosition || _hintsUsed < 2 || move == null || move.length < 4) {
      return null;
    }
    return move.substring(2, 4);
  }

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final theme = Theme.of(context);
    return Scaffold(
      appBar: AppBar(
        title: Text(strings.botGameTitle),
        actions: [
          IconButton(
            key: const Key('bot-game-settings'),
            tooltip: strings.botGameSettingsTitle,
            onPressed: _start == null ? null : _openGameSettings,
            icon: const Icon(Icons.tune_rounded),
          ),
        ],
      ),
      body: _loading && _positions.isEmpty
          ? const Center(child: CircularProgressIndicator())
          : _positions.isEmpty
          ? _BotGameLoadError(
              message: _error?.toString() ?? strings.botGameLoadFailed,
              onRetry: _loadGame,
            )
          : LayoutBuilder(
              builder: (context, constraints) {
                final wide = constraints.maxWidth >= 920;
                final board = _BotPlayBoard(
                  key: const Key('bot-game-board'),
                  position: _displayPosition,
                  selectedSquare: _selectedSquare,
                  lastMoveUci: _displayLastMoveUci,
                  hintSourceSquare: _visibleHintSource,
                  hintTargetSquare: _visibleHintTarget,
                  endgameResult: _start?.result,
                  endgameCheckmate: _start?.checkmate ?? false,
                  endgameUseGiveupForLoss: _start?.status == 'resigned',
                  showEndgame: _gameFinished && _atLivePosition,
                  enabled: !_inputLocked && _isHumanTurn,
                  onSquareTap: _onSquareTap,
                  onPieceDrop: _onPieceDrop,
                );
                final evaluationVisible = _showEvaluationBar &&
                    _evaluationLine != null;
                final boardWithEvaluation = Column(
                  mainAxisSize: MainAxisSize.min,
                  children: [
                    if (_showEvaluationBar) ...[
                      SizedBox(
                        height: 30,
                        child: evaluationVisible
                            ? _BotEvaluationBar(
                                line: _evaluationLine,
                                terminalResult: _gameFinished ? _start?.result : null,
                              )
                            : _BotEvaluationPending(
                                thinking: _evaluationThinking && _atLivePosition,
                              ),
                      ),
                      const SizedBox(height: 8),
                    ],
                    Flexible(child: board),
                  ],
                );
                final sidePanel = _BotGameSidePanel(
                  botElo: _effectiveBotElo,
                  playerColor: _start!.playerColor,
                  moves: _moves,
                  currentPly: _viewPly,
                  status: _statusText(strings),
                  botThinking: _botThinking,
                  hintThinking: _hintThinking,
                  hintsUsed: _hintsUsed,
                  hintEnabled: _atLivePosition &&
                      _isHumanTurn &&
                      !_gameFinished &&
                      !_botThinking &&
                      !_humanMovePending &&
                      !_hintThinking &&
                      !_evaluationThinking &&
                      _hintsUsed < 2,
                  error: _error,
                  gameActive: !_gameFinished,
                  onHint: _requestHint,
                  onResign: _resignGame,
                  onAbort: _abortGame,
                  onSelectPly: _goToPly,
                );
                final controls = _BotNavigationControls(
                  currentPly: _viewPly,
                  livePly: _positions.length - 1,
                  onBack: _viewPly > 0 ? () => _goToPly(_viewPly - 1) : null,
                  onForward: _viewPly < _positions.length - 1
                      ? () => _goToPly(_viewPly + 1)
                      : null,
                  onLive: !_atLivePosition
                      ? () => _goToPly(_positions.length - 1)
                      : null,
                );

                if (wide) {
                  return Padding(
                    padding: const EdgeInsets.all(20),
                    child: Row(
                      crossAxisAlignment: CrossAxisAlignment.stretch,
                      children: [
                        Expanded(
                          flex: 7,
                          child: Column(
                            children: [
                              Expanded(child: Center(child: boardWithEvaluation)),
                              const SizedBox(height: 12),
                              controls,
                            ],
                          ),
                        ),
                        const SizedBox(width: 20),
                        Expanded(flex: 4, child: sidePanel),
                      ],
                    ),
                  );
                }

                return ListView(
                  padding: const EdgeInsets.fromLTRB(12, 12, 12, 28),
                  children: [
                    if (_showEvaluationBar) ...[
                      SizedBox(
                        height: 30,
                        child: evaluationVisible
                            ? _BotEvaluationBar(
                                line: _evaluationLine,
                                terminalResult: _gameFinished ? _start?.result : null,
                              )
                            : _BotEvaluationPending(
                                thinking: _evaluationThinking && _atLivePosition,
                              ),
                      ),
                      const SizedBox(height: 8),
                    ],
                    AspectRatio(aspectRatio: 1, child: board),
                    const SizedBox(height: 10),
                    controls,
                    const SizedBox(height: 14),
                    SizedBox(height: 360, child: sidePanel),
                  ],
                );
              },
            ),
    );
  }
}

class _BotGameSettingsSheet extends StatelessWidget {
  const _BotGameSettingsSheet({
    required this.showEvaluationBar,
    required this.onShowEvaluationBarChanged,
  });

  final bool showEvaluationBar;
  final ValueChanged<bool> onShowEvaluationBarChanged;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    return SafeArea(
      top: false,
      child: Padding(
        padding: const EdgeInsets.fromLTRB(16, 0, 16, 24),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Text(
              strings.botGameSettingsTitle,
              style: Theme.of(context).textTheme.titleLarge?.copyWith(
                fontWeight: FontWeight.w800,
              ),
            ),
            const SizedBox(height: 14),
            SwitchListTile(
              key: const Key('bot-game-eval-toggle'),
              contentPadding: EdgeInsets.zero,
              secondary: const Icon(Icons.balance_rounded),
              title: Text(strings.evaluationBarSetting),
              value: showEvaluationBar,
              onChanged: onShowEvaluationBarChanged,
            ),
          ],
        ),
      ),
    );
  }
}


class _BotEvaluationBar extends StatelessWidget {
  const _BotEvaluationBar({required this.line, required this.terminalResult});

  final EngineLine? line;
  final String? terminalResult;

  ({double whiteShare, String label}) _value() {
    final result = terminalResult;
    if (result == '1-0') return (whiteShare: 1.0, label: '1-0');
    if (result == '0-1') return (whiteShare: 0.0, label: '0-1');
    if (result == '1/2-1/2' || result == '½-½') {
      return (whiteShare: 0.5, label: '½-½');
    }

    final cp = line?.evaluationCp;
    final mate = line?.mateIn;
    if (mate != null && mate != 0) {
      return (
        whiteShare: mate > 0 ? 1.0 : 0.0,
        label: mate > 0 ? 'M$mate' : '-M${mate.abs()}',
      );
    }
    if (cp == null) {
      return (whiteShare: 0.5, label: '0.0');
    }

    final nativeWhitePermille = line?.evaluationBarWhitePermille;
    final whiteShare = nativeWhitePermille != null
        ? (nativeWhitePermille.clamp(0, 1000) / 1000.0)
        : (0.5 + cp.clamp(-1000, 1000) / 2000)
              .clamp(0.0, 1.0)
              .toDouble();
    final pawns = cp / 100.0;
    final label = pawns.abs() < 0.05
        ? '0.0'
        : '${pawns > 0 ? '+' : ''}${pawns.toStringAsFixed(1)}';
    return (whiteShare: whiteShare, label: label);
  }

  @override
  Widget build(BuildContext context) {
    final value = _value();
    final scheme = Theme.of(context).colorScheme;
    return Semantics(
      label: '${AppLocalizations.of(context).evaluation}: ${value.label}',
      child: ClipRRect(
        key: const Key('bot-game-evaluation-bar'),
        borderRadius: BorderRadius.circular(7),
        child: LayoutBuilder(
          builder: (context, constraints) => TweenAnimationBuilder<double>(
            duration: const Duration(milliseconds: 220),
            curve: Curves.easeOutCubic,
            tween: Tween<double>(begin: 0.5, end: value.whiteShare),
            builder: (context, whiteShare, _) {
              final split = constraints.maxWidth * whiteShare;
              return Stack(
                fit: StackFit.expand,
                children: [
                  const ColoredBox(color: Color(0xFF232624)),
                  Align(
                    alignment: AlignmentDirectional.centerStart,
                    child: SizedBox(
                      width: split,
                      height: double.infinity,
                      child: const ColoredBox(color: Color(0xFFF2EFE7)),
                    ),
                  ),
                  if (terminalResult != '1-0' && terminalResult != '0-1')
                    Align(
                      alignment: Alignment.center,
                      child: Container(
                        width: 1,
                        color: scheme.outline.withValues(alpha: 0.75),
                      ),
                    ),
                  Center(
                    child: DecoratedBox(
                      decoration: BoxDecoration(
                        color: scheme.surface.withValues(alpha: 0.88),
                        borderRadius: BorderRadius.circular(5),
                        border: Border.all(
                          color: scheme.outlineVariant.withValues(alpha: 0.8),
                        ),
                      ),
                      child: Padding(
                        padding: const EdgeInsets.symmetric(horizontal: 7),
                        child: Text(
                          value.label,
                          textDirection: TextDirection.ltr,
                          style: Theme.of(context).textTheme.labelMedium?.copyWith(
                                fontWeight: FontWeight.w800,
                                height: 1.25,
                              ),
                        ),
                      ),
                    ),
                  ),
                ],
              );
            },
          ),
        ),
      ),
    );
  }
}

class _BotEvaluationPending extends StatelessWidget {
  const _BotEvaluationPending({required this.thinking});

  final bool thinking;

  @override
  Widget build(BuildContext context) {
    final scheme = Theme.of(context).colorScheme;
    return DecoratedBox(
      decoration: BoxDecoration(
        color: scheme.surfaceContainerHighest,
        borderRadius: BorderRadius.circular(7),
      ),
      child: Center(
        child: thinking
            ? const SizedBox.square(
                dimension: 16,
                child: CircularProgressIndicator(strokeWidth: 2),
              )
            : Text(
                '—',
                style: Theme.of(context).textTheme.titleMedium,
              ),
      ),
    );
  }
}

class _BotPlayedMove {
  const _BotPlayedMove({
    required this.uci,
    required this.san,
  });

  final String uci;
  final String san;
}

// -----------------------------------------------------------------------------
// Section: Bot game board presentation
// -----------------------------------------------------------------------------

class _BotPlayBoard extends StatelessWidget {
  const _BotPlayBoard({
    required this.position,
    required this.selectedSquare,
    required this.lastMoveUci,
    required this.hintSourceSquare,
    required this.hintTargetSquare,
    required this.endgameResult,
    required this.endgameCheckmate,
    required this.endgameUseGiveupForLoss,
    required this.showEndgame,
    required this.enabled,
    required this.onSquareTap,
    required this.onPieceDrop,
    super.key,
  });

  final BoardPosition position;
  final String? selectedSquare;
  final String? lastMoveUci;
  final String? hintSourceSquare;
  final String? hintTargetSquare;
  final String? endgameResult;
  final bool endgameCheckmate;
  final bool endgameUseGiveupForLoss;
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

  String _squareName(int index) {
    final file = String.fromCharCode('a'.codeUnitAt(0) + index % 8);
    final rank = 8 - index ~/ 8;
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
    return isWhite == (position.draggableColor == 'white');
  }

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;
    final endgameVisible =
        showEndgame && endgameResult != null && endgameResult != '*';
    final whiteKingSquare = boardKingSquareForColor(position.pieces, 'white');
    final blackKingSquare = boardKingSquareForColor(position.pieces, 'black');

    return AspectRatio(
      aspectRatio: 1,
      child: DecoratedBox(
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
                    itemBuilder: (context, index) {
                      final square = _squareName(index);
                      final piece = index < position.pieces.length
                          ? position.pieces[index]
                          : '';
                      final row = index ~/ 8;
                      final column = index % 8;
                      final light = (row + column).isEven;
                      final selected = selectedSquare == square;
                      final hintSource = hintSourceSquare == square;
                      final hintTarget = hintTargetSquare == square;
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
                          : hintTarget
                          ? scheme.secondaryContainer.withValues(alpha: 0.96)
                          : hintSource
                          ? scheme.primaryContainer.withValues(alpha: 0.78)
                          : lastMove
                          ? const Color(0xFF82A9C5)
                          : base;

                      Widget pieceWidget() {
                        final asset = _pieceAssets[piece];
                        if (asset == null) return const SizedBox.shrink();
                        return Padding(
                          padding: const EdgeInsets.all(3),
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
                              key: Key('bot-board-square-$square'),
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
                                    if (hintSource)
                                      Positioned.fill(
                                        child: IgnorePointer(
                                          child: DecoratedBox(
                                            decoration: BoxDecoration(
                                              border: Border.all(
                                                color: scheme.primary,
                                                width: 4,
                                              ),
                                            ),
                                          ),
                                        ),
                                      ),
                                    if (hintTarget)
                                      Center(
                                        child: IgnorePointer(
                                          child: Container(
                                            width: 22,
                                            height: 22,
                                            decoration: BoxDecoration(
                                              shape: BoxShape.circle,
                                              border: Border.all(
                                                color: scheme.secondary,
                                                width: 4,
                                              ),
                                            ),
                                          ),
                                        ),
                                      ),
                                    if (column == 0)
                                      Positioned(
                                        left: 3,
                                        top: 2,
                                        child: Text(
                                          '${8 - row}',
                                          style: TextStyle(
                                            fontSize: 10,
                                            fontWeight: FontWeight.w700,
                                            color: light
                                                ? const Color(0xFF43544E)
                                                : const Color(0xFFD4D1C9),
                                          ),
                                        ),
                                      ),
                                    if (row == 7)
                                      Positioned(
                                        right: 3,
                                        bottom: 1,
                                        child: Text(
                                          String.fromCharCode(
                                            'a'.codeUnitAt(0) + column,
                                          ),
                                          style: TextStyle(
                                            fontSize: 10,
                                            fontWeight: FontWeight.w700,
                                            color: light
                                                ? const Color(0xFF43544E)
                                                : const Color(0xFFD4D1C9),
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
                    blackAtBottom: false,
                    useGiveupForLoss: endgameUseGiveupForLoss,
                  ),
                ],
              );
            },
          ),
        ),
      ),
    );
  }
}

// -----------------------------------------------------------------------------
// Section: Bot game navigation and move list
// -----------------------------------------------------------------------------

class _BotNavigationControls extends StatelessWidget {
  const _BotNavigationControls({
    required this.currentPly,
    required this.livePly,
    required this.onBack,
    required this.onForward,
    required this.onLive,
  });

  final int currentPly;
  final int livePly;
  final VoidCallback? onBack;
  final VoidCallback? onForward;
  final VoidCallback? onLive;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    return Row(
      mainAxisAlignment: MainAxisAlignment.center,
      children: [
        IconButton.filledTonal(
          key: const Key('bot-game-back'),
          tooltip: strings.botPreviousMove,
          onPressed: onBack,
          icon: const Icon(Icons.chevron_left_rounded),
        ),
        const SizedBox(width: 10),
        Text(
          '$currentPly / $livePly',
          key: const Key('bot-game-ply-counter'),
          style: Theme.of(context).textTheme.titleMedium?.copyWith(
            fontWeight: FontWeight.w700,
          ),
        ),
        const SizedBox(width: 10),
        IconButton.filledTonal(
          key: const Key('bot-game-forward'),
          tooltip: strings.botNextMove,
          onPressed: onForward,
          icon: const Icon(Icons.chevron_right_rounded),
        ),
        const SizedBox(width: 10),
        IconButton(
          key: const Key('bot-game-live'),
          tooltip: strings.botReturnToLive,
          onPressed: onLive,
          icon: const Icon(Icons.skip_next_rounded),
        ),
      ],
    );
  }
}

class _BotGameSidePanel extends StatelessWidget {
  const _BotGameSidePanel({
    required this.botElo,
    required this.playerColor,
    required this.moves,
    required this.currentPly,
    required this.status,
    required this.botThinking,
    required this.hintThinking,
    required this.hintsUsed,
    required this.hintEnabled,
    required this.error,
    required this.gameActive,
    required this.onHint,
    required this.onResign,
    required this.onAbort,
    required this.onSelectPly,
  });

  final int botElo;
  final String playerColor;
  final List<_BotPlayedMove> moves;
  final int currentPly;
  final String status;
  final bool botThinking;
  final bool hintThinking;
  final int hintsUsed;
  final bool hintEnabled;
  final Object? error;
  final bool gameActive;
  final VoidCallback onHint;
  final VoidCallback onResign;
  final VoidCallback onAbort;
  final ValueChanged<int> onSelectPly;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final theme = Theme.of(context);
    return Card(
      clipBehavior: Clip.antiAlias,
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Padding(
            padding: const EdgeInsets.fromLTRB(16, 14, 16, 12),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                Row(
                  children: [
                    const Icon(Icons.smart_toy_outlined),
                    const SizedBox(width: 10),
                    Expanded(
                      child: Text(
                        '${strings.stockfish18} · ${strings.botElo} $botElo',
                        style: theme.textTheme.titleMedium?.copyWith(
                          fontWeight: FontWeight.w700,
                        ),
                      ),
                    ),
                  ],
                ),
                const SizedBox(height: 10),
                Row(
                  children: [
                    const Icon(Icons.person_outline_rounded, size: 20),
                    const SizedBox(width: 8),
                    Text(
                      '${strings.botYou} · ${playerColor == 'white' ? strings.whitePlayer : strings.blackPlayer}',
                    ),
                  ],
                ),
                const SizedBox(height: 12),
                DecoratedBox(
                  decoration: BoxDecoration(
                    color: theme.colorScheme.surfaceContainerHighest,
                    borderRadius: BorderRadius.circular(10),
                  ),
                  child: Padding(
                    padding: const EdgeInsets.symmetric(
                      horizontal: 12,
                      vertical: 10,
                    ),
                    child: Row(
                      children: [
                        if (botThinking)
                          const Padding(
                            padding: EdgeInsets.only(right: 10),
                            child: SizedBox.square(
                              dimension: 16,
                              child: CircularProgressIndicator(strokeWidth: 2),
                            ),
                          ),
                        Expanded(
                          child: Text(
                            status,
                            key: const Key('bot-game-status'),
                            style: theme.textTheme.bodyMedium?.copyWith(
                              fontWeight: FontWeight.w600,
                            ),
                          ),
                        ),
                      ],
                    ),
                  ),
                ),
                if (error != null) ...[
                  const SizedBox(height: 8),
                  Text(
                    error.toString(),
                    key: const Key('bot-game-error'),
                    style: TextStyle(color: theme.colorScheme.error),
                  ),
                ],
                const SizedBox(height: 12),
                FilledButton.tonalIcon(
                  key: const Key('bot-game-hint'),
                  onPressed: hintEnabled ? onHint : null,
                  icon: hintThinking
                      ? const SizedBox.square(
                          dimension: 16,
                          child: CircularProgressIndicator(strokeWidth: 2),
                        )
                      : const Icon(Icons.lightbulb_outline_rounded),
                  label: Text(
                    hintThinking
                        ? strings.botHintThinking
                        : hintsUsed == 0
                        ? strings.botHintPiece
                        : hintsUsed == 1
                        ? strings.botHintTarget
                        : strings.botHintsUsed,
                  ),
                ),
                const SizedBox(height: 6),
                Text(
                  strings.botHintCounter(hintsUsed),
                  key: const Key('bot-game-hint-counter'),
                  textAlign: TextAlign.center,
                  style: theme.textTheme.bodySmall?.copyWith(
                    color: theme.colorScheme.onSurfaceVariant,
                  ),
                ),
                const SizedBox(height: 12),
                Row(
                  children: [
                    Expanded(
                      child: OutlinedButton.icon(
                        key: const Key('bot-game-resign'),
                        onPressed: gameActive ? onResign : null,
                        icon: const Icon(Icons.flag_outlined),
                        label: Text(strings.statsTerminationResignation),
                      ),
                    ),
                    const SizedBox(width: 8),
                    Expanded(
                      child: OutlinedButton.icon(
                        key: const Key('bot-game-abort'),
                        onPressed: gameActive ? onAbort : null,
                        icon: const Icon(Icons.cancel_outlined),
                        label: Text(strings.cancelAction),
                      ),
                    ),
                  ],
                ),
              ],
            ),
          ),
          const Divider(height: 1),
          Padding(
            padding: const EdgeInsets.fromLTRB(16, 12, 16, 8),
            child: Text(
              strings.botMoveList,
              style: theme.textTheme.titleSmall?.copyWith(
                fontWeight: FontWeight.w700,
              ),
            ),
          ),
          Expanded(
            child: moves.isEmpty
                ? Center(child: Text(strings.botNoMovesYet))
                : ListView.builder(
                    key: const Key('bot-game-move-list'),
                    padding: const EdgeInsets.fromLTRB(10, 0, 10, 12),
                    itemCount: (moves.length + 1) ~/ 2,
                    itemBuilder: (context, row) {
                      final whiteIndex = row * 2;
                      final blackIndex = whiteIndex + 1;
                      return Row(
                        children: [
                          SizedBox(
                            width: 38,
                            child: Text('${row + 1}.'),
                          ),
                          Expanded(
                            child: _BotMoveButton(
                              move: whiteIndex < moves.length
                                  ? moves[whiteIndex]
                                  : null,
                              ply: whiteIndex + 1,
                              selected: currentPly == whiteIndex + 1,
                              onSelectPly: onSelectPly,
                            ),
                          ),
                          const SizedBox(width: 6),
                          Expanded(
                            child: _BotMoveButton(
                              move: blackIndex < moves.length
                                  ? moves[blackIndex]
                                  : null,
                              ply: blackIndex + 1,
                              selected: currentPly == blackIndex + 1,
                              onSelectPly: onSelectPly,
                            ),
                          ),
                        ],
                      );
                    },
                  ),
          ),
        ],
      ),
    );
  }
}

class _BotMoveButton extends StatelessWidget {
  const _BotMoveButton({
    required this.move,
    required this.ply,
    required this.selected,
    required this.onSelectPly,
  });

  final _BotPlayedMove? move;
  final int ply;
  final bool selected;
  final ValueChanged<int> onSelectPly;

  @override
  Widget build(BuildContext context) {
    final value = move;
    if (value == null) return const SizedBox(height: 38);
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 2),
      child: TextButton(
        style: TextButton.styleFrom(
          alignment: Alignment.centerLeft,
          backgroundColor: selected
              ? Theme.of(context).colorScheme.primaryContainer
              : null,
        ),
        onPressed: () => onSelectPly(ply),
        child: Text(value.san),
      ),
    );
  }
}

class _BotGameLoadError extends StatelessWidget {
  const _BotGameLoadError({required this.message, required this.onRetry});

  final String message;
  final VoidCallback onRetry;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    return Center(
      child: Padding(
        padding: const EdgeInsets.all(24),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            const Icon(Icons.error_outline_rounded, size: 48),
            const SizedBox(height: 12),
            Text(message, textAlign: TextAlign.center),
            const SizedBox(height: 16),
            FilledButton(
              onPressed: onRetry,
              child: Text(strings.retry),
            ),
          ],
        ),
      ),
    );
  }
}
