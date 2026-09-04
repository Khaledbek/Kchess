import 'package:kchess/ffi/core_gateway.dart';
import 'package:kchess/models/models.dart';

class FakeCoreGateway implements CoreGateway {
  FakeCoreGateway({
    List<AppProfile>? initialProfiles,
    List<GameSummary>? initialGames,
    this.currentSettings = const AppSettings(),
    this.analysisCompletesImmediately = true,
    this.analysisClassification = MoveClassification.theory,
    this.analysisRecommendedMove = 'e4',
    this.analysisProfileSide = 'unknown',
    this.createProviderError,
    this.syncProviderError,
  }) : storedProfiles = [...?initialProfiles],
       storedGames = [
         ...(initialGames ?? const [fixtureGame]),
       ] {
    active = storedProfiles.firstOrNull;
  }

  final List<AppProfile> storedProfiles;
  final List<GameSummary> storedGames;
  final List<FavoriteCollection> storedFavoriteCollections = [];
  AppProfile? active;
  AppSettings currentSettings;
  final bool analysisCompletesImmediately;
  final MoveClassification analysisClassification;
  final String analysisRecommendedMove;
  final String analysisProfileSide;
  final String? createProviderError;
  final String? syncProviderError;
  int startAnalysisCalls = 0;
  int analysisStatusCalls = 0;
  int variationAnalysisCalls = 0;
  int variationStatusCalls = 0;
  final Map<String, VariationAnalysisSnapshot> _variationJobs = {};
  int importPgnCalls = 0;
  int settingWrites = 0;
  int engineSettingWrites = 0;
  int clearCachedMonthCalls = 0;
  final List<String> clearedMonths = [];

  static const fixtureGame = GameSummary(
    id: 'fixture-test',
    kind: 'pgn',
    whiteName: 'Ada',
    blackName: 'Turing',
    whiteRating: 1840,
    blackRating: 1812,
    result: '1-0',
    timeControl: 'rapid',
    startingFen: 'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1',
    isFixture: true,
    providerGameId: 'online-fixture-1',
    providerUrl: 'https://example.invalid/game/1',
    providerOutcome: 'win',
    timeControlType: 'rapid',
    providerAccuracy: 81.2,
    localAccuracy: 92.4,
    accuracy: 92.4,
    accuracySource: 'local',
    endedAt: 1786363200,
  );

  static const completeSummary = AnalysisSummary(
    profileSide: 'white',
    white: PlayerAnalysisSummary(
      theory: 1,
      brilliant: 1,
      critical: 0,
      best: 2,
      excellent: 1,
      okay: 1,
      miss: 0,
      mistake: 1,
      blunder: 1,
      totalMoves: 8,
      analyzedMoves: 8,
      localAccuracy: 78.6,
    ),
    black: PlayerAnalysisSummary(
      theory: 1,
      brilliant: 0,
      critical: 0,
      best: 3,
      excellent: 1,
      okay: 2,
      miss: 0,
      mistake: 1,
      blunder: 0,
      totalMoves: 8,
      analyzedMoves: 8,
      localAccuracy: 87.2,
    ),
    classifierVersion: 1,
    accuracyAlgorithmVersion: 1,
    openingBookVersion: 'kcb1:test-fixture',
    engineDepth: 18,
    engineVersion: 'Stockfish 18',
  );

  static const partialSummary = AnalysisSummary(
    profileSide: 'white',
    white: PlayerAnalysisSummary(
      theory: 1,
      brilliant: 0,
      critical: 0,
      best: 0,
      excellent: 0,
      okay: 0,
      miss: 0,
      mistake: 0,
      blunder: 0,
      totalMoves: 4,
      analyzedMoves: 1,
    ),
    black: PlayerAnalysisSummary(
      theory: 0,
      brilliant: 0,
      critical: 0,
      best: 0,
      excellent: 0,
      okay: 0,
      miss: 0,
      mistake: 0,
      blunder: 0,
      totalMoves: 4,
      analyzedMoves: 0,
    ),
    classifierVersion: 0,
    accuracyAlgorithmVersion: 0,
    openingBookVersion: 'kcb1:test-fixture',
    engineDepth: 18,
    engineVersion: 'Stockfish 18',
  );

  @override
  Future<void> initialize() async {}

  @override
  Future<List<AppProfile>> profiles() async =>
      List.unmodifiable(storedProfiles);

  @override
  Future<AppProfile?> activeProfile() async => active;

  @override
  Future<AppProfile> createProfile({
    required ProfileType type,
    required String displayName,
    required String providerUsername,
  }) async {
    if (type != ProfileType.localPgnFen && createProviderError != null) {
      throw CoreGatewayException(createProviderError!);
    }
    final profile = AppProfile(
      id: 'profile-${storedProfiles.length + 1}',
      type: type,
      displayName: displayName,
      providerUsername: providerUsername.isEmpty ? null : providerUsername,
      avatarAsset: 'profile_unknown.png',
    );
    storedProfiles.add(profile);
    active = profile;
    return profile;
  }

  @override
  Future<void> setActiveProfile(String profileId) async {
    active = storedProfiles.firstWhere((profile) => profile.id == profileId);
  }

  @override
  Future<void> deleteProfile(String profileId) async {
    final wasActive = active?.id == profileId;
    storedProfiles.removeWhere((profile) => profile.id == profileId);
    if (wasActive) {
      active = storedProfiles.firstOrNull;
      if (active == null) storedGames.clear();
    }
  }

  @override
  Future<void> mergeLocalProfile(
    String sourceProfileId,
    String targetProfileId,
  ) async {
    final source = storedProfiles.firstWhere(
      (profile) => profile.id == sourceProfileId,
    );
    final target = storedProfiles.firstWhere(
      (profile) => profile.id == targetProfileId,
    );
    if (source.type != ProfileType.localPgnFen ||
        target.type == ProfileType.localPgnFen) {
      throw const CoreGatewayException('Invalid profile merge');
    }
    storedProfiles.removeWhere((profile) => profile.id == sourceProfileId);
    active = target;
  }

  @override
  Future<StatisticsOverview> statisticsOverview() async => StatisticsOverview(
    hasProfile: true,
    totalGames: 3,
    overall: const StatTally(
      games: 3,
      wins: 2,
      losses: 1,
      winRate: 0.6667,
      scorePercent: 0.6667,
    ),
    white: const StatTally(
      games: 2,
      wins: 1,
      losses: 1,
      winRate: 0.5,
      scorePercent: 0.5,
    ),
    black: const StatTally(games: 1, wins: 1, winRate: 1, scorePercent: 1),
    byTimeControl: const [
      StatTimeControl(
        type: 'blitz',
        tally: StatTally(
          games: 3,
          wins: 2,
          losses: 1,
          winRate: 0.6667,
          scorePercent: 0.6667,
        ),
      ),
    ],
    recentForm: const ['win', 'loss', 'win'],
  );

  @override
  Future<OpeningsStats> openingsStats() async => const OpeningsStats(
    hasProfile: true,
    gamesWithOpening: 3,
    gamesWithoutOpening: 0,
    distinctFamilies: 2,
    families: [
      OpeningFamily(
        familyName: 'Ruy Lopez',
        baseEco: 'C65',
        color: 'white',
        tally: StatTally(
          games: 2,
          wins: 1,
          losses: 1,
          winRate: 0.5,
          scorePercent: 0.5,
        ),
        variations: [
          OpeningVariation(
            eco: 'C65',
            name: 'Ruy Lopez: Berlin Defense',
            tally: StatTally(
              games: 2,
              wins: 1,
              losses: 1,
              winRate: 0.5,
              scorePercent: 0.5,
            ),
          ),
        ],
      ),
      OpeningFamily(
        familyName: 'Caro-Kann Defense',
        baseEco: 'B10',
        color: 'black',
        tally: StatTally(games: 1, wins: 1, winRate: 1, scorePercent: 1),
        variations: [
          OpeningVariation(
            eco: 'B10',
            name: 'Caro-Kann Defense',
            tally: StatTally(games: 1, wins: 1, winRate: 1, scorePercent: 1),
          ),
        ],
      ),
    ],
  );

  @override
  Future<TerminationStats> terminationStats() async => const TerminationStats(
    hasProfile: true,
    totalGames: 3,
    terminations: [
      GameTermination(type: 'resignation', count: 2),
      GameTermination(type: 'checkmate', count: 1),
    ],
  );

  @override
  Future<ProviderOverview> providerOverview(String profileId) async =>
      ProviderOverview(
        profile: storedProfiles.firstWhere((value) => value.id == profileId),
        stats: const [
          ProviderPerformance(
            key: 'rapid',
            currentRating: 1840,
            bestRating: 1902,
            games: 42,
            wins: 22,
            losses: 14,
            draws: 6,
          ),
        ],
        availableMonths: const ['2026-08'],
        offlineReady: true,
        retryAfterSeconds: 0,
      );

  @override
  Future<ProviderOverview> syncProvider(
    String profileId, {
    int year = 0,
    int month = 0,
  }) async {
    if (syncProviderError != null) {
      throw CoreGatewayException(syncProviderError!);
    }
    return providerOverview(profileId);
  }

  @override
  Future<AppSettings> settings() async => currentSettings;

  @override
  Future<void> setAnalysisDepthRange({
    required int minimumDepth,
    required int maximumDepth,
  }) async {
    settingWrites++;
    engineSettingWrites++;
    currentSettings = currentSettings.copyWith(
      minAnalysisDepth: minimumDepth,
      depth: maximumDepth,
    );
  }

  @override
  Future<void> setEngineSettings({
    required int depth,
    required int multiPv,
    required int timeLimitSeconds,
  }) async {
    settingWrites++;
    engineSettingWrites++;
    currentSettings = currentSettings.copyWith(
      depth: depth,
      multiPv: multiPv,
      timeLimitSeconds: timeLimitSeconds,
    );
  }

  @override
  Future<void> setEngineResources({
    required int threads,
    required int hashMb,
  }) async {
    settingWrites++;
    engineSettingWrites++;
    currentSettings = currentSettings.copyWith(
      threads: threads,
      hashMb: hashMb,
    );
  }

  @override
  Future<void> setShowBoardArrows(bool enabled) async {
    settingWrites++;
    currentSettings = currentSettings.copyWith(showBoardArrows: enabled);
  }

  @override
  Future<void> setBooleanSetting(String key, bool enabled) async {
    settingWrites++;
    currentSettings = switch (key) {
      'showBestMoveArrow' => currentSettings.copyWith(
        showBestMoveArrow: enabled,
      ),
      'showThreatArrow' => currentSettings.copyWith(showThreatArrow: enabled),
      'showEvaluationBar' => currentSettings.copyWith(
        showEvaluationBar: enabled,
      ),
      'showEngineLines' => currentSettings.copyWith(showEngineLines: enabled),
      'showClassifications' => currentSettings.copyWith(
        showClassifications: enabled,
      ),
      'showAccuracy' => currentSettings.copyWith(showAccuracy: enabled),
      'showTheory' => currentSettings.copyWith(showTheory: enabled),
      'showResultSymbols' => currentSettings.copyWith(
        showResultSymbols: enabled,
      ),
      'adaptiveEarlyStop' => currentSettings.copyWith(
        adaptiveEarlyStop: enabled,
      ),
      'showBoardCoordinates' => currentSettings.copyWith(
        showBoardCoordinates: enabled,
      ),
      'highlightLastMove' => currentSettings.copyWith(
        highlightLastMove: enabled,
      ),
      'highlightSelectedSquare' => currentSettings.copyWith(
        highlightSelectedSquare: enabled,
      ),
      'autoSyncOnline' => currentSettings.copyWith(autoSyncOnline: enabled),
      'confirmBeforeDelete' => currentSettings.copyWith(
        confirmBeforeDelete: enabled,
      ),
      'useGlobalAnalysisCache' => currentSettings.copyWith(
        useGlobalAnalysisCache: enabled,
      ),
      'diagnosticLogging' => currentSettings.copyWith(
        diagnosticLogging: enabled,
      ),
      _ => currentSettings,
    };
  }

  @override
  Future<void> setThemeMode(AppThemeMode mode) async {
    settingWrites++;
    currentSettings = currentSettings.copyWith(themeMode: mode);
  }

  @override
  Future<void> setLocale(String locale) async {
    settingWrites++;
    currentSettings = currentSettings.copyWith(locale: locale);
  }

  @override
  Future<List<GameSummary>> games() async =>
      active == null ? const [] : List.unmodifiable(storedGames);

  @override
  Future<List<GameSummary>> queryGames(GameQuery query) async => games();

  @override
  Future<GameDetail> game(String gameId) async => GameDetail(
    summary: fixtureGame,
    pgn: '1. e4 e5 2. Nf3 Nc6 3. Bb5 a6 4. Ba4 Nf6',
    startingPosition: BoardPosition(
      fen: 'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1',
      pieces: BoardPosition.empty.pieces,
      sideToMove: 'white',
      draggableColor: 'white',
    ),
    moves: [
      ParsedMove(
        plyIndex: 0,
        moveNumber: 1,
        sideToMove: 'white',
        san: 'e4',
        uci: 'e2e4',
        fenBefore: 'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1',
        fenAfter: 'rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1',
        positionAfter: BoardPosition(
          fen: 'rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1',
          pieces: BoardPosition.empty.pieces,
          sideToMove: 'black',
          draggableColor: 'black',
        ),
      ),
      ParsedMove(
        plyIndex: 1,
        moveNumber: 1,
        sideToMove: 'black',
        san: 'e5',
        uci: 'e7e5',
        fenBefore: 'rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1',
        fenAfter:
            'rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 2',
        positionAfter: BoardPosition(
          fen: 'rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 2',
          pieces: BoardPosition.empty.pieces,
          sideToMove: 'white',
          draggableColor: 'white',
        ),
      ),
    ],
  );

  @override
  Future<BoardMoveResolution> resolveBoardMove({
    required String gameId,
    required String fen,
    required String source,
    required String target,
    required int firstCandidatePly,
  }) async => BoardMoveResolution(
    uci: '$source$target',
    san: '$source$target',
    fenAfter: fen,
    positionAfter: BoardPosition.empty,
  );

  @override
  Future<GameSummary> importPgn(String pgn) async {
    importPgnCalls++;
    return fixtureGame;
  }

  @override
  Future<GameSummary> importFen({
    required String fen,
    required String name,
  }) async => fixtureGame;

  @override
  Future<AnalysisSnapshot> startAnalysis(String gameId) async {
    startAnalysisCalls++;
    return _snapshot(gameId, complete: analysisCompletesImmediately);
  }

  @override
  Future<AnalysisSnapshot> analysisStatus(String gameId) async {
    analysisStatusCalls++;
    return _snapshot(gameId, complete: true);
  }

  @override
  Future<AnalysisSnapshot> startMoveRefinement(String gameId, int ply) async =>
      _snapshot(gameId, complete: true);

  @override
  Future<AnalysisSnapshot> moveAnalysisStatus(String gameId, int ply) async =>
      _snapshot(gameId, complete: true);

  @override
  Future<void> cancelAnalysis(String gameId) async {}

  @override
  Future<void> deleteAnalysis(String gameId) async {}

  @override
  Future<void> clearEngineCache() async {}

  @override
  Future<VariationAnalysisSnapshot> startVariationAnalysis({
    required String fen,
    required String uci,
    int? depth,
    int? multiPv,
    int? threads,
    int? hashMb,
  }) async {
    variationAnalysisCalls++;
    final fixture = switch (uci) {
      'g1f3' => (
        san: 'Nf3',
        fen: 'rnbqkbnr/pppp1ppp/8/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R b KQkq - 1 2',
        best: 'b8c6',
      ),
      'b8c6' => (
        san: 'Nc6',
        fen: 'r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3',
        best: 'f1b5',
      ),
      'f1b5' => (
        san: 'Bb5',
        fen:
            'r1bqkbnr/pppp1ppp/2n5/1B2p3/4P3/5N2/PPPP1PPP/RNBQK2R b KQkq - 3 3',
        best: 'a7a6',
      ),
      'a7a6' => (
        san: 'a6',
        fen: 'r1bqkbnr/1ppp1ppp/p1n5/1B2p3/4P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 0 4',
        best: 'b5a4',
      ),
      'b5a4' => (
        san: 'Ba4',
        fen:
            'r1bqkbnr/1ppp1ppp/p1n5/4p3/B3P3/5N2/PPPP1PPP/RNBQK2R b KQkq - 1 4',
        best: 'g8f6',
      ),
      _ => throw const CoreGatewayException('Illegal chess move'),
    };
    final jobId = 'variation-$variationAnalysisCalls';
    _variationJobs[jobId] = VariationAnalysisSnapshot(
      jobId: jobId,
      status: 'complete',
      playedMove: uci,
      playedSan: fixture.san,
      fen: fixture.fen,
      position: BoardPosition(
        fen: fixture.fen,
        pieces: BoardPosition.empty.pieces,
        sideToMove: variationAnalysisCalls.isOdd ? 'black' : 'white',
        draggableColor: variationAnalysisCalls.isOdd ? 'black' : 'white',
      ),
      bestMove: fixture.best,
      moverEvaluationCp: 10 - variationAnalysisCalls,
      lines: [
        EngineLine(
          rank: 1,
          depth: 18,
          evaluationCp: variationAnalysisCalls - 10,
          nodes: 1000,
          moves: [fixture.best],
        ),
      ],
    );
    return VariationAnalysisSnapshot(
      jobId: jobId,
      status: 'running',
      playedMove: uci,
      playedSan: fixture.san,
      fen: fixture.fen,
      position: BoardPosition(
        fen: fixture.fen,
        pieces: BoardPosition.empty.pieces,
        sideToMove: variationAnalysisCalls.isOdd ? 'black' : 'white',
        draggableColor: variationAnalysisCalls.isOdd ? 'black' : 'white',
      ),
      bestMove: '',
      lines: [],
    );
  }

  @override
  Future<VariationAnalysisSnapshot> variationAnalysisStatus(
    String jobId,
  ) async {
    variationStatusCalls++;
    return _variationJobs[jobId]!;
  }

  @override
  Future<void> cancelVariationAnalysis(String jobId) async {
    _variationJobs.remove(jobId);
  }

  @override
  Future<void> setGameFavorite(String gameId, bool enabled) async {
    final index = storedGames.indexWhere((game) => game.id == gameId);
    if (index >= 0) {
      storedGames[index] = _copyGame(
        storedGames[index],
        favorite: enabled,
        clearFavoriteCollection: !enabled,
      );
    }
  }

  @override
  Future<List<GameSummary>> favoriteGames() async =>
      storedGames.where((game) => game.favorite).toList(growable: false);

  @override
  Future<List<FavoriteCollection>> favoriteCollections() async =>
      storedFavoriteCollections
          .map(
            (collection) => FavoriteCollection(
              id: collection.id,
              name: collection.name,
              gameCount: storedGames
                  .where(
                    (game) =>
                        game.favorite &&
                        game.favoriteCollectionId == collection.id,
                  )
                  .length,
              createdAt: collection.createdAt,
            ),
          )
          .toList(growable: false);

  @override
  Future<FavoriteCollection> createFavoriteCollection(String name) async {
    final collection = FavoriteCollection(
      id: 'collection-${storedFavoriteCollections.length + 1}',
      name: name.trim(),
      gameCount: 0,
      createdAt: DateTime.now().millisecondsSinceEpoch ~/ 1000,
    );
    storedFavoriteCollections.add(collection);
    return collection;
  }

  @override
  Future<void> renameFavoriteCollection(
    String collectionId,
    String name,
  ) async {
    final index = storedFavoriteCollections.indexWhere(
      (c) => c.id == collectionId,
    );
    if (index >= 0) {
      final old = storedFavoriteCollections[index];
      storedFavoriteCollections[index] = FavoriteCollection(
        id: old.id,
        name: name.trim(),
        gameCount: old.gameCount,
        createdAt: old.createdAt,
      );
    }
  }

  @override
  Future<void> deleteFavoriteCollection(String collectionId) async {
    storedFavoriteCollections.removeWhere((c) => c.id == collectionId);
    for (var index = 0; index < storedGames.length; index++) {
      final game = storedGames[index];
      if (game.favoriteCollectionId == collectionId) {
        storedGames[index] = _copyGame(game, clearFavoriteCollection: true);
      }
    }
  }

  @override
  Future<void> setGameFavoriteCollection(
    String gameId,
    String? collectionId,
  ) async {
    final index = storedGames.indexWhere((game) => game.id == gameId);
    if (index >= 0) {
      storedGames[index] = _copyGame(
        storedGames[index],
        favorite: true,
        favoriteCollectionId: collectionId,
        clearFavoriteCollection: collectionId == null,
      );
    }
  }

  @override
  Future<void> setGameDownloaded(String gameId, bool enabled) async {
    final index = storedGames.indexWhere((game) => game.id == gameId);
    if (index >= 0) {
      storedGames[index] = _copyGame(storedGames[index], downloaded: enabled);
    }
  }

  @override
  Future<void> deleteLocalGame(String gameId) async {
    storedGames.removeWhere((game) => game.id == gameId);
  }

  @override
  Future<void> clearCachedMonth(String profileId, String month) async {
    clearCachedMonthCalls++;
    clearedMonths.add(month);
  }

  GameSummary _copyGame(
    GameSummary game, {
    bool? favorite,
    bool? downloaded,
    String? favoriteCollectionId,
    bool clearFavoriteCollection = false,
  }) => GameSummary(
    id: game.id,
    kind: game.kind,
    whiteName: game.whiteName,
    blackName: game.blackName,
    whiteRating: game.whiteRating,
    blackRating: game.blackRating,
    result: game.result,
    event: game.event,
    date: game.date,
    timeControl: game.timeControl,
    startingFen: game.startingFen,
    isFixture: game.isFixture,
    providerGameId: game.providerGameId,
    providerUrl: game.providerUrl,
    providerOutcome: game.providerOutcome,
    timeControlType: game.timeControlType,
    providerAccuracy: game.providerAccuracy,
    localAccuracy: game.localAccuracy,
    accuracy: game.accuracy,
    accuracySource: game.accuracySource,
    favorite: favorite ?? game.favorite,
    favoriteCollectionId: clearFavoriteCollection
        ? null
        : favoriteCollectionId ?? game.favoriteCollectionId,
    downloaded: downloaded ?? game.downloaded,
    analyzed: game.analyzed,
    endedAt: game.endedAt,
  );

  AnalysisSnapshot _snapshot(String gameId, {required bool complete}) =>
      AnalysisSnapshot(
        gameId: gameId,
        status: complete ? 'complete' : 'running',
        jobState: complete
            ? AnalysisJobState.completed
            : AnalysisJobState.running,
        completedPlies: complete ? 8 : 1,
        totalPlies: 8,
        progress: complete ? 1 : 0.125,
        currentPly: complete ? 7 : 0,
        bestMove: 'e2e4',
        recommendedMove: analysisRecommendedMove,
        engineVersion: 'Stockfish 18',
        configHash: 'test',
        classification: complete ? analysisClassification : null,
        classifierVersion: complete ? 1 : 0,
        expectedScoreBest: complete ? 0.72 : null,
        expectedScorePlayed: complete ? 0.70 : null,
        expectedScoreLoss: complete ? 0.02 : null,
        theory: complete && analysisClassification == MoveClassification.theory
            ? const TheoryMoveInfo(
                games: 1284211,
                whiteWins: 600000,
                draws: 300000,
                blackWins: 384211,
              )
            : null,
        lines: const [
          EngineLine(
            rank: 1,
            depth: 18,
            evaluationCp: 20,
            nodes: 1000,
            moves: ['e2e4', 'e7e5'],
          ),
        ],
        summary: _summaryWithSide(complete ? completeSummary : partialSummary),
      );

  AnalysisSummary _summaryWithSide(AnalysisSummary value) => AnalysisSummary(
    profileSide: analysisProfileSide,
    white: value.white,
    black: value.black,
    classifierVersion: value.classifierVersion,
    accuracyAlgorithmVersion: value.accuracyAlgorithmVersion,
    openingBookVersion: value.openingBookVersion,
    engineDepth: value.engineDepth,
    engineVersion: value.engineVersion,
  );

  @override
  void dispose() {}
}

extension<T> on List<T> {
  T? get firstOrNull => isEmpty ? null : first;
}
