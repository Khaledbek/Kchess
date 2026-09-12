import 'dart:async';

import 'package:flutter/material.dart';

import '../../../ffi/core_gateway.dart';
import '../../../localization/generated/app_localizations.dart';
import '../../../shared/models/models.dart';
import '../../../ui/shared/promotion_dialog.dart';
import '../models/coach_ui_models.dart';
import 'coach_context_dialogs.dart';
import 'coach_screen.dart';

// -----------------------------------------------------------------------------
// Section: Native-backed coach session
// -----------------------------------------------------------------------------

class CoachSessionScreen extends StatefulWidget {
  const CoachSessionScreen({
    required this.gateway,
    this.position = BoardPosition.empty,
    this.surface = CoachSurface.general,
    this.contextId,
    this.contextPly,
    this.profileId,
    this.blackAtBottom = false,
    this.embedded = false,
    this.onExit,
    super.key,
  });

  final CoreGateway gateway;
  final BoardPosition position;
  final CoachSurface surface;
  final String? contextId;
  final int? contextPly;
  final String? profileId;
  final bool blackAtBottom;
  final bool embedded;
  final VoidCallback? onExit;

  @override
  State<CoachSessionScreen> createState() => _CoachSessionScreenState();
}

class _CoachSessionScreenState extends State<CoachSessionScreen> {
  final List<CoachUiMessage> _messages = [];
  late final String _sessionId;
  late BoardPosition _position;
  late CoachSurface _surface;
  String? _contextId;
  int? _contextPly;
  String? _gamePgn;
  List<BoardPosition>? _mainLinePositions;
  List<String>? _mainLineMoves;
  int _mainLinePly = -1;
  int _sideLineRootPly = -1;
  final List<BoardPosition> _sideLinePositions = [];
  final List<String> _sideLineMoves = [];
  int _sideLineIndex = -1;
  CoachResponseDepth _depth = CoachResponseDepth.balanced;
  String _playerColor = 'white';
  EngineLine? _evaluationLine;
  MoveClassification? _classification;
  String? _classificationSquare;
  String? _variationCoachJobId;
  String? _variationPreviousFen;
  int _boardAnalysisGeneration = 0;
  int _hintStage = 0;
  bool _hintBusy = false;
  String? _hintFen;
  List<String> _hintMoves = const <String>[];
  List<String> _coachBoardMoves = const [];
  Set<String> _coachFocusSquares = const {};
  int _positionRevision = 0;
  bool _loading = false;
  bool _boardMovePending = false;
  final Set<String> _automaticRequestedPositions = <String>{};
  int _automaticGeneration = 0;
  String? _selectedSquare;
  String? _error;

  bool get _hasPgn => _mainLinePositions != null;
  bool get _onSideLine => _sideLineIndex >= 0;
  int get _maximumMainLinePly => (_mainLinePositions?.length ?? 1) - 2;
  bool get _canFirst => _hasPgn && (_onSideLine || _mainLinePly >= 0);
  bool get _canPrevious => _canFirst;
  bool get _canNext =>
      _hasPgn &&
      (_onSideLine
          ? _sideLineIndex + 1 < _sideLinePositions.length
          : _mainLinePly < _maximumMainLinePly);
  bool get _canLast =>
      _hasPgn &&
      (_onSideLine
          ? _sideLineIndex + 1 < _sideLinePositions.length
          : _mainLinePly < _maximumMainLinePly);
  bool get _hintEnabled => true;
  Set<String> get _hintSourceSquares => _hintStage >= 1
      ? _hintMoves
            .where((move) => move.length >= 4)
            .map((move) => move.substring(0, 2))
            .toSet()
      : _coachFocusSquares;
  Set<String> get _hintTargetSquares => _hintStage >= 2
      ? _hintMoves
            .where((move) => move.length >= 4)
            .map((move) => move.substring(2, 4))
            .toSet()
      : const <String>{};
  List<String> get _visibleArrowMoves =>
      _hintStage >= 2 && _hintMoves.isNotEmpty
      ? _hintMoves
      : _hintStage == 1
      ? const []
      : _coachBoardMoves;

  @override
  void initState() {
    super.initState();
    _sessionId = 'coach-${DateTime.now().microsecondsSinceEpoch}';
    _position = widget.position.fen == BoardPosition.empty.fen
        ? BoardPosition.initial
        : widget.position;
    _surface = widget.surface;
    _playerColor = widget.blackAtBottom ? 'black' : 'white';
    _contextId = widget.contextId;
    _contextPly = widget.contextPly;
    WidgetsBinding.instance.addPostFrameCallback((_) {
      unawaited(_initializeContext());
    });
  }

  @override
  void didUpdateWidget(CoachSessionScreen oldWidget) {
    super.didUpdateWidget(oldWidget);
    var positionChanged = false;
    if (oldWidget.position.fen != widget.position.fen && !_hasPgn) {
      _position = widget.position.fen == BoardPosition.empty.fen
          ? BoardPosition.initial
          : widget.position;
      positionChanged = true;
    }
    if (oldWidget.surface != widget.surface) _surface = widget.surface;
    if (oldWidget.contextId != widget.contextId) {
      _contextId = widget.contextId;
      positionChanged = true;
    }
    if (oldWidget.contextPly != widget.contextPly) {
      _contextPly = widget.contextPly;
      positionChanged = true;
    }
    if (positionChanged) {
      _resetHintForPosition();
      _clearBoardAnalysis();
      WidgetsBinding.instance.addPostFrameCallback((_) {
        final ply = _contextPly;
        if (ply != null && ply >= 0) {
          unawaited(_refreshPersistedAnalysis(ply));
        }
        _scheduleAutomaticCoach();
      });
    }
  }

  @override
  void dispose() {
    super.dispose();
  }

  Future<void> _initializeContext() async {
    final contextId = _contextId;
    if (contextId != null) {
      try {
        final detail = await widget.gateway.game(contextId);
        if (mounted) {
          setState(() {
            _mainLineMoves = detail.moves
                .map((move) => move.uci)
                .toList(growable: false);
            if (detail.summary.profileColor == 'white' ||
                detail.summary.profileColor == 'black') {
              _playerColor = detail.summary.profileColor;
            }
          });
        }
      } catch (_) {
        // Context can still be a transient/non-library surface.
      }
    }
    if (!mounted) return;
    if (_contextPly != null && _contextPly! >= 0) {
      unawaited(_refreshPersistedAnalysis(_contextPly!));
    }
    _scheduleAutomaticCoach();
  }

  void _cancelScheduledAutomaticCoach() {
    _automaticGeneration++;
  }

  String? _automaticPositionKey() {
    if (_variationCoachJobId != null)
      return 'variation:$_variationCoachJobId:${_position.fen}';
    final contextId = _contextId;
    final contextPly = _contextPly;
    if (contextId == null || contextPly == null) return null;
    return '$contextId:$contextPly:${_position.fen}';
  }

  void _scheduleAutomaticCoach() {
    _cancelScheduledAutomaticCoach();
    final key = _automaticPositionKey();
    if (key == null || _automaticRequestedPositions.contains(key)) return;
    final generation = _automaticGeneration;
    if (!mounted || generation != _automaticGeneration) return;
    unawaited(_requestAutomaticCoach(key, generation));
  }

  Future<void> _requestAutomaticCoach(String key, int generation) async {
    if (_loading || _hintBusy || _hintStage > 0) return;
    if (!mounted || generation != _automaticGeneration) return;
    if (key != _automaticPositionKey()) return;
    if (!_automaticRequestedPositions.add(key)) return;

    final contextId = _contextId;
    final contextPly = _contextPly;
    if (_variationCoachJobId == null &&
        (contextId == null || contextPly == null))
      return;
    try {
      final reply = await widget.gateway.coachAutomatic({
        'locale': Localizations.localeOf(context).languageCode,
        'surface': _surface.wireName,
        'sessionId': _sessionId,
        if (_variationCoachJobId != null)
          'variationJobId': _variationCoachJobId,
        if (_variationPreviousFen != null) 'previousFen': _variationPreviousFen,
        if (contextId != null) 'contextId': contextId,
        if (contextPly != null) 'contextPly': contextPly,
        'playerColor': _playerColor,
        if (widget.profileId != null) 'profileId': widget.profileId!,
      });
      if (!mounted || generation != _automaticGeneration) return;
      if (key != _automaticPositionKey() || reply['status'] != 'ok') return;
      final answer = (reply['answer'] as String? ?? '').trim();
      if (answer.isEmpty) return;
      setState(() {
        final message = CoachUiMessage.fromReply(reply);
        _messages.add(message);
        _applyBoardMessageOverlay(message);
      });
    } catch (_) {
      _automaticRequestedPositions.remove(key);
      // Automatic coaching is opportunistic; manual input stays available.
    }
  }

  Future<void> _submit(
    String text, {
    String mode = 'answer',
    bool echo = true,
    String? hintMoveUci,
  }) async {
    if (_loading || text.trim().isEmpty) return;
    final question = text.trim();
    final revision = _positionRevision;
    _cancelScheduledAutomaticCoach();
    setState(() {
      if (echo) {
        _messages.add(
          CoachUiMessage(role: CoachMessageRole.user, text: question),
        );
      }
      _loading = true;
      _error = null;
    });

    try {
      final locale = Localizations.localeOf(context).languageCode;
      final reply = await widget.gateway.coachAsk({
        'text': question,
        'locale': locale,
        'mode': mode,
        'depth': switch (_depth) {
          CoachResponseDepth.concise => 'concise',
          CoachResponseDepth.balanced => 'standard',
          CoachResponseDepth.deep => 'detailed',
        },
        'surface': _surface.wireName,
        'sessionId': _sessionId,
        'positionFen': _position.fen,
        if (_variationCoachJobId != null)
          'variationJobId': _variationCoachJobId,
        'playerColor': _playerColor,
        if (hintMoveUci != null) 'hintMoveUci': hintMoveUci,
        if (_gamePgn != null) 'gamePgn': _gamePgn!,
        if (_contextId != null) 'contextId': _contextId!,
        if (_contextPly != null) 'contextPly': _contextPly!,
        if (widget.profileId != null) 'profileId': widget.profileId!,
      });
      if (!mounted) return;
      if (revision != _positionRevision ||
          reply['positionFen'] != _position.fen) {
        setState(() => _loading = false);
        return;
      }
      final strings = AppLocalizations.of(context);
      final status = reply['status'] as String? ?? 'provider_unavailable';
      final answer = (reply['answer'] as String? ?? '').trim();
      setState(() {
        _loading = false;
        if (status == 'ok' && answer.isNotEmpty) {
          final message = CoachUiMessage.fromReply(reply);
          _messages.add(message);
          _applyBoardMessageOverlay(message);
        } else if (status == 'off_topic') {
          _error = strings.coachOffTopic;
        } else if (status == 'validation_failed') {
          _error = strings.coachValidationFailed;
        } else {
          _error = strings.coachUnavailable;
        }
      });
    } catch (caught) {
      if (!mounted) return;
      setState(() {
        _loading = false;
        if (revision == _positionRevision) _error = caught.toString();
      });
    }
  }

  // ---------------------------------------------------------------------------
  // Section: Engine board presentation and staged hints
  // ---------------------------------------------------------------------------

  void _applyBoardMessageOverlay(CoachUiMessage message) {
    _coachBoardMoves = message.boardMoves;
    _coachFocusSquares = message.focusSquares.toSet();
  }

  Future<void> _showBoardMessage(CoachUiMessage message) async {
    final fen = message.positionFen?.trim();
    if (fen == null || fen.isEmpty) return;

    if (_position.fen == fen) {
      setState(() {
        _hintStage = 0;
        _applyBoardMessageOverlay(message);
      });
      return;
    }

    final mainLinePly = _mainLinePlyForFen(fen);
    if (mainLinePly != null && _mainLinePositions != null) {
      setState(() {
        _mainLinePly = mainLinePly;
        _sideLineIndex = -1;
        _position = _mainLinePositions![mainLinePly + 1];
        _contextPly = _contextId != null ? mainLinePly + 1 : null;
        _selectedSquare = null;
        _resetHintForPosition();
        _clearBoardAnalysis();
        _applyBoardMessageOverlay(message);
      });
      return;
    }

    final sideLineIndex =
        _sideLinePositions.indexWhere((position) => position.fen == fen);
    if (sideLineIndex >= 0) {
      setState(() {
        _sideLineIndex = sideLineIndex;
        _position = _sideLinePositions[sideLineIndex];
        _contextPly = null;
        _selectedSquare = null;
        _resetHintForPosition();
        _clearBoardAnalysis();
        _applyBoardMessageOverlay(message);
      });
      return;
    }

    // Free-board/FEN conversations have no PGN navigation history. Resolve the
    // message FEN natively so older coach messages can still be shown safely.
    if (_hasPgn) return;
    try {
      final result = await widget.gateway.coachContext({'fen': fen});
      final rawPosition = result['position'];
      if (!mounted || rawPosition is! Map<String, Object?>) return;
      final position = BoardPosition.fromJson(rawPosition);
      setState(() {
        _position = position;
        _contextPly = null;
        _selectedSquare = null;
        _resetHintForPosition();
        _clearBoardAnalysis();
        _applyBoardMessageOverlay(message);
      });
    } catch (_) {
      // Invalid/stale message metadata must never disturb the current board.
    }
  }

  Future<void> _requestBoardLesson(String mode) async {
    if (_loading || _hintBusy || _boardMovePending) return;
    final strings = AppLocalizations.of(context);
    _cancelScheduledAutomaticCoach();
    setState(() {
      _hintStage = 0;
      _coachBoardMoves = const [];
      _coachFocusSquares = const {};
      _error = null;
    });
    // C++ reuses existing analysis or chooses a bounded fresh engine budget.
    await _submit(
      mode == 'compare'
          ? strings.coachCompareRequest
          : strings.coachChallengeRequest,
      mode: mode,
      echo: false,
    );
  }

  void _resetHintForPosition() {
    _variationCoachJobId = null;
    _variationPreviousFen = null;
    _positionRevision++;
    _coachBoardMoves = const [];
    _coachFocusSquares = const {};
    _hintStage = 0;
    _hintBusy = false;
    _hintFen = null;
    _hintMoves = const <String>[];
  }

  void _clearBoardAnalysis() {
    _boardAnalysisGeneration++;
    _evaluationLine = null;
    _classification = null;
    _classificationSquare = null;
  }

  Future<void> _refreshPersistedAnalysis(
    int ply, {
    String? fallbackFenBefore,
    String? fallbackUci,
    String? fallbackExpectedFen,
  }) async {
    final contextId = _contextId;
    final expectedFen = _position.fen;
    if (contextId == null || ply < 0) return;
    final generation = ++_boardAnalysisGeneration;
    try {
      final snapshot = await widget.gateway.moveAnalysisStatus(contextId, ply);
      if (!mounted || generation != _boardAnalysisGeneration) return;
      if (_position.fen != expectedFen) return;
      final moveIndex = ply - 1;
      final move =
          _mainLineMoves != null &&
              moveIndex >= 0 &&
              moveIndex < _mainLineMoves!.length
          ? _mainLineMoves![moveIndex]
          : null;
      setState(() {
        _evaluationLine = snapshot.lines.isEmpty ? null : snapshot.lines.first;
        _classification = ply > 0 ? snapshot.classification : null;
        _classificationSquare = move != null && move.length >= 4
            ? move.substring(2, 4)
            : null;
      });
    } catch (_) {
      if (!mounted || generation != _boardAnalysisGeneration) return;
      if (fallbackFenBefore != null &&
          fallbackUci != null &&
          fallbackExpectedFen != null) {
        await _refreshVariationAnalysis(
          fenBefore: fallbackFenBefore,
          uci: fallbackUci,
          expectedFen: fallbackExpectedFen,
        );
        return;
      }
      setState(_clearBoardAnalysis);
    }
  }

  Future<void> _refreshVariationAnalysis({
    required String fenBefore,
    required String uci,
    required String expectedFen,
  }) async {
    final generation = ++_boardAnalysisGeneration;
    try {
      var snapshot = await widget.gateway.startVariationAnalysis(
        fen: fenBefore,
        uci: uci,
      );
      while (mounted &&
          generation == _boardAnalysisGeneration &&
          snapshot.isRunning) {
        await Future<void>.delayed(const Duration(milliseconds: 180));
        if (!mounted || generation != _boardAnalysisGeneration) return;
        snapshot = await widget.gateway.variationAnalysisStatus(snapshot.jobId);
      }
      if (!mounted || generation != _boardAnalysisGeneration) return;
      if (_position.fen != expectedFen) return;
      setState(() {
        _evaluationLine = snapshot.lines.isEmpty ? null : snapshot.lines.first;
        _variationCoachJobId = snapshot.jobId;
        _variationPreviousFen = fenBefore;
        _classification = snapshot.classification;
        _classificationSquare = uci.length >= 4 ? uci.substring(2, 4) : null;
      });
      _scheduleAutomaticCoach();
    } catch (_) {
      // A newer move/hint may deliberately cancel the previous variation job.
    }
  }

  void _refreshSideLineAnalysis(int index) {
    if (index < 0 ||
        index >= _sideLineMoves.length ||
        index >= _sideLinePositions.length) {
      return;
    }
    final mainLine = _mainLinePositions;
    if (index == 0 && mainLine == null) return;
    final fenBefore = index == 0
        ? mainLine![_sideLineRootPly + 1].fen
        : _sideLinePositions[index - 1].fen;
    unawaited(
      _refreshVariationAnalysis(
        fenBefore: fenBefore,
        uci: _sideLineMoves[index],
        expectedFen: _sideLinePositions[index].fen,
      ),
    );
  }

  void _appendHintMessage(String text) {
    _messages.add(CoachUiMessage(role: CoachMessageRole.coach, text: text));
  }

  Future<void> _advanceHint() async {
    if (_hintBusy || _loading) return;
    if (_hintFen != null && _hintFen != _position.fen) {
      setState(_resetHintForPosition);
    }

    final strings = AppLocalizations.of(context);
    if (_hintStage == 0) {
      if (_hintFen == _position.fen && _hintMoves.isNotEmpty) {
        setState(() {
          _hintStage = 1;
          _error = null;
          _appendHintMessage(strings.coachHintPieceMessage);
        });
        return;
      }

      setState(() {
        _hintBusy = true;
        _error = null;
      });
      final fen = _position.fen;
      try {
        final result = await widget.gateway.coachHint({'fen': fen});
        if (!mounted || _position.fen != fen) return;
        final rawMoves =
            (result['moves'] as List<Object?>? ?? const <Object?>[])
                .whereType<Map<String, Object?>>()
                .map((value) => value['uci'] as String? ?? '')
                .where((move) => move.length >= 4)
                .take(2)
                .toList(growable: false);
        final rawLine = result['line'];
        setState(() {
          _hintBusy = false;
          _hintFen = fen;
          _hintMoves = rawMoves;
          _hintStage = rawMoves.isEmpty ? 0 : 1;
          if (rawMoves.isNotEmpty) {
            _appendHintMessage(strings.coachHintPieceMessage);
          }
          if (rawLine is Map<String, Object?>) {
            _evaluationLine = EngineLine.fromJson(rawLine);
          }
        });
      } catch (caught) {
        if (!mounted) return;
        setState(() {
          _hintBusy = false;
          _error = caught.toString();
        });
      }
      return;
    }

    if (_hintStage == 1) {
      setState(() {
        _hintStage = 2;
        _appendHintMessage(strings.coachHintTargetMessage);
      });
      return;
    }

    if (_hintStage == 2) {
      final fen = _position.fen;
      final moves = _hintMoves.join(', ');
      await _submit(
        strings.coachHintExplainRequest,
        mode: 'explain',
        echo: false,
        hintMoveUci: moves,
      );
      if (!mounted || _position.fen != fen) return;
      setState(() => _hintStage = 0);
    }
  }

  // ---------------------------------------------------------------------------
  // Section: Board moves and transient one-line variation
  // ---------------------------------------------------------------------------

  Future<void> _onBoardSquare(String square) async {
    if (_boardMovePending) return;
    final source = _selectedSquare;
    if (source == null) {
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
    await _playBoardMove(source, square);
  }

  int? _mainLinePlyForFen(String fen) {
    final positions = _mainLinePositions;
    if (positions == null) return null;
    for (var index = 0; index < positions.length; index++) {
      if (positions[index].fen == fen) return index - 1;
    }
    return null;
  }

  void _acceptResolvedBoardMove(BoardPosition positionAfter, String uci) {
    final matchingMainLinePly = _mainLinePlyForFen(positionAfter.fen);
    if (_onSideLine) {
      if (matchingMainLinePly != null) {
        _position = positionAfter;
        _mainLinePly = matchingMainLinePly;
        _sideLineIndex = -1;
        _contextPly = _contextId != null && matchingMainLinePly >= 0
            ? matchingMainLinePly + 1
            : null;
        return;
      }
      if (_sideLineIndex + 1 < _sideLinePositions.length) {
        _sideLinePositions.removeRange(
          _sideLineIndex + 1,
          _sideLinePositions.length,
        );
        _sideLineMoves.removeRange(_sideLineIndex + 1, _sideLineMoves.length);
      }
      _sideLinePositions.add(positionAfter);
      _sideLineMoves.add(uci);
      _sideLineIndex = _sideLinePositions.length - 1;
      _position = positionAfter;
      _contextPly = null;
      return;
    }

    final positions = _mainLinePositions;
    final nextPositionIndex = _mainLinePly + 2;
    if (positions != null &&
        nextPositionIndex >= 0 &&
        nextPositionIndex < positions.length &&
        positions[nextPositionIndex].fen == positionAfter.fen) {
      _mainLinePly++;
      _position = positionAfter;
      _contextPly = _contextId != null ? _mainLinePly + 1 : null;
      return;
    }

    if (positions != null) {
      _sideLineRootPly = _mainLinePly;
      _sideLinePositions
        ..clear()
        ..add(positionAfter);
      _sideLineMoves
        ..clear()
        ..add(uci);
      _sideLineIndex = 0;
      _position = positionAfter;
      _contextPly = null;
      return;
    }

    _position = positionAfter;
    _contextPly = null;
    if (_contextId == null && _gamePgn == null) {
      _surface = CoachSurface.freeBoard;
    }
  }

  Future<void> _playBoardMove(String source, String target) async {
    if (_boardMovePending || source == target) return;
    final fenBefore = _position.fen;
    setState(() {
      _boardMovePending = true;
      _error = null;
    });
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
          if (mounted) {
            setState(() {
              _boardMovePending = false;
              _selectedSquare = null;
            });
          }
          return;
        }
        resolvedTarget = '$target$promotion';
      }
      final resolved = await widget.gateway.resolveFreeBoardMove(
        fen: _position.fen,
        source: source,
        target: resolvedTarget,
      );
      if (!mounted) return;
      setState(() {
        _acceptResolvedBoardMove(resolved.positionAfter, resolved.uci);
        _selectedSquare = null;
        _boardMovePending = false;
        _resetHintForPosition();
        _clearBoardAnalysis();
      });
      final contextPly = _contextPly;
      if (contextPly != null) {
        unawaited(
          _refreshPersistedAnalysis(
            contextPly,
            fallbackFenBefore: fenBefore,
            fallbackUci: resolved.uci,
            fallbackExpectedFen: resolved.positionAfter.fen,
          ),
        );
      } else {
        unawaited(
          _refreshVariationAnalysis(
            fenBefore: fenBefore,
            uci: resolved.uci,
            expectedFen: resolved.positionAfter.fen,
          ),
        );
      }
      _scheduleAutomaticCoach();
    } catch (caught) {
      if (!mounted) return;
      setState(() {
        _selectedSquare = null;
        _boardMovePending = false;
        _error = caught.toString();
      });
    }
  }

  // ---------------------------------------------------------------------------
  // Section: Main-line navigation
  // ---------------------------------------------------------------------------

  void _selectMainLinePly(int value) {
    _cancelScheduledAutomaticCoach();
    final positions = _mainLinePositions;
    if (positions == null) return;
    final maximum = positions.length - 2;
    final selected = value < -1 ? -1 : (value > maximum ? maximum : value);
    setState(() {
      _mainLinePly = selected;
      _sideLineIndex = -1;
      _position = positions[selected + 1];
      _contextPly = _contextId != null ? selected + 1 : null;
      _selectedSquare = null;
      _error = null;
      _resetHintForPosition();
      _clearBoardAnalysis();
    });
    if (selected >= 0 &&
        _mainLineMoves != null &&
        selected < _mainLineMoves!.length) {
      final fenBefore = positions[selected].fen;
      final uci = _mainLineMoves![selected];
      final expectedFen = positions[selected + 1].fen;
      if (_contextPly != null) {
        unawaited(
          _refreshPersistedAnalysis(
            _contextPly!,
            fallbackFenBefore: fenBefore,
            fallbackUci: uci,
            fallbackExpectedFen: expectedFen,
          ),
        );
      } else {
        unawaited(
          _refreshVariationAnalysis(
            fenBefore: fenBefore,
            uci: uci,
            expectedFen: expectedFen,
          ),
        );
      }
    } else if (_contextPly != null) {
      unawaited(_refreshPersistedAnalysis(_contextPly!));
    }
    _scheduleAutomaticCoach();
  }

  void _navigateFirst() => _selectMainLinePly(-1);

  void _navigatePrevious() {
    _cancelScheduledAutomaticCoach();
    if (_onSideLine) {
      if (_sideLineIndex > 0) {
        setState(() {
          _sideLineIndex--;
          _position = _sideLinePositions[_sideLineIndex];
          _selectedSquare = null;
          _resetHintForPosition();
          _clearBoardAnalysis();
        });
        _refreshSideLineAnalysis(_sideLineIndex);
      } else {
        _selectMainLinePly(_sideLineRootPly);
      }
      return;
    }
    _selectMainLinePly(_mainLinePly - 1);
  }

  void _navigateNext() {
    _cancelScheduledAutomaticCoach();
    if (_onSideLine) {
      if (_sideLineIndex + 1 < _sideLinePositions.length) {
        setState(() {
          _sideLineIndex++;
          _position = _sideLinePositions[_sideLineIndex];
          _selectedSquare = null;
          _resetHintForPosition();
          _clearBoardAnalysis();
        });
        _refreshSideLineAnalysis(_sideLineIndex);
      }
      return;
    }
    _selectMainLinePly(_mainLinePly + 1);
  }

  void _navigateLast() {
    _cancelScheduledAutomaticCoach();
    if (_onSideLine) {
      setState(() {
        _sideLineIndex = _sideLinePositions.length - 1;
        _position = _sideLinePositions[_sideLineIndex];
        _selectedSquare = null;
        _resetHintForPosition();
        _clearBoardAnalysis();
      });
      _refreshSideLineAnalysis(_sideLineIndex);
      return;
    }
    _selectMainLinePly(_maximumMainLinePly);
  }

  // ---------------------------------------------------------------------------
  // Section: Manual FEN / PGN context
  // ---------------------------------------------------------------------------

  void _clearPgnNavigation() {
    _mainLinePositions = null;
    _mainLineMoves = null;
    _mainLinePly = -1;
    _sideLineRootPly = -1;
    _sideLinePositions.clear();
    _sideLineMoves.clear();
    _sideLineIndex = -1;
  }

  void _installPgnLine(List<BoardPosition> positions, List<String> moves) {
    _mainLinePositions = positions.isEmpty
        ? <BoardPosition>[BoardPosition.initial]
        : List<BoardPosition>.unmodifiable(positions);
    _mainLineMoves = List<String>.unmodifiable(moves);
    _mainLinePly = -1;
    _sideLineRootPly = -1;
    _sideLinePositions.clear();
    _sideLineMoves.clear();
    _sideLineIndex = -1;
    _position = _mainLinePositions!.first;
    _contextPly = _contextId != null ? 0 : null;
    _resetHintForPosition();
    _clearBoardAnalysis();
  }

  Future<void> _loadFen() async {
    _cancelScheduledAutomaticCoach();
    final fen = await showCoachFenDialog(context);
    if (!mounted || fen == null) return;
    try {
      final result = await widget.gateway.coachContext({'fen': fen});
      final position = BoardPosition.fromJson(
        result['position']! as Map<String, Object?>,
      );
      if (!mounted) return;
      setState(() {
        _position = position;
        _surface = CoachSurface.freeBoard;
        _contextId = null;
        _contextPly = null;
        _gamePgn = null;
        _clearPgnNavigation();
        _selectedSquare = null;
        _resetHintForPosition();
        _clearBoardAnalysis();
        _error = null;
      });
    } catch (caught) {
      if (!mounted) return;
      setState(() => _error = caught.toString());
    }
  }

  Future<void> _loadPgn() async {
    _cancelScheduledAutomaticCoach();
    final source = await showCoachPgnSourceDialog(context);
    if (!mounted || source == null) return;
    if (source == CoachPgnSource.paste) {
      final pgn = await showCoachPgnDialog(context);
      if (!mounted || pgn == null) return;
      try {
        final result = await widget.gateway.coachContext({'pgn': pgn});
        final positions = (result['positions'] as List<Object?>? ?? const [])
            .cast<Map<String, Object?>>()
            .map(BoardPosition.fromJson)
            .toList(growable: false);
        final moves = (result['moves'] as List<Object?>? ?? const <Object?>[])
            .whereType<String>()
            .toList(growable: false);
        final fallback = BoardPosition.fromJson(
          result['position']! as Map<String, Object?>,
        );
        if (!mounted) return;
        setState(() {
          _installPgnLine(
            positions.isEmpty ? <BoardPosition>[fallback] : positions,
            moves,
          );
          _surface = CoachSurface.pgn;
          _contextId = null;
          _gamePgn = pgn;
          _selectedSquare = null;
          _error = null;
        });
      } catch (caught) {
        if (!mounted) return;
        setState(() => _error = caught.toString());
      }
      return;
    }

    try {
      final games = await widget.gateway.games();
      if (!mounted) return;
      final selected = await showCoachGamePicker(context, games);
      if (!mounted || selected == null) return;
      final detail = await widget.gateway.game(selected.id);
      if (!mounted) return;
      final positions = <BoardPosition>[
        detail.startingPosition,
        ...detail.moves.map((move) => move.positionAfter),
      ];
      setState(() {
        _contextId = selected.id;
        _installPgnLine(
          positions,
          detail.moves.map((move) => move.uci).toList(growable: false),
        );
        _surface = CoachSurface.pgn;
        _gamePgn = null;
        if (selected.profileColor == 'white' ||
            selected.profileColor == 'black') {
          _playerColor = selected.profileColor;
        }
        _selectedSquare = null;
        _error = null;
      });
    } catch (caught) {
      if (!mounted) return;
      setState(() => _error = caught.toString());
    }
  }

  @override
  Widget build(BuildContext context) => CoachScreen(
    position: _position,
    blackAtBottom: widget.blackAtBottom,
    playerColor: _playerColor,
    messages: List<CoachUiMessage>.unmodifiable(_messages),
    isLoading: _loading,
    errorMessage: _error,
    depth: _depth,
    onSubmit: (value) => unawaited(_submit(value)),
    onShowBoard: (message) => unawaited(_showBoardMessage(message)),
    onCompare: () => unawaited(_requestBoardLesson('compare')),
    onQuiz: () => unawaited(_requestBoardLesson('quiz')),
    onHintRequested: () => unawaited(_advanceHint()),
    hintEnabled: _hintEnabled,
    hintBusy: _hintBusy,
    onFenRequested: _loading ? null : () => unawaited(_loadFen()),
    onPgnRequested: _loading ? null : () => unawaited(_loadPgn()),
    onPlayerColorChanged: (value) => setState(() => _playerColor = value),
    onBoardSquareTap: (square) => unawaited(_onBoardSquare(square)),
    onBoardPieceDrop: (source, target) =>
        unawaited(_playBoardMove(source, target)),
    selectedSquare: _selectedSquare,
    hintSourceSquares: _hintSourceSquares,
    hintTargetSquares: _hintTargetSquares,
    arrowMoves: _visibleArrowMoves,
    evaluationLine: _evaluationLine,
    classification: _classification,
    classificationSquare: _classificationSquare,
    boardBusy: _boardMovePending,
    showPgnControls: _hasPgn,
    onFirstMove: _canFirst ? _navigateFirst : null,
    onPreviousMove: _canPrevious ? _navigatePrevious : null,
    onNextMove: _canNext ? _navigateNext : null,
    onLastMove: _canLast ? _navigateLast : null,
    onDepthChanged: (value) => setState(() => _depth = value),
    showAppBar: !widget.embedded,
    onExit: widget.onExit,
  );
}

// -----------------------------------------------------------------------------
// Section: Thin navigation helper
// -----------------------------------------------------------------------------

Future<void> openCoachSession(
  BuildContext context, {
  required CoreGateway gateway,
  BoardPosition position = BoardPosition.empty,
  CoachSurface surface = CoachSurface.general,
  String? contextId,
  int? contextPly,
  String? profileId,
  bool blackAtBottom = false,
}) => Navigator.of(context).push(
  MaterialPageRoute<void>(
    builder: (_) => CoachSessionScreen(
      gateway: gateway,
      position: position,
      surface: surface,
      contextId: contextId,
      contextPly: contextPly,
      profileId: profileId,
      blackAtBottom: blackAtBottom,
    ),
  ),
);
