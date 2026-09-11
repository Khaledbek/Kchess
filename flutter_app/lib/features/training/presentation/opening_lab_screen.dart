import 'dart:async';

import 'package:flutter/material.dart';

import '../../../ffi/core_gateway.dart';
import '../../../localization/generated/app_localizations.dart';
import '../../../services/training_progress_service.dart';
import '../../../shared/models/models.dart';
import '../../../shared/theme/app_theme.dart';
import '../../../shared/widgets/chess_board_view.dart';
import '../../../shared/widgets/evaluation_bar.dart';
import '../data/opening_database.dart';
import '../models/models.dart';
import 'tree/opening_tree_controller.dart';
import 'tree/skill_tree_canvas.dart';

class OpeningLabScreen extends StatefulWidget {
  const OpeningLabScreen({
    required this.gateway,
    required this.progress,
    this.request,
    this.replyDelay = const Duration(milliseconds: 300),
    super.key,
  });

  final CoreGateway gateway;
  final TrainingProgressService progress;
  final OpeningTrainingRequest? request;
  final Duration replyDelay;

  @override
  State<OpeningLabScreen> createState() => _OpeningLabScreenState();
}

class _OpeningLabScreenState extends State<OpeningLabScreen> {
  OpeningLine? _line;
  bool _loading = false;
  bool _requestHandled = false;
  PieceColor _selectedColor = PieceColor.white;
  late final OpeningTreeController _treeController;
  StreamSubscription<TrainingProgressSnapshot>? _progressSubscription;

  @override
  void initState() {
    super.initState();
    unawaited(widget.progress.load().then((_) {
      if (mounted) setState(() {});
    }));
    // The cards read mastery straight off the service, so a line finished in
    // the player has to repaint its node on the way back out.
    _progressSubscription = widget.progress.changes.listen((_) {
      if (mounted) setState(() {});
    });
    _treeController = OpeningTreeController(gateway: widget.gateway);
    _loadRequest();
  }

  @override
  void dispose() {
    _progressSubscription?.cancel();
    _treeController.dispose();
    super.dispose();
  }

  /// Opens the line behind a tree card.
  ///
  /// Tree node ids and `openings` row ids are separate sequences, so the old
  /// `getLineById(node.id)` always came back empty and the button looked dead.
  Future<void> _openNode(OpeningTreeNode node) async {
    setState(() => _loading = true);
    final line = await OpeningDatabase.getLineForNode(node, _selectedColor);
    if (!mounted) return;
    setState(() {
      _line = line;
      _loading = false;
    });
    if (line == null) {
      final strings = AppLocalizations.of(context);
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text(strings.trainingOpeningTreeLoadFailed)),
      );
    }
  }

  @override
  void didUpdateWidget(covariant OpeningLabScreen oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.request != widget.request) {
      _loadRequest();
    }
  }

  Future<void> _loadRequest() async {
    final request = widget.request;
    if (request == null) {
      if (mounted) {
        setState(() {
          _line = null;
          _requestHandled = true;
          _loading = false;
        });
      }
      return;
    }
    setState(() => _loading = true);
    
    PieceColor color = PieceColor.white;
    if (request.color == 'black') color = PieceColor.black;
    setState(() => _selectedColor = color);

    try {
      final line = await OpeningDatabase.matching(
        eco: request.eco,
        name: request.openingName,
        playerColor: color,
      );
      if (!mounted) return;
      setState(() {
        _line = line;
        _loading = false;
        _requestHandled = true;
      });
    } catch (_) {
      if (!mounted) return;
      setState(() {
        _loading = false;
        _requestHandled = true;
      });
    }
  }

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final line = _line;

    return Scaffold(
      appBar: AppBar(
        title: Text(
          line == null
              ? strings.trainingOpeningTitle
              : (widget.request?.openingName ?? (line.name.contains(':') ? line.name.split(':').first.trim() : line.name)),
        ),
        leading: line == null
            ? null
            : IconButton(
                key: const Key('opening-lab-back'),
                icon: const BackButtonIcon(),
                tooltip: strings.trainingOpeningLabBackToOverview,
                onPressed: () => setState(() => _line = null),
              ),
        actions: [
          if (line == null)
            Padding(
              padding: const EdgeInsets.symmetric(horizontal: 16.0),
              child: SegmentedButton<PieceColor>(
                segments: [
                  ButtonSegment(value: PieceColor.white, label: Text(strings.statsCompareColorWhite)),
                  ButtonSegment(value: PieceColor.black, label: Text(strings.statsCompareColorBlack)),
                ],
                selected: {_selectedColor},
                onSelectionChanged: (set) => setState(() => _selectedColor = set.first),
              ),
            ),
        ],
      ),
      body: ExcludeSemantics(
        child: SafeArea(
          child: !_requestHandled || _loading
              ? const Center(child: CircularProgressIndicator())
              : (line == null
                  ? ListenableBuilder(
                      listenable: _treeController,
                      builder: (context, _) => SkillTreeCanvas(
                        controller: _treeController,
                        progress: widget.progress,
                        onNodeSelected: (node) => unawaited(_openNode(node)),
                      ),
                    )
                  : OpeningLinePlayer(
                      key: ValueKey(line.id),
                      line: line,
                      gateway: widget.gateway,
                      progress: widget.progress,
                      replyDelay: widget.replyDelay,
                      onBackToCatalogue: () => setState(() => _line = null),
                    )),
        ),
      ),
    );
  }
}

class OpeningLinePlayer extends StatefulWidget {
  const OpeningLinePlayer({
    required this.line,
    required this.gateway,
    required this.progress,
    required this.onBackToCatalogue,
    this.replyDelay = const Duration(milliseconds: 300),
    super.key,
  });

  final OpeningLine line;
  final CoreGateway gateway;
  final TrainingProgressService progress;
  final VoidCallback onBackToCatalogue;
  final Duration replyDelay;

  @override
  State<OpeningLinePlayer> createState() => _OpeningLinePlayerState();
}

class _OpeningLinePlayerState extends State<OpeningLinePlayer> {
  static const _flashDuration = Duration(milliseconds: 450);

  int _ply = 0;
  BoardPosition? _position;
  List<BoardMoveOption> _legalMoves = const [];
  bool _loading = true;
  bool _failed = false;
  bool _busy = false;
  bool _done = false;

  bool _wrongMove = false;
  ({String square, bool correct})? _flash;
  Timer? _flashTimer;

  String? _selected;
  String? _evaluationJobId;
  EngineLine? _evaluationLine;
  Timer? _evalTimer;

  @override
  void initState() {
    super.initState();
    unawaited(_restart());
  }

  @override
  void dispose() {
    _flashTimer?.cancel();
    _evalTimer?.cancel();
    if (_evaluationJobId != null) {
      widget.gateway.cancelVariationAnalysis(_evaluationJobId!);
    }
    super.dispose();
  }

  Future<void> _restart() async {
    _flashTimer?.cancel();
    setState(() {
      _ply = 0;
      _loading = true;
      _failed = false;
      _busy = false;
      _done = false;
      _wrongMove = false;
      _flash = null;
      _selected = null;
    });
    await _loadPosition(widget.line.startingFen);
    if (!mounted || _failed) return;
    await _playComputerReplies();
  }

  Future<void> _loadPosition(String fen) async {
    try {
      final position = await widget.gateway.boardPosition(fen);
      final moves = await widget.gateway.boardLegalMoves(fen);
      if (!mounted) return;
      setState(() {
        _position = position;
        _legalMoves = moves;
        _loading = false;
      });
      _startEvaluation(fen);
    } catch (_) {
      if (!mounted) return;
      setState(() {
        _loading = false;
        _failed = true;
      });
    }
  }

  Future<void> _startEvaluation(String fen) async {
    _evalTimer?.cancel();
    if (_evaluationJobId != null) {
      try {
        await widget.gateway.cancelVariationAnalysis(_evaluationJobId!);
      } catch (_) {}
      _evaluationJobId = null;
    }
    try {
      final moves = widget.line.moves;
      final lastUci = _ply > 0 && _ply <= moves.length ? moves[_ply - 1].san : '';
      final snapshot = await widget.gateway.startVariationAnalysis(fen: fen, uci: lastUci);
      _evaluationJobId = snapshot.jobId;
      if (mounted) {
        setState(() {
          _evaluationLine = snapshot.lines.isNotEmpty ? snapshot.lines.first : null;
        });
        if (!snapshot.isComplete && snapshot.lines.isEmpty && snapshot.jobId.isNotEmpty) {
          _evalTimer = Timer(const Duration(milliseconds: 300), () => _pollEvaluation(_evaluationJobId!));
        }
      }
    } catch (_) {
      _evaluationJobId = null;
    }
  }

  Future<void> _pollEvaluation(String jobId) async {
    if (!mounted || jobId != _evaluationJobId) return;
    try {
      final snapshot = await widget.gateway.variationAnalysisStatus(jobId);
      if (!mounted || jobId != _evaluationJobId) return;
      setState(() {
        _evaluationLine = snapshot.lines.isNotEmpty ? snapshot.lines.first : null;
      });
      if (!snapshot.isComplete && snapshot.lines.isEmpty) {
        _evalTimer?.cancel();
        _evalTimer = Timer(const Duration(milliseconds: 300), () => _pollEvaluation(jobId));
      }
    } catch (_) {}
  }

  BoardMoveOption? _moveBySquares(String squares) {
    for (final option in _legalMoves) {
      if (option.squares == squares) return option;
    }
    return null;
  }

  BoardMoveOption? _moveBySan(String san) {
    for (final option in _legalMoves) {
      if (option.san == san) return option;
    }
    return null;
  }

  void _flashSquare(String square, {required bool correct}) {
    _flashTimer?.cancel();
    setState(() => _flash = (square: square, correct: correct));
    _flashTimer = Timer(_flashDuration, () {
      if (mounted) setState(() => _flash = null);
    });
  }

  Set<String> _targetsFor(String? from) {
    if (from == null || _busy || _done || _loading) return const {};
    return {
      for (final option in _legalMoves)
        if (option.squares.startsWith(from)) option.squares.substring(2, 4),
    };
  }

  void _onSquareTap(String square) {
    if (_busy || _done || _loading || _failed) return;
    final from = _selected;
    if (from != null && _targetsFor(from).contains(square)) {
      setState(() => _selected = null);
      unawaited(_onPieceDrop(from, square));
      return;
    }
    setState(() => _selected = _targetsFor(square).isEmpty ? null : square);
  }

  Future<void> _onPieceDrop(String source, String target) async {
    if (_busy || _done || _loading || _failed) return;
    final moves = widget.line.moves;
    if (_ply >= moves.length) return;

    final option = _moveBySquares('$source$target');
    if (option == null || option.san != moves[_ply].san) {
      setState(() {
        _wrongMove = true;
        _selected = null;
      });
      _flashSquare(target, correct: false);
      return;
    }

    setState(() {
      _wrongMove = false;
      _selected = null;
    });
    _flashSquare(target, correct: true);
    await _applyMove(option);
    if (!mounted || _failed) return;
    await _playComputerReplies();
  }

  Future<void> _playComputerReplies() async {
    final moves = widget.line.moves;
    while (_ply < moves.length && !widget.line.isPlayerMove(_ply)) {
      setState(() => _busy = true);
      await Future<void>.delayed(widget.replyDelay);
      if (!mounted) return;

      final reply = _moveBySan(moves[_ply].san);
      if (reply == null) {
        setState(() {
          _busy = false;
          _failed = true;
        });
        return;
      }
      _flashSquare(reply.squares.substring(2, 4), correct: true);
      await _applyMove(reply);
      if (!mounted || _failed) {
        if (mounted) setState(() => _busy = false);
        return;
      }
    }
    if (!mounted) return;
    setState(() => _busy = false);
    if (_ply >= moves.length) await _complete();
  }

  Future<void> _applyMove(BoardMoveOption option) async {
    setState(() {
      _ply += 1;
    });
    await _loadPosition(option.fenAfter);
  }

  Future<void> _complete() async {
    if (_done) return;
    setState(() => _done = true);
    await widget.progress.markExerciseCompleted('opening_${widget.line.id}', true);
  }

  Color _tintFor(String square, Color base) {
    if (square == _selected) {
      return Color.alphaBlend(
        Theme.of(context).colorScheme.tertiary.withValues(alpha: 0.4),
        base,
      );
    }
    final flash = _flash;
    if (flash == null || flash.square != square) return base;
    return Color.alphaBlend(
      (flash.correct ? AppTheme.success : Theme.of(context).colorScheme.error)
          .withValues(alpha: 0.55),
      base,
    );
  }

  Widget _buildMoveList() {
    final moves = widget.line.moves;
    final theme = Theme.of(context);
    return Container(
      decoration: BoxDecoration(
        color: theme.colorScheme.surfaceContainerHigh,
        borderRadius: BorderRadius.circular(8),
      ),
      padding: const EdgeInsets.all(8),
      child: ListView.builder(
        itemCount: (moves.length / 2).ceil(),
        itemBuilder: (context, i) {
          final moveNum = i + 1;
          final whiteIdx = i * 2;
          final blackIdx = i * 2 + 1;
          
          final whiteMove = whiteIdx < _ply ? moves[whiteIdx].san : (whiteIdx < moves.length ? '...' : '');
          final blackMove = blackIdx < _ply ? moves[blackIdx].san : (blackIdx < moves.length ? '...' : '');

          return Padding(
            padding: const EdgeInsets.symmetric(vertical: 4),
            child: Row(
              children: [
                SizedBox(width: 30, child: Text('$moveNum.', style: theme.textTheme.bodySmall?.copyWith(color: theme.colorScheme.outline))),
                Expanded(child: Text(whiteMove, style: theme.textTheme.bodyMedium?.copyWith(fontWeight: whiteIdx == _ply - 1 ? FontWeight.bold : FontWeight.normal))),
                Expanded(child: Text(blackMove, style: theme.textTheme.bodyMedium?.copyWith(fontWeight: blackIdx == _ply - 1 ? FontWeight.bold : FontWeight.normal))),
              ],
            ),
          );
        },
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;
    final position = _position;

    return LayoutBuilder(
      builder: (context, constraints) {
        final isWide = constraints.maxWidth > 600;

        final boardWidget = position == null 
          ? const Center(child: SizedBox.shrink())
          : AspectRatio(
              aspectRatio: 1,
              child: ClipRRect(
                borderRadius: BorderRadius.circular(10),
                child: ChessBoardView(
                  position: position,
                  blackAtBottom: !widget.line.playerColor.isWhite,
                  interactive: !_busy && !_done,
                  moveTargets: _targetsFor(_selected),
                  squareTint: _tintFor,
                  onSquareTap: _onSquareTap,
                  onPieceDrop: (source, target) => unawaited(_onPieceDrop(source, target)),
                  onDragStarted: (square) => setState(() => _selected = square),
                ),
              ),
            );

        final evalWidget = SizedBox(
          width: isWide ? 24 : null,
          height: isWide ? null : 24,
          child: RotatedBox(
            quarterTurns: isWide ? 1 : 0,
            child: EvaluationBar(
              line: _wrongMove ? null : _evaluationLine,
              terminalResult: null,
            ),
          ),
        );

        final headerWidget = Row(
          children: [
            Chip(label: Text(widget.line.eco), visualDensity: VisualDensity.compact),
            const SizedBox(width: 8),
            Expanded(
              child: Text(
                strings.trainingOpeningLabPlayingAs(
                  widget.line.playerColor.isWhite ? strings.statsCompareColorWhite : strings.statsCompareColorBlack,
                ),
                style: theme.textTheme.bodySmall?.copyWith(color: scheme.onSurfaceVariant),
              ),
            ),
            Text(
              strings.trainingMoveProgress(widget.line.playerMovesPlayed(_ply), widget.line.playerMoveCount),
              key: const Key('opening-move-progress'),
              style: theme.textTheme.labelLarge?.copyWith(fontWeight: FontWeight.w800, color: scheme.onSurfaceVariant),
            ),
          ],
        );

        final noticeWidget = _done
          ? Container(
              padding: const EdgeInsets.all(16),
              decoration: BoxDecoration(
                color: AppTheme.success.withValues(alpha: 0.1),
                borderRadius: BorderRadius.circular(12),
                border: Border.all(color: AppTheme.success),
              ),
              child: Column(
                children: [
                  Text(strings.trainingOpeningLabDoneTitle, style: theme.textTheme.titleMedium?.copyWith(color: AppTheme.success, fontWeight: FontWeight.bold)),
                  const SizedBox(height: 12),
                  Row(
                    mainAxisAlignment: MainAxisAlignment.center,
                    children: [
                      OutlinedButton(onPressed: () => unawaited(_restart()), child: Text(strings.trainingPracticeAgain)),
                      const SizedBox(width: 8),
                      FilledButton(onPressed: widget.onBackToCatalogue, child: Text(strings.trainingOpeningLabBackToOverview)),
                    ],
                  )
                ],
              ),
            )
          : Container(
              padding: const EdgeInsets.all(14),
              decoration: BoxDecoration(
                color: _wrongMove ? scheme.errorContainer : scheme.surfaceContainerHigh,
                borderRadius: BorderRadius.circular(12),
                border: Border.all(color: _wrongMove ? scheme.error : scheme.outlineVariant),
              ),
              child: Row(
                children: [
                  Icon(_wrongMove ? Icons.close_rounded : (_busy ? Icons.more_horiz : Icons.touch_app_outlined), 
                       color: _wrongMove ? scheme.onErrorContainer : scheme.onSurfaceVariant),
                  const SizedBox(width: 10),
                  Expanded(child: Text(_wrongMove ? strings.trainingOpeningLabWrongMove : (_busy ? strings.trainingOpponentThinking : strings.trainingYourMove), 
                                       style: theme.textTheme.bodySmall?.copyWith(color: _wrongMove ? scheme.onErrorContainer : scheme.onSurfaceVariant))),
                ],
              ),
            );

        if (isWide) {
          return Padding(
            padding: const EdgeInsets.all(24),
            child: Row(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                Expanded(flex: 2, child: _buildMoveList()),
                const SizedBox(width: 24),
                Expanded(
                  flex: 5,
                  child: Column(
                    children: [
                      headerWidget,
                      const SizedBox(height: 16),
                      Expanded(
                        child: Row(
                          crossAxisAlignment: CrossAxisAlignment.stretch,
                          children: [
                            evalWidget,
                            const SizedBox(width: 8),
                            Expanded(child: Center(child: boardWidget)),
                          ],
                        ),
                      ),
                      const SizedBox(height: 16),
                      noticeWidget,
                    ],
                  ),
                ),
              ],
            ),
          );
        } else {
          return ListView(
            padding: const EdgeInsets.all(16),
            children: [
              headerWidget,
              const SizedBox(height: 16),
              boardWidget,
              const SizedBox(height: 8),
              evalWidget,
              const SizedBox(height: 16),
              noticeWidget,
              const SizedBox(height: 16),
              SizedBox(height: 200, child: _buildMoveList()),
            ],
          );
        }
      },
    );
  }
}
