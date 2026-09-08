import 'dart:async';

import 'package:flutter/material.dart';

import '../../../ffi/core_gateway.dart';
import '../../../localization/generated/app_localizations.dart';
import '../../../services/training_progress_service.dart';
import '../../../shared/models/models.dart';
import '../../../shared/theme/app_theme.dart';
import '../../../shared/widgets/chess_board_view.dart';
import '../data/training_library.dart';
import '../models/models.dart';
import 'training_arena_screen.dart' show MasteryProgressBar;

/// Listudy-style catalogue of theoretical endgames: every seeded position with
/// its mastery state, tapping one opens the interactive player.
class EndgameAcademyScreen extends StatefulWidget {
  const EndgameAcademyScreen({
    required this.gateway,
    required this.progress,
    super.key,
  });

  final CoreGateway gateway;
  final TrainingProgressService progress;

  @override
  State<EndgameAcademyScreen> createState() => _EndgameAcademyScreenState();
}

class _EndgameAcademyScreenState extends State<EndgameAcademyScreen> {
  @override
  void initState() {
    super.initState();
    widget.progress.load();
  }

  void _open(TrainingExercise exercise) {
    Navigator.of(context).push(
      MaterialPageRoute<void>(
        builder: (_) => EndgameExercisePlayer(
          exercise: exercise,
          gateway: widget.gateway,
          progress: widget.progress,
        ),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final exercises = TrainingLibrary.byCategory(TrainingCategories.endgame);

    return Scaffold(
      appBar: AppBar(title: Text(strings.trainingEndgameTitle)),
      body: StreamBuilder<TrainingProgressSnapshot>(
        stream: widget.progress.changes,
        initialData: widget.progress.snapshot,
        builder: (context, snapshot) {
          final progress =
              snapshot.data ?? const TrainingProgressSnapshot.empty();
          final mastered = progress.masteryCount(
            TrainingCategories.endgame,
            TrainingLibrary.all,
          );
          return ListView(
            padding: const EdgeInsets.fromLTRB(16, 12, 16, 28),
            children: [
              Center(
                child: ConstrainedBox(
                  constraints: const BoxConstraints(maxWidth: 780),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.stretch,
                    children: [
                      Card(
                        child: Padding(
                          padding: const EdgeInsets.all(18),
                          child: MasteryProgressBar(
                            mastered: mastered,
                            total: exercises.length,
                          ),
                        ),
                      ),
                      const SizedBox(height: 16),
                      for (var i = 0; i < exercises.length; i++) ...[
                        if (i > 0) const SizedBox(height: 12),
                        _ExerciseCard(
                          exercise: exercises[i],
                          progress: progress.progressFor(exercises[i].id),
                          onTap: () => _open(exercises[i]),
                        ),
                      ],
                    ],
                  ),
                ),
              ),
            ],
          );
        },
      ),
    );
  }
}

/// Catalogue card: title, the hint as its description, and a mastery badge.
class _ExerciseCard extends StatelessWidget {
  const _ExerciseCard({
    required this.exercise,
    required this.progress,
    required this.onTap,
  });

  final TrainingExercise exercise;
  final ExerciseProgress progress;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;
    final lastAttempt = progress.lastAttemptDate;

    return Card(
      child: InkWell(
        onTap: onTap,
        child: Padding(
          padding: const EdgeInsets.all(16),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Row(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Expanded(
                    child: Text(
                      exercise.title,
                      style: theme.textTheme.titleSmall?.copyWith(
                        fontWeight: FontWeight.w800,
                      ),
                    ),
                  ),
                  const SizedBox(width: 10),
                  _MasteryBadge(progress: progress),
                ],
              ),
              const SizedBox(height: 8),
              Text(
                exercise.hintText,
                style: theme.textTheme.bodySmall?.copyWith(
                  color: scheme.onSurfaceVariant,
                ),
              ),
              const SizedBox(height: 10),
              Row(
                children: [
                  Icon(Icons.history, size: 15, color: scheme.onSurfaceVariant),
                  const SizedBox(width: 6),
                  Expanded(
                    child: Text(
                      lastAttempt == null
                          ? strings.trainingNeverAttempted
                          : strings.trainingLastAttempt(
                              _formatDate(lastAttempt),
                            ),
                      style: theme.textTheme.bodySmall?.copyWith(
                        color: scheme.onSurfaceVariant,
                      ),
                    ),
                  ),
                  Icon(Icons.chevron_right, color: scheme.onSurfaceVariant),
                ],
              ),
            ],
          ),
        ),
      ),
    );
  }
}

class _MasteryBadge extends StatelessWidget {
  const _MasteryBadge({required this.progress});

  final ExerciseProgress progress;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;
    final mastered = progress.isMastered;

    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 5),
      decoration: BoxDecoration(
        color: mastered
            ? AppTheme.success.withValues(alpha: 0.16)
            : scheme.surfaceContainerHigh,
        borderRadius: BorderRadius.circular(9),
        border: Border.all(
          color: mastered ? AppTheme.success : scheme.outlineVariant,
        ),
      ),
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          Icon(
            mastered ? Icons.star_rounded : Icons.timeline,
            size: 15,
            color: mastered ? AppTheme.success : scheme.onSurfaceVariant,
          ),
          const SizedBox(width: 5),
          Text(
            mastered
                ? strings.trainingMastered
                : strings.trainingStreak(
                    progress.successStreak,
                    ExerciseProgress.masteryThreshold,
                  ),
            style: theme.textTheme.bodySmall?.copyWith(
              fontWeight: FontWeight.w700,
              color: mastered ? AppTheme.success : scheme.onSurfaceVariant,
            ),
          ),
        ],
      ),
    );
  }
}

/// Interactive player for one exercise.
///
/// Every chess question — which drags are legal, what a move is called, which
/// position it reaches — is answered by the native core through
/// [CoreGateway.boardLegalMoves]; this widget only compares strings and walks
/// the exercise's line.
class EndgameExercisePlayer extends StatefulWidget {
  const EndgameExercisePlayer({
    required this.exercise,
    required this.gateway,
    required this.progress,
    super.key,
  });

  final TrainingExercise exercise;
  final CoreGateway gateway;
  final TrainingProgressService progress;

  @override
  State<EndgameExercisePlayer> createState() => _EndgameExercisePlayerState();
}

class _EndgameExercisePlayerState extends State<EndgameExercisePlayer> {
  /// How long the opponent "thinks" before its scripted reply appears.
  static const _replyDelay = Duration(milliseconds: 350);

  /// How long a square stays tinted after a right or wrong move.
  static const _flashDuration = Duration(milliseconds: 450);

  /// Index into [TrainingExercise.targetMovesSan] of the move due next.
  int _ply = 0;

  BoardPosition? _position;
  List<BoardMoveOption> _legalMoves = const [];
  bool _loading = true;
  bool _failed = false;
  bool _busy = false;
  bool _solved = false;

  /// Whether this run has already needed a correction — only a clean run counts
  /// towards mastery, which is what "completed 3 times without error" means.
  bool _hadError = false;
  bool _hintVisible = false;
  String? _feedback;
  ({String square, bool correct})? _flash;
  Timer? _flashTimer;

  /// The side the solver plays, fixed by whoever is to move in the start FEN.
  late final bool _solverIsWhite =
      widget.exercise.startingFen.split(' ').elementAtOrNull(1) != 'b';

  @override
  void initState() {
    super.initState();
    _restart();
  }

  @override
  void dispose() {
    _flashTimer?.cancel();
    super.dispose();
  }

  Future<void> _restart() async {
    _flashTimer?.cancel();
    setState(() {
      _ply = 0;
      _loading = true;
      _failed = false;
      _busy = false;
      _solved = false;
      _hadError = false;
      _hintVisible = false;
      _feedback = null;
      _flash = null;
    });
    await _loadPosition(widget.exercise.startingFen);
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
    } catch (_) {
      if (!mounted) return;
      setState(() {
        _loading = false;
        _failed = true;
      });
    }
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

  Future<void> _onPieceDrop(String source, String target) async {
    if (_busy || _solved || _loading || _failed) return;
    final line = widget.exercise.targetMovesSan;
    if (_ply >= line.length) return;

    final option = _moveBySquares('$source$target');
    // An illegal drag and a legal-but-wrong move get the same answer: the board
    // never leaves the last correct position, so the piece simply snaps back.
    if (option == null || option.san != line[_ply]) {
      setState(() {
        _hadError = true;
        _feedback = AppLocalizations.of(context).trainingWrongMove;
      });
      _flashSquare(target, correct: false);
      return;
    }

    setState(() => _feedback = null);
    _flashSquare(target, correct: true);
    await _applyMove(option);
    if (!mounted) return;

    if (_ply >= line.length) {
      await _complete();
      return;
    }

    // Remaining entries alternate, so the next one is the opponent's reply.
    setState(() => _busy = true);
    await Future<void>.delayed(_replyDelay);
    if (!mounted) return;

    final reply = _moveBySan(line[_ply]);
    if (reply == null) {
      // The line disagrees with the move generator; the native line test guards
      // against this, so surface it rather than hanging on a dead position.
      setState(() {
        _busy = false;
        _failed = true;
      });
      return;
    }
    _flashSquare(reply.squares.substring(2, 4), correct: true);
    await _applyMove(reply);
    if (!mounted) return;
    setState(() => _busy = false);
    if (_ply >= line.length) await _complete();
  }

  Future<void> _applyMove(BoardMoveOption option) async {
    setState(() => _ply += 1);
    await _loadPosition(option.fenAfter);
  }

  Future<void> _complete() async {
    setState(() => _solved = true);
    await widget.progress.markExerciseCompleted(widget.exercise.id, !_hadError);
  }

  Color _tintFor(String square, Color base) {
    final flash = _flash;
    if (flash == null || flash.square != square) return base;
    return Color.alphaBlend(
      (flash.correct ? AppTheme.success : Theme.of(context).colorScheme.error)
          .withValues(alpha: 0.55),
      base,
    );
  }

  String _goalText(AppLocalizations strings) {
    final side = _solverIsWhite
        ? strings.statsCompareColorWhite
        : strings.statsCompareColorBlack;
    return widget.exercise.goal == TrainingGoals.draw
        ? strings.trainingGoalDraw(side)
        : strings.trainingGoalWin(side);
  }

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;
    final exercise = widget.exercise;
    final position = _position;
    final solverMovesPlayed = (_ply + 1) ~/ 2;

    return Scaffold(
      appBar: AppBar(title: Text(exercise.title)),
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
                    solverMovesPlayed,
                    exercise.solverMoveCount,
                  ),
                  style: theme.textTheme.bodySmall?.copyWith(
                    color: scheme.onSurfaceVariant,
                  ),
                ),
                const SizedBox(height: 14),
                if (_failed)
                  _PlayerMessage(
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
                            blackAtBottom: !_solverIsWhite,
                            interactive: !_busy && !_solved,
                            squareTint: _tintFor,
                            onSquareTap: (_) {},
                            onPieceDrop: (source, target) =>
                                unawaited(_onPieceDrop(source, target)),
                          ),
                        ),
                        if (_solved)
                          _SolvedOverlay(
                            hadError: _hadError,
                            onPractiseAgain: _restart,
                            onNext: TrainingLibrary.next(exercise) == null
                                ? null
                                : () => _openNext(),
                          ),
                      ],
                    ),
                  ),
                const SizedBox(height: 14),
                _StatusLine(busy: _busy, solved: _solved, feedback: _feedback),
                const SizedBox(height: 12),
                if (_hintVisible)
                  _PlayerMessage(
                    icon: Icons.lightbulb_outline,
                    text: exercise.hintText,
                    alignStart: true,
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

  void _openNext() {
    final next = TrainingLibrary.next(widget.exercise);
    if (next == null) return;
    // Replace rather than stack, so working through the catalogue does not grow
    // the back stack and "back" still lands on the list.
    Navigator.of(context).pushReplacement(
      MaterialPageRoute<void>(
        builder: (_) => EndgameExercisePlayer(
          exercise: next,
          gateway: widget.gateway,
          progress: widget.progress,
        ),
      ),
    );
  }
}

/// Whose move it is, or why the last one was rejected.
class _StatusLine extends StatelessWidget {
  const _StatusLine({
    required this.busy,
    required this.solved,
    required this.feedback,
  });

  final bool busy;
  final bool solved;
  final String? feedback;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;

    final (icon, text, color) = switch ((solved, busy, feedback)) {
      (true, _, _) => (
        Icons.check_circle_outline,
        strings.trainingSolvedTitle,
        AppTheme.success,
      ),
      (_, _, final String message) => (
        Icons.info_outline,
        message,
        scheme.error,
      ),
      (_, true, _) => (
        Icons.more_horiz,
        strings.trainingOpponentThinking,
        scheme.onSurfaceVariant,
      ),
      _ => (
        Icons.touch_app_outlined,
        strings.trainingYourMove,
        scheme.onSurfaceVariant,
      ),
    };

    return Row(
      key: const Key('training-status'),
      children: [
        Icon(icon, size: 18, color: color),
        const SizedBox(width: 8),
        Expanded(
          child: Text(
            text,
            style: theme.textTheme.bodyMedium?.copyWith(
              color: color,
              fontWeight: FontWeight.w600,
            ),
          ),
        ),
      ],
    );
  }
}

/// Celebration panel over the solved board.
class _SolvedOverlay extends StatelessWidget {
  const _SolvedOverlay({
    required this.hadError,
    required this.onPractiseAgain,
    required this.onNext,
  });

  final bool hadError;
  final VoidCallback onPractiseAgain;
  final VoidCallback? onNext;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;

    return Container(
      key: const Key('training-solved-overlay'),
      alignment: Alignment.center,
      decoration: BoxDecoration(
        color: scheme.surface.withValues(alpha: 0.88),
        borderRadius: BorderRadius.circular(10),
      ),
      child: Padding(
        padding: const EdgeInsets.all(20),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Icon(
              Icons.emoji_events_rounded,
              size: 44,
              color: hadError ? scheme.tertiary : AppTheme.success,
            ),
            const SizedBox(height: 12),
            Text(
              strings.trainingSolvedTitle,
              textAlign: TextAlign.center,
              style: theme.textTheme.titleMedium?.copyWith(
                fontWeight: FontWeight.w800,
              ),
            ),
            const SizedBox(height: 6),
            Text(
              hadError
                  ? strings.trainingSolvedWithErrors
                  : strings.trainingSolvedClean,
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
                OutlinedButton(
                  onPressed: onPractiseAgain,
                  child: Text(strings.trainingPracticeAgain),
                ),
                if (onNext != null)
                  FilledButton(
                    onPressed: onNext,
                    child: Row(
                      mainAxisSize: MainAxisSize.min,
                      children: [
                        Text(strings.trainingNextEndgame),
                        const SizedBox(width: 8),
                        const Icon(Icons.arrow_forward_rounded, size: 18),
                      ],
                    ),
                  )
                else
                  FilledButton(
                    onPressed: () => Navigator.of(context).pop(),
                    child: Text(strings.trainingBackToList),
                  ),
              ],
            ),
          ],
        ),
      ),
    );
  }
}

class _PlayerMessage extends StatelessWidget {
  const _PlayerMessage({
    required this.icon,
    required this.text,
    this.action,
    this.alignStart = false,
  });

  final IconData icon;
  final String text;
  final Widget? action;
  final bool alignStart;

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
        crossAxisAlignment: alignStart
            ? CrossAxisAlignment.start
            : CrossAxisAlignment.center,
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

/// Same `yyyy-MM-dd` shape the statistics tab uses for game dates.
String _formatDate(DateTime date) {
  final local = date.toLocal();
  String two(int value) => value.toString().padLeft(2, '0');
  return '${local.year}-${two(local.month)}-${two(local.day)}';
}
