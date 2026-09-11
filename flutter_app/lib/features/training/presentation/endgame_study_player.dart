import 'dart:async';

import 'package:flutter/material.dart';

import '../../../ffi/core_gateway.dart';
import '../../../localization/generated/app_localizations.dart';
import '../../../services/training_progress_service.dart';
import '../../../shared/models/models.dart';
import '../../../shared/theme/app_theme.dart';
import '../../../shared/widgets/chess_board_view.dart';
import '../data/endgame_defender_bot.dart';
import '../data/endgame_studies_library.dart';
import '../models/endgame_study.dart';
import 'endgame_drill_player.dart' show DrillDifficultyChip, DrillOutcome;

/// Play-versus-engine player for a classical Kling & Horwitz (1851) endgame study.
class EndgameStudyPlayer extends StatefulWidget {
  const EndgameStudyPlayer({
    required this.study,
    required this.section,
    required this.gateway,
    required this.progress,
    this.bot,
    this.replyDelay = const Duration(milliseconds: 300),
    super.key,
  });

  final EndgameStudy study;
  final EndgameStudySection section;
  final CoreGateway gateway;
  final TrainingProgressService progress;

  /// Injected by tests; production builds one from [gateway].
  final EndgameDefenderBot? bot;
  final Duration replyDelay;

  @override
  State<EndgameStudyPlayer> createState() => _EndgameStudyPlayerState();
}

class _EndgameStudyPlayerState extends State<EndgameStudyPlayer> {
  late final EndgameDefenderBot _bot =
      widget.bot ?? EndgameDefenderBot(widget.gateway);

  BoardPosition? _position;
  List<BoardMoveOption> _legalMoves = const [];
  String _fen = '';

  int _moves = 0;
  bool _loading = true;
  bool _failed = false;
  bool _busy = false;
  bool _engineDegraded = false;
  DrillOutcome _outcome = DrillOutcome.playing;

  /// Square whose legal moves are currently shown as dots.
  String? _selected;

  late final bool _solverIsWhite = widget.study.solverColor != 'black';

  @override
  void initState() {
    super.initState();
    widget.progress.load();
    unawaited(_restart());
  }

  @override
  void dispose() {
    _bot.dispose();
    super.dispose();
  }

  Future<void> _restart() async {
    setState(() {
      _loading = true;
      _failed = false;
      _busy = false;
      _engineDegraded = false;
      _moves = 0;
      _outcome = DrillOutcome.playing;
      _selected = null;
    });
    await _load(widget.study.fen);
  }

  Future<void> _load(String fen) async {
    try {
      final position = await widget.gateway.boardPosition(fen);
      final moves = await widget.gateway.boardLegalMoves(fen);
      if (!mounted) return;
      setState(() {
        _fen = fen;
        _position = position;
        _legalMoves = moves;
        _loading = false;
      });
    } on CoreGatewayException {
      if (!mounted) return;
      setState(() {
        _loading = false;
        _failed = true;
      });
    }
  }

  /// Squares the piece on [from] can legally reach, for the board's dots.
  Set<String> _targetsFor(String? from) {
    if (from == null || _busy || _outcome != DrillOutcome.playing) {
      return const {};
    }
    return {
      for (final option in _legalMoves)
        if (option.squares.startsWith(from)) option.squares.substring(2, 4),
    };
  }

  /// Tap-to-move: the first tap picks a piece that has moves, the second either
  /// plays to a highlighted square or re-selects.
  void _onSquareTap(String square) {
    if (_busy || _loading || _outcome != DrillOutcome.playing) return;
    final from = _selected;
    if (from != null && _targetsFor(from).contains(square)) {
      setState(() => _selected = null);
      unawaited(_onPieceDrop(from, square));
      return;
    }
    setState(() {
      _selected = _targetsFor(square).isEmpty ? null : square;
    });
  }

  BoardMoveOption? _moveBySquares(String squares) {
    for (final option in _legalMoves) {
      if (option.squares == squares) return option;
    }
    return null;
  }

  Future<void> _onPieceDrop(String source, String target) async {
    if (_busy || _loading || _outcome != DrillOutcome.playing) return;
    final option = _moveBySquares('$source$target');
    if (option == null) return;

    final fenBefore = _fen;
    setState(() {
      _moves += 1;
      _selected = null;
    });
    await _load(option.fenAfter);
    if (!mounted || _failed) return;

    final afterPlayer = _position;
    if (afterPlayer == null) return;
    if (afterPlayer.isCheckmate) {
      await _finish(DrillOutcome.checkmate);
      return;
    }
    if (afterPlayer.isStalemate) {
      await _finish(DrillOutcome.stalemate);
      return;
    }
    if (afterPlayer.insufficientMaterial) {
      await _finish(DrillOutcome.drawn);
      return;
    }

    setState(() => _busy = true);
    try {
      await _playDefenderReply(fenBefore, option.uci);
    } catch (_) {
      if (mounted) setState(() => _engineDegraded = true);
    } finally {
      if (mounted) setState(() => _busy = false);
    }
    if (!mounted) return;

    final afterDefender = _position;
    if (afterDefender == null) return;
    if (!afterDefender.isPlayable || afterDefender.insufficientMaterial) {
      await _finish(DrillOutcome.drawn);
    }
  }

  Future<void> _playDefenderReply(String fenBefore, String playerUci) async {
    await Future<void>.delayed(widget.replyDelay);
    if (!mounted) return;
    final reply = await _bot.reply(
      fenBeforePlayerMove: fenBefore,
      playerUci: playerUci,
      defenderMoves: _legalMoves,
    );
    if (!mounted || reply == null) return;
    if (!reply.fromEngine) setState(() => _engineDegraded = true);
    await _load(reply.move.fenAfter);
  }

  Future<void> _finish(DrillOutcome outcome) async {
    setState(() => _outcome = outcome);
    await widget.progress.markExerciseCompleted(
      widget.study.id,
      outcome == DrillOutcome.checkmate,
    );
  }

  void _openStudy(EndgameStudy study) {
    Navigator.of(context).pushReplacement(
      MaterialPageRoute<void>(
        builder: (_) => EndgameStudyPlayer(
          study: study,
          section: widget.section,
          gateway: widget.gateway,
          progress: widget.progress,
        ),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;
    final position = _position;
    final study = widget.study;
    final nextStudy = EndgameStudiesLibrary.nextStudyInSection(
      widget.section,
      study,
    );

    return Scaffold(
      appBar: AppBar(
        title: Text(widget.section.title),
        actions: [
          IconButton(
            onPressed: _loading ? null : () => unawaited(_restart()),
            tooltip: strings.trainingRestart,
            icon: const Icon(Icons.restart_alt),
          ),
        ],
      ),
      body: SafeArea(
        child: Center(
          child: ConstrainedBox(
            constraints: const BoxConstraints(maxWidth: 620),
            child: ListView(
              padding: const EdgeInsets.fromLTRB(16, 12, 16, 24),
              children: [
                Row(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Expanded(
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          Text(
                            strings.trainingStudyNumber(study.number),
                            style: theme.textTheme.titleMedium?.copyWith(
                              fontWeight: FontWeight.w800,
                            ),
                          ),
                          const SizedBox(height: 3),
                          Text(
                            strings.trainingStudySourceYear,
                            style: theme.textTheme.bodySmall?.copyWith(
                              color: scheme.onSurfaceVariant,
                            ),
                          ),
                          const SizedBox(height: 6),
                          DrillDifficultyChip(difficulty: study.difficulty),
                        ],
                      ),
                    ),
                    const SizedBox(width: 12),
                    Text(
                      strings.trainingStudyMoves(_moves),
                      key: const Key('study-move-counter'),
                      style: theme.textTheme.titleMedium?.copyWith(
                        fontWeight: FontWeight.w800,
                        color: scheme.onSurfaceVariant,
                      ),
                    ),
                  ],
                ),
                const SizedBox(height: 14),
                if (_failed)
                  _NoticeBox(
                    icon: Icons.error_outline,
                    text: strings.trainingBoardError,
                    action: TextButton(
                      onPressed: () => unawaited(_restart()),
                      child: Text(strings.trainingRestart),
                    ),
                  )
                else if (_loading || position == null)
                  const SizedBox(
                    height: 280,
                    child: Center(child: CircularProgressIndicator()),
                  )
                else
                  AspectRatio(
                    aspectRatio: 1,
                    child: Stack(
                      key: const Key('study-board'),
                      fit: StackFit.expand,
                      children: [
                        ClipRRect(
                          borderRadius: BorderRadius.circular(10),
                          child: ChessBoardView(
                            position: position,
                            blackAtBottom: !_solverIsWhite,
                            interactive:
                                !_busy && _outcome == DrillOutcome.playing,
                            moveTargets: _targetsFor(_selected),
                            squareTint: (square, base) => square == _selected
                                ? Color.alphaBlend(
                                    Theme.of(context).colorScheme.tertiary
                                        .withValues(alpha: 0.4),
                                    base,
                                  )
                                : base,
                            onSquareTap: _onSquareTap,
                            onPieceDrop: (source, target) =>
                                unawaited(_onPieceDrop(source, target)),
                            onDragStarted: (square) =>
                                setState(() => _selected = square),
                          ),
                        ),
                        if (_outcome != DrillOutcome.playing)
                          _StudyOutcomeOverlay(
                            outcome: _outcome,
                            moves: _moves,
                            onRetry: () => unawaited(_restart()),
                            onNextStudy: nextStudy == null
                                ? null
                                : () => _openStudy(nextStudy),
                          ),
                      ],
                    ),
                  ),
                const SizedBox(height: 14),
                if (_engineDegraded) ...[
                  _NoticeBox(
                    icon: Icons.warning_amber_rounded,
                    text: strings.trainingDrillEngineFallback,
                  ),
                  const SizedBox(height: 12),
                ],
                _NoticeBox(
                  icon: _busy ? Icons.more_horiz : Icons.sports_martial_arts,
                  text: _busy
                      ? strings.trainingOpponentThinking
                      : _solverIsWhite
                      ? strings.trainingGoalWin(strings.statsCompareColorWhite)
                      : strings.trainingGoalWin(strings.statsCompareColorBlack),
                ),
              ],
            ),
          ),
        ),
      ),
    );
  }
}

class _StudyOutcomeOverlay extends StatelessWidget {
  const _StudyOutcomeOverlay({
    required this.outcome,
    required this.moves,
    required this.onRetry,
    this.onNextStudy,
  });

  final DrillOutcome outcome;
  final int moves;
  final VoidCallback onRetry;
  final VoidCallback? onNextStudy;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;
    final won = outcome == DrillOutcome.checkmate;

    final (icon, title, body) = switch (outcome) {
      DrillOutcome.checkmate => (
        Icons.emoji_events_rounded,
        strings.trainingDrillVictoryTitle,
        strings.trainingDrillVictoryBody(moves),
      ),
      DrillOutcome.stalemate => (
        Icons.pause_circle_outline,
        strings.trainingDrillStalemateTitle,
        strings.trainingDrillStalemateBody,
      ),
      _ => (
        Icons.handshake_outlined,
        strings.trainingDrillDrawTitle,
        strings.trainingDrillDrawBody,
      ),
    };

    return Container(
      key: Key('study-outcome-${outcome.name}'),
      alignment: Alignment.center,
      decoration: BoxDecoration(
        color: scheme.surface.withValues(alpha: 0.9),
        borderRadius: BorderRadius.circular(10),
      ),
      child: Padding(
        padding: const EdgeInsets.all(20),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Icon(icon, size: 44, color: won ? AppTheme.success : scheme.error),
            const SizedBox(height: 12),
            Text(
              title,
              textAlign: TextAlign.center,
              style: theme.textTheme.titleMedium?.copyWith(
                fontWeight: FontWeight.w800,
              ),
            ),
            const SizedBox(height: 6),
            Text(
              body,
              textAlign: TextAlign.center,
              style: theme.textTheme.bodySmall?.copyWith(
                color: scheme.onSurfaceVariant,
              ),
            ),
            const SizedBox(height: 18),
            Wrap(
              spacing: 10,
              runSpacing: 10,
              alignment: WrapAlignment.center,
              children: [
                if (onNextStudy == null)
                  FilledButton(
                    onPressed: onRetry,
                    child: Text(
                      won
                          ? strings.trainingPracticeAgain
                          : strings.trainingDrillRetry,
                    ),
                  )
                else ...[
                  OutlinedButton(
                    onPressed: onRetry,
                    child: Text(strings.trainingPracticeAgain),
                  ),
                  FilledButton(
                    key: const Key('study-next-button'),
                    onPressed: onNextStudy,
                    child: Row(
                      mainAxisSize: MainAxisSize.min,
                      children: [
                        Text(strings.trainingNextStudy),
                        const SizedBox(width: 8),
                        const Icon(Icons.arrow_forward_rounded, size: 18),
                      ],
                    ),
                  ),
                ],
              ],
            ),
          ],
        ),
      ),
    );
  }
}

class _NoticeBox extends StatelessWidget {
  const _NoticeBox({required this.icon, required this.text, this.action});

  final IconData icon;
  final String text;
  final Widget? action;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;
    return Container(
      padding: const EdgeInsets.all(14),
      decoration: BoxDecoration(
        color: scheme.surfaceContainerHigh,
        borderRadius: BorderRadius.circular(12),
        border: Border.all(color: scheme.outlineVariant),
      ),
      child: Row(
        children: [
          Icon(icon, size: 18, color: scheme.onSurfaceVariant),
          const SizedBox(width: 10),
          Expanded(
            child: Text(
              text,
              style: theme.textTheme.bodySmall?.copyWith(
                color: scheme.onSurfaceVariant,
              ),
            ),
          ),
          ?action,
        ],
      ),
    );
  }
}
