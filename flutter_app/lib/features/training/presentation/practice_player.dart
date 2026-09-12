// -----------------------------------------------------------------------------
// Section: Native practice presentation and defender-job polling
// -----------------------------------------------------------------------------
import 'dart:async';

import 'package:flutter/material.dart';

import '../../../ffi/core_gateway.dart';
import '../../../localization/generated/app_localizations.dart';
import '../../../shared/widgets/chess_board_view.dart';
import '../../../shared/widgets/evaluation_bar.dart';
import '../../../shared/models/models.dart';
import '../../../ui/shared/promotion_dialog.dart';
import '../../coach/models/coach_ui_models.dart';
import '../../coach/presentation/coach_session_screen.dart';
import '../models/practice_models.dart';

class PracticePlayer extends StatefulWidget {
  const PracticePlayer({
    required this.gateway,
    required this.request,
    required this.title,
    this.hint,
    super.key,
  });
  final CoreGateway gateway;
  final Map<String, Object?> request;
  final String title;
  final String? hint;
  @override
  State<PracticePlayer> createState() => _PracticePlayerState();
}

class _PracticePlayerState extends State<PracticePlayer> {
  PracticeSnapshot? _snapshot;
  bool _busy = true, _error = false, _showHint = false;
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

  Future<void> _cancel(String id) async {
    try {
      await widget.gateway.practiceCommand({'op': 'cancel', 'session': id});
    } catch (_) {}
  }

  Future<PracticeSnapshot> _command(Map<String, Object?> request) async =>
      PracticeSnapshot.fromJson(
        (await widget.gateway.practiceCommand(request))!
            as Map<String, Object?>,
      );

  Future<void> _restart() async {
    final generation = ++_generation;
    setState(() {
      _busy = true;
      _error = false;
      _showHint = false;
    });
    final previous = _snapshot?.id;
    if (previous != null) await _cancel(previous);
    try {
      final result = await _command({...widget.request, 'op': 'start'});
      if (!mounted || generation != _generation) {
        await _cancel(result.id);
        return;
      }
      await _adopt(result, generation);
    } catch (_) {
      if (mounted && generation == _generation) {
        setState(() {
          _busy = false;
          _error = true;
        });
      }
    }
  }

  Future<void> _adopt(PracticeSnapshot result, int generation) async {
    while (mounted && generation == _generation) {
      setState(() {
        _snapshot = result;
        _busy = result.status == 'thinking';
      });
      if (result.status != 'thinking') return;
      await Future<void>.delayed(const Duration(milliseconds: 160));
      if (!mounted || generation != _generation) return;
      result = await _command({'op': 'poll', 'session': result.id});
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
        final promotion = await showPromotionChoiceDialog(
          context: context,
          options: result.promotions,
          sideToMove: current.position.sideToMove,
        );
        if (!mounted || generation != _generation) return;
        if (promotion == null) {
          setState(() => _busy = false);
          return;
        }
        result = await _command({...request, 'promotion': promotion});
      }
      if (!mounted || generation != _generation) return;
      await _adopt(result, generation);
    } catch (_) {
      if (mounted && generation == _generation) {
        setState(() {
          _busy = false;
          _error = true;
        });
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final current = _snapshot;
    final status = current?.status;
    final evaluation = current?.evaluation;
    final line = evaluation == null
        ? null
        : EngineLine(
            rank: 1,
            depth: 0,
            nodes: 0,
            moves: const [],
            evaluationCp: evaluation['evaluationCp'] as int?,
            mateIn: evaluation['mateIn'] as int?,
          );
    final message = _error || status == 'error'
        ? strings.trainingBoardError
        : status == 'failed'
        ? strings.practiceFailed
        : status == 'completed'
        ? (current!.clean
              ? strings.trainingSolvedClean
              : strings.trainingSolvedWithErrors)
        : status == 'thinking'
        ? strings.practiceThinking
        : current?.accepted == false
        ? strings.trainingWrongMove
        : strings.trainingYourMove;
    return Scaffold(
      appBar: AppBar(
        title: Text(widget.title),
        actions: [
          if (current != null)
            IconButton(
              key: const Key('open-coach-from-training'),
              tooltip: strings.coach,
              onPressed: () => unawaited(
                openCoachSession(
                  context,
                  gateway: widget.gateway,
                  position: current.position,
                  surface: widget.request['kind'] == 'opening'
                      ? CoachSurface.opening
                      : CoachSurface.training,
                  contextId: current.id,
                  blackAtBottom: current.solverColor == 'black',
                ),
              ),
              icon: const Icon(Icons.school_outlined),
            ),
        ],
      ),
      body: Center(
        child: ConstrainedBox(
          constraints: const BoxConstraints(maxWidth: 660),
          child: ListView(
            padding: const EdgeInsets.all(16),
            children: [
              if (current == null && _busy)
                const Center(child: CircularProgressIndicator()),
              if (current != null) ...[
                Text(
                  current.maxMoves > 0
                      ? strings.trainingMoveProgress(
                          current.played,
                          current.maxMoves,
                        )
                      : strings.practiceMoves(current.played),
                ),
                const SizedBox(height: 12),
                AspectRatio(
                  aspectRatio: 1,
                  child: ChessBoardView(
                    position: current.position,
                    blackAtBottom: current.solverColor == 'black',
                    interactive: !_busy && !_error && status == 'active',
                    onSquareTap: (_) {},
                    onPieceDrop: (source, target) =>
                        unawaited(_drop(source, target)),
                  ),
                ),
                if (line != null)
                  Padding(
                    padding: const EdgeInsets.only(top: 8),
                    child: SizedBox(
                      height: 32,
                      child: EvaluationBar(line: line),
                    ),
                  ),
              ],
              const SizedBox(height: 16),
              Text(message),
              if (widget.hint != null) ...[
                TextButton(
                  onPressed: () => setState(() => _showHint = !_showHint),
                  child: Text(strings.trainingShowHint),
                ),
                if (_showHint) Text(widget.hint!),
              ],
              const SizedBox(height: 12),
              FilledButton(
                onPressed: _busy ? null : _restart,
                child: Text(strings.trainingRestart),
              ),
            ],
          ),
        ),
      ),
    );
  }
}
