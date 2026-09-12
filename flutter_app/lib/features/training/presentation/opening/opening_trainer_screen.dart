// -----------------------------------------------------------------------------
// Section: Opening drill — find the book move against whatever the book plays
// -----------------------------------------------------------------------------

import 'dart:async';
import 'dart:math' as math;

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import '../../../../ffi/core_gateway.dart';
import '../../../../localization/generated/app_localizations.dart';
import '../../../../shared/widgets/chess_board_view.dart';
import '../../../analysis/presentation/analysis_move_arrow.dart';
import '../../models/practice_models.dart';
import 'opening_trainer_panels.dart';

/// A distraction-free drill over one native opening session.
///
/// Native owns the whole scenario: it plays a weighted book reply for the
/// opponent, judges every user move against the book for the position actually
/// on the board, and reveals the answer only after a miss. This screen announces
/// the reply, forwards drags, and paints what the snapshot says.
class OpeningTrainerScreen extends StatefulWidget {
  const OpeningTrainerScreen({
    required this.gateway,
    required this.request,
    required this.title,
    super.key,
  });
  final CoreGateway gateway;
  final Map<String, Object?> request;
  final String title;
  @override
  State<OpeningTrainerScreen> createState() => _OpeningTrainerScreenState();
}

class _OpeningTrainerScreenState extends State<OpeningTrainerScreen>
    with SingleTickerProviderStateMixin {
  PracticeSnapshot? _snapshot;
  bool _busy = true, _error = false;
  int _generation = 0;

  /// Answers found in a row without a miss — the drill's own scoreboard.
  int _streak = 0;

  /// Flashes the board frame green for one beat after an accepted answer.
  bool _applause = false;

  /// Drives the halo on the piece the user should have moved.
  late final AnimationController _hintPulse = AnimationController(
    vsync: this,
    duration: const Duration(milliseconds: 900),
  );

  @override
  void initState() {
    super.initState();
    unawaited(_restart());
  }

  @override
  void dispose() {
    ++_generation;
    _hintPulse.dispose();
    final id = _snapshot?.id;
    if (id != null) unawaited(_cancel(id));
    super.dispose();
  }

  // ---------------------------------------------------------------------------
  // Section: Native session conversation
  // ---------------------------------------------------------------------------

  Future<void> _cancel(String id) async {
    try {
      await widget.gateway.practiceCommand({'op': 'cancel', 'session': id});
    } catch (_) {}
  }

  Future<PracticeSnapshot> _command(Map<String, Object?> request) async =>
      PracticeSnapshot.fromJson(
        (await widget.gateway.practiceCommand(request))! as Map<String, Object?>,
      );

  Future<void> _restart() async {
    final generation = ++_generation;
    setState(() {
      _busy = true;
      _error = false;
      _streak = 0;
    });
    final previous = _snapshot?.id;
    if (previous != null) await _cancel(previous);
    try {
      final result = await _command({...widget.request, 'op': 'start'});
      if (!mounted || generation != _generation) {
        await _cancel(result.id);
        return;
      }
      _adopt(result);
    } catch (_) {
      if (mounted && generation == _generation) {
        setState(() {
          _busy = false;
          _error = true;
        });
      }
    }
  }

  void _adopt(PracticeSnapshot result) {
    setState(() {
      _snapshot = result;
      _busy = false;
    });
    _syncHintPulse(result);
  }

  Future<void> _drop(String source, String target) async {
    final current = _snapshot;
    if (current == null || _busy || current.status != 'active') return;
    final generation = _generation;
    setState(() => _busy = true);
    try {
      final request = <String, Object?>{
        'op': 'move',
        'session': current.id,
        'source': source,
        'target': target,
      };
      var result = await _command(request);
      if (!mounted || generation != _generation) return;
      if (result.promotions.isNotEmpty) {
        result = await _command({...request, 'promotion': 'q'});
      }
      if (!mounted || generation != _generation) return;
      _score(current, result);
      _adopt(result);
    } catch (_) {
      if (mounted && generation == _generation) {
        setState(() {
          _busy = false;
          _error = true;
        });
      }
    }
  }

  /// Keeps the streak and flashes the frame on an accepted answer. An illegal
  /// drop is refused too, but only a judged miss raises the attempt count.
  void _score(PracticeSnapshot before, PracticeSnapshot after) {
    if (after.accepted == true) {
      _streak++;
      _applause = true;
      unawaited(HapticFeedback.selectionClick());
      Future<void>.delayed(const Duration(milliseconds: 640), () {
        if (mounted) setState(() => _applause = false);
      });
    } else if ((after.drill?.attempts ?? 0) > (before.drill?.attempts ?? 0)) {
      _streak = 0;
      unawaited(HapticFeedback.mediumImpact());
    }
  }

  /// Runs the halo pulse exactly while native reveals a missed answer.
  void _syncHintPulse(PracticeSnapshot snapshot) {
    if (_hintMove(snapshot) == null) {
      _hintPulse
        ..stop()
        ..value = 0;
    } else if (!_hintPulse.isAnimating) {
      unawaited(_hintPulse.repeat(reverse: true));
    }
  }

  // ---------------------------------------------------------------------------
  // Section: Snapshot reading
  // ---------------------------------------------------------------------------

  static String? _hintMove(PracticeSnapshot snapshot) {
    final hint = snapshot.drill?.hint;
    return hint != null && hint.length >= 4 ? hint : null;
  }

  static bool _finished(PracticeSnapshot snapshot) =>
      snapshot.status == 'completed' || snapshot.status == 'failed';

  /// The last move of the setup line in move-number notation, e.g. `1... c5`.
  static String? _setupMove(OpeningDrillState drill) {
    final moves = drill.openingMoves;
    if (moves.isEmpty) return null;
    final index = moves.length - 1;
    final number = index ~/ 2 + 1;
    return index.isEven ? '$number. ${moves[index]}' : '$number... ${moves[index]}';
  }

  static String _answerVerdict(AppLocalizations strings, OpeningDrillAnswer answer) =>
      answer.rank <= 1
      ? strings.trainingOpeningCorrect(answer.san)
      : strings.trainingOpeningAlternative(answer.san, answer.rank);

  // ---------------------------------------------------------------------------
  // Section: Build
  // ---------------------------------------------------------------------------

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    return Theme(
      data: openingStudioTheme(),
      child: OpeningStudioBackdrop(
        child: Scaffold(
          backgroundColor: Colors.transparent,
          appBar: AppBar(
            systemOverlayStyle: SystemUiOverlayStyle.light,
            backgroundColor: Colors.transparent,
            surfaceTintColor: Colors.transparent,
            centerTitle: true,
            foregroundColor: OpeningStudio.textPrimary,
            title: Text(
              widget.title,
              overflow: TextOverflow.ellipsis,
              style: const TextStyle(
                fontSize: 17,
                fontWeight: FontWeight.w700,
                letterSpacing: -0.2,
              ),
            ),
            actions: [
              IconButton(
                key: const Key('opening-trainer-restart'),
                tooltip: strings.trainingRestart,
                onPressed: _busy ? null : _restart,
                icon: const Icon(Icons.restart_alt_rounded),
              ),
              const SizedBox(width: 4),
            ],
            bottom: _busy
                ? const PreferredSize(
                    preferredSize: Size.fromHeight(2),
                    child: LinearProgressIndicator(
                      minHeight: 2,
                      backgroundColor: Colors.transparent,
                    ),
                  )
                : null,
          ),
          body: SafeArea(child: _buildBody(strings)),
        ),
      ),
    );
  }

  Widget _buildBody(AppLocalizations strings) {
    if (_error) return _buildError(strings);
    final current = _snapshot;
    final drill = current?.drill;
    if (current == null || drill == null) {
      return const Center(child: CircularProgressIndicator());
    }
    final finished = _finished(current);

    return LayoutBuilder(
      builder: (context, constraints) {
        // Room the banner and the depth meter want around the board.
        const chrome = 132.0 + 76.0 + 56.0;
        final width = math.min(constraints.maxWidth, 560.0);
        final boardSide = math.max(
          240.0,
          math.min(width - 32, constraints.maxHeight - chrome),
        );
        return SingleChildScrollView(
          padding: const EdgeInsets.fromLTRB(16, 8, 16, 24),
          child: Center(
            child: ConstrainedBox(
              constraints: BoxConstraints(maxWidth: math.max(boardSide, width - 32)),
              child: Column(
                children: [
                  _buildBanner(strings, current, drill),
                  const SizedBox(height: 18),
                  OpeningBoardFrame(
                    side: boardSide,
                    accent: _applause ? OpeningStudio.success : null,
                    child: _buildBoardStack(current, drill),
                  ),
                  const SizedBox(height: 18),
                  OpeningDepthMeter(
                    depth: drill.depth,
                    targetDepth: drill.targetDepth,
                    finished: finished,
                    streak: _streak,
                  ),
                  if (finished) ...[
                    const SizedBox(height: 14),
                    OpeningDrillResultCard(
                      depth: drill.depth,
                      clean: current.clean,
                      bookExhausted: drill.bookExhausted,
                      onDrillAgain: _restart,
                    ),
                  ],
                ],
              ),
            ),
          ),
        );
      },
    );
  }

  Widget _buildError(AppLocalizations strings) => Center(
    child: Padding(
      padding: const EdgeInsets.all(28),
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          const Icon(
            Icons.cloud_off_rounded,
            size: 42,
            color: OpeningStudio.textMuted,
          ),
          const SizedBox(height: 14),
          Text(
            strings.trainingOpeningTreeLoadFailed,
            textAlign: TextAlign.center,
            style: const TextStyle(color: OpeningStudio.textPrimary),
          ),
          const SizedBox(height: 22),
          FilledButton.icon(
            onPressed: _restart,
            icon: const Icon(Icons.refresh_rounded, size: 18),
            label: Text(strings.trainingRestart),
          ),
        ],
      ),
    ),
  );

  Widget _buildBanner(
    AppLocalizations strings,
    PracticeSnapshot current,
    OpeningDrillState drill,
  ) {
    final hintSan = drill.hintSan ?? drill.hint;
    if (_hintMove(current) != null && hintSan != null) {
      return OpeningDrillBanner(
        tone: OpeningBannerTone.miss,
        headline: strings.trainingOpeningPlayInstead(hintSan),
        highlight: hintSan,
        instruction:
            '${strings.trainingOpeningIncorrectMove}. ${strings.trainingOpeningTryAgain}',
      );
    }

    final setup = _setupMove(drill);
    if (_finished(current)) {
      final nothingAsked = drill.bookExhausted && drill.depth == 0;
      final answer = drill.answer;
      return OpeningDrillBanner(
        tone: OpeningBannerTone.done,
        headline: nothingAsked && setup != null
            ? strings.trainingOpeningScenarioReady(setup)
            : strings.trainingOpeningDepthReached(drill.depth),
        highlight: nothingAsked ? setup : null,
        instruction: nothingAsked
            ? strings.trainingOpeningNoBook
            : drill.bookExhausted
            ? strings.trainingOpeningBookExhausted
            : answer != null
            ? _answerVerdict(strings, answer)
            : strings.trainingOpeningDrillClean,
      );
    }

    final reply = drill.opponentMove;
    final answer = drill.answer;
    if (reply != null) {
      return OpeningDrillBanner(
        tone: OpeningBannerTone.prompt,
        headline: strings.trainingOpeningOpponentPlayed(reply.notation),
        highlight: reply.notation,
        instruction: strings.trainingOpeningFindBest,
        footnote: answer == null ? null : '✓ ${_answerVerdict(strings, answer)}',
      );
    }
    return OpeningDrillBanner(
      tone: OpeningBannerTone.prompt,
      headline: setup == null
          ? strings.trainingYourMove
          : strings.trainingOpeningScenarioReady(setup),
      highlight: setup,
      instruction: strings.trainingOpeningFindBest,
    );
  }

  // ---------------------------------------------------------------------------
  // Section: Board layers
  // ---------------------------------------------------------------------------

  Widget _buildBoardStack(PracticeSnapshot current, OpeningDrillState drill) {
    final blackAtBottom = current.solverColor == 'black';
    final hint = _hintMove(current);
    // The opponent's reply stays marked until the user answers or misses.
    final reply = hint == null && !_finished(current) ? drill.opponentMove : null;

    final tints = <String, Color>{
      if (hint != null) ...{
        hint.substring(0, 2): OpeningStudio.success.withValues(alpha: 0.34),
        hint.substring(2, 4): OpeningStudio.success.withValues(alpha: 0.22),
      } else if (reply != null) ...{
        reply.uci.substring(0, 2): OpeningStudio.accent.withValues(alpha: 0.18),
        reply.uci.substring(2, 4): OpeningStudio.accent.withValues(alpha: 0.30),
      },
    };

    return Stack(
      key: const Key('opening-trainer-board'),
      fit: StackFit.expand,
      children: [
        ChessBoardView(
          position: current.position,
          blackAtBottom: blackAtBottom,
          interactive: !_busy && current.status == 'active',
          squareTint: tints.isEmpty
              ? null
              : (square, base) {
                  final tint = tints[square];
                  return tint == null ? base : Color.alphaBlend(tint, base);
                },
          onSquareTap: (_) {},
          onPieceDrop: _drop,
        ),
        if (reply != null && reply.uci.length >= 4) ...[
          OpeningSquareGlow(
            key: const Key('opening-trainer-reply-glow'),
            squares: {reply.uci.substring(2, 4): OpeningStudio.accent},
            blackAtBottom: blackAtBottom,
          ),
          AnalysisMoveArrow(
            move: reply.uci,
            color: Colors.white.withValues(alpha: 0.62),
            blackAtBottom: blackAtBottom,
            thickness: 0.12,
            paintKey: ValueKey('opening-reply-${reply.uci}'),
          ),
        ],
        if (hint != null) ...[
          // The piece that should move breathes; its square and target glow.
          AnimatedBuilder(
            animation: _hintPulse,
            builder: (context, _) => OpeningSquareGlow(
              key: const Key('opening-trainer-hint-glow'),
              squares: {
                hint.substring(0, 2): OpeningStudio.success.withValues(
                  alpha: 0.5 + 0.5 * _hintPulse.value,
                ),
                hint.substring(2, 4): OpeningStudio.success.withValues(alpha: 0.7),
              },
              blackAtBottom: blackAtBottom,
            ),
          ),
          AnalysisMoveArrow(
            move: hint,
            color: OpeningStudio.success,
            blackAtBottom: blackAtBottom,
            thickness: 0.22,
            glow: 0.2,
            paintKey: ValueKey('opening-hint-$hint'),
          ),
        ],
      ],
    );
  }
}
