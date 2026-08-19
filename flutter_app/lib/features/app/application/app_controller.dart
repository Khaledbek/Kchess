import 'dart:async';

import 'package:flutter/foundation.dart';

import '../../../ffi/core_gateway.dart';
import '../../../shared/models/models.dart';

enum AppPhase { loading, firstRun, ready, error }

class AppController extends ChangeNotifier {
  AppController(this.gateway);

  final CoreGateway gateway;
  AppPhase phase = AppPhase.loading;
  List<AppProfile> profiles = const [];
  AppProfile? activeProfile;
  AppSettings settings = const AppSettings();
  List<GameSummary> games = const [];
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
      settings = await gateway.settings();
      profiles = await gateway.profiles();
      activeProfile = await gateway.activeProfile();
      if (profiles.isEmpty || activeProfile == null) {
        phase = AppPhase.firstRun;
        games = const [];
        favoriteCollections = const [];
      } else {
        games = await gateway.games();
        favoriteCollections = await gateway.favoriteCollections();
        if (activeProfile!.type != ProfileType.localPgnFen) {
          providerOverview = await gateway.providerOverview(activeProfile!.id);
          selectedMonth = _currentMonth();
        }
        phase = AppPhase.ready;
      }
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
    if (profiles.isEmpty || activeProfile == null) {
      games = const [];
      favoriteCollections = const [];
      settings = await gateway.settings();
      phase = AppPhase.firstRun;
      notifyListeners();
      return;
    }
    settings = await gateway.settings();
    games = await gateway.games();
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
    } catch (caught) {
      if (generation != _providerSyncGeneration ||
          activeProfile?.id != profile.id) {
        return;
      }
      providerNotice = caught.toString();
      games = await gateway.games();
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
    favoriteCollections = await gateway.favoriteCollections();
    notifyListeners();
  }

  Future<void> createFavoriteCollection(String name) async {
    await gateway.createFavoriteCollection(name.trim());
    favoriteCollections = await gateway.favoriteCollections();
    notifyListeners();
  }

  Future<void> renameFavoriteCollection(
    FavoriteCollection collection,
    String name,
  ) async {
    await gateway.renameFavoriteCollection(collection.id, name.trim());
    favoriteCollections = await gateway.favoriteCollections();
    notifyListeners();
  }

  Future<void> deleteFavoriteCollection(FavoriteCollection collection) async {
    await gateway.deleteFavoriteCollection(collection.id);
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
    favoriteCollections = await gateway.favoriteCollections();
    notifyListeners();
  }

  Future<void> toggleDownload(GameSummary game) async {
    await gateway.setGameDownloaded(game.id, !game.downloaded);
    games = await gateway.games();
    notifyListeners();
  }

  Future<void> deleteLocalGame(GameSummary game) async {
    if (activeProfile?.type != ProfileType.localPgnFen) {
      throw const CoreGatewayException(
        'Only local PGN/FEN entries can be deleted',
      );
    }
    await gateway.deleteLocalGame(game.id);
    games = await gateway.games();
    favoriteCollections = await gateway.favoriteCollections();
    notifyListeners();
  }

  Future<void> clearCachedMonth(String month) async {
    final profile = activeProfile;
    if (profile == null || profile.type == ProfileType.localPgnFen) return;
    await gateway.clearCachedMonth(profile.id, month);
    games = await gateway.games();
    providerOverview = await gateway.providerOverview(profile.id);
    notifyListeners();
  }

  String _currentMonth() {
    final now = DateTime.now();
    return '${now.year.toString().padLeft(4, '0')}-${now.month.toString().padLeft(2, '0')}';
  }

  Future<GameSummary> importPgn(String pgn) async {
    final game = await gateway.importPgn(pgn);
    games = [game, ...games.where((value) => value.id != game.id)];
    notifyListeners();
    return game;
  }

  Future<GameSummary> importFen({
    required String fen,
    required String name,
  }) async {
    final game = await gateway.importFen(fen: fen, name: name);
    games = [game, ...games.where((value) => value.id != game.id)];
    notifyListeners();
    return game;
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

  Future<void> setDepth(int depth) =>
      _setEngineSettings(settings.copyWith(depth: depth));

  Future<void> setMultiPv(int multiPv) =>
      _setEngineSettings(settings.copyWith(multiPv: multiPv));

  Future<void> setTimeLimitSeconds(int seconds) =>
      _setEngineSettings(settings.copyWith(timeLimitSeconds: seconds));

  Future<void> setThreads(int threads) =>
      _setEngineResources(settings.copyWith(threads: threads));

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
  bool _disposed = false;

  Future<void> open() async {
    try {
      detail = await gateway.game(game.id);
      snapshot = await gateway.startAnalysis(game.id);
      displayedSnapshot = snapshot;
      if (!_disposed) notifyListeners();
      if (snapshot?.isRunning == true) {
        _schedulePoll();
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
    } catch (caught) {
      error = caught;
      if (!_disposed) notifyListeners();
    }
  }

  void _schedulePoll() {
    _timer?.cancel();
    _timer = Timer(pollInterval, _poll);
  }

  Future<void> _poll() async {
    try {
      snapshot = await gateway.analysisStatus(game.id);
      if (selectedPly == null) displayedSnapshot = snapshot;
      if (!_disposed) notifyListeners();
      if (snapshot?.isRunning == true) {
        _schedulePoll();
      }
    } catch (caught) {
      error = caught;
      if (!_disposed) notifyListeners();
    }
  }

  Future<void> selectPly(int ply) async {
    final maximum = detail?.moves.isEmpty == true
        ? 0
        : (detail?.moves.length ?? 1) - 1;
    final selected = ply < 0 ? 0 : (ply > maximum ? maximum : ply);
    selectedPly = selected;
    if (selected < (snapshot?.completedPlies ?? 0)) {
      try {
        displayedSnapshot = await gateway.moveAnalysisStatus(game.id, selected);
      } catch (caught) {
        error = caught;
      }
    } else {
      displayedSnapshot = null;
    }
    if (!_disposed) notifyListeners();
  }

  void followLatest() {
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
    _timer?.cancel();
    super.dispose();
  }
}
