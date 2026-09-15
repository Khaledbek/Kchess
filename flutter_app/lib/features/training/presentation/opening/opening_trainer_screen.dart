// -----------------------------------------------------------------------------
// Section: Opening drill — find the book move against whatever the book plays
// -----------------------------------------------------------------------------

import 'dart:async';

import 'package:flutter/material.dart';

import '../../../../ffi/core_gateway.dart';
import '../../../../localization/generated/app_localizations.dart';
import '../../../../shared/theme/app_theme.dart';
import '../../../../shared/widgets/chess_board_view.dart';
import '../../../analysis/presentation/analysis_move_arrow.dart';
import '../../models/practice_models.dart';
import 'opening_trainer_panels.dart';

/// A calm drill over one native opening session, laid out like the app's other
/// training boards.
///
/// Native owns the whole scenario: it plays a weighted book reply for the
/// opponent, judges every user move against the book for the position actually
/// on the board, and reveals the answer only after a miss. This screen names
/// the reply, forwards drags, and paints what the snapshot says.
///
/// Nothing around the board changes size between moves, and nothing animates
/// on its own: the prompt, the status line and the depth meter keep fixed
/// heights, busy state shows inside the status line, and hints are still
/// square tints and one arrow, as on the analysis board.
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

class _OpeningTrainerScreenState extends State<OpeningTrainerScreen> {
  /// The last-move tint the bot and analysis boards use.
  static const _lastMoveTint = Color(0xFF82A9C5);

  PracticeSnapshot? _snapshot;
  bool _busy = true, _error = false;
  int _generation = 0;

  @override
  void initState() {
    super.initState();
    unawaited(_restart());
  }

  @override
  void dispose() {
    ++_generation;
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
    });
    final previous = _snapshot?.id;
    if (previous != null) await _cancel(previous);
    try {
      final result = await _command({...widget.request, 'op': 'start'});
      if (!mounted || generation != _generation) {
        await _cancel(result.id);
        return;
      }
      setState(() {
        _snapshot = result;
        _busy = false;
      });
    } catch (_) {
      if (mounted && generation == _generation) {
        setState(() {
          _busy = false;
          _error = true;
        });
      }
    }
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
      setState(() {
        _snapshot = result;
        _busy = false;
      });
    } catch (_) {
      if (mounted && generation == _generation) {
        setState(() {
          _busy = false;
          _error = true;
        });
      }
    }
  }

  // ---------------------------------------------------------------------------
  // Section: Snapshot reading
  // ---------------------------------------------------------------------------

  static String? _hintMove(OpeningDrillState drill) {
    final hint = drill.hint;
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
    return Scaffold(
      appBar: AppBar(
        title: Text(widget.title, overflow: TextOverflow.ellipsis),
        actions: [
          IconButton(
            key: const Key('opening-trainer-restart'),
            tooltip: strings.trainingRestart,
            onPressed: _busy ? null : _restart,
            icon: const Icon(Icons.restart_alt_rounded),
          ),
        ],
      ),
      body: SafeArea(child: _buildBody(strings)),
    );
  }

  Widget _buildBody(AppLocalizations strings) {
    if (_error) {
      return Center(
        child: Padding(
          padding: const EdgeInsets.all(16),
          child: _DrillMessage(
            icon: Icons.error_outline,
            text: strings.trainingOpeningTreeLoadFailed,
            action: TextButton(onPressed: _restart, child: Text(strings.trainingRestart)),
          ),
        ),
      );
    }
    final current = _snapshot;
    final drill = current?.drill;
    if (current == null || drill == null) {
      return const Center(child: CircularProgressIndicator());
    }
    final finished = _finished(current);
    return Center(
      child: ConstrainedBox(
        // The same column the endgame player uses: the board's size follows
        // the width alone, so nothing below it can nudge it.
        constraints: const BoxConstraints(maxWidth: 620),
        child: ListView(
          padding: const EdgeInsets.fromLTRB(16, 12, 16, 24),
          children: [
            _buildPrompt(strings, current, drill),
            const SizedBox(height: 12),
            AspectRatio(
              aspectRatio: 1,
              child: Stack(
                key: const Key('opening-trainer-board'),
                fit: StackFit.expand,
                children: [
                  ClipRRect(
                    borderRadius: BorderRadius.circular(10),
                    child: _buildBoard(current, drill),
                  ),
                  if (finished)
                    _DrillFinishedOverlay(
                      depth: drill.depth,
                      clean: current.clean,
                      bookExhausted: drill.bookExhausted,
                      onDrillAgain: _restart,
                    ),
                ],
              ),
            ),
            const SizedBox(height: 12),
            _buildStatus(strings, current, drill),
            const SizedBox(height: 12),
            _DepthMeter(
              depth: drill.depth,
              targetDepth: drill.targetDepth,
              finished: finished,
            ),
          ],
        ),
      ),
    );
  }

  /// What just happened and what to do, in two lines that never change height.
  Widget _buildPrompt(
    AppLocalizations strings,
    PracticeSnapshot current,
    OpeningDrillState drill,
  ) {
    final scheme = Theme.of(context).colorScheme;
    final setup = _setupMove(drill);
    final reply = drill.opponentMove;
    final hintSan = drill.hintSan ?? drill.hint;

    final String headline;
    final String? move;
    final String instruction;
    var missed = false;
    if (_hintMove(drill) != null && hintSan != null) {
      headline = strings.trainingOpeningPlayInstead(hintSan);
      move = hintSan;
      instruction = '${strings.trainingOpeningIncorrectMove}. ${strings.trainingOpeningTryAgain}';
      missed = true;
    } else if (_finished(current)) {
      final nothingAsked = drill.bookExhausted && drill.depth == 0;
      headline = nothingAsked && setup != null
          ? strings.trainingOpeningScenarioReady(setup)
          : strings.trainingOpeningDepthReached(drill.depth);
      move = nothingAsked ? setup : null;
      instruction = nothingAsked
          ? strings.trainingOpeningNoBook
          : drill.bookExhausted
          ? strings.trainingOpeningBookExhausted
          : current.clean
          ? strings.trainingOpeningDrillClean
          : strings.trainingOpeningDrillWithErrors;
    } else if (reply != null) {
      headline = strings.trainingOpeningOpponentPlayed(reply.notation);
      move = reply.notation;
      instruction = strings.trainingOpeningFindBest;
    } else {
      headline = setup == null
          ? strings.trainingYourMove
          : strings.trainingOpeningScenarioReady(setup);
      move = setup;
      instruction = strings.trainingOpeningFindBest;
    }

    return _DrillMessage(
      key: const Key('opening-trainer-banner'),
      icon: missed ? Icons.close_rounded : Icons.menu_book_outlined,
      iconColor: missed ? scheme.error : scheme.primary,
      headline: _PromptHeadline(
        text: headline,
        move: move,
        moveColor: missed ? scheme.error : scheme.primary,
      ),
      text: instruction,
    );
  }

  Widget _buildBoard(PracticeSnapshot current, OpeningDrillState drill) {
    final scheme = Theme.of(context).colorScheme;
    final blackAtBottom = current.solverColor == 'black';
    final hint = _hintMove(drill);
    final reply = drill.opponentMove;
    final tints = <String, Color>{
      if (hint != null) ...{
        hint.substring(0, 2): scheme.primaryContainer.withValues(alpha: 0.78),
        hint.substring(2, 4): scheme.secondaryContainer.withValues(alpha: 0.96),
      } else if (reply != null && reply.uci.length >= 4) ...{
        reply.uci.substring(0, 2): _lastMoveTint,
        reply.uci.substring(2, 4): _lastMoveTint,
      },
    };
    return Stack(
      fit: StackFit.expand,
      children: [
        ChessBoardView(
          position: current.position,
          blackAtBottom: blackAtBottom,
          interactive: !_busy && current.status == 'active',
          squareTint: tints.isEmpty ? null : (square, base) => tints[square] ?? base,
          onSquareTap: (_) {},
          onPieceDrop: (source, target) => unawaited(_drop(source, target)),
        ),
        if (hint != null)
          AnalysisMoveArrow(
            move: hint,
            color: scheme.tertiary,
            blackAtBottom: blackAtBottom,
            paintKey: ValueKey('opening-hint-$hint'),
          ),
      ],
    );
  }

  /// One line under the board, always the same height: busy, the verdict on
  /// the last answer, the miss, or whose move it is.
  Widget _buildStatus(
    AppLocalizations strings,
    PracticeSnapshot current,
    OpeningDrillState drill,
  ) {
    final scheme = Theme.of(context).colorScheme;
    final answer = drill.answer;
    final missed = _hintMove(drill) != null;
    final (IconData icon, Color color, String text) = missed
        ? (Icons.close_rounded, scheme.error, strings.trainingOpeningIncorrectMove)
        : answer != null
        ? (Icons.check_circle_outline, AppTheme.success, _answerVerdict(strings, answer))
        : (Icons.touch_app_outlined, scheme.onSurfaceVariant, strings.trainingYourMove);
    return SizedBox(
      key: const Key('opening-trainer-status'),
      height: 24,
      child: Row(
        children: [
          SizedBox.square(
            dimension: 18,
            child: _busy
                ? const CircularProgressIndicator(strokeWidth: 2)
                : Icon(icon, size: 18, color: color),
          ),
          const SizedBox(width: 8),
          Expanded(
            child: Text(
              text,
              maxLines: 1,
              overflow: TextOverflow.ellipsis,
              style: TextStyle(color: color, fontWeight: FontWeight.w600),
            ),
          ),
        ],
      ),
    );
  }
}

// -----------------------------------------------------------------------------
// Section: Drill widgets
// -----------------------------------------------------------------------------

/// The app's training message box, with room for exactly two lines.
class _DrillMessage extends StatelessWidget {
  const _DrillMessage({
    required this.icon,
    required this.text,
    this.headline,
    this.iconColor,
    this.action,
    super.key,
  });

  final IconData icon;
  final Widget? headline;
  final String text;
  final Color? iconColor;
  final Widget? action;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;
    final body = Text(
      text,
      key: const Key('opening-trainer-banner-instruction'),
      maxLines: 1,
      overflow: TextOverflow.ellipsis,
      style: theme.textTheme.bodyMedium?.copyWith(color: scheme.onSurfaceVariant),
    );
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 12),
      decoration: BoxDecoration(
        color: scheme.surfaceContainerHigh,
        borderRadius: BorderRadius.circular(12),
        border: Border.all(color: scheme.outlineVariant),
      ),
      child: Row(
        children: [
          Icon(icon, size: 20, color: iconColor ?? scheme.onSurfaceVariant),
          const SizedBox(width: 10),
          Expanded(
            child: headline == null
                ? body
                : Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [headline!, const SizedBox(height: 2), body],
                  ),
          ),
          ?action,
        ],
      ),
    );
  }
}

/// The prompt's first line, with the move itself picked out.
class _PromptHeadline extends StatelessWidget {
  const _PromptHeadline({required this.text, required this.move, required this.moveColor});

  final String text;
  final String? move;
  final Color moveColor;

  @override
  Widget build(BuildContext context) {
    final style = Theme.of(context).textTheme.titleSmall?.copyWith(
      fontWeight: FontWeight.w700,
    );
    final at = move == null || move!.isEmpty ? -1 : text.indexOf(move!);
    final span = at < 0
        ? TextSpan(text: text)
        : TextSpan(
            children: [
              TextSpan(text: text.substring(0, at)),
              // Notation reads left to right even inside an Arabic sentence.
              TextSpan(
                text: '\u2066$move\u2069',
                style: TextStyle(color: moveColor, fontWeight: FontWeight.w800),
              ),
              TextSpan(text: text.substring(at + move!.length)),
            ],
          );
    return Text.rich(
      span,
      key: const Key('opening-trainer-banner-headline'),
      maxLines: 1,
      overflow: TextOverflow.ellipsis,
      style: style,
    );
  }
}

/// "Current depth: move 2 / 10" over a segmented bar.
class _DepthMeter extends StatelessWidget {
  const _DepthMeter({
    required this.depth,
    required this.targetDepth,
    required this.finished,
  });

  final int depth;
  final int targetDepth;

  /// Once the run is over the meter reports what was reached, not what is next.
  final bool finished;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final theme = Theme.of(context);
    final total = targetDepth < 1 ? 1 : targetDepth;
    final current = finished ? depth.clamp(0, total) : (depth + 1).clamp(1, total);
    return Column(
      key: const Key('opening-trainer-progress'),
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(
          strings.trainingOpeningCurrentDepth(current, total),
          key: const Key('opening-trainer-depth-label'),
          style: theme.textTheme.labelLarge?.copyWith(
            color: theme.colorScheme.onSurfaceVariant,
          ),
        ),
        const SizedBox(height: 8),
        OpeningSegmentedMeter(
          done: depth,
          total: total,
          color: theme.colorScheme.primary,
          trackColor: theme.colorScheme.surfaceContainerHighest,
          glow: false,
        ),
      ],
    );
  }
}

/// End of the run, laid over the board like the endgame player's solved
/// overlay, so the layout below does not move when it appears.
class _DrillFinishedOverlay extends StatelessWidget {
  const _DrillFinishedOverlay({
    required this.depth,
    required this.clean,
    required this.bookExhausted,
    required this.onDrillAgain,
  });

  final int depth;
  final bool clean;
  final bool bookExhausted;
  final VoidCallback onDrillAgain;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final scheme = Theme.of(context).colorScheme;
    final nothingAsked = bookExhausted && depth == 0;
    return ClipRRect(
      borderRadius: BorderRadius.circular(10),
      child: Container(
        key: const Key('opening-trainer-completed'),
        alignment: Alignment.center,
        color: scheme.surface.withValues(alpha: 0.88),
        padding: const EdgeInsets.all(20),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Icon(
              nothingAsked ? Icons.menu_book_outlined : Icons.emoji_events_rounded,
              size: 44,
              color: nothingAsked
                  ? scheme.onSurfaceVariant
                  : clean
                  ? AppTheme.success
                  : scheme.tertiary,
            ),
            const SizedBox(height: 12),
            Text(
              nothingAsked
                  ? strings.trainingOpeningNoBook
                  : strings.trainingOpeningDepthReached(depth),
              textAlign: TextAlign.center,
              style: const TextStyle(fontWeight: FontWeight.w800),
            ),
            if (!nothingAsked) ...[
              const SizedBox(height: 6),
              Text(
                clean
                    ? strings.trainingOpeningDrillClean
                    : strings.trainingOpeningDrillWithErrors,
                textAlign: TextAlign.center,
              ),
            ],
            const SizedBox(height: 18),
            Wrap(
              spacing: 10,
              runSpacing: 10,
              alignment: WrapAlignment.center,
              children: [
                OutlinedButton(
                  onPressed: () => Navigator.of(context).pop(),
                  child: Text(strings.trainingOpeningLabBackToOverview),
                ),
                FilledButton.icon(
                  key: const Key('opening-trainer-practise-again'),
                  onPressed: onDrillAgain,
                  icon: const Icon(Icons.replay_rounded, size: 18),
                  label: Text(strings.trainingOpeningDrillAgain),
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }
}
