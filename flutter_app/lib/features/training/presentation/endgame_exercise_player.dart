// -----------------------------------------------------------------------------
// Section: Native training attempt presentation
// -----------------------------------------------------------------------------

import 'dart:async';

import 'package:flutter/material.dart';

import '../../../ffi/core_gateway.dart';
import '../../../localization/generated/app_localizations.dart';
import '../../../shared/models/models.dart';
import '../../../shared/theme/app_theme.dart';
import '../../../shared/widgets/chess_board_view.dart';
import 'training_localizations.dart';
import 'training_player_widgets.dart';

class EndgameExercisePlayer extends StatefulWidget {
  const EndgameExercisePlayer({
    required this.exercise,
    required this.gateway,
    super.key,
  });

  final TrainingExercise exercise;
  final CoreGateway gateway;

  @override
  State<EndgameExercisePlayer> createState() =>
      _EndgameExercisePlayerState();
}

class _EndgameExercisePlayerState extends State<EndgameExercisePlayer> {
  TrainingAttempt? _attempt;
  BoardPosition? _position;
  bool _loading = true;
  bool _failed = false;
  bool _busy = false;
  bool _solved = false;
  bool _hintVisible = false;
  bool _clean = false;
  int _solverMovesPlayed = 0;
  String? _feedback;
  ({String square, bool correct})? _flash;

  @override
  void initState() {
    super.initState();
    unawaited(_restart());
  }

  Future<void> _restart() async {
    setState(() {
      _loading = true;
      _failed = false;
      _busy = false;
      _solved = false;
      _hintVisible = false;
      _clean = false;
      _solverMovesPlayed = 0;
      _feedback = null;
      _flash = null;
    });
    try {
      final attempt = await widget.gateway.startTrainingAttempt(
        widget.exercise.id,
      );
      if (!mounted) return;
      setState(() {
        _attempt = attempt;
        _position = attempt.position;
        _solverMovesPlayed = attempt.solverMovesPlayed;
        _loading = false;
      });
    } catch (_) {
      if (!mounted) return;
      setState(() {
        _loading = false;
        _failed = true;
      });
    }
  }

  Future<void> _onPieceDrop(String source, String target) async {
    final attempt = _attempt;
    if (attempt == null || _busy || _solved || _failed) return;
    setState(() => _busy = true);
    try {
      final result = await widget.gateway.playTrainingMove(
        attemptId: attempt.attemptId,
        source: source,
        target: target,
      );
      if (!mounted) return;
      setState(() {
        _busy = false;
        _position = result.position;
        _solverMovesPlayed = result.solverMovesPlayed;
        _flash = (square: target, correct: result.accepted);
        _feedback = result.accepted
            ? null
            : AppLocalizations.of(context).trainingWrongMove;
        _solved = result.completed;
        _clean = result.clean ?? false;
      });
    } catch (_) {
      if (!mounted) return;
      setState(() {
        _busy = false;
        _failed = true;
      });
    }
  }

  Color _tintFor(String square, Color base) {
    final flash = _flash;
    if (flash == null || flash.square != square) return base;
    final accent = flash.correct
        ? AppTheme.success
        : Theme.of(context).colorScheme.error;
    return Color.alphaBlend(accent.withValues(alpha: 0.55), base);
  }

  String _goalText(AppLocalizations strings) {
    final side = widget.exercise.solverColor == 'black'
        ? strings.statsCompareColorBlack
        : strings.statsCompareColorWhite;
    return widget.exercise.goal == 'draw'
        ? strings.trainingGoalDraw(side)
        : strings.trainingGoalWin(side);
  }

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final theme = Theme.of(context);
    final position = _position;
    return Scaffold(
      appBar: AppBar(
        title: Text(trainingExerciseTitle(strings, widget.exercise.id)),
      ),
      body: SafeArea(
        child: Center(
          child: ConstrainedBox(
            constraints: const BoxConstraints(maxWidth: 620),
            child: ListView(
              padding: const EdgeInsets.fromLTRB(16, 12, 16, 24),
              children: [
                Text(
                  _goalText(strings),
                  style: theme.textTheme.titleMedium?.copyWith(
                    fontWeight: FontWeight.w800,
                  ),
                ),
                const SizedBox(height: 6),
                Text(
                  strings.trainingMoveProgress(
                    _solverMovesPlayed,
                    widget.exercise.solverMoveCount,
                  ),
                ),
                const SizedBox(height: 14),
                if (_failed)
                  TrainingPlayerMessage(
                    icon: Icons.error_outline,
                    text: strings.trainingBoardError,
                    action: TextButton(
                      onPressed: _restart,
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
                      key: const Key('training-board'),
                      fit: StackFit.expand,
                      children: [
                        ClipRRect(
                          borderRadius: BorderRadius.circular(10),
                          child: ChessBoardView(
                            position: position,
                            blackAtBottom:
                                widget.exercise.solverColor == 'black',
                            interactive: !_busy && !_solved,
                            squareTint: _tintFor,
                            onSquareTap: (_) {},
                            onPieceDrop: (source, target) =>
                                unawaited(_onPieceDrop(source, target)),
                          ),
                        ),
                        if (_solved)
                          TrainingSolvedOverlay(
                            clean: _clean,
                            onPractiseAgain: _restart,
                            onNext: widget.exercise.nextExerciseId == null
                                ? null
                                : _openNext,
                          ),
                      ],
                    ),
                  ),
                const SizedBox(height: 14),
                TrainingStatusLine(
                  busy: _busy,
                  solved: _solved,
                  feedback: _feedback,
                ),
                const SizedBox(height: 12),
                if (_hintVisible)
                  TrainingPlayerMessage(
                    icon: Icons.lightbulb_outline,
                    text: trainingExerciseHint(strings, widget.exercise.id),
                  )
                else
                  Align(
                    alignment: AlignmentDirectional.centerStart,
                    child: TextButton.icon(
                      onPressed: () => setState(() => _hintVisible = true),
                      icon: const Icon(Icons.lightbulb_outline, size: 18),
                      label: Text(strings.trainingShowHint),
                    ),
                  ),
              ],
            ),
          ),
        ),
      ),
    );
  }

  Future<void> _openNext() async {
    final nextId = widget.exercise.nextExerciseId;
    if (nextId == null) return;
    final overview = await widget.gateway.trainingOverview();
    final next = overview.exercise(nextId);
    if (!mounted || next == null) return;
    await Navigator.of(context).pushReplacement(
      MaterialPageRoute<void>(
        builder: (_) => EndgameExercisePlayer(
          exercise: next,
          gateway: widget.gateway,
        ),
      ),
    );
  }
}
