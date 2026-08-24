import 'dart:async';
import 'dart:io';
import 'dart:math' as math;

import 'package:flutter/foundation.dart';

import '../../../ffi/core_gateway.dart';
import '../../../shared/models/models.dart';

enum AppPhase { loading, firstRun, ready, error }

class AppController extends ChangeNotifier {
  AppController(this.gateway);

  static int get maximumEngineWorkerThreads =>
      math.max(1, math.min(32, Platform.numberOfProcessors ~/ 2));

  static int clampEngineWorkerThreads(int threads) =>
      math.max(1, math.min(threads, maximumEngineWorkerThreads));

  final CoreGateway gateway;
  AppPhase phase = AppPhase.loading;
  List<AppProfile> profiles = const [];
  AppProfile? activeProfile;
  AppSettings settings = const AppSettings();
  List<GameSummary> games = const [];
  List<GameSummary> favoriteGames = const [];
  List<FavoriteCollection> favoriteCollections = const [];
  ProviderOverview? providerOverview;
  bool providerSyncing = false;
  String? providerNotice;
  String? selectedMonth;
  Object? error;
  int _providerSyncGeneration = 0;

  Future<void> initialize() async {
    phase = AppPhase.loading;
    error = null;
    notifyListeners();
    try {
      await gateway.initialize();
      profiles = await gateway.profiles();
      activeProfile = await gateway.activeProfile();
      await _ensureActiveProfile();
      settings = await gateway.settings();
      games = await gateway.games();
      favoriteGames = await gateway.favoriteGames();
      favoriteCollections = await gateway.favoriteCollections();
      if (activeProfile!.type != ProfileType.localPgnFen) {
        providerOverview = await gateway.providerOverview(activeProfile!.id);
        selectedMonth = _currentMonth();
      } else {
        providerOverview = null;
        selectedMonth = null;
      }
      phase = AppPhase.ready;
    } catch (caught) {
      error = caught;
      phase = AppPhase.error;
    }
    notifyListeners();
    if (phase == AppPhase.ready &&
        settings.autoSyncOnline &&
        activeProfile?.type != ProfileType.localPgnFen) {
      unawaited(syncProvider());
    }
  }

  Future<void> createProfile(ProfileType type, String input) async {
    final value = input.trim();
    if (value.isEmpty) {
      throw const CoreGatewayException('A profile value is required');
    }
    final profile = await gateway.createProfile(
      type: type,
      displayName: value,
      providerUsername: type == ProfileType.localPgnFen ? '' : value,
    );
    profiles = await gateway.profiles();
    activeProfile = profile;
    settings = await gateway.settings();
    games = await gateway.games();
    favoriteGames = await gateway.favoriteGames();
    favoriteCollections = await gateway.favoriteCollections();
    if (type != ProfileType.localPgnFen) {
      providerOverview = await gateway.providerOverview(profile.id);
      selectedMonth = _currentMonth();
    } else {
      providerOverview = null;
      selectedMonth = null;
    }
    phase = AppPhase.ready;
    notifyListeners();
  }

  Future<void> switchProfile(AppProfile profile) async {
    _providerSyncGeneration++;
    providerSyncing = false;
    await gateway.setActiveProfile(profile.id);
    activeProfile = profile;
    settings = await gateway.settings();
    games = await gateway.games();
    favoriteGames = await gateway.favoriteGames();
    favoriteCollections = await gateway.favoriteCollections();
    providerNotice = null;
    if (profile.type != ProfileType.localPgnFen) {
      providerOverview = await gateway.providerOverview(profile.id);
      selectedMonth = _currentMonth();
    } else {
      providerOverview = null;
      selectedMonth = null;
    }
    notifyListeners();
    if (settings.autoSyncOnline && profile.type != ProfileType.localPgnFen) {
      unawaited(syncProvider());
    }
  }

  Future<void> deleteProfile(AppProfile profile) async {
    _providerSyncGeneration++;
    providerSyncing = false;
    await gateway.deleteProfile(profile.id);
    profiles = await gateway.profiles();
    activeProfile = await gateway.activeProfile();
    providerNotice = null;
    providerOverview = null;
    selectedMonth = null;
    await _ensureActiveProfile();
    settings = await gateway.settings();
    games = await gateway.games();
    favoriteGames = await gateway.favoriteGames();
    favoriteCollections = await gateway.favoriteCollections();
    if (activeProfile!.type != ProfileType.localPgnFen) {
      providerOverview = await gateway.providerOverview(activeProfile!.id);
      selectedMonth = _currentMonth();
    }
    phase = AppPhase.ready;
    notifyListeners();
    if (settings.autoSyncOnline &&
        activeProfile!.type != ProfileType.localPgnFen) {
      unawaited(syncProvider());
    }
  }

  Future<void> mergeLocalProfile(
    AppProfile source,
    AppProfile target,
  ) async {
    if (source.type != ProfileType.localPgnFen ||
        target.type == ProfileType.localPgnFen) {
      throw const CoreGatewayException('Invalid profile merge');
    }

    _providerSyncGeneration++;
    providerSyncing = false;
    await gateway.mergeLocalProfile(source.id, target.id);
    profiles = await gateway.profiles();
    activeProfile = await gateway.activeProfile();
    providerNotice = null;
    providerOverview = null;
    selectedMonth = null;
    await _ensureActiveProfile();
    settings = await gateway.settings();
    games = await gateway.games();
    favoriteGames = await gateway.favoriteGames();
    favoriteCollections = await gateway.favoriteCollections();
    if (activeProfile!.type != ProfileType.localPgnFen) {
      providerOverview = await gateway.providerOverview(activeProfile!.id);
      selectedMonth = _currentMonth();
    }
    phase = AppPhase.ready;
    notifyListeners();
    if (settings.autoSyncOnline &&
        activeProfile!.type != ProfileType.localPgnFen) {
      unawaited(syncProvider());
    }
  }

  Future<void> syncProvider({String? month}) async {
    final profile = activeProfile;
    if (profile == null || profile.type == ProfileType.localPgnFen) return;
    final generation = ++_providerSyncGeneration;
    var year = 0;
    var monthNumber = 0;
    if (month != null) {
      final parts = month.split('-');
      if (parts.length == 2) {
        year = int.tryParse(parts[0]) ?? 0;
        monthNumber = int.tryParse(parts[1]) ?? 0;
      }
      selectedMonth = month;
    }
    providerSyncing = true;
    providerNotice = null;
    notifyListeners();
    try {
      final synced = await gateway.syncProvider(
        profile.id,
        year: year,
        month: monthNumber,
      );
      if (generation != _providerSyncGeneration ||
          activeProfile?.id != profile.id) {
        return;
      }
      providerOverview = synced;
      activeProfile = providerOverview!.profile;
      profiles = await gateway.profiles();
      games = await gateway.games();
      favoriteGames = await gateway.favoriteGames();
    } catch (caught) {
      if (generation != _providerSyncGeneration ||
          activeProfile?.id != profile.id) {
        return;
      }
      providerNotice = caught.toString();
      games = await gateway.games();
      favoriteGames = await gateway.favoriteGames();
    } finally {
      if (generation == _providerSyncGeneration) {
        providerSyncing = false;
        notifyListeners();
      }
    }
  }

  Future<void> toggleFavorite(GameSummary game) async {
    await gateway.setGameFavorite(game.id, !game.favorite);
    games = await gateway.games();
    favoriteGames = await gateway.favoriteGames();
    favoriteCollections = await gateway.favoriteCollections();
    notifyListeners();
  }

  Future<void> createFavoriteCollection(String name) async {
    await gateway.createFavoriteCollection(name.trim());
    favoriteGames = await gateway.favoriteGames();
    favoriteCollections = await gateway.favoriteCollections();
    notifyListeners();
  }

  Future<void> renameFavoriteCollection(
    FavoriteCollection collection,
    String name,
  ) async {
    await gateway.renameFavoriteCollection(collection.id, name.trim());
    favoriteGames = await gateway.favoriteGames();
    favoriteCollections = await gateway.favoriteCollections();
    notifyListeners();
  }

  Future<void> deleteFavoriteCollection(FavoriteCollection collection) async {
    await gateway.deleteFavoriteCollection(collection.id);
    favoriteGames = await gateway.favoriteGames();
    favoriteCollections = await gateway.favoriteCollections();
    games = await gateway.games();
    notifyListeners();
  }

  Future<void> moveFavoriteToCollection(
    GameSummary game,
    FavoriteCollection? collection,
  ) async {
    await gateway.setGameFavoriteCollection(game.id, collection?.id);
    games = await gateway.games();
    favoriteGames = await gateway.favoriteGames();
    favoriteCollections = await gateway.favoriteCollections();
    notifyListeners();
  }

  Future<void> saveToDownloads(GameSummary game) async {
    // Downloads are no longer a separate state. Saving an online game makes it
    // a global favorite and places it in the top-level Downloads collection.
    await gateway.setGameDownloaded(game.id, true);
    games = await gateway.games();
    favoriteGames = await gateway.favoriteGames();
    favoriteCollections = await gateway.favoriteCollections();
    notifyListeners();
  }

  Future<void> deleteLocalGame(GameSummary game) async {
    await gateway.deleteLocalGame(game.id);
    games = await gateway.games();
    favoriteGames = await gateway.favoriteGames();
    favoriteCollections = await gateway.favoriteCollections();
    notifyListeners();
  }

  Future<void> clearCachedMonth(String month) async {
    final profile = activeProfile;
    if (profile == null || profile.type == ProfileType.localPgnFen) return;
    await gateway.clearCachedMonth(profile.id, month);
    games = await gateway.games();
    favoriteGames = await gateway.favoriteGames();
    favoriteCollections = await gateway.favoriteCollections();
    providerOverview = await gateway.providerOverview(profile.id);
    notifyListeners();
  }

  Future<void> _ensureActiveProfile() async {
    if (profiles.isEmpty) {
      final profile = await gateway.createProfile(
        type: ProfileType.localPgnFen,
        displayName: 'Kchess',
        providerUsername: '',
      );
      profiles = await gateway.profiles();
      activeProfile = profile;
      return;
    }
    if (activeProfile == null) {
      final profile = profiles.first;
      await gateway.setActiveProfile(profile.id);
      activeProfile = profile;
    }
  }

  String _currentMonth() {
    final now = DateTime.now();
    return '${now.year.toString().padLeft(4, '0')}-${now.month.toString().padLeft(2, '0')}';
  }

  Future<GameSummary> importPgn(String pgn) async {
    final game = await gateway.importPgn(pgn);
    // PGN/FEN imports always belong to the local library. The native layer may
    // create that profile on demand while preserving the currently active one.
    profiles = await gateway.profiles();
    activeProfile = await gateway.activeProfile();
    games = await gateway.games();
    favoriteGames = await gateway.favoriteGames();
    favoriteCollections = await gateway.favoriteCollections();
    notifyListeners();
    return game;
  }

  Future<GameSummary> importFen({
    required String fen,
    required String name,
  }) async {
    final game = await gateway.importFen(fen: fen, name: name);
    profiles = await gateway.profiles();
    activeProfile = await gateway.activeProfile();
    games = await gateway.games();
    favoriteGames = await gateway.favoriteGames();
    favoriteCollections = await gateway.favoriteCollections();
    notifyListeners();
    return game;
  }

  Future<void> refreshAfterAnalysis() async {
    // Analysis is persisted asynchronously in native storage. Refresh summaries
    // after the analysis workflow closes so local imports immediately show their
    // analyzed state/accuracy without a profile switch or app restart.
    games = await gateway.games();
    favoriteGames = await gateway.favoriteGames();
    favoriteCollections = await gateway.favoriteCollections();
    notifyListeners();
  }

  Future<void> setShowBoardArrows(bool enabled) => setShowBestMoveArrow(enabled);

  Future<void> setShowBestMoveArrow(bool enabled) => _setBooleanSetting(
    'showBestMoveArrow',
    settings.copyWith(showBestMoveArrow: enabled),
  );

  Future<void> setShowThreatArrow(bool enabled) => _setBooleanSetting(
    'showThreatArrow',
    settings.copyWith(showThreatArrow: enabled),
  );

  Future<void> setShowEvaluationBar(bool enabled) => _setBooleanSetting(
    'showEvaluationBar',
    settings.copyWith(showEvaluationBar: enabled),
  );

  Future<void> setShowEngineLines(bool enabled) => _setBooleanSetting(
    'showEngineLines',
    settings.copyWith(showEngineLines: enabled),
  );

  Future<void> setShowClassifications(bool enabled) => _setBooleanSetting(
    'showClassifications',
    settings.copyWith(showClassifications: enabled),
  );

  Future<void> setShowAccuracy(bool enabled) => _setBooleanSetting(
    'showAccuracy',
    settings.copyWith(showAccuracy: enabled),
  );

  Future<void> setShowTheory(bool enabled) => _setBooleanSetting(
    'showTheory',
    settings.copyWith(showTheory: enabled),
  );

  Future<void> setShowBoardCoordinates(bool enabled) => _setBooleanSetting(
    'showBoardCoordinates',
    settings.copyWith(showBoardCoordinates: enabled),
  );

  Future<void> setHighlightLastMove(bool enabled) => _setBooleanSetting(
    'highlightLastMove',
    settings.copyWith(highlightLastMove: enabled),
  );

  Future<void> setHighlightSelectedSquare(bool enabled) => _setBooleanSetting(
    'highlightSelectedSquare',
    settings.copyWith(highlightSelectedSquare: enabled),
  );

  Future<void> setAutoSyncOnline(bool enabled) => _setBooleanSetting(
    'autoSyncOnline',
    settings.copyWith(autoSyncOnline: enabled),
  );

  Future<void> setConfirmBeforeDelete(bool enabled) => _setBooleanSetting(
    'confirmBeforeDelete',
    settings.copyWith(confirmBeforeDelete: enabled),
  );

  Future<void> setUseGlobalAnalysisCache(bool enabled) => _setBooleanSetting(
    'useGlobalAnalysisCache',
    settings.copyWith(useGlobalAnalysisCache: enabled),
  );

  Future<void> setDiagnosticLogging(bool enabled) => _setBooleanSetting(
    'diagnosticLogging',
    settings.copyWith(diagnosticLogging: enabled),
  );

  Future<void> clearEngineCache() => gateway.clearEngineCache();

  Future<void> reloadSettings() async {
    settings = await gateway.settings();
    notifyListeners();
  }

  Future<void> _setBooleanSetting(String key, AppSettings updated) async {
    final enabled = switch (key) {
      'showBestMoveArrow' => updated.showBestMoveArrow,
      'showThreatArrow' => updated.showThreatArrow,
      'showEvaluationBar' => updated.showEvaluationBar,
      'showEngineLines' => updated.showEngineLines,
      'showClassifications' => updated.showClassifications,
      'showAccuracy' => updated.showAccuracy,
      'showTheory' => updated.showTheory,
      'showBoardCoordinates' => updated.showBoardCoordinates,
      'highlightLastMove' => updated.highlightLastMove,
      'highlightSelectedSquare' => updated.highlightSelectedSquare,
      'autoSyncOnline' => updated.autoSyncOnline,
      'confirmBeforeDelete' => updated.confirmBeforeDelete,
      'useGlobalAnalysisCache' => updated.useGlobalAnalysisCache,
      'diagnosticLogging' => updated.diagnosticLogging,
      _ => throw ArgumentError.value(key, 'key'),
    };
    await gateway.setBooleanSetting(key, enabled);
    settings = updated;
    notifyListeners();
  }

  Future<void> setMinAnalysisDepth(int depth) async {
    final minimum = depth < 1 ? 1 : (depth > settings.depth ? settings.depth : depth);
    await gateway.setAnalysisDepthRange(minimumDepth: minimum, maximumDepth: settings.depth);
    settings = settings.copyWith(minAnalysisDepth: minimum);
    notifyListeners();
  }

  Future<void> setDepth(int depth) async {
    final maximum = depth < 1 ? 1 : (depth > 64 ? 64 : depth);
    final minimum = settings.minAnalysisDepth > maximum ? maximum : settings.minAnalysisDepth;
    await gateway.setAnalysisDepthRange(minimumDepth: minimum, maximumDepth: maximum);
    settings = settings.copyWith(minAnalysisDepth: minimum, depth: maximum);
    notifyListeners();
  }

  Future<void> setMultiPv(int multiPv) =>
      _setEngineSettings(settings.copyWith(multiPv: multiPv));

  Future<void> setTimeLimitSeconds(int seconds) =>
      _setEngineSettings(settings.copyWith(timeLimitSeconds: seconds));

  Future<void> setThreads(int threads) {
    final bounded = clampEngineWorkerThreads(threads);
    return _setEngineResources(settings.copyWith(threads: bounded));
  }

  Future<void> setHashMb(int hashMb) =>
      _setEngineResources(settings.copyWith(hashMb: hashMb));

  Future<void> _setEngineSettings(AppSettings updated) async {
    await gateway.setEngineSettings(
      depth: updated.depth,
      multiPv: updated.multiPv,
      timeLimitSeconds: updated.timeLimitSeconds,
    );
    settings = updated;
    notifyListeners();
  }

  Future<void> _setEngineResources(AppSettings updated) async {
    await gateway.setEngineResources(
      threads: updated.threads,
      hashMb: updated.hashMb,
    );
    settings = updated;
    notifyListeners();
  }

  Future<void> setThemeMode(AppThemeMode mode) async {
    await gateway.setThemeMode(mode);
    settings = settings.copyWith(themeMode: mode);
    notifyListeners();
  }

  Future<void> setLocale(String locale) async {
    await gateway.setLocale(locale);
    settings = settings.copyWith(locale: locale);
    notifyListeners();
  }

  @override
  void dispose() {
    _providerSyncGeneration++;
    gateway.dispose();
    super.dispose();
  }
}

class AnalysisController extends ChangeNotifier {
  AnalysisController(
    this.gateway,
    this.game, {
    this.pollInterval = const Duration(milliseconds: 300),
  });

  final CoreGateway gateway;
  final GameSummary game;
  final Duration pollInterval;
  GameDetail? detail;
  AnalysisSnapshot? snapshot;
  AnalysisSnapshot? displayedSnapshot;
  int? selectedPly;
  Object? error;
  Timer? _timer;
  Timer? _refinementTimer;
  int _refinementGeneration = 0;
  bool _variationPaused = false;
  bool _disposed = false;

  bool _isUnratedTerminalPly(int ply) {
    final loaded = detail;
    if (loaded == null || loaded.moves.length <= 1) return false;
    final result = loaded.summary.result.trim();
    return (result == '1-0' ||
            result == '0-1' ||
            result == '1/2-1/2' ||
            result == '½-½')
        && ply == loaded.moves.length - 1;
  }

  Future<void> open() async {
    try {
      detail = await gateway.game(game.id);
      snapshot = await gateway.startAnalysis(game.id);
      displayedSnapshot = snapshot;
      if (!_disposed) notifyListeners();
      if (snapshot?.isRunning == true) {
        _schedulePoll();
      } else if (detail?.moves.isNotEmpty == true) {
        final last = detail!.moves.length - 1;
        selectedPly = last;
        await refinePly(last);
      }
    } catch (caught) {
      error = caught;
      if (!_disposed) notifyListeners();
    }
  }

  Future<void> openPrepared(AnalysisSnapshot prepared) async {
    try {
      detail = await gateway.game(game.id);
      snapshot = prepared;
      displayedSnapshot = prepared;
      if (!_disposed) notifyListeners();

      // A prepared snapshot normally means the minimum pass is already
      // complete.  Still start/retarget the maximum live refinement
      // immediately.  This is essential after engine settings changed: the
      // minimum configuration may be unchanged while the maximum target has a
      // new config hash and must not remain idle until the user clicks a move.
      if (detail?.moves.isNotEmpty == true) {
        final last = detail!.moves.length - 1;
        selectedPly = last;
        await refinePly(last);
      }
    } catch (caught) {
      error = caught;
      if (!_disposed) notifyListeners();
    }
  }

  void _schedulePoll() {
    if (_variationPaused) return;
    _timer?.cancel();
    _timer = Timer(pollInterval, _poll);
  }

  Future<void> _poll() async {
    if (_variationPaused) return;
    try {
      snapshot = await gateway.analysisStatus(game.id);
      if (selectedPly == null) displayedSnapshot = snapshot;
      if (!_disposed) notifyListeners();
      if (snapshot?.isRunning == true) {
        _schedulePoll();
      } else if (selectedPly == null && (snapshot?.completedPlies ?? 0) > 0) {
        final lastIndex = (detail?.moves.length ?? 1) - 1;
        final rawLatest = snapshot!.completedPlies - 1;
        final latest = rawLatest < 0 ? 0 : (rawLatest > lastIndex ? lastIndex : rawLatest);
        selectedPly = latest;
        await refinePly(latest);
      }
    } catch (caught) {
      error = caught;
      if (!_disposed) notifyListeners();
    }
  }

  Future<void> refinePly(int ply) async {
    if (_variationPaused) return;
    final generation = ++_refinementGeneration;
    _timer?.cancel();
    if (_isUnratedTerminalPly(ply)) {
      _refinementTimer?.cancel();
      displayedSnapshot = null;
      error = null;
      if (!_disposed) notifyListeners();
      return;
    }
    try {
      final started = await gateway.startMoveRefinement(game.id, ply);
      if (_disposed || generation != _refinementGeneration || selectedPly != ply) {
        return;
      }
      if (started.lines.isNotEmpty) displayedSnapshot = started;
      notifyListeners();
      _scheduleRefinementPoll(ply, generation);
    } catch (caught) {
      if (_disposed || generation != _refinementGeneration || selectedPly != ply) {
        return;
      }
      error = caught;
      notifyListeners();
    }
  }

  void _scheduleRefinementPoll(int ply, int generation) {
    if (_variationPaused ||
        _disposed ||
        generation != _refinementGeneration ||
        selectedPly != ply) {
      return;
    }
    _refinementTimer?.cancel();
    _refinementTimer = Timer(pollInterval, () async {
      if (_disposed || generation != _refinementGeneration || selectedPly != ply) {
        return;
      }
      try {
        final next = await gateway.moveAnalysisStatus(game.id, ply);
        if (_disposed || generation != _refinementGeneration || selectedPly != ply) {
          return;
        }
        displayedSnapshot = next;
        notifyListeners();
        if (next.isRunning) _scheduleRefinementPoll(ply, generation);
      } catch (_) {
        if (!_disposed && generation == _refinementGeneration && selectedPly == ply) {
          _scheduleRefinementPoll(ply, generation);
        }
      }
    });
  }

  Future<void> selectPly(int ply) async {
    final maximum = detail?.moves.isEmpty == true
        ? 0
        : (detail?.moves.length ?? 1) - 1;
    final selected = ply < 0 ? 0 : (ply > maximum ? maximum : ply);
    selectedPly = selected;
    // Do not issue a separate status request before retargeting. The native
    // worker returns the best persisted checkpoint with the retarget call,
    // which avoids stale async responses when the user skips several moves.
    displayedSnapshot = null;
    if (!_disposed) notifyListeners();
    if (!_disposed) await refinePly(selected);
  }


  void pauseForVariation() {
    _variationPaused = true;
    _refinementGeneration++;
    _timer?.cancel();
    _refinementTimer?.cancel();
  }

  Future<void> resumeAfterVariation(int ply) async {
    if (_disposed) return;
    _variationPaused = false;
    selectedPly = ply;
    await refinePly(ply);
  }

  void followLatest() {
    _refinementGeneration++;
    _refinementTimer?.cancel();
    selectedPly = null;
    displayedSnapshot = snapshot;
    if (!_disposed) notifyListeners();
  }

  Future<void> cancel() async {
    try {
      await gateway.cancelAnalysis(game.id);
      snapshot = await gateway.analysisStatus(game.id);
      if (selectedPly == null) displayedSnapshot = snapshot;
      _timer?.cancel();
    } catch (caught) {
      error = caught;
    }
    if (!_disposed) notifyListeners();
  }

  @override
  void dispose() {
    _disposed = true;
    _refinementGeneration++;
    _timer?.cancel();
    _refinementTimer?.cancel();
    super.dispose();
  }
}
