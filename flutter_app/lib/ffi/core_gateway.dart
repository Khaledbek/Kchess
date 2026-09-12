// -----------------------------------------------------------------------------
// Section: Native gateway contract
// -----------------------------------------------------------------------------

import '../shared/models/models.dart';

abstract interface class CoreGateway {
  Future<void> initialize();
  Future<List<AppProfile>> profiles();
  Future<AppProfile?> activeProfile();
  Future<AppProfile> createProfile({
    required ProfileType type,
    required String displayName,
    required String providerUsername,
  });
  Future<void> setActiveProfile(String profileId);
  Future<void> deleteProfile(String profileId);
  Future<void> mergeLocalProfile(
    String sourceProfileId,
    String targetProfileId,
  );
  Future<ProviderOverview> providerOverview(String profileId);

  /// Deep scouting report for a public player: profile, ratings and their
  /// win/draw/loss by colour, time control, termination and opening, aggregated
  /// natively from recent archives without persistence.
  Future<ScoutReport> scoutReport(String username);
  Future<StatisticsOverview> statisticsOverview();
  Future<OpeningsStats> openingsStats({String timeControl = 'all'});
  Future<TerminationStats> terminationStats();
  Future<PhaseStats> phaseStats();
  Future<StatisticsTimeline> statisticsTimeline(GameQuery query);
  Future<ProviderOverview> syncProvider(
    String profileId, {
    int year = 0,
    int month = 0,
  });
  Future<AppSettings> settings();
  Future<void> setAnalysisDepthRange({
    required int minimumDepth,
    required int maximumDepth,
  });
  Future<void> setEngineSettings({
    required int depth,
    required int multiPv,
    required int timeLimitSeconds,
  });
  Future<void> setEngineResources({required int threads, required int hashMb});
  Future<void> setSidelineEngineSettings({
    required int depth,
    required int multiPv,
    required int threads,
    required int hashMb,
  });
  Future<void> setShowBoardArrows(bool enabled);
  Future<void> setBooleanSetting(String key, bool enabled);
  Future<void> setThemeMode(AppThemeMode mode);
  Future<void> setLocale(String locale);
  Future<void> setEngineId(String engineId);
  Future<List<GameSummary>> games();
  Future<List<GameSummary>> queryGames(GameQuery query);
  Future<List<GameSummary>> favoriteGames();
  Future<GameDetail> game(String gameId);
  Future<BotGameSession> createBotGame(int requestedElo);
  Future<BotGameSession?> activeBotGame();
  Future<BotGameSession> botGame(String gameId);
  Future<List<BotGameSummary>> botGames();
  Future<GameSummary> botGameAnalysisGame(String gameId);
  Future<BoardMoveResolution> recordBotGameMove({
    required String gameId,
    required String expectedFenBefore,
    required String uci,
  });
  Future<BoardMoveResolution> replaceBotGameContinuation({
    required String gameId,
    required int basePly,
    required String expectedFenBefore,
    required String uci,
  });
  Future<void> resignBotGame(String gameId);
  Future<void> abortBotGame(String gameId);
  Future<void> deleteBotGame(String gameId);
  Future<void> setBotGameShowEvaluationBar(String gameId, bool enabled);
  Future<List<String>> boardPromotionOptions({
    required String fen,
    required String source,
    required String target,
  });
  Future<BoardMoveResolution> resolveFreeBoardMove({
    required String fen,
    required String source,
    required String target,
  });
  Future<BotMoveSnapshot> startBotMove({
    required String fen,
    required int requestedElo,
  });
  Future<BotMoveSnapshot> botMoveStatus(String jobId);
  Future<void> cancelBotMove(String jobId);
  Future<Map<String, Object?>> coachAsk(Map<String, Object?> request);
  Future<Map<String, Object?>> coachContext(Map<String, Object?> request);
  Future<Map<String, Object?>> coachAutomatic(Map<String, Object?> request);
  Future<Map<String, Object?>> coachHint(Map<String, Object?> request);
  Future<TrainingOverview> trainingOverview();
  Future<Object?> practiceCommand(Map<String, Object?> request);
  Future<TrainingAttempt> startTrainingAttempt(String exerciseId);
  Future<TrainingMoveResult> playTrainingMove({
    required String attemptId,
    required String source,
    required String target,
  });
  Future<BoardMoveResolution> resolveBoardMove({
    required String gameId,
    required String fen,
    required String source,
    required String target,
    required int firstCandidatePly,
  });
  Future<GameSummary> importPgn(String pgn);
  Future<GameSummary> importFen({required String fen, required String name});
  Future<AnalysisSnapshot> startAnalysis(String gameId);
  Future<AnalysisSnapshot> analysisStatus(String gameId);
  Future<AnalysisSnapshot> moveAnalysisStatus(String gameId, int ply);
  Future<AnalysisSnapshot> startMoveRefinement(String gameId, int ply);
  Future<void> cancelAnalysis(String gameId);
  Future<void> deleteAnalysis(String gameId);
  Future<void> clearEngineCache();
  Future<VariationAnalysisSnapshot> startVariationAnalysis({
    required String fen,
    required String uci,
    int? depth,
    int? multiPv,
    int? threads,
    int? hashMb,
  });
  Future<VariationAnalysisSnapshot> variationAnalysisStatus(String jobId);
  Future<void> cancelVariationAnalysis(String jobId);
  Future<void> setGameFavorite(String gameId, bool enabled);
  Future<List<FavoriteCollection>> favoriteCollections();
  Future<FavoriteCollection> createFavoriteCollection(String name);
  Future<void> renameFavoriteCollection(String collectionId, String name);
  Future<void> deleteFavoriteCollection(String collectionId);
  Future<void> setGameFavoriteCollection(String gameId, String? collectionId);
  Future<void> setGameDownloaded(String gameId, bool enabled);
  Future<void> deleteLocalGame(String gameId);
  Future<void> clearCachedMonth(String profileId, String month);
  void dispose();
}

enum CoreErrorCode {
  none,
  invalidArgument,
  notFound,
  databaseError,
  engineUnavailable,
  internalError,
  unknown;

  static CoreErrorCode fromNative(int value) => switch (value) {
    0 => CoreErrorCode.none,
    1 => CoreErrorCode.invalidArgument,
    2 => CoreErrorCode.notFound,
    3 => CoreErrorCode.databaseError,
    4 => CoreErrorCode.engineUnavailable,
    5 => CoreErrorCode.internalError,
    _ => CoreErrorCode.unknown,
  };
}

class CoreGatewayException implements Exception {
  const CoreGatewayException(this.message, {this.code = CoreErrorCode.unknown});

  final String message;
  final CoreErrorCode code;

  @override
  String toString() => message;
}
