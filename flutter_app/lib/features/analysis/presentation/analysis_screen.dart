import 'dart:async';
import 'dart:math' as math;

import 'package:flutter/material.dart';
import 'package:flutter_svg/flutter_svg.dart';

import '../../../ffi/core_gateway.dart';
import '../../../localization/generated/app_localizations.dart';
import '../../../shared/models/models.dart';
import '../../app/application/app_controller.dart';

String _classificationLabel(
  AppLocalizations strings,
  MoveClassification classification,
) => switch (classification) {
  MoveClassification.theory => strings.theory,
  MoveClassification.brilliant => strings.brilliant,
  MoveClassification.critical => strings.critical,
  MoveClassification.best => strings.best,
  MoveClassification.excellent => strings.excellent,
  MoveClassification.okay => strings.okay,
  MoveClassification.miss => strings.miss,
  MoveClassification.mistake => strings.mistake,
  MoveClassification.blunder => strings.blunder,
  MoveClassification.unknown => strings.classificationPending,
};

String? _analysisClassificationAsset(MoveClassification classification) =>
    switch (classification) {
      MoveClassification.theory => 'assets/analysis_img/move_book.png',
      MoveClassification.brilliant => 'assets/analysis_img/move_brilliant.png',
      MoveClassification.best => 'assets/analysis_img/move_best.png',
      MoveClassification.excellent => 'assets/analysis_img/move_excellent.png',
      MoveClassification.okay => 'assets/analysis_img/move_okay.png',
      MoveClassification.miss => 'assets/analysis_img/move_miss.png',
      MoveClassification.mistake => 'assets/analysis_img/move_mistake.png',
      MoveClassification.blunder => 'assets/analysis_img/move_blunder.png',
      MoveClassification.critical => 'assets/analysis_img/move_great.png',
      MoveClassification.unknown => null,
    };

String? _knownGameResult(String? raw) {
  final result = raw?.trim();
  return switch (result) {
    '1-0' || '0-1' || '1/2-1/2' || '½-½' => result,
    _ => null,
  };
}

bool _isDrawResult(String result) =>
    result == '1/2-1/2' || result == '½-½';

String? _resultAssetForColor(
  String color,
  String? result,
  bool checkmate,
) {
  if (result == null) return null;
  if (_isDrawResult(result)) {
    return 'assets/analysis_img/result_draw.png';
  }
  final whiteWon = result == '1-0';
  final colorIsWhite = color == 'white';
  if (whiteWon == colorIsWhite) {
    return 'assets/analysis_img/result_win.png';
  }
  return checkmate
      ? 'assets/analysis_img/result_loss.png'
      : 'assets/analysis_img/result_giveup.png';
}

({String result, bool checkmate})? _terminalBoardOutcome(
  ParsedMove? move,
  String? rawResult,
  EngineLine? terminalLine,
) {
  if (move != null) {
    // SAN is deterministic for a delivered mate and is more trustworthy than
    // a missing or malformed imported Result tag.
    final san = move.san.trim();
    if (san.endsWith('#') || san.endsWith('++')) {
      return (
        result: move.sideToMove == 'black' ? '0-1' : '1-0',
        checkmate: true,
      );
    }

    // Keep support for adapters that expose a synthetic terminal WDL line.
    final wdl = terminalLine?.wdl;
    if (terminalLine != null &&
        terminalLine.depth == 0 &&
        terminalLine.moves.isEmpty &&
        wdl != null) {
      if (wdl.draws > wdl.wins && wdl.draws > wdl.losses) {
        return (result: '1/2-1/2', checkmate: false);
      }
      if (wdl.losses > wdl.wins && wdl.losses > wdl.draws) {
        return (
          result: move.sideToMove == 'black' ? '0-1' : '1-0',
          checkmate: true,
        );
      }
    }
  }

  // A decisive PGN result without a mating SAN is treated as a non-board
  // termination (resignation/timeout/etc.).  The losing king then receives
  // the dedicated give-up PNG instead of a mate-loss badge.
  final known = _knownGameResult(rawResult);
  return known == null ? null : (result: known, checkmate: false);
}

Color _classificationColor(
  BuildContext context,
  MoveClassification classification,
) {
  final scheme = Theme.of(context).colorScheme;
  return switch (classification) {
    MoveClassification.theory => scheme.primary,
    MoveClassification.brilliant => const Color(0xFF00A6A6),
    MoveClassification.critical => const Color(0xFFE39A18),
    MoveClassification.best => const Color(0xFF2E9B55),
    MoveClassification.excellent => const Color(0xFF4A9B73),
    MoveClassification.okay => scheme.onSurfaceVariant,
    MoveClassification.miss => const Color(0xFFE08A1E),
    MoveClassification.mistake => const Color(0xFFD9682A),
    MoveClassification.blunder => scheme.error,
    MoveClassification.unknown => scheme.onSurfaceVariant,
  };
}

Future<void> openAnalysisWorkflow({
  required BuildContext context,
  required CoreGateway gateway,
  required GameSummary game,
  required AppSettings settings,
}) async {
  try {
    var snapshot = await gateway.startAnalysis(game.id);
    if (!context.mounted) return;
    if (!snapshot.isComplete) {
      final completed = await showDialog<AnalysisSnapshot>(
        context: context,
        barrierDismissible: false,
        builder: (_) => _AnalysisPreparationDialog(
          gateway: gateway,
          game: game,
          initial: snapshot,
          settings: settings,
        ),
      );
      if (completed == null || !context.mounted) return;
      snapshot = completed;
    }
    await Navigator.of(context).push(
      MaterialPageRoute<void>(
        builder: (_) => AnalysisScreen(
          gateway: gateway,
          game: game,
          settings: settings,
          initialSnapshot: snapshot,
        ),
      ),
    );
  } catch (error) {
    if (context.mounted) {
      ScaffoldMessenger.of(context)
          .showSnackBar(SnackBar(content: Text(error.toString())));
    }
  }
}

class _AnalysisPreparationDialog extends StatefulWidget {
  const _AnalysisPreparationDialog({
    required this.gateway,
    required this.game,
    required this.initial,
    required this.settings,
  });

  final CoreGateway gateway;
  final GameSummary game;
  final AnalysisSnapshot initial;
  final AppSettings settings;

  @override
  State<_AnalysisPreparationDialog> createState() =>
      _AnalysisPreparationDialogState();
}

class _AnalysisPreparationDialogState
    extends State<_AnalysisPreparationDialog> {
  late AnalysisSnapshot _snapshot;
  Timer? _timer;
  Object? _error;

  @override
  void initState() {
    super.initState();
    _snapshot = widget.initial;
    _schedulePoll();
  }

  @override
  void dispose() {
    _timer?.cancel();
    super.dispose();
  }

  void _schedulePoll() {
    if (!_snapshot.isRunning) return;
    _timer = Timer(const Duration(milliseconds: 300), () async {
      try {
        final next = await widget.gateway.analysisStatus(widget.game.id);
        if (!mounted) return;
        setState(() => _snapshot = next);
        _schedulePoll();
      } catch (error) {
        if (mounted) setState(() => _error = error);
      }
    });
  }

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final summary = _snapshot.summary;
    final profileIsWhite = summary?.profileSide == 'white';
    final profileIsBlack = summary?.profileSide == 'black';
    final first = profileIsBlack ? summary?.black : summary?.white;
    final second = profileIsBlack ? summary?.white : summary?.black;
    final firstTitle = profileIsWhite || profileIsBlack
        ? strings.myPlayer
        : strings.whitePlayer;
    final secondTitle = profileIsWhite || profileIsBlack
        ? strings.opponent
        : strings.blackPlayer;
    final canClose =
        _snapshot.isComplete ||
        _snapshot.isCancelled ||
        _snapshot.status == 'error' ||
        _error != null;
    return PopScope(
      canPop: canClose,
      child: AlertDialog(
        key: const Key('analysis-preparation-modal'),
        title: Text(strings.analyzingGame),
        content: SizedBox(
          width: 620,
          child: SingleChildScrollView(
            child: Column(
              mainAxisSize: MainAxisSize.min,
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                LinearProgressIndicator(
                  key: const Key('analysis-modal-progress'),
                  value: _snapshot.isComplete ? 1 : _snapshot.progress,
                ),
                const SizedBox(height: 8),
                Text(
                  strings.analyzedMovesProgress(
                    _snapshot.completedPlies,
                    _snapshot.totalPlies,
                  ),
                  key: const Key('analysis-modal-moves'),
                ),
                const SizedBox(height: 12),
                LayoutBuilder(
                  builder: (context, constraints) {
                    final firstBlock = _LivePlayerSummary(
                      key: const Key('analysis-modal-first-side'),
                      title: firstTitle,
                      summary: first,
                      complete: _snapshot.isComplete,
                      settings: widget.settings,
                    );
                    final secondBlock = _LivePlayerSummary(
                      key: const Key('analysis-modal-second-side'),
                      title: secondTitle,
                      summary: second,
                      complete: _snapshot.isComplete,
                      settings: widget.settings,
                    );
                    if (constraints.maxWidth >= 520) {
                      return Row(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          Expanded(child: firstBlock),
                          const VerticalDivider(width: 28),
                          Expanded(child: secondBlock),
                        ],
                      );
                    }
                    return Column(
                      crossAxisAlignment: CrossAxisAlignment.stretch,
                      children: [
                        firstBlock,
                        const Divider(height: 24),
                        secondBlock,
                      ],
                    );
                  },
                ),
                if (_error != null || _snapshot.status == 'error') ...[
                  const SizedBox(height: 12),
                  Text(
                    _error?.toString() ??
                        _snapshot.error ??
                        strings.coreUnavailable,
                  ),
                ],
              ],
            ),
          ),
        ),
        actions: [
          if (_snapshot.isComplete)
            FilledButton(
              key: const Key('open-analysis'),
              onPressed: () => Navigator.pop(context, _snapshot),
              child: Text(strings.openAnalysis),
            )
          else if (canClose)
            TextButton(
              onPressed: () => Navigator.pop(context),
              child: Text(strings.close),
            ),
        ],
      ),
    );
  }
}

class _LivePlayerSummary extends StatelessWidget {
  const _LivePlayerSummary({
    required this.title,
    required this.summary,
    required this.complete,
    required this.settings,
    super.key,
  });

  final String title;
  final PlayerAnalysisSummary? summary;
  final bool complete;
  final AppSettings settings;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final value = summary;
    final counts = <(String, int)>[
      if (settings.showTheory) (strings.theory, value?.theory ?? 0),
      (strings.brilliant, value?.brilliant ?? 0),
      (strings.critical, value?.critical ?? 0),
      (strings.best, value?.best ?? 0),
      (strings.excellent, value?.excellent ?? 0),
      (strings.okay, value?.okay ?? 0),
      (strings.miss, value?.miss ?? 0),
      (strings.mistake, value?.mistake ?? 0),
      (strings.blunder, value?.blunder ?? 0),
    ];
    final accuracy = value?.localAccuracy;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        Text(title, style: Theme.of(context).textTheme.titleMedium),
        if (settings.showAccuracy && complete && accuracy != null)
          Text(
            '${strings.localAccuracy}: ${accuracy.toStringAsFixed(1)}%',
            key: const Key('analysis-modal-accuracy'),
          ),
        if (settings.showClassifications) const SizedBox(height: 6),
        if (settings.showClassifications)
          for (final (label, count) in counts)
            Padding(
            padding: const EdgeInsets.symmetric(vertical: 1),
            child: Row(
              mainAxisAlignment: MainAxisAlignment.spaceBetween,
              children: [Text(label), Text('$count')],
            ),
          ),
      ],
    );
  }
}

class AnalysisScreen extends StatefulWidget {
  const AnalysisScreen({
    required this.gateway,
    required this.game,
    required this.settings,
    this.initialSnapshot,
    super.key,
  });

  final CoreGateway gateway;
  final GameSummary game;
  final AppSettings settings;
  final AnalysisSnapshot? initialSnapshot;

  @override
  State<AnalysisScreen> createState() => _AnalysisScreenState();
}

class _VariationSession {
  _VariationSession({required this.parentPly, required this.startingFen});

  final int parentPly;
  final String startingFen;
  final List<VariationAnalysisSnapshot> moves = [];
  int currentIndex = -1;

  VariationAnalysisSnapshot? get current =>
      currentIndex >= 0 && currentIndex < moves.length
      ? moves[currentIndex]
      : null;
}

class _AnalysisScreenState extends State<AnalysisScreen> {
  late final AnalysisController _controller;
  late AppSettings _settings;
  bool _followLatest = false;
  bool _playing = false;
  bool _boardRotated = false;
  bool _resultPresentationStarted = false;
  bool _resultPresentationDocked = false;
  int _currentPly = -1;
  int _selectedLine = 0;
  String? _selectedSquare;
  AnalysisSnapshot? _afterMoveSnapshot;
  int? _afterMoveSlot;
  int? _afterMoveLoadingSlot;
  int _afterMoveLoadGeneration = 0;
  _VariationSession? _variationSession;
  Object? _variationError;
  Timer? _playTimer;
  Timer? _variationTimer;
  String? _activeVariationJobId;
  bool _variationMovePending = false;
  late int _sidelineDepth;
  late int _sidelineMultiPv;
  late int _sidelineThreads;
  late int _sidelineHashMb;

  @override
  void initState() {
    super.initState();
    _settings = widget.settings;
    _sidelineDepth = _settings.depth;
    _sidelineMultiPv = _settings.multiPv;
    _sidelineThreads = AppController.clampEngineWorkerThreads(
      _settings.threads,
    );
    _sidelineHashMb = _settings.hashMb;
    _controller = AnalysisController(widget.gateway, widget.game)
      ..addListener(_refresh);
    final prepared = widget.initialSnapshot;
    if (prepared == null) {
      _controller.open();
    } else {
      _controller.openPrepared(prepared);
    }
  }

  @override
  void dispose() {
    _playTimer?.cancel();
    _variationTimer?.cancel();
    final activeVariationJobId = _activeVariationJobId;
    if (activeVariationJobId != null) {
      unawaited(widget.gateway.cancelVariationAnalysis(activeVariationJobId));
    }
    _controller
      ..removeListener(_refresh)
      ..dispose();
    super.dispose();
  }

  Future<void> _deleteStoredAnalysis() async {
    final strings = AppLocalizations.of(context);
    var confirmed = true;
    if (_settings.confirmBeforeDelete) {
      confirmed =
          await showDialog<bool>(
            context: context,
            builder: (dialogContext) => AlertDialog(
              title: Text(strings.deleteAnalysisQuestion),
              content: Text(strings.deleteAnalysisBody),
              actions: [
                TextButton(
                  onPressed: () => Navigator.of(dialogContext).pop(false),
                  child: Text(strings.cancelAction),
                ),
                FilledButton(
                  onPressed: () => Navigator.of(dialogContext).pop(true),
                  child: Text(strings.deleteAction),
                ),
              ],
            ),
          ) ??
          false;
    }
    if (!confirmed) return;

    try {
      await widget.gateway.deleteAnalysis(widget.game.id);
      if (!mounted) return;
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text(strings.analysisDeleted)),
      );
      Navigator.of(context).pop();
    } catch (error) {
      if (!mounted) return;
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text(error.toString())),
      );
    }
  }

  void _refresh() {
    if (!mounted) return;
    final completedMoves = _controller.snapshot?.completedPlies ?? 0;
    if (_followLatest && completedMoves > 0) {
      final latestMove = completedMoves - 1;
      _currentPly = latestMove < _maximumPly ? latestMove : _maximumPly;
    }
    final afterMoveMatchesCurrent = _afterMoveSlot == _currentPly + 1;
    final lineCount = afterMoveMatchesCurrent
        ? (_afterMoveSnapshot?.lines.length ?? 0)
        : (_controller.displayedSnapshot?.lines.length ?? 0);
    if (_selectedLine >= lineCount) _selectedLine = 0;
    setState(() {});
    if (_controller.detail?.moves.isNotEmpty == true) {
      unawaited(_loadAfterMoveSnapshot(_currentPly));
    }
  }

  Future<void> _loadAfterMoveSnapshot(int moveIndex) async {
    final moves = _controller.detail?.moves;
    if (moves == null || moves.isEmpty) return;
    if (moveIndex < 0) {
      _afterMoveLoadGeneration++;
      if (mounted && (_afterMoveSnapshot != null || _afterMoveSlot != null)) {
        setState(() {
          _afterMoveSnapshot = null;
          _afterMoveSlot = null;
          _afterMoveLoadingSlot = null;
        });
      }
      return;
    }
    final clampedMoveIndex = moveIndex < 0
        ? 0
        : (moveIndex >= moves.length ? moves.length - 1 : moveIndex);
    final slot = clampedMoveIndex + 1;
    final analysisRunning = _controller.snapshot?.isRunning == true ||
        _controller.displayedSnapshot?.isRunning == true;
    if (_afterMoveLoadingSlot == slot) return;
    if (_afterMoveSlot == slot &&
        _afterMoveSnapshot != null &&
        !analysisRunning) {
      return;
    }

    final generation = ++_afterMoveLoadGeneration;
    _afterMoveLoadingSlot = slot;
    try {
      final snapshot = await widget.gateway.moveAnalysisStatus(
        widget.game.id,
        slot,
      );
      if (!mounted || generation != _afterMoveLoadGeneration) return;
      setState(() {
        _afterMoveSlot = slot;
        _afterMoveSnapshot = snapshot;
        _afterMoveLoadingSlot = null;
      });
    } catch (_) {
      if (!mounted || generation != _afterMoveLoadGeneration) return;
      setState(() {
        _afterMoveSlot = slot;
        _afterMoveSnapshot = null;
        _afterMoveLoadingSlot = null;
      });
    }
  }

  int get _maximumPly {
    final moves = _controller.detail?.moves;
    return moves == null || moves.isEmpty ? 0 : moves.length - 1;
  }

  Future<void> _selectPly(int value) async {
    if (_variationSession != null) _returnToMainLine();
    final selected = value < -1
        ? -1
        : (value > _maximumPly ? _maximumPly : value);
    setState(() {
      _followLatest = false;
      _currentPly = selected;
      _selectedLine = 0;
      _selectedSquare = null;
    });
    if (selected < 0) {
      await _controller.selectPly(0);
      await _loadAfterMoveSnapshot(-1);
      return;
    }
    await _controller.selectPly(selected);
    await _loadAfterMoveSnapshot(selected);
  }

  void _togglePlayback() {
    _playTimer?.cancel();
    setState(() => _playing = !_playing);
    if (!_playing) return;
    _playTimer = Timer.periodic(const Duration(milliseconds: 850), (timer) {
      if (_currentPly >= _maximumPly) {
        timer.cancel();
        if (mounted) setState(() => _playing = false);
      } else {
        _selectPly(_currentPly + 1);
      }
    });
  }

  Future<void> _showSummary(AnalysisSummary summary) =>
      showModalBottomSheet<void>(
        context: context,
        showDragHandle: true,
        isScrollControlled: true,
        builder: (context) =>
            _SummarySheet(summary: summary),
      );

  AppSettings _withDisplaySetting(
    AppSettings settings,
    String key,
    bool enabled,
  ) => switch (key) {
    'showBestMoveArrow' => settings.copyWith(showBestMoveArrow: enabled),
    'showThreatArrow' => settings.copyWith(showThreatArrow: enabled),
    'showEvaluationBar' => settings.copyWith(showEvaluationBar: enabled),
    'showEngineLines' => settings.copyWith(showEngineLines: enabled),
    'showClassifications' =>
      settings.copyWith(showClassifications: enabled),
    'showResultSymbols' => settings.copyWith(showResultSymbols: enabled),
    _ => settings,
  };

  Future<bool> _setDisplaySetting(String key, bool enabled) async {
    try {
      await widget.gateway.setBooleanSetting(key, enabled);
      if (!mounted) return true;
      setState(() => _settings = _withDisplaySetting(_settings, key, enabled));
      return true;
    } catch (error) {
      if (mounted) {
        ScaffoldMessenger.of(context)
            .showSnackBar(SnackBar(content: Text(error.toString())));
      }
      return false;
    }
  }

  Future<void> _showQuickAnalysisSettings() async {
    var quickSettings = _settings;
    var quickDepth = _sidelineDepth;
    var quickMultiPv = _sidelineMultiPv;
    var quickThreads = _sidelineThreads;
    var quickHashMb = _sidelineHashMb;
    const hashOptions = <int>[16, 32, 64, 128, 256, 512, 1024, 2048];

    await showModalBottomSheet<void>(
      context: context,
      showDragHandle: true,
      isScrollControlled: true,
      builder: (sheetContext) => StatefulBuilder(
        builder: (sheetContext, setSheetState) {
          final strings = AppLocalizations.of(sheetContext);
          final theme = Theme.of(sheetContext);

          Future<void> toggle(String key, bool enabled) async {
            final before = quickSettings;
            setSheetState(() {
              quickSettings = _withDisplaySetting(
                quickSettings,
                key,
                enabled,
              );
            });
            final saved = await _setDisplaySetting(key, enabled);
            if (!saved && sheetContext.mounted) {
              setSheetState(() => quickSettings = before);
            }
          }

          void updateSideline({
            int? depth,
            int? multiPv,
            int? threads,
            int? hashMb,
          }) {
            setSheetState(() {
              quickDepth = depth ?? quickDepth;
              quickMultiPv = multiPv ?? quickMultiPv;
              quickThreads = threads ?? quickThreads;
              quickHashMb = hashMb ?? quickHashMb;
            });
            if (mounted) {
              setState(() {
                _sidelineDepth = depth ?? _sidelineDepth;
                _sidelineMultiPv = multiPv ?? _sidelineMultiPv;
                _sidelineThreads = threads ?? _sidelineThreads;
                _sidelineHashMb = hashMb ?? _sidelineHashMb;
              });
            }
          }

          Widget settingTile({
            required String key,
            required String title,
            required String subtitle,
            required bool value,
            required IconData icon,
          }) => SwitchListTile(
            secondary: Icon(icon),
            title: Text(title),
            subtitle: Text(subtitle),
            value: value,
            onChanged: (enabled) => toggle(key, enabled),
          );

          Widget numberSlider({
            required String title,
            required int value,
            required int min,
            required int max,
            required ValueChanged<int> onChanged,
            String Function(int value)? valueLabel,
          }) {
            final label = valueLabel?.call(value) ?? '$value';
            return Padding(
              padding: const EdgeInsets.fromLTRB(8, 2, 8, 2),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.stretch,
                children: [
                  Row(
                    children: [
                      Expanded(
                        child: Text(
                          title,
                          style: theme.textTheme.bodyMedium?.copyWith(
                            fontWeight: FontWeight.w600,
                          ),
                        ),
                      ),
                      Text(
                        label,
                        textDirection: TextDirection.ltr,
                        style: theme.textTheme.labelLarge?.copyWith(
                          fontWeight: FontWeight.w800,
                        ),
                      ),
                    ],
                  ),
                  Slider(
                    value: value.toDouble(),
                    min: min.toDouble(),
                    max: max.toDouble(),
                    divisions: max - min,
                    label: label,
                    onChanged: (next) => onChanged(next.round()),
                  ),
                ],
              ),
            );
          }

          final foundHashIndex = hashOptions.indexOf(quickHashMb);
          final hashIndex = foundHashIndex < 0 ? 0 : foundHashIndex;

          return SafeArea(
            top: false,
            child: ConstrainedBox(
              constraints: const BoxConstraints(maxWidth: 620),
              child: SingleChildScrollView(
                padding: const EdgeInsets.fromLTRB(16, 0, 16, 20),
                child: Column(
                  mainAxisSize: MainAxisSize.min,
                  crossAxisAlignment: CrossAxisAlignment.stretch,
                  children: [
                    Text(
                      strings.analysisSettingsTitle,
                      style: theme.textTheme.titleLarge,
                    ),
                    const SizedBox(height: 4),
                    Text(
                      strings.analysisSettingsSubtitle,
                      style: theme.textTheme.bodyMedium,
                    ),
                    const SizedBox(height: 8),
                    settingTile(
                      key: 'showBestMoveArrow',
                      title: strings.bestMoveArrow,
                      subtitle: strings.bestMoveArrowHelp,
                      value: quickSettings.showBestMoveArrow,
                      icon: Icons.arrow_outward,
                    ),
                    settingTile(
                      key: 'showThreatArrow',
                      title: strings.threatArrow,
                      subtitle: strings.threatArrowHelp,
                      value: quickSettings.showThreatArrow,
                      icon: Icons.warning_amber_rounded,
                    ),
                    settingTile(
                      key: 'showEvaluationBar',
                      title: strings.evaluationBarSetting,
                      subtitle: strings.evaluationBarSettingHelp,
                      value: quickSettings.showEvaluationBar,
                      icon: Icons.balance,
                    ),
                    settingTile(
                      key: 'showEngineLines',
                      title: strings.showEngineLinesSetting,
                      subtitle: strings.showEngineLinesSettingHelp,
                      value: quickSettings.showEngineLines,
                      icon: Icons.account_tree_outlined,
                    ),
                    settingTile(
                      key: 'showClassifications',
                      title: strings.showClassificationsSetting,
                      subtitle: strings.showClassificationsSettingHelp,
                      value: quickSettings.showClassifications,
                      icon: Icons.auto_awesome,
                    ),
                    settingTile(
                      key: 'showResultSymbols',
                      title: strings.showResultSymbolsSetting,
                      subtitle: strings.showResultSymbolsSettingHelp,
                      value: quickSettings.showResultSymbols,
                      icon: Icons.emoji_events_outlined,
                    ),
                    const Divider(height: 26),
                    Row(
                      children: [
                        const Icon(Icons.memory_outlined, size: 20),
                        const SizedBox(width: 8),
                        Expanded(
                          child: Column(
                            crossAxisAlignment: CrossAxisAlignment.start,
                            children: [
                              Text(
                                strings.sidelineEngineTitle,
                                style: theme.textTheme.titleMedium?.copyWith(
                                  fontWeight: FontWeight.w800,
                                ),
                              ),
                              Text(
                                strings.sidelineEngineSubtitle,
                                style: theme.textTheme.bodySmall,
                              ),
                            ],
                          ),
                        ),
                      ],
                    ),
                    const SizedBox(height: 8),
                    numberSlider(
                      title: strings.depth,
                      value: quickDepth,
                      min: 1,
                      max: 64,
                      onChanged: (value) => updateSideline(depth: value),
                    ),
                    numberSlider(
                      title: strings.numberOfLines,
                      value: quickMultiPv,
                      min: 1,
                      max: 8,
                      onChanged: (value) => updateSideline(multiPv: value),
                    ),
                    numberSlider(
                      title: strings.threads,
                      value: AppController.clampEngineWorkerThreads(
                        quickThreads,
                      ),
                      min: 1,
                      max: AppController.maximumEngineWorkerThreads,
                      onChanged: (value) => updateSideline(
                        threads: AppController.clampEngineWorkerThreads(value),
                      ),
                    ),
                    Padding(
                      padding: const EdgeInsets.fromLTRB(8, 2, 8, 2),
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.stretch,
                        children: [
                          Row(
                            children: [
                              Expanded(
                                child: Text(
                                  strings.hashMemory,
                                  style: theme.textTheme.bodyMedium?.copyWith(
                                    fontWeight: FontWeight.w600,
                                  ),
                                ),
                              ),
                              Text(
                                '$quickHashMb MB',
                                textDirection: TextDirection.ltr,
                                style: theme.textTheme.labelLarge?.copyWith(
                                  fontWeight: FontWeight.w800,
                                ),
                              ),
                            ],
                          ),
                          Slider(
                            value: hashIndex.toDouble(),
                            min: 0,
                            max: (hashOptions.length - 1).toDouble(),
                            divisions: hashOptions.length - 1,
                            label: '$quickHashMb MB',
                            onChanged: (next) => updateSideline(
                              hashMb: hashOptions[next.round()],
                            ),
                          ),
                        ],
                      ),
                    ),
                  ],
                ),
              ),
            ),
          );
        },
      ),
    );
  }

  Future<void> _onBoardSquare(String square) async {
    if (_variationMovePending) return;
    final source = _selectedSquare;
    if (source == null) {
      setState(() {
        _selectedSquare = square;
        _variationError = null;
      });
      return;
    }
    if (source == square) {
      setState(() => _selectedSquare = null);
      return;
    }
    await _playBoardMove(source, square);
  }

  Future<void> _onBoardDrop(String source, String target) async {
    if (source == target || _variationMovePending) return;
    await _playBoardMove(source, target);
  }

  void _onBoardDragStarted(String square) {
    if (!mounted) return;
    setState(() {
      _selectedSquare = square;
      _variationError = null;
    });
  }

  void _onBoardDragEnded() {
    if (!mounted || _selectedSquare == null) return;
    setState(() => _selectedSquare = null);
  }

  Future<void> _playBoardMove(String source, String target) async {
    if (_variationMovePending) return;
    final session = _variationSession;
    final activeFen = session == null
        ? _displayFen
        : session.current?.fen ?? session.startingFen;
    var uci = '$source$target';
    final fields = activeFen.split(' ');
    final placement = fields.isEmpty ? '' : fields.first;
    if ((target.endsWith('8') || target.endsWith('1')) &&
        _pieceAt(placement, source).toLowerCase() == 'p') {
      uci += 'q';
    }

    // A move played on the board is only a sideline when it actually deviates
    // from the recorded PGN. If this exact position + move occurs later in the
    // main line (including a rejoin after a transposition), jump back onto the
    // PGN instead of starting a variation engine job.
    final mainLinePly = _matchingMainLinePly(activeFen, uci);
    if (mainLinePly != null) {
      if (session == null) {
        await _selectPly(mainLinePly);
      } else {
        await _rejoinMainLineAt(mainLinePly);
      }
      return;
    }

    // Entering a sideline pauses every main-line poll immediately. Native code
    // then transfers the single Stockfish slot to the sideline worker.
    if (session == null) _controller.pauseForVariation();
    _variationTimer?.cancel();
    setState(() {
      _variationMovePending = true;
      _selectedSquare = null;
      _selectedLine = 0;
      _variationError = null;
    });
    try {
      final variation = await widget.gateway.startVariationAnalysis(
        fen: activeFen,
        uci: uci,
        depth: _sidelineDepth,
        multiPv: _sidelineMultiPv,
        threads: _sidelineThreads,
        hashMb: _sidelineHashMb,
      );
      if (!mounted) {
        if (variation.isRunning) {
          unawaited(widget.gateway.cancelVariationAnalysis(variation.jobId));
        }
        return;
      }
      setState(() {
        final activeSession = _variationSession ??=
            _VariationSession(parentPly: _currentPly, startingFen: _displayFen);
        if (activeSession.currentIndex + 1 < activeSession.moves.length) {
          activeSession.moves.removeRange(
            activeSession.currentIndex + 1,
            activeSession.moves.length,
          );
        }
        // Previous sideline positions are snapshots only. They must never keep
        // a hidden engine alive after the user has played the next move.
        for (var index = 0; index < activeSession.moves.length; index++) {
          final old = activeSession.moves[index];
          if (old.isRunning) {
            activeSession.moves[index] = old.copyWith(status: 'paused');
          }
        }
        activeSession.moves.add(variation);
        activeSession.currentIndex = activeSession.moves.length - 1;
        _activeVariationJobId = variation.jobId;
        _variationMovePending = false;
      });
      _pollVariation(variation.jobId, _variationSession!.currentIndex);
    } catch (error) {
      if (mounted) {
        setState(() {
          _variationMovePending = false;
          _variationError = error;
        });
        if (_variationSession == null) {
          unawaited(_controller.resumeAfterVariation(math.max(0, _currentPly)));
        }
      }
    }
  }

  String get _displayFen {
    final detail = _controller.detail;
    if (detail == null || detail.moves.isEmpty) {
      return detail?.summary.startingFen ?? widget.game.startingFen;
    }
    if (_currentPly < 0) {
      return detail.summary.startingFen;
    }
    final index = _currentPly >= detail.moves.length
        ? detail.moves.length - 1
        : _currentPly;
    return detail.moves[index].fenAfter;
  }

  int? _matchingMainLinePly(String fen, String uci) {
    final moves = _controller.detail?.moves;
    if (moves == null || moves.isEmpty) return null;

    final session = _variationSession;
    final firstCandidate = session == null
        ? _currentPly + 1
        : session.parentPly + 1;
    final normalizedUci = uci.toLowerCase();
    for (var index = firstCandidate < 0 ? 0 : firstCandidate;
        index < moves.length;
        index++) {
      final recorded = moves[index];
      if (recorded.uci.toLowerCase() == normalizedUci &&
          _sameChessPosition(recorded.fenBefore, fen)) {
        return index;
      }
    }
    return null;
  }

  bool _sameChessPosition(String left, String right) {
    final leftFields = left.trim().split(RegExp(r'\s+'));
    final rightFields = right.trim().split(RegExp(r'\s+'));
    if (leftFields.length < 4 || rightFields.length < 4) return left == right;
    for (var index = 0; index < 4; index++) {
      if (leftFields[index] != rightFields[index]) return false;
    }
    return true;
  }

  Future<void> _rejoinMainLineAt(int ply) async {
    _variationTimer?.cancel();
    final activeJobId = _activeVariationJobId;
    _activeVariationJobId = null;
    if (mounted) {
      setState(() {
        _variationSession = null;
        _variationError = null;
        _variationMovePending = false;
        _selectedSquare = null;
        _selectedLine = 0;
        _followLatest = false;
        _currentPly = ply;
      });
    }
    if (activeJobId != null) {
      try {
        await widget.gateway.cancelVariationAnalysis(activeJobId);
      } catch (_) {
        // The sideline worker may have completed just before the rejoin.
      }
    }
    if (!mounted) return;
    await _controller.resumeAfterVariation(ply);
    await _loadAfterMoveSnapshot(ply);
  }

  String _pieceAt(String placement, String square) {
    if (square.length != 2) return '';
    final target =
        (8 - (int.tryParse(square[1]) ?? 0)) * 8 +
        square.codeUnitAt(0) -
        'a'.codeUnitAt(0);
    var index = 0;
    for (final character in placement.split('')) {
      final empty = int.tryParse(character);
      if (empty != null) {
        if (target >= index && target < index + empty) return '';
        index += empty;
      } else if (character != '/') {
        if (index == target) return character;
        index++;
      }
    }
    return '';
  }

  void _pollVariation(String jobId, int moveIndex) {
    _variationTimer?.cancel();
    final session = _variationSession;
    final current = session != null &&
            moveIndex >= 0 &&
            moveIndex < session.moves.length
        ? session.moves[moveIndex]
        : null;
    if (current == null ||
        current.jobId != jobId ||
        _activeVariationJobId != jobId ||
        !current.isRunning) {
      return;
    }
    _variationTimer = Timer(const Duration(milliseconds: 300), () async {
      try {
        final updated = await widget.gateway.variationAnalysisStatus(jobId);
        final session = _variationSession;
        if (!mounted ||
            session == null ||
            moveIndex >= session.moves.length ||
            session.moves[moveIndex].jobId != jobId) {
          return;
        }
        setState(() {
          session.moves[moveIndex] = updated;
          if (!updated.isRunning && _activeVariationJobId == jobId) {
            _activeVariationJobId = null;
          }
        });
        if (updated.isRunning) _pollVariation(jobId, moveIndex);
      } catch (error) {
        if (mounted) setState(() => _variationError = error);
      }
    });
  }

  void _returnToMainLine() {
    _variationTimer?.cancel();
    final activeJobId = _activeVariationJobId;
    _activeVariationJobId = null;
    setState(() {
      _variationSession = null;
      _variationError = null;
      _selectedSquare = null;
      _variationMovePending = false;
    });
    unawaited(_leaveVariationAndResume(activeJobId));
  }

  Future<void> _leaveVariationAndResume(String? activeJobId) async {
    if (activeJobId != null) {
      try {
        await widget.gateway.cancelVariationAnalysis(activeJobId);
      } catch (_) {
        // A just-finished live job may already have been reaped natively.
      }
    }
    if (mounted) await _controller.resumeAfterVariation(math.max(0, _currentPly));
  }

  void _navigateVariation(int index) {
    final session = _variationSession;
    if (session == null) return;
    final selected = index < -1
        ? -1
        : (index >= session.moves.length ? session.moves.length - 1 : index);
    _variationTimer?.cancel();
    setState(() {
      session.currentIndex = selected;
      _selectedLine = 0;
      _selectedSquare = null;
    });
    final current = session.current;
    if (current?.isRunning == true &&
        current!.jobId == _activeVariationJobId) {
      _pollVariation(current.jobId, selected);
    }
  }

  String _variationPgn(ParsedMove? parent) {
    final session = _variationSession;
    if (session == null || session.moves.isEmpty) return '';
    final fields = session.startingFen.split(' ');
    var whiteToMove = fields.length > 1 ? fields[1] == 'w' : true;
    var moveNumber = fields.length > 5 ? int.tryParse(fields[5]) ?? 1 : 1;
    final tokens = <String>[];
    for (var index = 0; index < session.moves.length; index++) {
      final san = session.moves[index].playedSan;
      if (whiteToMove) {
        tokens.add('$moveNumber. $san');
      } else {
        tokens.add(index == 0 ? '$moveNumber... $san' : san);
        moveNumber++;
      }
      whiteToMove = !whiteToMove;
    }
    final anchor = parent == null
        ? ''
        : '${parent.moveNumber}${parent.sideToMove == 'black' ? '…' : '.'} ${parent.san} ';
    return '$anchor(${tokens.join(' ')})';
  }

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final snapshot = _controller.snapshot;
    final displayed = _controller.displayedSnapshot;
    final detail = _controller.detail;
    final gameSummary = detail?.summary ?? widget.game;
    final atInitialPosition =
        detail != null && detail.moves.isNotEmpty && _currentPly < 0;
    final moveIndex = detail == null || detail.moves.isEmpty
        ? -1
        : (atInitialPosition
              ? -1
              : (_currentPly >= detail.moves.length
                    ? detail.moves.length - 1
                    : _currentPly));
    final move = detail != null && moveIndex >= 0
        ? detail.moves[moveIndex]
        : null;
    final originalFen = atInitialPosition
        ? detail.summary.startingFen
        : move?.fenAfter ??
              detail?.summary.startingFen ??
              widget.game.startingFen;
    final variation = _variationSession?.current;
    final fen = variation?.fen ?? _variationSession?.startingFen ?? originalFen;
    final afterMoveSnapshot = moveIndex >= 0 && _afterMoveSlot == moveIndex + 1
        ? _afterMoveSnapshot
        : null;
    final mainPositionLines = move == null
        ? (displayed?.lines ?? const <EngineLine>[])
        : (afterMoveSnapshot?.lines ?? const <EngineLine>[]);
    final lines = variation?.lines ?? mainPositionLines;
    final lineIndex = _selectedLine < 0
        ? 0
        : (_selectedLine >= lines.length ? lines.length - 1 : _selectedLine);
    final selectedLine = lines.isEmpty ? null : lines[lineIndex];
    final arrowMove =
        selectedLine?.bestMove ??
        variation?.bestMove ??
        (move == null ? displayed?.bestMove : afterMoveSnapshot?.bestMove) ??
        '';
    final profileSide = snapshot?.summary?.profileSide ?? gameSummary.profileColor;
    final playerIsBlack = profileSide == 'black';
    final opponentName = playerIsBlack
        ? gameSummary.whiteName
        : gameSummary.blackName;
    final opponentRating = playerIsBlack
        ? gameSummary.whiteRating
        : gameSummary.blackRating;
    final playerName = playerIsBlack
        ? gameSummary.blackName
        : gameSummary.whiteName;
    final playerRating = playerIsBlack
        ? gameSummary.blackRating
        : gameSummary.whiteRating;
    final openingName = gameSummary.openingName?.trim() ?? '';
    final openingEco = gameSummary.openingEco?.trim() ?? '';
    final openingLabel = openingName.isEmpty
        ? null
        : (openingEco.isEmpty ? openingName : '$openingEco · $openingName');
    final hasKnownProfileColor = profileSide == 'white' || profileSide == 'black';
    final playerColor = playerIsBlack ? 'black' : 'white';
    final opponentColor = playerIsBlack ? 'white' : 'black';
    // By default the account/profile player is always at the bottom.
    // The rotate button explicitly inverts that orientation.
    final blackAtBottom = playerIsBlack != _boardRotated;
    final fenFields = fen.split(' ');
    final sideToMove = fenFields.length > 1
        ? (fenFields[1] == 'b' ? 'black' : 'white')
        : 'unknown';
    final opponentToMove =
        (profileSide == 'white' || profileSide == 'black') &&
        sideToMove != profileSide;
    final bestArrowMove = opponentToMove ? '' : arrowMove;
    final threatArrowMove = opponentToMove ? arrowMove : '';
    final atMainLineEnd =
        _variationSession == null &&
        detail != null &&
        detail.moves.isNotEmpty &&
        moveIndex == detail.moves.length - 1;
    final terminalLine = afterMoveSnapshot != null &&
            afterMoveSnapshot.lines.isNotEmpty
        ? afterMoveSnapshot.lines.first
        : null;
    final terminalOutcome = atMainLineEnd
        ? _terminalBoardOutcome(move, detail.summary.result, terminalLine)
        : null;
    final terminalResult = terminalOutcome?.result;
    final finalMove = detail != null && detail.moves.isNotEmpty
        ? detail.moves.last
        : null;
    final gameOutcome = _terminalBoardOutcome(
      finalMove,
      detail?.summary.result ?? widget.game.result,
      null,
    );

    return Scaffold(
      appBar: AppBar(
        title: Text(strings.analysis),
        actions: [
          if (snapshot?.isRunning == true)
            IconButton(
              key: const Key('cancel-analysis'),
              onPressed: _controller.cancel,
              tooltip: strings.cancelAnalysis,
              icon: const Icon(Icons.stop_circle_outlined),
            ),
          IconButton(
            key: const Key('rotate-analysis-board'),
            onPressed: () => setState(() => _boardRotated = !_boardRotated),
            tooltip: strings.rotateBoard,
            icon: const Icon(Icons.rotate_90_degrees_ccw_rounded),
          ),
          IconButton(
            key: const Key('delete-saved-analysis'),
            onPressed: _deleteStoredAnalysis,
            tooltip: strings.deleteAnalysis,
            icon: const Icon(Icons.delete_outline_rounded),
          ),
          if (snapshot?.summary case final summary?) ...[
            Builder(
              builder: (context) {
                var beforeDepth = displayed?.lines.isEmpty ?? true
                    ? 0
                    : displayed!.lines.first.depth;
                var afterDepth = afterMoveSnapshot?.lines.isEmpty ?? true
                    ? 0
                    : afterMoveSnapshot!.lines.first.depth;
                final liveSnapshot = displayed?.isRunning == true
                    ? displayed
                    : (afterMoveSnapshot?.isRunning == true
                          ? afterMoveSnapshot
                          : null);
                if (liveSnapshot != null && liveSnapshot.liveDepth > 0) {
                  if (liveSnapshot.currentPly == _currentPly) {
                    beforeDepth = math.max(beforeDepth, liveSnapshot.liveDepth);
                  } else if (liveSnapshot.currentPly == _currentPly + 1) {
                    afterDepth = math.max(afterDepth, liveSnapshot.liveDepth);
                  }
                }
                final minDepth = _settings.minAnalysisDepth;
                final maxDepth = _settings.depth;
                final theoryMove =
                    displayed?.classification == MoveClassification.theory;
                double depthProgress(int depth, bool qualityComplete) =>
                    qualityComplete || maxDepth <= minDepth
                    ? 1.0
                    : ((depth - minDepth) / (maxDepth - minDepth))
                          .clamp(0.0, 1.0);
                final beforeProgress = depthProgress(
                  beforeDepth,
                  displayed?.qualityComplete ?? false,
                );
                final afterProgress = depthProgress(
                  afterDepth,
                  afterMoveSnapshot?.qualityComplete ?? false,
                );
                final progress = theoryMove
                    ? 1.0
                    : (move == null
                          ? beforeProgress
                          : (beforeProgress + afterProgress) / 2.0);
                final finished = progress >= 1.0;
                final scheme = Theme.of(context).colorScheme;
                return Tooltip(
                  message: theoryMove
                      ? strings.liveEngineTheorySkipped
                      : (finished
                            ? strings.liveEngineTargetReached
                            : strings.liveEngineProgress(
                                (progress * 100).round(),
                              )),
                  child: Padding(
                    padding: const EdgeInsets.symmetric(horizontal: 4),
                    child: SizedBox(
                      key: const Key('live-engine-status'),
                      width: 28,
                      height: 28,
                      child: Stack(
                        alignment: Alignment.center,
                        children: [
                          SizedBox(
                            width: 26,
                            height: 26,
                            child: CircularProgressIndicator(
                              value: progress,
                              strokeWidth: 2.6,
                              strokeCap: StrokeCap.round,
                              color: Colors.green,
                              backgroundColor: scheme.outlineVariant,
                            ),
                          ),
                          Icon(
                            Icons.memory_rounded,
                            size: 16,
                            color: finished ? Colors.green : scheme.outline,
                          ),
                        ],
                      ),
                    ),
                  ),
                );
              },
            ),
            TextButton.icon(
              key: const Key('reopen-summary'),
              onPressed: () => _showSummary(summary),
              icon: const Icon(Icons.assessment_outlined),
              label: Text(strings.summary),
            ),
          ],
        ],
      ),
      body: Column(
        children: [
          if (snapshot?.isRunning == true)
            LinearProgressIndicator(
              key: const Key('analysis-progress'),
              value: snapshot?.progress,
              semanticsLabel: strings.analyzing,
            ),
          Expanded(
            child: LayoutBuilder(
              builder: (context, constraints) {
                final lastMoveUci = variation?.playedMove ??
                    (_variationSession == null ? move?.uci ?? '' : '');
                final board = _Board(
                  fen: fen,
                  bestArrowMove: bestArrowMove,
                  threatArrowMove: threatArrowMove,
                  lastMoveUci: lastMoveUci,
                  showBestArrow: _settings.showBestMoveArrow,
                  showThreatArrow: _settings.showThreatArrow,
                  showCoordinates: _settings.showBoardCoordinates,
                  highlightLastMove: _settings.highlightLastMove,
                  highlightSelectedSquare:
                      _settings.highlightSelectedSquare,
                  opponentName: opponentName,
                  opponentRating: opponentRating,
                  opponentColor: opponentColor,
                  playerName: playerName,
                  playerRating: playerRating,
                  playerColor: playerColor,
                  playerOpening: hasKnownProfileColor ? openingLabel : null,
                  blackAtBottom: blackAtBottom,
                  evaluationLine: lines.isEmpty ? null : lines.first,
                  showEvaluationBar: _settings.showEvaluationBar,
                  showResultSymbols: _settings.showResultSymbols,
                  terminalResult: terminalResult,
                  terminalCheckmate: terminalOutcome?.checkmate ?? false,
                  gameResult: gameOutcome?.result,
                  gameCheckmate: gameOutcome?.checkmate ?? false,
                  resultPresentationStarted: _resultPresentationStarted,
                  resultPresentationDocked: _resultPresentationDocked,
                  onResultPresentationStarted: () {
                    if (!mounted || _resultPresentationStarted) return;
                    setState(() => _resultPresentationStarted = true);
                  },
                  onResultPresentationDocked: () {
                    if (!mounted || _resultPresentationDocked) return;
                    setState(() => _resultPresentationDocked = true);
                  },
                  currentMoveClassification: _settings.showClassifications &&
                          !atInitialPosition
                      ? (variation?.classification ?? displayed?.classification)
                      : null,
                  classificationMoveUci: atInitialPosition
                      ? ''
                      : variation?.playedMove ?? (move?.uci ?? ''),
                  selectedSquare: _selectedSquare,
                  onSquareTap: _onBoardSquare,
                  onPieceDrop: _onBoardDrop,
                  onDragStarted: _onBoardDragStarted,
                  onDragEnded: _onBoardDragEnded,
                );
                final details = _AnalysisDetails(
                  status: snapshot,
                  displayed: displayed,
                  detail: detail,
                  currentFen: fen,
                  move: move,
                  selectedLine: _selectedLine,
                  onSelectLine: (index) =>
                      setState(() => _selectedLine = index),
                  error: _controller.error,
                  variation: variation,
                  variationActive: _variationSession != null,
                  variationPgn: _variationPgn(move),
                  variationIndex: _variationSession?.currentIndex ?? -1,
                  variationLength: _variationSession?.moves.length ?? 0,
                  onVariationFirst: () => _navigateVariation(-1),
                  onVariationPrevious: () => _navigateVariation(
                    (_variationSession?.currentIndex ?? 0) - 1,
                  ),
                  onVariationNext: () => _navigateVariation(
                    (_variationSession?.currentIndex ?? -1) + 1,
                  ),
                  onVariationLast: () => _navigateVariation(
                    (_variationSession?.moves.length ?? 1) - 1,
                  ),
                  variationError: _variationError,
                  currentPositionLines: mainPositionLines,
                  onReturnToMainLine: _returnToMainLine,
                  settings: _settings,
                );
                if (constraints.maxWidth >= 900) {
                  return Row(
                    crossAxisAlignment: CrossAxisAlignment.stretch,
                    children: [
                      Flexible(
                        flex: 68,
                        child: SizedBox.expand(child: board),
                      ),
                      const VerticalDivider(width: 1),
                      Flexible(flex: 32, child: details),
                    ],
                  );
                }
                final boardHeight = math.min(
                  constraints.maxHeight * 0.66,
                  constraints.maxWidth + 84,
                );
                return Column(
                  children: [
                    SizedBox(
                      height: boardHeight,
                      width: double.infinity,
                      child: board,
                    ),
                    const Divider(height: 1),
                    Expanded(child: details),
                  ],
                );
              },
            ),
          ),
          SafeArea(
            top: false,
            child: _AnalysisControls(
              playing: _playing,
              onFirst: () => _selectPly(-1),
              onPrevious: () => _selectPly(_currentPly - 1),
              onPlayPause: _togglePlayback,
              onNext: () => _selectPly(_currentPly + 1),
              onLast: () => _selectPly(_maximumPly),
              onSettings: _showQuickAnalysisSettings,
            ),
          ),
        ],
      ),
    );
  }
}

class _Board extends StatelessWidget {
  const _Board({
    required this.fen,
    required this.bestArrowMove,
    required this.threatArrowMove,
    required this.lastMoveUci,
    required this.showBestArrow,
    required this.showThreatArrow,
    required this.showCoordinates,
    required this.highlightLastMove,
    required this.highlightSelectedSquare,
    required this.opponentName,
    required this.opponentRating,
    required this.opponentColor,
    required this.playerName,
    required this.playerRating,
    required this.playerColor,
    required this.playerOpening,
    required this.blackAtBottom,
    required this.evaluationLine,
    required this.showEvaluationBar,
    required this.showResultSymbols,
    required this.terminalResult,
    required this.terminalCheckmate,
    required this.gameResult,
    required this.gameCheckmate,
    required this.resultPresentationStarted,
    required this.resultPresentationDocked,
    required this.onResultPresentationStarted,
    required this.onResultPresentationDocked,
    required this.currentMoveClassification,
    required this.classificationMoveUci,
    required this.selectedSquare,
    required this.onSquareTap,
    required this.onPieceDrop,
    required this.onDragStarted,
    required this.onDragEnded,
  });

  final String fen;
  final String bestArrowMove;
  final String threatArrowMove;
  final String lastMoveUci;
  final bool showBestArrow;
  final bool showThreatArrow;
  final bool showCoordinates;
  final bool highlightLastMove;
  final bool highlightSelectedSquare;
  final String opponentName;
  final int? opponentRating;
  final String opponentColor;
  final String playerName;
  final int? playerRating;
  final String playerColor;
  final String? playerOpening;
  final bool blackAtBottom;
  final EngineLine? evaluationLine;
  final bool showEvaluationBar;
  final bool showResultSymbols;
  final String? terminalResult;
  final bool terminalCheckmate;
  final String? gameResult;
  final bool gameCheckmate;
  final bool resultPresentationStarted;
  final bool resultPresentationDocked;
  final VoidCallback onResultPresentationStarted;
  final VoidCallback onResultPresentationDocked;
  final MoveClassification? currentMoveClassification;
  final String classificationMoveUci;
  final String? selectedSquare;
  final ValueChanged<String> onSquareTap;
  final void Function(String source, String target) onPieceDrop;
  final ValueChanged<String> onDragStarted;
  final VoidCallback onDragEnded;

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


  List<String> get _pieces {
    final fields = fen.split(' ');
    final placement = fields.isEmpty ? '' : fields.first;
    final result = <String>[];
    for (final character in placement.split('')) {
      final empty = int.tryParse(character);
      if (empty != null) {
        result.addAll(List.filled(empty, ''));
      } else if (character != '/') {
        result.add(character);
      }
    }
    if (result.length != 64) return List.filled(64, '');
    return result;
  }

  bool _canDragPiece(String piece) {
    if (piece.isEmpty) return false;
    final fields = fen.split(' ');
    if (fields.length < 2) return true;
    final whiteToMove = fields[1] == 'w';
    final whitePiece = piece == piece.toUpperCase();
    return whitePiece == whiteToMove;
  }

  @override
  Widget build(BuildContext context) {
    final pieces = _pieces;
    return Padding(
      padding: const EdgeInsets.all(8),
      child: LayoutBuilder(
        builder: (context, constraints) {
          const stripHeight = 58.0;
          const stripGap = 4.0;
          const evaluationHeight = 22.0;
          const evaluationGap = 4.0;
          final evaluationSpace =
              showEvaluationBar ? evaluationHeight + evaluationGap : 0.0;
          final boardSide = math
              .max(
                0.0,
                math.min(
                  constraints.maxWidth,
                  constraints.maxHeight -
                      stripHeight * 2 -
                      stripGap * 2 -
                      evaluationSpace,
                ),
              )
              .toDouble();
          final board = SizedBox.square(
            dimension: boardSide,
            child: Stack(
              key: const Key('analysis-board'),
              fit: StackFit.expand,
              children: [
                GridView.builder(
                  physics: const NeverScrollableScrollPhysics(),
                  gridDelegate: const SliverGridDelegateWithFixedCrossAxisCount(
                    crossAxisCount: 8,
                  ),
                  itemCount: 64,
                  itemBuilder: (context, index) {
                    final row = index ~/ 8;
                    final column = index % 8;
                    final fileIndex = blackAtBottom ? 7 - column : column;
                    final rank = blackAtBottom ? row + 1 : 8 - row;
                    final square =
                        '${String.fromCharCode('a'.codeUnitAt(0) + fileIndex)}$rank';
                    final lightSquare = (row + column).isEven;
                    final baseColor = lightSquare
                        ? const Color(0xFFE8E5DC)
                        : const Color(0xFF71867D);
                    final lastMove = highlightLastMove &&
                        lastMoveUci.length >= 4 &&
                        (lastMoveUci.substring(0, 2) == square ||
                            lastMoveUci.substring(2, 4) == square);
                    final selected =
                        highlightSelectedSquare && selectedSquare == square;
                    final scheme = Theme.of(context).colorScheme;
                    final classification = currentMoveClassification;
                    final classificationSource =
                        classificationMoveUci.length >= 4
                            ? classificationMoveUci.substring(0, 2)
                            : '';
                    final classificationTarget =
                        classificationMoveUci.length >= 4
                            ? classificationMoveUci.substring(2, 4)
                            : '';
                    final classificationSquare =
                        classification != null &&
                        classification != MoveClassification.unknown &&
                        (classificationSource == square ||
                            classificationTarget == square);
                    final moveColor = classificationSquare
                        ? Color.alphaBlend(
                            _classificationColor(
                              context,
                              classification,
                            ).withValues(alpha: 0.42),
                            baseColor,
                          )
                        : lastMove
                        ? Color.alphaBlend(
                            scheme.primary.withValues(alpha: 0.30),
                            baseColor,
                          )
                        : baseColor;
                    final squareColor = selected
                        ? Color.alphaBlend(
                            scheme.tertiary.withValues(alpha: 0.40),
                            moveColor,
                          )
                        : moveColor;
                    final coordinateColor = lightSquare
                        ? const Color(0xFF53655E)
                        : const Color(0xFFE8E5DC);
                    final pieceIndex = (8 - rank) * 8 + fileIndex;
                    final piece = pieces[pieceIndex];
                    final pieceAsset = _pieceAssets[piece];
                    final showClassificationBadge =
                        classification != null &&
                        classification != MoveClassification.unknown &&
                        classificationTarget == square;
                    final canDrag = pieceAsset != null && _canDragPiece(piece);
                    final squareSide = boardSide / 8;
                    final pieceInset = squareSide * 0.055;

                    Widget pieceImage() => Padding(
                      padding: EdgeInsets.all(pieceInset),
                      child: SvgPicture.asset(
                        pieceAsset!,
                        fit: BoxFit.contain,
                        alignment: Alignment.center,
                      ),
                    );

                    return DragTarget<String>(
                      onWillAcceptWithDetails: (details) =>
                          details.data != square,
                      onAcceptWithDetails: (details) =>
                          onPieceDrop(details.data, square),
                      builder: (context, candidateData, rejectedData) {
                        final targetColor = candidateData.isNotEmpty
                            ? Color.alphaBlend(
                                scheme.secondary.withValues(alpha: 0.34),
                                squareColor,
                              )
                            : squareColor;
                        return InkWell(
                          key: Key('board-square-$square'),
                          onTap: () => onSquareTap(square),
                          child: ColoredBox(
                            color: targetColor,
                            child: Stack(
                              fit: StackFit.expand,
                              children: [
                                if (pieceAsset != null)
                                  Positioned.fill(
                                    child: canDrag
                                        ? Draggable<String>(
                                            data: square,
                                            onDragStarted: () =>
                                                onDragStarted(square),
                                            onDragEnd: (_) => onDragEnded(),
                                            feedback: Material(
                                              color: Colors.transparent,
                                              child: SizedBox.square(
                                                dimension: squareSide,
                                                child: pieceImage(),
                                              ),
                                            ),
                                            childWhenDragging:
                                                const SizedBox.expand(),
                                            child: pieceImage(),
                                          )
                                        : pieceImage(),
                                  ),
                                if (showClassificationBadge &&
                                    classification != null)
                                  Positioned(
                                    top: 2,
                                    right: 2,
                                    child: IgnorePointer(
                                      child: Builder(
                                        builder: (context) {
                                          final badgeSize = math.max(
                                            18.0,
                                            math.min(
                                              36.0,
                                              boardSide / 8 * 0.46,
                                            ),
                                          );
                                          final asset = _analysisClassificationAsset(classification);
                                          if (asset != null) {
                                            return Image.asset(
                                              asset,
                                              key: Key(
                                                'board-classification-${classification.name}',
                                              ),
                                              width: badgeSize,
                                              height: badgeSize,
                                              fit: BoxFit.contain,
                                              errorBuilder: (_, _, _) =>
                                                  const SizedBox.shrink(),
                                            );
                                          }
                                          return Icon(
                                            classification ==
                                                    MoveClassification.critical
                                                ? Icons.bolt_rounded
                                                : Icons.auto_awesome,
                                            key: Key(
                                              'board-classification-${classification.name}',
                                            ),
                                            size: badgeSize * 0.82,
                                            color: _classificationColor(
                                              context,
                                              classification,
                                            ),
                                          );
                                        },
                                      ),
                                    ),
                                  ),
                                if (showCoordinates && column == 0)
                                  Positioned(
                                    left: 3,
                                    top: 2,
                                    child: Text(
                                      '$rank',
                                      textDirection: TextDirection.ltr,
                                      style: TextStyle(
                                        color: coordinateColor,
                                        fontSize: 10,
                                        fontWeight: FontWeight.w700,
                                      ),
                                    ),
                                  ),
                                if (showCoordinates && row == 7)
                                  Positioned(
                                    right: 3,
                                    bottom: 1,
                                    child: Text(
                                      String.fromCharCode(
                                        'a'.codeUnitAt(0) + fileIndex,
                                      ),
                                      textDirection: TextDirection.ltr,
                                      style: TextStyle(
                                        color: coordinateColor,
                                        fontSize: 10,
                                        fontWeight: FontWeight.w700,
                                      ),
                                    ),
                                  ),
                              ],
                            ),
                          ),
                        );
                      },
                    );
                  },
                ),
                if (showBestArrow && bestArrowMove.length >= 4)
                  IgnorePointer(
                    child: CustomPaint(
                      key: const Key('best-move-arrow'),
                      painter: _ArrowPainter(
                        bestArrowMove,
                        Theme.of(context).colorScheme.tertiary,
                        blackAtBottom: blackAtBottom,
                      ),
                    ),
                  ),
                if (showThreatArrow && threatArrowMove.length >= 4)
                  IgnorePointer(
                    child: CustomPaint(
                      key: const Key('threat-arrow'),
                      painter: _ArrowPainter(
                        threatArrowMove,
                        Theme.of(context).colorScheme.error,
                        blackAtBottom: blackAtBottom,
                      ),
                    ),
                  ),
              ],
            ),
          );

          final topBoardColor = blackAtBottom ? 'white' : 'black';
          final playerOnTop = playerColor == topBoardColor;
          final topName = playerOnTop ? playerName : opponentName;
          final topRating = playerOnTop ? playerRating : opponentRating;
          final topColor = playerOnTop ? playerColor : opponentColor;
          final topOpening = playerOnTop ? playerOpening : null;
          final bottomName = playerOnTop ? opponentName : playerName;
          final bottomRating = playerOnTop ? opponentRating : playerRating;
          final bottomColor = playerOnTop ? opponentColor : playerColor;
          final bottomOpening = playerOnTop ? null : playerOpening;

          final dockedResultVisible =
              showResultSymbols && resultPresentationDocked;
          final topResultAsset = dockedResultVisible
              ? _resultAssetForColor(topColor, gameResult, gameCheckmate)
              : null;
          final bottomResultAsset = dockedResultVisible
              ? _resultAssetForColor(bottomColor, gameResult, gameCheckmate)
              : null;
          final totalHeight =
              evaluationSpace +
              stripHeight * 2 +
              stripGap * 2 +
              boardSide;
          final boardTop = evaluationSpace + stripHeight + stripGap;

          return Align(
            alignment: Alignment.topCenter,
            child: SizedBox(
              width: boardSide,
              height: totalHeight,
              child: Stack(
                clipBehavior: Clip.none,
                children: [
                  Column(
                    crossAxisAlignment: CrossAxisAlignment.center,
                    children: [
                    if (showEvaluationBar) ...[
                      SizedBox(
                        width: boardSide,
                        height: evaluationHeight,
                        child: _EvaluationBar(
                          line: evaluationLine,
                          fen: fen,
                          terminalResult: terminalResult,
                        ),
                      ),
                      const SizedBox(height: evaluationGap),
                    ],
                    SizedBox(
                      width: boardSide,
                      height: stripHeight,
                      child: _BoardPlayerStrip(
                        name: topName,
                        rating: topRating,
                        color: topColor,
                        opening: topOpening,
                        resultAsset: topResultAsset,
                        emphasize: playerOnTop,
                      ),
                    ),
                    const SizedBox(height: stripGap),
                    board,
                    const SizedBox(height: stripGap),
                    SizedBox(
                      width: boardSide,
                      height: stripHeight,
                      child: _BoardPlayerStrip(
                        name: bottomName,
                        rating: bottomRating,
                        color: bottomColor,
                        opening: bottomOpening,
                        resultAsset: bottomResultAsset,
                        emphasize: !playerOnTop,
                      ),
                    ),
                    ],
                  ),
                  if (showResultSymbols &&
                    terminalResult != null &&
                    !resultPresentationDocked)
                  Positioned.fill(
                    child: _BoardResultAnimation(
                      result: terminalResult!,
                      checkmate: terminalCheckmate,
                      topColor: topColor,
                      bottomColor: bottomColor,
                      topName: topName,
                      topRating: topRating,
                      bottomName: bottomName,
                      bottomRating: bottomRating,
                      boardSide: boardSide,
                      boardTop: boardTop,
                      stripHeight: stripHeight,
                      bottomStripTop:
                          boardTop + boardSide + stripGap,
                      shouldAnimate: !resultPresentationStarted,
                      onStarted: onResultPresentationStarted,
                      onCompleted: onResultPresentationDocked,
                    ),
                  ),
                ],
              ),
            ),
          );
        },
      ),
    );
  }
}

class _BoardResultAnimation extends StatefulWidget {
  const _BoardResultAnimation({
    required this.result,
    required this.checkmate,
    required this.topColor,
    required this.bottomColor,
    required this.topName,
    required this.topRating,
    required this.bottomName,
    required this.bottomRating,
    required this.boardSide,
    required this.boardTop,
    required this.stripHeight,
    required this.bottomStripTop,
    required this.shouldAnimate,
    required this.onStarted,
    required this.onCompleted,
  });

  final String result;
  final bool checkmate;
  final String topColor;
  final String bottomColor;
  final String topName;
  final int? topRating;
  final String bottomName;
  final int? bottomRating;
  final double boardSide;
  final double boardTop;
  final double stripHeight;
  final double bottomStripTop;
  final bool shouldAnimate;
  final VoidCallback onStarted;
  final VoidCallback onCompleted;

  @override
  State<_BoardResultAnimation> createState() => _BoardResultAnimationState();
}

class _BoardResultAnimationState extends State<_BoardResultAnimation>
    with SingleTickerProviderStateMixin {
  late final AnimationController _controller;
  late final bool _animate;
  bool _completed = false;

  @override
  void initState() {
    super.initState();
    _animate = widget.shouldAnimate;
    _controller = AnimationController(
      vsync: this,
      duration: const Duration(milliseconds: 1450),
    )..addStatusListener((status) {
        if (status == AnimationStatus.completed) _finish();
      });
    WidgetsBinding.instance.addPostFrameCallback((_) {
      if (!mounted) return;
      if (!_animate) {
        _finish();
        return;
      }
      widget.onStarted();
      _controller.forward();
    });
  }

  void _finish() {
    if (_completed) return;
    _completed = true;
    widget.onCompleted();
  }

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  double _labelTargetX(BuildContext context, String name, int? rating) {
    final label = rating == null ? name : '$name ($rating)';
    final style = Theme.of(context).textTheme.bodyMedium ?? const TextStyle();
    final painter = TextPainter(
      text: TextSpan(text: label, style: style),
      textDirection: TextDirection.ltr,
      maxLines: 1,
    )..layout(maxWidth: widget.boardSide * 0.62);
    // Strip padding (10) + color dot (11) + gap (7) + text + small gap.
    return math.min(
      widget.boardSide - 18,
      10 + 11 + 7 + painter.width + 6 + 14,
    );
  }

  Widget _animatedBadge({
    required BuildContext context,
    required String asset,
    required bool top,
    required String name,
    required int? rating,
    required double progress,
  }) {
    const split = 0.52;
    final start = Offset(
      widget.boardSide / 2,
      widget.boardTop + widget.boardSide / 2,
    );
    final peak = Offset(
      widget.boardSide / 2,
      widget.boardTop + widget.boardSide * (top ? 0.25 : 0.75),
    );
    final end = Offset(
      _labelTargetX(context, name, rating),
      (top ? 0.0 : widget.bottomStripTop) + widget.stripHeight / 2,
    );

    late final Offset center;
    late final double size;
    if (progress <= split) {
      final local = Curves.easeOutCubic.transform(progress / split);
      center = Offset.lerp(start, peak, local)!;
      final sizeProgress = Curves.easeOutBack.transform(progress / split);
      size = 18 + (widget.boardSide * 0.50 - 18) * sizeProgress;
    } else {
      final local = Curves.easeInOutCubic.transform(
        (progress - split) / (1 - split),
      );
      center = Offset.lerp(peak, end, local)!;
      size = widget.boardSide * 0.50 +
          (28 - widget.boardSide * 0.50) * local;
    }
    final opacity = (progress / 0.12).clamp(0.0, 1.0);

    return Positioned(
      left: center.dx - size / 2,
      top: center.dy - size / 2,
      width: size,
      height: size,
      child: Opacity(
        opacity: opacity,
        child: Image.asset(
          asset,
          key: Key('board-result-animation-${top ? 'top' : 'bottom'}'),
          fit: BoxFit.contain,
          errorBuilder: (_, _, _) => const SizedBox.shrink(),
        ),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    final topAsset = _resultAssetForColor(
      widget.topColor,
      widget.result,
      widget.checkmate,
    );
    final bottomAsset = _resultAssetForColor(
      widget.bottomColor,
      widget.result,
      widget.checkmate,
    );
    if (topAsset == null && bottomAsset == null) {
      WidgetsBinding.instance.addPostFrameCallback((_) {
        if (mounted) _finish();
      });
      return const SizedBox.shrink();
    }

    return IgnorePointer(
      child: AnimatedBuilder(
        animation: _controller,
        builder: (context, _) {
          final progress = _animate ? _controller.value : 1.0;
          return Stack(
            clipBehavior: Clip.none,
            children: [
              if (topAsset != null)
                _animatedBadge(
                  context: context,
                  asset: topAsset,
                  top: true,
                  name: widget.topName,
                  rating: widget.topRating,
                  progress: progress,
                ),
              if (bottomAsset != null)
                _animatedBadge(
                  context: context,
                  asset: bottomAsset,
                  top: false,
                  name: widget.bottomName,
                  rating: widget.bottomRating,
                  progress: progress,
                ),
            ],
          );
        },
      ),
    );
  }
}

class _BoardPlayerStrip extends StatelessWidget {
  const _BoardPlayerStrip({
    required this.name,
    required this.rating,
    required this.color,
    required this.opening,
    this.resultAsset,
    this.emphasize = false,
  });

  final String name;
  final int? rating;
  final String color;
  final String? opening;
  final String? resultAsset;
  final bool emphasize;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;
    final label = rating == null ? name : '$name ($rating)';
    return DecoratedBox(
      decoration: BoxDecoration(
        color: emphasize
            ? scheme.surfaceContainerHigh
            : scheme.surfaceContainerLow,
        borderRadius: BorderRadius.circular(8),
      ),
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 5),
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Row(
              children: [
                Container(
                  width: 11,
                  height: 11,
                  decoration: BoxDecoration(
                    shape: BoxShape.circle,
                    color: color == 'white' ? Colors.white : Colors.black,
                    border: Border.all(color: scheme.outline),
                  ),
                ),
                const SizedBox(width: 7),
                Flexible(
                  fit: FlexFit.loose,
                  child: Text(
                    label,
                    maxLines: 1,
                    overflow: TextOverflow.ellipsis,
                    style: theme.textTheme.bodyMedium?.copyWith(
                      fontWeight: emphasize ? FontWeight.w700 : FontWeight.w600,
                    ),
                  ),
                ),
                if (resultAsset != null) ...[
                  const SizedBox(width: 6),
                  SizedBox.square(
                    dimension: 28,
                    child: Image.asset(
                      resultAsset!,
                      key: Key('player-result-$color'),
                      fit: BoxFit.contain,
                      errorBuilder: (_, _, _) => const SizedBox.shrink(),
                    ),
                  ),
                ],
              ],
            ),
            if (opening != null) ...[
              const SizedBox(height: 3),
              Row(
                children: [
                  Icon(
                    Icons.account_tree_outlined,
                    size: 15,
                    color: scheme.primary,
                  ),
                  const SizedBox(width: 5),
                  Expanded(
                    child: _OpeningMarquee(
                      text: opening!,
                      style: theme.textTheme.bodySmall?.copyWith(
                        color: scheme.onSurfaceVariant,
                        fontWeight: FontWeight.w600,
                      ),
                    ),
                  ),
                ],
              ),
            ],
          ],
        ),
      ),
    );
  }
}

class _OpeningMarquee extends StatefulWidget {
  const _OpeningMarquee({required this.text, required this.style});

  final String text;
  final TextStyle? style;

  @override
  State<_OpeningMarquee> createState() => _OpeningMarqueeState();
}

class _OpeningMarqueeState extends State<_OpeningMarquee>
    with SingleTickerProviderStateMixin {
  late final AnimationController _controller;
  double _configuredOverflow = -1;

  @override
  void initState() {
    super.initState();
    _controller = AnimationController(vsync: this);
  }

  @override
  void didUpdateWidget(covariant _OpeningMarquee oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.text != widget.text || oldWidget.style != widget.style) {
      _configuredOverflow = -1;
    }
  }

  void _configureAnimation(double overflow) {
    if ((_configuredOverflow - overflow).abs() < 0.5) return;
    _configuredOverflow = overflow;
    WidgetsBinding.instance.addPostFrameCallback((_) {
      if (!mounted) return;
      _controller
        ..stop()
        ..value = 0;
      if (overflow <= 0) return;
      final milliseconds = math.max(8000, (overflow / 14 * 1000).round());
      _controller
        ..duration = Duration(milliseconds: milliseconds)
        ..repeat();
    });
  }

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return LayoutBuilder(
      builder: (context, constraints) {
        final painter = TextPainter(
          text: TextSpan(text: widget.text, style: widget.style),
          maxLines: 1,
          textDirection: TextDirection.ltr,
          textScaler: MediaQuery.textScalerOf(context),
        )..layout();
        final overflow = math.max(0.0, painter.width - constraints.maxWidth);
        _configureAnimation(overflow);
        if (overflow <= 0) {
          return Text(
            widget.text,
            maxLines: 1,
            textDirection: TextDirection.ltr,
            style: widget.style,
          );
        }
        return ClipRect(
          child: AnimatedBuilder(
            animation: _controller,
            child: Text(
              widget.text,
              maxLines: 1,
              softWrap: false,
              textDirection: TextDirection.ltr,
              style: widget.style,
            ),
            builder: (context, child) => Transform.translate(
              offset: Offset(-overflow * _controller.value, 0),
              child: child,
            ),
          ),
        );
      },
    );
  }
}

class _ArrowPainter extends CustomPainter {
  const _ArrowPainter(
    this.move,
    this.color, {
    required this.blackAtBottom,
  });

  final String move;
  final Color color;
  final bool blackAtBottom;

  Offset? _squareCenter(String squareName, double square) {
    if (squareName.length != 2) return null;
    final file = squareName.codeUnitAt(0) - 'a'.codeUnitAt(0);
    final rank = int.tryParse(squareName[1]);
    if (file < 0 || file > 7 || rank == null || rank < 1 || rank > 8) {
      return null;
    }
    if (blackAtBottom) {
      return Offset((7 - file + 0.5) * square, (rank - 1 + 0.5) * square);
    }
    return Offset((file + 0.5) * square, (8 - rank + 0.5) * square);
  }

  @override
  void paint(Canvas canvas, Size size) {
    final square = size.width / 8;
    final start = _squareCenter(move.substring(0, 2), square);
    final end = _squareCenter(move.substring(2, 4), square);
    if (start == null || end == null) return;
    final vector = end - start;
    final length = vector.distance;
    if (length == 0) return;
    final unit = vector / length;
    final base = end - unit * square * 0.34;
    final perpendicular = Offset(-unit.dy, unit.dx);
    final paint = Paint()
      ..color = color
      ..strokeWidth = square * 0.17
      ..strokeCap = StrokeCap.round;
    canvas.drawLine(start, base, paint);
    final path = Path()
      ..moveTo(end.dx, end.dy)
      ..lineTo(
        base.dx + perpendicular.dx * square * 0.24,
        base.dy + perpendicular.dy * square * 0.24,
      )
      ..lineTo(
        base.dx - perpendicular.dx * square * 0.24,
        base.dy - perpendicular.dy * square * 0.24,
      )
      ..close();
    canvas.drawPath(path, paint..style = PaintingStyle.fill);
  }

  @override
  bool shouldRepaint(covariant _ArrowPainter oldDelegate) =>
      oldDelegate.move != move ||
      oldDelegate.color != color ||
      oldDelegate.blackAtBottom != blackAtBottom;
}

class _EvaluationBar extends StatelessWidget {
  const _EvaluationBar({
    required this.line,
    required this.fen,
    required this.terminalResult,
  });

  final EngineLine? line;
  final String fen;
  final String? terminalResult;

  ({double whiteShare, String label}) _value() {
    final result = terminalResult;
    if (result == '1-0') return (whiteShare: 1.0, label: '1-0');
    if (result == '0-1') return (whiteShare: 0.0, label: '0-1');
    if (result != null && _isDrawResult(result)) {
      return (whiteShare: 0.5, label: '½-½');
    }

    final fields = fen.split(' ');
    final blackToMove = fields.length > 1 && fields[1] == 'b';
    final cp = line?.evaluationCp;
    final mate = line?.mateIn;

    if (mate != null && mate != 0) {
      final whiteMate = blackToMove ? -mate : mate;
      return (
        whiteShare: whiteMate > 0 ? 1.0 : 0.0,
        label: whiteMate > 0 ? 'M$whiteMate' : '-M${whiteMate.abs()}',
      );
    }

    if (cp == null) {
      return (whiteShare: 0.5, label: '0.0');
    }

    final whiteCp = blackToMove ? -cp : cp;
    final whiteShare = (0.5 + whiteCp.clamp(-1000, 1000) / 2000)
        .clamp(0.0, 1.0)
        .toDouble();
    final pawns = whiteCp / 100.0;
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
        key: const Key('analysis-evaluation-bar'),
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

class _AnalysisDetails extends StatelessWidget {
  const _AnalysisDetails({
    required this.status,
    required this.displayed,
    required this.detail,
    required this.currentFen,
    required this.move,
    required this.selectedLine,
    required this.onSelectLine,
    required this.error,
    required this.variation,
    required this.variationActive,
    required this.variationPgn,
    required this.variationIndex,
    required this.variationLength,
    required this.onVariationFirst,
    required this.onVariationPrevious,
    required this.onVariationNext,
    required this.onVariationLast,
    required this.variationError,
    required this.currentPositionLines,
    required this.onReturnToMainLine,
    required this.settings,
  });

  final AnalysisSnapshot? status;
  final AnalysisSnapshot? displayed;
  final GameDetail? detail;
  final String currentFen;
  final ParsedMove? move;
  final int selectedLine;
  final ValueChanged<int> onSelectLine;
  final Object? error;
  final VariationAnalysisSnapshot? variation;
  final bool variationActive;
  final String variationPgn;
  final int variationIndex;
  final int variationLength;
  final VoidCallback onVariationFirst;
  final VoidCallback onVariationPrevious;
  final VoidCallback onVariationNext;
  final VoidCallback onVariationLast;
  final Object? variationError;
  final List<EngineLine> currentPositionLines;
  final VoidCallback onReturnToMainLine;
  final AppSettings settings;

  String _score(EngineLine line) {
    // Native Stockfish scores are relative to the side to move. The eval bar
    // is displayed from White's perspective, so engine-line numbers must use
    // the same convention or their sign flips whenever Black is to move.
    final fields = currentFen.split(' ');
    final blackToMove = fields.length > 1 && fields[1] == 'b';
    if (line.mateIn != null) {
      final whiteMate = blackToMove ? -line.mateIn! : line.mateIn!;
      if (whiteMate == 0) return 'M0';
      return whiteMate > 0 ? 'M$whiteMate' : '-M${whiteMate.abs()}';
    }
    if (line.evaluationCp != null) {
      final whiteCp = blackToMove ? -line.evaluationCp! : line.evaluationCp!;
      final value = whiteCp / 100;
      return '${value >= 0 ? '+' : ''}${value.toStringAsFixed(2)}';
    }
    return '—';
  }

  String _wdlScore(WdlScore wdl) {
    final fields = currentFen.split(' ');
    final blackToMove = fields.length > 1 && fields[1] == 'b';
    final whiteWins = blackToMove ? wdl.losses : wdl.wins;
    final whiteLosses = blackToMove ? wdl.wins : wdl.losses;
    return '$whiteWins/${wdl.draws}/$whiteLosses';
  }

  String _variationScore(VariationAnalysisSnapshot value) {
    if (value.moverMateIn != null) return 'M${value.moverMateIn}';
    if (value.moverEvaluationCp != null) {
      final score = value.moverEvaluationCp! / 100;
      return '${score >= 0 ? '+' : ''}${score.toStringAsFixed(2)}';
    }
    return '—';
  }

  String _bestContinuationLabel(BuildContext context) {
    final language = Localizations.localeOf(context).languageCode;
    return switch (language) {
      'de' => 'Beste Fortsetzung',
      'ar' => 'أفضل متابعة',
      _ => 'Best continuation',
    };
  }

  Widget _currentMoveCard(
    BuildContext context,
    AppLocalizations strings, {
    required String? moveNumberLabel,
    required String playedMove,
    required MoveClassification? classification,
    required TheoryMoveInfo? theory,
    required bool showRecommendedMove,
    required String? recommendedMove,
    required EngineLine? continuationLine,
  }) {
    final theme = Theme.of(context);
    return Card(
      margin: EdgeInsets.zero,
      child: Padding(
        padding: const EdgeInsets.fromLTRB(14, 12, 14, 12),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Text(
              strings.currentMove,
              style: theme.textTheme.labelLarge?.copyWith(
                color: theme.colorScheme.onSurfaceVariant,
                fontWeight: FontWeight.w700,
              ),
            ),
            const SizedBox(height: 8),
            if (settings.showClassifications && classification != null)
              Row(
                key: Key('move-classification-${classification.name}'),
                crossAxisAlignment: CrossAxisAlignment.center,
                children: [
                  if (moveNumberLabel != null) ...[
                    Text(
                      moveNumberLabel,
                      textDirection: TextDirection.ltr,
                      style: theme.textTheme.titleMedium?.copyWith(
                        fontWeight: FontWeight.w800,
                      ),
                    ),
                    const SizedBox(width: 8),
                  ],
                  if (_analysisClassificationAsset(classification) case final asset?)
                    Image.asset(
                      asset,
                      key: Key('move-icon-${classification.name}'),
                      width: 28,
                      height: 28,
                      errorBuilder: (_, _, _) => const SizedBox.shrink(),
                    )
                  else
                    Icon(
                      Icons.auto_awesome,
                      size: 22,
                      color: _classificationColor(context, classification),
                    ),
                  const SizedBox(width: 8),
                  Expanded(
                    child: Text(
                      '$playedMove · ${_classificationLabel(strings, classification)}',
                      key: const Key('move-quality-line'),
                      textDirection: TextDirection.ltr,
                      style: theme.textTheme.titleMedium?.copyWith(
                        color: _classificationColor(context, classification),
                        fontWeight: FontWeight.w800,
                      ),
                    ),
                  ),
                ],
              )
            else
              Text(
                [if (moveNumberLabel != null) moveNumberLabel, playedMove]
                    .join(' '),
                textDirection: TextDirection.ltr,
                style: theme.textTheme.titleMedium?.copyWith(
                  fontWeight: FontWeight.w700,
                ),
              ),
            if (showRecommendedMove ||
                (settings.showEngineLines &&
                    continuationLine?.moves.isNotEmpty == true)) ...[
              const SizedBox(height: 5),
              Text.rich(
                TextSpan(
                  children: [
                    if (showRecommendedMove)
                      TextSpan(
                        text: '✓ ${strings.bestMoveText(recommendedMove!)}',
                        style: const TextStyle(
                          color: Color(0xFF2E9B55),
                          fontWeight: FontWeight.w700,
                        ),
                      ),
                    if (showRecommendedMove &&
                        settings.showEngineLines &&
                        continuationLine?.moves.isNotEmpty == true)
                      TextSpan(
                        text: '  ·  ',
                        style: TextStyle(
                          color: theme.colorScheme.onSurfaceVariant,
                        ),
                      ),
                    if (settings.showEngineLines &&
                        continuationLine?.moves.isNotEmpty == true)
                      TextSpan(
                        text:
                            '${_bestContinuationLabel(context)}: ${continuationLine!.moves.take(4).join(' ')}',
                        style: TextStyle(
                          color: theme.colorScheme.onSurfaceVariant,
                          fontWeight: FontWeight.w600,
                        ),
                      ),
                  ],
                ),
                key: const Key('recommended-best-move'),
                maxLines: 1,
                overflow: TextOverflow.ellipsis,
                style: theme.textTheme.bodySmall,
              ),
            ],
            if (settings.showTheory && theory != null) ...[
              const SizedBox(height: 5),
              Text(
                '${strings.bookGames}: ${theory.games}',
                style: theme.textTheme.bodySmall?.copyWith(
                  color: theme.colorScheme.onSurfaceVariant,
                ),
              ),
            ],
          ],
        ),
      ),
    );
  }

  String _analysisModeLabel(AppLocalizations strings, bool variationMode) =>
      variationMode ? strings.sidelineLabel : strings.mainLineLabel;

  Widget _variationCard(BuildContext context, AppLocalizations strings) {
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;
    final value = variation;
    final variationClassification = value?.classification;
    return Card(
      key: const Key('variation-analysis'),
      margin: EdgeInsets.zero,
      clipBehavior: Clip.antiAlias,
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Container(
            padding: const EdgeInsets.fromLTRB(12, 9, 12, 8),
            color: scheme.tertiaryContainer.withValues(alpha: 0.45),
            child: Row(
              children: [
                Icon(Icons.alt_route, size: 18, color: scheme.tertiary),
                const SizedBox(width: 7),
                Expanded(
                  child: Text(
                    _analysisModeLabel(strings, true),
                    style: theme.textTheme.titleSmall?.copyWith(
                      fontWeight: FontWeight.w800,
                    ),
                  ),
                ),
                if (variationLength > 0)
                  Text(
                    '${variationIndex + 1} / $variationLength',
                    textDirection: TextDirection.ltr,
                    style: theme.textTheme.labelSmall?.copyWith(
                      color: scheme.onSurfaceVariant,
                      fontWeight: FontWeight.w700,
                    ),
                  ),
              ],
            ),
          ),
          Padding(
            padding: const EdgeInsets.fromLTRB(12, 9, 12, 9),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                Text(
                  variationPgn,
                  key: const Key('variation-pgn'),
                  textDirection: TextDirection.ltr,
                  maxLines: 2,
                  overflow: TextOverflow.ellipsis,
                  style: theme.textTheme.bodyMedium?.copyWith(
                    fontWeight: FontWeight.w700,
                  ),
                ),
                if (settings.showClassifications &&
                    variationClassification != null &&
                    variationClassification != MoveClassification.unknown) ...[
                  const SizedBox(height: 7),
                  Row(
                    key: Key(
                      'variation-classification-${variationClassification.name}',
                    ),
                    children: [
                      if (_analysisClassificationAsset(variationClassification)
                          case final asset?)
                        Image.asset(
                          asset,
                          width: 24,
                          height: 24,
                          errorBuilder: (_, _, _) => const SizedBox.shrink(),
                        )
                      else
                        Icon(
                          Icons.auto_awesome,
                          size: 20,
                          color: _classificationColor(
                            context,
                            variationClassification,
                          ),
                        ),
                      const SizedBox(width: 7),
                      Expanded(
                        child: Text(
                          '${value!.playedSan} · ${_classificationLabel(strings, variationClassification)}',
                          key: const Key('variation-move-quality-line'),
                          textDirection: TextDirection.ltr,
                          style: theme.textTheme.bodyMedium?.copyWith(
                            color: _classificationColor(
                              context,
                              variationClassification,
                            ),
                            fontWeight: FontWeight.w800,
                          ),
                        ),
                      ),
                    ],
                  ),
                ],
                if (value != null &&
                    value.lines.isNotEmpty &&
                    value.status != 'error') ...[
                  const SizedBox(height: 5),
                  Align(
                    alignment: AlignmentDirectional.centerEnd,
                    child: Text(
                      _variationScore(value),
                      key: const Key('variation-live-score'),
                      textDirection: TextDirection.ltr,
                      style: theme.textTheme.bodyMedium?.copyWith(
                        fontWeight: FontWeight.w800,
                      ),
                    ),
                  ),
                ],
                if (value?.isRunning == true) ...[
                  const SizedBox(height: 6),
                  const LinearProgressIndicator(
                    key: Key('variation-progress'),
                    minHeight: 3,
                  ),
                ],
                const SizedBox(height: 4),
                Row(
                  children: [
                    IconButton(
                      key: const Key('variation-first'),
                      onPressed: variationIndex > -1 ? onVariationFirst : null,
                      tooltip: strings.variationStart,
                      visualDensity: VisualDensity.compact,
                      icon: const Icon(Icons.first_page, size: 20),
                    ),
                    IconButton(
                      key: const Key('variation-previous'),
                      onPressed:
                          variationIndex > -1 ? onVariationPrevious : null,
                      tooltip: strings.previous,
                      visualDensity: VisualDensity.compact,
                      icon: const Icon(Icons.chevron_left, size: 20),
                    ),
                    const Spacer(),
                    IconButton(
                      key: const Key('variation-next'),
                      onPressed: variationIndex + 1 < variationLength
                          ? onVariationNext
                          : null,
                      tooltip: strings.next,
                      visualDensity: VisualDensity.compact,
                      icon: const Icon(Icons.chevron_right, size: 20),
                    ),
                    IconButton(
                      key: const Key('variation-last'),
                      onPressed: variationIndex + 1 < variationLength
                          ? onVariationLast
                          : null,
                      tooltip: strings.last,
                      visualDensity: VisualDensity.compact,
                      icon: const Icon(Icons.last_page, size: 20),
                    ),
                  ],
                ),
                SizedBox(
                  height: 32,
                  child: OutlinedButton.icon(
                    key: const Key('return-main-line'),
                    onPressed: onReturnToMainLine,
                    icon: const Icon(Icons.undo, size: 16),
                    label: Text(strings.returnToMainLine),
                  ),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }

  Widget _engineLineSequence(
    BuildContext context,
    EngineLine line, {
    required bool selected,
  }) {
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;
    if (line.moves.isEmpty) {
      return Text('—', style: theme.textTheme.bodySmall);
    }
    final firstMove = line.moves.first;
    final continuation = line.moves.skip(1).join(' ');
    return Row(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(
          firstMove,
          textDirection: TextDirection.ltr,
          style: theme.textTheme.bodyMedium?.copyWith(
            fontWeight: FontWeight.w800,
          ),
        ),
        if (continuation.isNotEmpty) ...[
          const SizedBox(width: 7),
          Expanded(
            child: Text(
              continuation,
              textDirection: TextDirection.ltr,
              maxLines: selected ? 2 : 1,
              overflow: TextOverflow.ellipsis,
              style: theme.textTheme.bodySmall?.copyWith(
                color: scheme.onSurfaceVariant,
                height: 1.25,
              ),
            ),
          ),
        ],
      ],
    );
  }

  Widget _engineLinesCard(
    BuildContext context,
    AppLocalizations strings,
    List<EngineLine> shownLines, {
    required bool variationMode,
  }) {
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;
    final accent = variationMode ? scheme.tertiary : scheme.primary;
    final accentOn = variationMode ? scheme.onTertiary : scheme.onPrimary;
    return Card(
      margin: EdgeInsets.zero,
      clipBehavior: Clip.antiAlias,
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Padding(
            padding: const EdgeInsets.fromLTRB(11, 9, 11, 7),
            child: Row(
              children: [
                Container(
                  width: 3,
                  height: 22,
                  decoration: BoxDecoration(
                    color: accent,
                    borderRadius: BorderRadius.circular(3),
                  ),
                ),
                const SizedBox(width: 8),
                Icon(
                  variationMode
                      ? Icons.alt_route
                      : Icons.account_tree_outlined,
                  size: 18,
                  color: accent,
                ),
                const SizedBox(width: 7),
                Expanded(
                  child: Text(
                    strings.engineLines,
                    style: theme.textTheme.titleSmall?.copyWith(
                      fontWeight: FontWeight.w800,
                    ),
                  ),
                ),
                Container(
                  padding:
                      const EdgeInsets.symmetric(horizontal: 8, vertical: 3),
                  decoration: BoxDecoration(
                    color: accent.withValues(alpha: 0.10),
                    borderRadius: BorderRadius.circular(20),
                    border: Border.all(
                      color: accent.withValues(alpha: 0.24),
                    ),
                  ),
                  child: Text(
                    _analysisModeLabel(strings, variationMode),
                    style: theme.textTheme.labelSmall?.copyWith(
                      color: accent,
                      fontWeight: FontWeight.w800,
                    ),
                  ),
                ),
              ],
            ),
          ),
          if (shownLines.isEmpty)
            Padding(
              padding: const EdgeInsets.fromLTRB(12, 2, 12, 12),
              child: Text(
                strings.analyzing,
                style: theme.textTheme.bodySmall,
              ),
            )
          else
            for (var index = 0; index < shownLines.length; index++) ...[
              if (index != 0) const Divider(height: 1),
              Material(
                color: index == selectedLine
                    ? accent.withValues(alpha: 0.08)
                    : Colors.transparent,
                child: InkWell(
                  onTap: () => onSelectLine(index),
                  child: IntrinsicHeight(
                    child: Row(
                      crossAxisAlignment: CrossAxisAlignment.stretch,
                      children: [
                        AnimatedContainer(
                          duration: const Duration(milliseconds: 140),
                          width: index == selectedLine ? 3 : 0,
                          color: accent,
                        ),
                        Expanded(
                          child: Padding(
                            padding: const EdgeInsets.fromLTRB(10, 7, 9, 7),
                            child: Row(
                              crossAxisAlignment: CrossAxisAlignment.start,
                              children: [
                                SizedBox(
                                  width: 26,
                                  child: Column(
                                    children: [
                                      Container(
                                        width: 23,
                                        height: 23,
                                        alignment: Alignment.center,
                                        decoration: BoxDecoration(
                                          shape: BoxShape.circle,
                                          color: index == selectedLine
                                              ? accent
                                              : scheme.surfaceContainerHighest,
                                        ),
                                        child: index == 0
                                            ? Icon(
                                                Icons.star_rounded,
                                                size: 14,
                                                color: index == selectedLine
                                                    ? accentOn
                                                    : scheme.onSurfaceVariant,
                                              )
                                            : Text(
                                                '${shownLines[index].rank}',
                                                style: theme
                                                    .textTheme.labelSmall
                                                    ?.copyWith(
                                                  color: index == selectedLine
                                                      ? accentOn
                                                      : scheme
                                                          .onSurfaceVariant,
                                                  fontWeight: FontWeight.w800,
                                                ),
                                              ),
                                      ),
                                    ],
                                  ),
                                ),
                                const SizedBox(width: 7),
                                Expanded(
                                  child: Column(
                                    crossAxisAlignment:
                                        CrossAxisAlignment.start,
                                    children: [
                                      Row(
                                        children: [
                                          Text(
                                            _score(shownLines[index]),
                                            textDirection: TextDirection.ltr,
                                            style: theme.textTheme.bodyMedium
                                                ?.copyWith(
                                              fontWeight: FontWeight.w800,
                                            ),
                                          ),
                                          const SizedBox(width: 7),
                                          Text(
                                            'd${shownLines[index].depth}',
                                            textDirection: TextDirection.ltr,
                                            style: theme.textTheme.labelSmall
                                                ?.copyWith(
                                              color: scheme.onSurfaceVariant,
                                            ),
                                          ),
                                          if (index == 0) ...[
                                            const SizedBox(width: 7),
                                            Icon(
                                              Icons.arrow_forward_rounded,
                                              size: 14,
                                              color: accent,
                                            ),
                                          ],
                                          const Spacer(),
                                          if (index == selectedLine) ...[
                                            if (shownLines[index].wdl
                                                case final wdl?)
                                              Text(
                                                _wdlScore(wdl),
                                                textDirection: TextDirection.ltr,
                                                style: theme.textTheme.labelSmall
                                                    ?.copyWith(
                                                  color:
                                                      scheme.onSurfaceVariant,
                                                ),
                                              ),
                                          ],
                                        ],
                                      ),
                                      const SizedBox(height: 2),
                                      _engineLineSequence(
                                        context,
                                        shownLines[index],
                                        selected: index == selectedLine,
                                      ),
                                    ],
                                  ),
                                ),
                              ],
                            ),
                          ),
                        ),
                      ],
                    ),
                  ),
                ),
              ),
            ],
        ],
      ),
    );
  }

  Widget _statusFooter(BuildContext context, AppLocalizations strings) {
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;
    final snapshot = status;
    final text = snapshot?.isCancelled == true
        ? strings.analysisCancelled
        : snapshot?.status == 'error'
        ? (snapshot?.error ?? strings.coreUnavailable)
        : snapshot?.isComplete == true
        ? strings.analysisComplete
        : strings.analyzing;
    final icon = snapshot?.isCancelled == true
        ? Icons.cancel_outlined
        : snapshot?.status == 'error'
        ? Icons.error_outline
        : snapshot?.isComplete == true
        ? Icons.check_circle_outline
        : Icons.hourglass_top_rounded;
    final color = snapshot?.status == 'error'
        ? scheme.error
        : scheme.onSurfaceVariant;

    return Padding(
      padding: const EdgeInsets.fromLTRB(2, 2, 2, 6),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              Icon(icon, size: 16, color: color),
              const SizedBox(width: 6),
              Expanded(
                child: Text(
                  text,
                  style: theme.textTheme.bodySmall?.copyWith(color: color),
                ),
              ),
              if (snapshot != null)
                Text(
                  '${snapshot.completedPlies}/${snapshot.totalPlies}',
                  textDirection: TextDirection.ltr,
                  style: theme.textTheme.labelSmall?.copyWith(
                    color: scheme.onSurfaceVariant,
                  ),
                ),
            ],
          ),
          if (snapshot?.engineVersion.isNotEmpty == true) ...[
            const SizedBox(height: 3),
            Text(
              snapshot!.engineVersion,
              textDirection: TextDirection.ltr,
              maxLines: 1,
              overflow: TextOverflow.ellipsis,
              style: theme.textTheme.labelSmall?.copyWith(
                color: scheme.onSurfaceVariant,
              ),
            ),
          ],
        ],
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    if (error != null) return Center(child: Text(error.toString()));

    final moveNumberLabel = move == null
        ? null
        : '${move!.moveNumber}${move!.sideToMove == 'black' ? '…' : '.'}';
    final playedMove = move?.san ?? 'FEN';
    final shownLines = variation?.lines ?? currentPositionLines;
    final rawClassification = displayed?.classification;
    final classification = rawClassification == MoveClassification.unknown
        ? null
        : rawClassification;
    final theory = displayed?.theory;
    final recommendedMove = displayed?.recommendedMove;
    final showRecommendedMove =
        settings.showClassifications &&
        classification != null &&
        classification != MoveClassification.theory &&
        classification != MoveClassification.brilliant &&
        classification != MoveClassification.critical &&
        classification != MoveClassification.best &&
        recommendedMove != null &&
        recommendedMove.isNotEmpty &&
        recommendedMove != playedMove;

    return ListView(
      padding: const EdgeInsets.fromLTRB(12, 12, 12, 10),
      children: [
        if (!variationActive)
          _currentMoveCard(
            context,
            strings,
            moveNumberLabel: moveNumberLabel,
            playedMove: playedMove,
            classification: classification,
            theory: theory,
            showRecommendedMove: showRecommendedMove,
            recommendedMove: recommendedMove,
            continuationLine:
                currentPositionLines.isEmpty ? null : currentPositionLines.first,
          ),
        if (variationActive) ...[
          const SizedBox(height: 10),
          _variationCard(context, strings),
        ],
        if (variationError != null) ...[
          const SizedBox(height: 8),
          Text(
            strings.illegalOrFailedMove(variationError.toString()),
            key: const Key('variation-error'),
            style: Theme.of(context).textTheme.bodySmall?.copyWith(
              color: Theme.of(context).colorScheme.error,
            ),
          ),
        ],
        if (settings.showEngineLines) ...[
          const SizedBox(height: 10),
          _engineLinesCard(
            context,
            strings,
            shownLines,
            variationMode: variationActive,
          ),
        ],
        if (!variationActive && detail?.pgn.isNotEmpty == true) ...[
          const SizedBox(height: 8),
          Card(
            margin: EdgeInsets.zero,
            child: ExpansionTile(
              dense: true,
              tilePadding: const EdgeInsets.symmetric(horizontal: 14),
              childrenPadding: const EdgeInsets.fromLTRB(14, 0, 14, 12),
              title: Text(
                strings.pgnLabel,
                style: Theme.of(context).textTheme.titleSmall?.copyWith(
                  fontWeight: FontWeight.w700,
                ),
              ),
              children: [
                Align(
                  alignment: AlignmentDirectional.centerStart,
                  child: SelectableText(
                    detail!.pgn,
                    textDirection: TextDirection.ltr,
                    style: Theme.of(context).textTheme.bodySmall,
                  ),
                ),
              ],
            ),
          ),
        ],
        const SizedBox(height: 10),
        _statusFooter(context, strings),
      ],
    );
  }
}

class _AnalysisControls extends StatelessWidget {
  const _AnalysisControls({
    required this.playing,
    required this.onFirst,
    required this.onPrevious,
    required this.onPlayPause,
    required this.onNext,
    required this.onLast,
    required this.onSettings,
  });

  final bool playing;
  final VoidCallback onFirst;
  final VoidCallback onPrevious;
  final VoidCallback onPlayPause;
  final VoidCallback onNext;
  final VoidCallback onLast;
  final VoidCallback onSettings;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    return Material(
      elevation: 8,
      child: SizedBox(
        height: 64,
        width: double.infinity,
        child: Padding(
          padding: const EdgeInsets.symmetric(horizontal: 8),
          child: Row(
            children: [
              const SizedBox(width: 56),
              Expanded(
                child: Center(
                  child: FittedBox(
                    fit: BoxFit.scaleDown,
                    child: SizedBox(
                      width: 340,
                      child: Row(
                        mainAxisAlignment: MainAxisAlignment.spaceBetween,
                        children: [
                          IconButton(
                            onPressed: onFirst,
                            tooltip: strings.first,
                            icon: const Icon(Icons.first_page, size: 28),
                          ),
                          IconButton(
                            onPressed: onPrevious,
                            tooltip: strings.previous,
                            icon: const Icon(Icons.chevron_left, size: 30),
                          ),
                          IconButton.filled(
                            onPressed: onPlayPause,
                            tooltip: strings.playPause,
                            icon: Icon(
                              playing ? Icons.pause : Icons.play_arrow,
                              size: 28,
                            ),
                          ),
                          IconButton(
                            onPressed: onNext,
                            tooltip: strings.next,
                            icon: const Icon(Icons.chevron_right, size: 30),
                          ),
                          IconButton(
                            onPressed: onLast,
                            tooltip: strings.last,
                            icon: const Icon(Icons.last_page, size: 28),
                          ),
                        ],
                      ),
                    ),
                  ),
                ),
              ),
              SizedBox(
                width: 56,
                child: Center(
                  child: IconButton(
                    key: const Key('analysis-quick-settings'),
                    onPressed: onSettings,
                    tooltip: strings.settings,
                    icon: const Icon(Icons.tune, size: 28),
                  ),
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }
}

class _SummarySheet extends StatelessWidget {
  const _SummarySheet({required this.summary});

  final AnalysisSummary summary;

  @override
  Widget build(BuildContext context) {
    return SafeArea(
      child: SingleChildScrollView(
        padding: const EdgeInsets.fromLTRB(20, 8, 20, 20),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Align(
              alignment: AlignmentDirectional.centerEnd,
              child: IconButton(
                key: const Key('summary-close'),
                onPressed: () => Navigator.pop(context),
                icon: const Icon(Icons.close_rounded),
              ),
            ),
            const SizedBox(height: 4),
            Row(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Expanded(
                  child: _PlayerSummaryBlock(
                    key: const Key('white-summary'),
                    isWhite: true,
                    summary: summary.white,
                  ),
                ),
                const SizedBox(width: 18),
                Expanded(
                  child: _PlayerSummaryBlock(
                    key: const Key('black-summary'),
                    isWhite: false,
                    summary: summary.black,
                  ),
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }
}

class _PlayerSummaryBlock extends StatelessWidget {
  const _PlayerSummaryBlock({
    required this.isWhite,
    required this.summary,
    super.key,
  });

  final bool isWhite;
  final PlayerAnalysisSummary summary;
  @override
  Widget build(BuildContext context) {
    final accuracy = summary.localAccuracy;
    final counts = <(MoveClassification, int)>[
      (MoveClassification.theory, summary.theory),
      (MoveClassification.brilliant, summary.brilliant),
      (MoveClassification.critical, summary.critical),
      (MoveClassification.best, summary.best),
      (MoveClassification.excellent, summary.excellent),
      (MoveClassification.okay, summary.okay),
      (MoveClassification.miss, summary.miss),
      (MoveClassification.mistake, summary.mistake),
      (MoveClassification.blunder, summary.blunder),
    ];
    final visibleCounts = counts.where((entry) => entry.$2 > 0).toList();
    final kingAsset = isWhite
        ? 'assets/analysis_img/piece_white_king.svg'
        : 'assets/analysis_img/piece_black_king.svg';

    return Column(
      crossAxisAlignment: CrossAxisAlignment.center,
      children: [
        SvgPicture.asset(
          kingAsset,
          width: 34,
          height: 34,
          semanticsLabel: isWhite ? 'White' : 'Black',
        ),
        if (accuracy != null) ...[
          const SizedBox(height: 8),
          Text(
            '${accuracy.toStringAsFixed(1)}%',
            key: Key(isWhite ? 'white-summary-accuracy' : 'black-summary-accuracy'),
            style: Theme.of(context).textTheme.titleLarge?.copyWith(
              fontWeight: FontWeight.w700,
            ),
          ),
        ],
        if (visibleCounts.isNotEmpty) ...[
          const SizedBox(height: 14),
          for (final (classification, value) in visibleCounts)
            _SummaryClassificationRow(
              classification: classification,
              value: value,
            ),
        ],
      ],
    );
  }
}

class _SummaryClassificationRow extends StatelessWidget {
  const _SummaryClassificationRow({
    required this.classification,
    required this.value,
  });

  final MoveClassification classification;
  final int value;

  @override
  Widget build(BuildContext context) {
    final asset = _analysisClassificationAsset(classification);
    if (asset == null) return const SizedBox.shrink();

    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 4),
      child: Row(
        mainAxisAlignment: MainAxisAlignment.center,
        children: [
          Image.asset(
            asset,
            width: 28,
            height: 28,
            fit: BoxFit.contain,
            errorBuilder: (_, __, ___) => const SizedBox.shrink(),
          ),
          const SizedBox(width: 8),
          Text(
            '$value',
            style: Theme.of(context).textTheme.titleMedium?.copyWith(
              fontWeight: FontWeight.w700,
            ),
          ),
        ],
      ),
    );
  }
}
