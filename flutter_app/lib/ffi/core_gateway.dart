import '../models/models.dart';

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
  Future<ProviderOverview> providerOverview(String profileId);
  Future<StatisticsOverview> statisticsOverview();
  Future<OpeningsStats> openingsStats();
  Future<ProviderOverview> syncProvider(
    String profileId, {
    int year = 0,
    int month = 0,
  });
  Future<AppSettings> settings();
  Future<void> setEngineSettings({
    required int depth,
    required int multiPv,
    required int timeLimitSeconds,
  });
  Future<void> setEngineResources({
    required int threads,
    required int hashMb,
  });
  Future<void> setShowBoardArrows(bool enabled);
  Future<void> setBooleanSetting(String key, bool enabled);
  Future<void> setThemeMode(AppThemeMode mode);
  Future<void> setLocale(String locale);
  Future<List<GameSummary>> games();
  Future<GameDetail> game(String gameId);
  Future<GameSummary> importPgn(String pgn);
  Future<GameSummary> importFen({required String fen, required String name});
  Future<AnalysisSnapshot> startAnalysis(String gameId);
  Future<AnalysisSnapshot> analysisStatus(String gameId);
  Future<AnalysisSnapshot> moveAnalysisStatus(String gameId, int ply);
  Future<void> cancelAnalysis(String gameId);
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
  const CoreGatewayException(
    this.message, {
    this.code = CoreErrorCode.unknown,
  });

  final String message;
  final CoreErrorCode code;

  @override
  String toString() => message;
}
