import 'dart:async';

import 'package:flutter/material.dart';

import '../../../ffi/core_gateway.dart';
import '../../../localization/generated/app_localizations.dart';
import '../../../services/training_progress_service.dart';
import '../../../shared/models/models.dart';
import '../../../shared/theme/app_theme.dart';
import '../../../shared/widgets/chess_board_view.dart';
import '../data/endgame_defender_bot.dart';
import '../data/endgame_position_generator.dart';
import '../models/endgame_drill.dart';

/// How a drill attempt ended.
enum DrillOutcome { playing, checkmate, stalemate, moveLimit, drawn }

/// Play-versus-engine drill: a generated position, the user attacking, the
/// app's Stockfish defending.
class EndgameDrillPlayer extends StatefulWidget {
  const EndgameDrillPlayer({
    required this.drill,
    required this.level,
    required this.gateway,
    required this.progress,
    this.generator,
    this.bot,
    this.replyDelay = const Duration(milliseconds: 300),
    super.key,
  });

  final EndgameDrill drill;
  final DrillLevel level;
  final CoreGateway gateway;
  final TrainingProgressService progress;

  /// Injected by tests; production builds one from [gateway].
  final EndgamePositionGenerator? generator;
  final EndgameDefenderBot? bot;
  final Duration replyDelay;

  @override
  State<EndgameDrillPlayer> createState() => _EndgameDrillPlayerState();
}

class _EndgameDrillPlayerState extends State<EndgameDrillPlayer> {
  late final EndgamePositionGenerator _generator =
      widget.generator ?? EndgamePositionGenerator(widget.gateway);
  late final EndgameDefenderBot _bot =
      widget.bot ?? EndgameDefenderBot(widget.gateway);

  BoardPosition? _position;
  List<BoardMoveOption> _legalMoves = const [];
  String _fen = '';

  /// Attacker moves played so far — the number the counter shows.
  int _moves = 0;
  bool _loading = true;
  bool _failedToGenerate = false;
  bool _busy = false;
  bool _engineDegraded = false;
  DrillOutcome _outcome = DrillOutcome.playing;

  /// Set when the finished attempt mastered this level and a harder one exists.
  DrillLevel? _unlockedLevel;

  @override
  void initState() {
    super.initState();
    widget.progress.load();
    unawaited(_newPosition());
  }

  @override
  void dispose() {
    _bot.dispose();
    super.dispose();
  }

  String get _progressId => widget.drill.progressId(widget.level.index);

  Future<void> _newPosition() async {
    setState(() {
      _loading = true;
      _failedToGenerate = false;
      _busy = false;
      _engineDegraded = false;
      _moves = 0;
      _outcome = DrillOutcome.playing;
      _unlockedLevel = null;
    });
    final fen = await _generator.generate(widget.drill, widget.level);
    if (!mounted) return;
    if (fen == null) {
      setState(() {
        _loading = false;
        _failedToGenerate = true;
      });
      return;
    }
    await _load(fen);
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
        _failedToGenerate = true;
      });
    }
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
    // Illegal drags simply snap back — in a free-play drill there is no "wrong
    // move" to correct, only moves the rules do not allow.
    if (option == null) return;

    final fenBefore = _fen;
    setState(() => _moves += 1);
    await _load(option.fenAfter);
    if (!mounted || _failedToGenerate) return;

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
    if (_moves >= widget.level.maxMoves) {
      await _finish(DrillOutcome.moveLimit);
      return;
    }

    // The busy flag locks the board while the defender thinks, so it has to be
    // cleared even if the reply throws — otherwise one failure freezes the
    // drill for good.
    setState(() => _busy = true);
    try {
      await _playDefenderReply(fenBefore, option.uci);
    } catch (_) {
      // The bot absorbs engine failures itself, so anything surfacing here is
      // unexpected. Show it rather than let it vanish into the caller's
      // unawaited future, which is how a silent defender went unnoticed before.
      if (mounted) setState(() => _engineDegraded = true);
    } finally {
      if (mounted) setState(() => _busy = false);
    }
    if (!mounted) return;

    // After the defender's reply it is the attacker to move again; no legal
    // move here means the attacker is mated or stalemated, so the drill is lost
    // rather than won.
    final afterDefender = _position;
    if (afterDefender == null) return;
    // The defender capturing the last mating piece ends it just as surely as
    // having no legal move.
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
    final updated = await widget.progress.markExerciseCompleted(
      _progressId,
      outcome == DrillOutcome.checkmate,
    );
    // Offer the next tier the moment this one is mastered, so a finished level
    // does not dead-end on "another position of the same difficulty".
    final next = widget.drill.levelAt(widget.level.index + 1);
    if (!mounted || next == null || !updated.isMastered) return;
    setState(() => _unlockedLevel = next);
  }

  void _openLevel(DrillLevel level) {
    // Replace, so working up the tiers does not stack routes behind you.
    Navigator.of(context).pushReplacement(
      MaterialPageRoute<void>(
        builder: (_) => EndgameDrillPlayer(
          drill: widget.drill,
          level: level,
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

    return Scaffold(
      appBar: AppBar(
        title: Text(widget.drill.title),
        actions: [
          IconButton(
            onPressed: _loading ? null : () => unawaited(_newPosition()),
            tooltip: strings.trainingDrillNewPosition,
            icon: const Icon(Icons.casino_outlined),
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
                _DrillHeader(
                  level: widget.level,
                  moves: _moves,
                  goalText: strings.trainingDrillGoalCheckmate,
                ),
                const SizedBox(height: 14),
                if (_failedToGenerate)
                  _DrillNotice(
                    icon: Icons.error_outline,
                    text: strings.trainingDrillGenerateFailed,
                    action: TextButton(
                      onPressed: () => unawaited(_newPosition()),
                      child: Text(strings.trainingDrillRetry),
                    ),
                  )
                else if (_loading || position == null)
                  SizedBox(
                    height: 280,
                    child: Center(
                      child: Column(
                        mainAxisSize: MainAxisSize.min,
                        children: [
                          const CircularProgressIndicator(),
                          const SizedBox(height: 12),
                          Text(
                            strings.trainingDrillGenerating,
                            style: theme.textTheme.bodySmall?.copyWith(
                              color: scheme.onSurfaceVariant,
                            ),
                          ),
                        ],
                      ),
                    ),
                  )
                else
                  AspectRatio(
                    aspectRatio: 1,
                    child: Stack(
                      key: const Key('drill-board'),
                      fit: StackFit.expand,
                      children: [
                        ClipRRect(
                          borderRadius: BorderRadius.circular(10),
                          child: ChessBoardView(
                            position: position,
                            interactive:
                                !_busy && _outcome == DrillOutcome.playing,
                            onSquareTap: (_) {},
                            onPieceDrop: (source, target) =>
                                unawaited(_onPieceDrop(source, target)),
                          ),
                        ),
                        if (_outcome != DrillOutcome.playing)
                          _DrillOutcomeOverlay(
                            outcome: _outcome,
                            moves: _moves,
                            maxMoves: widget.level.maxMoves,
                            onRetry: () => unawaited(_newPosition()),
                            onNextLevel: _unlockedLevel == null
                                ? null
                                : () => _openLevel(_unlockedLevel!),
                          ),
                      ],
                    ),
                  ),
                const SizedBox(height: 14),
                if (_engineDegraded) ...[
                  _DrillNotice(
                    icon: Icons.warning_amber_rounded,
                    text: strings.trainingDrillEngineFallback,
                  ),
                  const SizedBox(height: 12),
                ],
                _DrillNotice(
                  icon: _busy ? Icons.more_horiz : Icons.lightbulb_outline,
                  text: _busy
                      ? strings.trainingOpponentThinking
                      : widget.drill.tip,
                ),
              ],
            ),
          ),
        ),
      ),
    );
  }
}

/// Goal line, difficulty chip and the move budget.
class _DrillHeader extends StatelessWidget {
  const _DrillHeader({
    required this.level,
    required this.moves,
    required this.goalText,
  });

  final DrillLevel level;
  final int moves;
  final String goalText;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;
    // The counter turns amber as the budget runs out, so the limit never
    // arrives as a surprise.
    final tight = moves >= level.maxMoves - 3;

    return Row(
      children: [
        Expanded(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text(
                goalText,
                style: theme.textTheme.titleMedium?.copyWith(
                  fontWeight: FontWeight.w800,
                ),
              ),
              const SizedBox(height: 4),
              DrillDifficultyChip(difficulty: level.difficulty),
            ],
          ),
        ),
        const SizedBox(width: 12),
        Text(
          strings.trainingDrillMoveCounter(moves, level.maxMoves),
          key: const Key('drill-move-counter'),
          style: theme.textTheme.titleMedium?.copyWith(
            fontWeight: FontWeight.w800,
            color: tight ? AppTheme.warning : scheme.onSurfaceVariant,
          ),
        ),
      ],
    );
  }
}

/// Difficulty tag shared by the catalogue and the player header.
class DrillDifficultyChip extends StatelessWidget {
  const DrillDifficultyChip({required this.difficulty, super.key});

  final String difficulty;

  static String labelFor(AppLocalizations strings, String difficulty) =>
      switch (difficulty) {
        DrillDifficulties.beginner => strings.trainingDifficultyBeginner,
        DrillDifficulties.intermediate =>
          strings.trainingDifficultyIntermediate,
        _ => strings.trainingDifficultyMaster,
      };

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;
    final color = switch (difficulty) {
      DrillDifficulties.beginner => AppTheme.success,
      DrillDifficulties.intermediate => AppTheme.warning,
      _ => scheme.error,
    };

    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 9, vertical: 3),
      decoration: BoxDecoration(
        color: color.withValues(alpha: 0.15),
        borderRadius: BorderRadius.circular(8),
        border: Border.all(color: color.withValues(alpha: 0.7)),
      ),
      child: Text(
        labelFor(strings, difficulty),
        style: theme.textTheme.bodySmall?.copyWith(
          fontWeight: FontWeight.w700,
          color: color,
        ),
      ),
    );
  }
}

class _DrillOutcomeOverlay extends StatelessWidget {
  const _DrillOutcomeOverlay({
    required this.outcome,
    required this.moves,
    required this.maxMoves,
    required this.onRetry,
    this.onNextLevel,
  });

  final DrillOutcome outcome;
  final int moves;
  final int maxMoves;
  final VoidCallback onRetry;

  /// Non-null only when the win just mastered this level and a harder tier
  /// exists.
  final VoidCallback? onNextLevel;

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
      DrillOutcome.moveLimit => (
        Icons.timer_off_outlined,
        strings.trainingDrillLimitTitle,
        strings.trainingDrillLimitBody(maxMoves),
      ),
      _ => (
        Icons.handshake_outlined,
        strings.trainingDrillDrawTitle,
        strings.trainingDrillDrawBody,
      ),
    };

    return Container(
      key: Key('drill-outcome-${outcome.name}'),
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
                if (onNextLevel == null)
                  FilledButton(
                    onPressed: onRetry,
                    child: Text(
                      won
                          ? strings.trainingDrillNewPosition
                          : strings.trainingDrillRetry,
                    ),
                  )
                else ...[
                  OutlinedButton(
                    onPressed: onRetry,
                    child: Text(strings.trainingDrillNewPosition),
                  ),
                  FilledButton(
                    key: const Key('drill-next-level'),
                    onPressed: onNextLevel,
                    child: Row(
                      mainAxisSize: MainAxisSize.min,
                      children: [
                        Text(strings.trainingDrillNextLevel),
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

class _DrillNotice extends StatelessWidget {
  const _DrillNotice({required this.icon, required this.text, this.action});

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
