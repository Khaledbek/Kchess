// -----------------------------------------------------------------------------
// Section: Application DTOs
// -----------------------------------------------------------------------------

part 'statistics_details.dart';
part 'training_models.dart';

enum ProfileType {
  chessCom,
  lichess,
  localPgnFen;

  static ProfileType fromJson(String value) => switch (value) {
    'chessCom' => ProfileType.chessCom,
    'lichess' => ProfileType.lichess,
    _ => ProfileType.localPgnFen,
  };

  int get nativeValue => index;
}

class AppProfile {
  const AppProfile({
    required this.id,
    required this.type,
    required this.displayName,
    required this.avatarAsset,
    this.providerUsername,
    this.title,
    this.avatarUrl,
    this.avatarFile,
    this.flair,
    this.fide,
    this.providerGames,
    this.providerWins,
    this.providerLosses,
    this.providerDraws,
    this.providerDisabled = false,
  });

  factory AppProfile.fromJson(Map<String, Object?> json) => AppProfile(
    id: json['id']! as String,
    type: ProfileType.fromJson(json['type']! as String),
    displayName: json['displayName']! as String,
    providerUsername: json['providerUsername'] as String?,
    avatarAsset: json['avatarAsset']! as String,
    title: json['title'] as String?,
    avatarUrl: json['avatarUrl'] as String?,
    avatarFile: json['avatarFile'] as String?,
    flair: json['flair'] as String?,
    fide: json['fide'] as int?,
    providerGames: json['providerGames'] as int?,
    providerWins: json['providerWins'] as int?,
    providerLosses: json['providerLosses'] as int?,
    providerDraws: json['providerDraws'] as int?,
    providerDisabled: json['providerDisabled'] as bool? ?? false,
  );

  final String id;
  final ProfileType type;
  final String displayName;
  final String? providerUsername;
  final String avatarAsset;
  final String? title;
  final String? avatarUrl;
  final String? avatarFile;
  final String? flair;
  final int? fide;
  final int? providerGames;
  final int? providerWins;
  final int? providerLosses;
  final int? providerDraws;
  final bool providerDisabled;
}

class ProviderPerformance {
  const ProviderPerformance({
    required this.key,
    this.currentRating,
    this.games,
    this.wins,
    this.losses,
    this.draws,
  });

  factory ProviderPerformance.fromJson(Map<String, Object?> json) =>
      ProviderPerformance(
        key: json['key']! as String,
        currentRating: json['currentRating'] as int?,
        games: json['games'] as int?,
        wins: json['wins'] as int?,
        losses: json['losses'] as int?,
        draws: json['draws'] as int?,
      );

  final String key;
  final int? currentRating;
  final int? games;
  final int? wins;
  final int? losses;
  final int? draws;
}

class ProviderOverview {
  const ProviderOverview({
    required this.profile,
    required this.stats,
    required this.availableMonths,
  });

  factory ProviderOverview.fromJson(Map<String, Object?> json) =>
      ProviderOverview(
        profile: AppProfile.fromJson(json['profile']! as Map<String, Object?>),
        stats: (json['stats'] as List<Object?>? ?? const [])
            .cast<Map<String, Object?>>()
            .map(ProviderPerformance.fromJson)
            .toList(growable: false),
        availableMonths: (json['availableMonths'] as List<Object?>? ?? const [])
            .cast<String>(),
      );

  final AppProfile profile;
  final List<ProviderPerformance> stats;
  final List<String> availableMonths;
}

/// A win/draw/loss tally from the profile's perspective, with derived rates.
/// [winRate] and [scorePercent] are null when no game has a decided result.
class StatTally {
  const StatTally({
    this.games = 0,
    this.wins = 0,
    this.draws = 0,
    this.losses = 0,
    this.winRate,
    this.winShare,
    this.scorePercent,
  });

  factory StatTally.fromJson(Map<String, Object?> json) => StatTally(
    games: json['games'] as int? ?? 0,
    wins: json['wins'] as int? ?? 0,
    draws: json['draws'] as int? ?? 0,
    losses: json['losses'] as int? ?? 0,
    winRate: (json['winRate'] as num?)?.toDouble(),
    winShare: (json['winShare'] as num?)?.toDouble(),
    scorePercent: (json['scorePercent'] as num?)?.toDouble(),
  );

  final int games;
  final int wins;
  final int draws;
  final int losses;
  final double? winRate;
  final double? winShare;
  final double? scorePercent;
}

/// One time-control bucket (bullet, blitz, rapid, ...) with its tally.
class StatTimeControl {
  const StatTimeControl({required this.type, required this.tally});

  factory StatTimeControl.fromJson(Map<String, Object?> json) =>
      StatTimeControl(
        type: json['type'] as String? ?? 'unknown',
        tally: StatTally.fromJson(json),
      );

  final String type;
  final StatTally tally;
}

/// Aggregated performance overview for the active profile.
class StatisticsOverview {
  const StatisticsOverview({
    this.hasProfile = false,
    this.totalGames = 0,
    this.overall = const StatTally(),
    this.white = const StatTally(),
    this.black = const StatTally(),
    this.byTimeControl = const [],
  });

  factory StatisticsOverview.fromJson(Map<String, Object?> json) {
    final byColor = json['byColor'] as Map<String, Object?>? ?? const {};
    return StatisticsOverview(
      hasProfile: json['hasProfile'] as bool? ?? false,
      totalGames: json['totalGames'] as int? ?? 0,
      overall: StatTally.fromJson(
        json['overall'] as Map<String, Object?>? ?? const {},
      ),
      white: StatTally.fromJson(
        byColor['white'] as Map<String, Object?>? ?? const {},
      ),
      black: StatTally.fromJson(
        byColor['black'] as Map<String, Object?>? ?? const {},
      ),
      byTimeControl: (json['byTimeControl'] as List<Object?>? ?? const [])
          .cast<Map<String, Object?>>()
          .map(StatTimeControl.fromJson)
          .toList(growable: false),
    );
  }

  final bool hasProfile;
  final int totalGames;
  final StatTally overall;
  final StatTally white;
  final StatTally black;
  final List<StatTimeControl> byTimeControl;

  bool get isEmpty => totalGames == 0;
}

/// One specific line within an opening family (e.g. "Scandinavian Defense:
/// Mieses-Kotroc Variation"), with its win/draw/loss tally from the profile's
/// perspective.
class OpeningVariation {
  const OpeningVariation({
    required this.eco,
    required this.name,
    required this.tally,
  });

  factory OpeningVariation.fromJson(Map<String, Object?> json) =>
      OpeningVariation(
        eco: json['eco'] as String? ?? '',
        name: json['name'] as String? ?? '',
        tally: StatTally.fromJson(json),
      );

  final String eco;
  final String name; // full opening name, including the family prefix
  final StatTally tally;
}

/// A base opening family (e.g. "Scandinavian Defense") aggregating every
/// variation the profile played with a given color, most played first.
class OpeningFamily {
  const OpeningFamily({
    required this.familyName,
    required this.baseEco,
    required this.color,
    required this.tally,
    required this.variations,
    this.hasDistinctVariations = false,
  });

  factory OpeningFamily.fromJson(Map<String, Object?> json) => OpeningFamily(
    familyName: json['family'] as String? ?? '',
    hasDistinctVariations: json['hasDistinctVariations'] as bool? ?? false,
    baseEco: json['eco'] as String? ?? '',
    color: json['color'] as String? ?? 'unknown',
    tally: StatTally.fromJson(json),
    variations: (json['variations'] as List<Object?>? ?? const [])
        .cast<Map<String, Object?>>()
        .map(OpeningVariation.fromJson)
        .toList(growable: false),
  );

  final String familyName;
  final String baseEco;
  final String color; // white | black | unknown
  final StatTally tally;
  final List<OpeningVariation> variations;

  final bool hasDistinctVariations;
}

/// Opening families for the active profile, grouped and most played first.
class OpeningsStats {
  const OpeningsStats({
    this.hasProfile = false,
    this.gamesWithOpening = 0,
    this.bestWinRateFamilies = const [],
    this.nemesis,
    this.defaultColor = 'white',
    this.families = const [],
  });

  factory OpeningsStats.fromJson(Map<String, Object?> json) => OpeningsStats(
    hasProfile: json['hasProfile'] as bool? ?? false,
    gamesWithOpening: json['gamesWithOpening'] as int? ?? 0,
    defaultColor: json['defaultColor'] as String? ?? 'white',
    bestWinRateFamilies:
        (json['bestWinRateFamilies'] as List<Object?>? ?? const [])
            .cast<Map<String, Object?>>()
            .map(OpeningFamily.fromJson)
            .toList(growable: false),
    nemesis: json['nemesis'] == null
        ? null
        : OpeningFamily.fromJson(json['nemesis']! as Map<String, Object?>),
    families: (json['families'] as List<Object?>? ?? const [])
        .cast<Map<String, Object?>>()
        .map(OpeningFamily.fromJson)
        .toList(growable: false),
  );

  final bool hasProfile;
  final int gamesWithOpening;
  final String defaultColor;
  final List<OpeningFamily> bestWinRateFamilies;
  final OpeningFamily? nemesis;
  final List<OpeningFamily> families;

  bool get isEmpty => gamesWithOpening == 0;
}

/// One game-termination bucket (checkmate, resignation, timeout, draw, other)
/// with the profile's win/draw/loss split among the games that ended that way.
class GameTermination {
  const GameTermination({required this.type, required this.tally});

  factory GameTermination.fromJson(Map<String, Object?> json) =>
      GameTermination(
        type: json['type'] as String? ?? 'other',
        tally: StatTally.fromJson(json),
      );

  final String type; // checkmate | resignation | timeout | draw | other
  final StatTally tally;

  int get count => tally.games;
}

/// How the active profile's games ended, aggregated in the native layer from
/// the stored PGN Termination tags.
class TerminationStats {
  const TerminationStats({
    this.hasProfile = false,
    this.totalGames = 0,
    this.terminations = const [],
    this.spotlight,
  });

  factory TerminationStats.fromJson(Map<String, Object?> json) =>
      TerminationStats(
        hasProfile: json['hasProfile'] as bool? ?? false,
        spotlight: json['spotlight'] == null
            ? null
            : TerminationSpotlight.fromJson(
                json['spotlight']! as Map<String, Object?>,
              ),
        totalGames: json['totalGames'] as int? ?? 0,
        terminations: (json['terminations'] as List<Object?>? ?? const [])
            .cast<Map<String, Object?>>()
            .map(GameTermination.fromJson)
            .toList(growable: false),
      );

  final bool hasProfile;
  final int totalGames;
  final List<GameTermination> terminations;
  final TerminationSpotlight? spotlight;

  bool get isEmpty => terminations.isEmpty;
}

/// Win/draw/loss tally for the games that ended in one game phase (opening,
/// middlegame, endgame), from the profile's perspective.
class GamePhase {
  const GamePhase({required this.phase, required this.tally});

  factory GamePhase.fromJson(Map<String, Object?> json) => GamePhase(
    phase: json['phase'] as String? ?? 'unknown',
    tally: StatTally.fromJson(json),
  );

  final String phase; // opening | middlegame | endgame
  final StatTally tally;
}

/// Distribution of the active profile's games across the phase in which they
/// ended, aggregated in the native layer from each game's final move number.
class PhaseStats {
  const PhaseStats({
    this.hasProfile = false,
    this.totalGames = 0,
    this.classified = 0,
    this.overall = const StatTally(),
    this.phases = const [],
  });

  factory PhaseStats.fromJson(Map<String, Object?> json) => PhaseStats(
    hasProfile: json['hasProfile'] as bool? ?? false,
    totalGames: json['totalGames'] as int? ?? 0,
    classified: json['classified'] as int? ?? 0,
    overall: StatTally.fromJson(
      json['overall'] as Map<String, Object?>? ?? const {},
    ),
    phases: (json['phases'] as List<Object?>? ?? const [])
        .cast<Map<String, Object?>>()
        .map(GamePhase.fromJson)
        .toList(growable: false),
  );

  final bool hasProfile;
  final int totalGames;
  final int classified;
  final StatTally overall;
  final List<GamePhase> phases;

  bool get isEmpty => classified == 0;
}

/// One opening a scouted opponent played with a given colour, with their
/// win/draw/loss tally. `eco` is the join key for the repertoire clash.
class ScoutOpening {
  const ScoutOpening({
    required this.eco,
    required this.name,
    required this.color,
    required this.tally,
  });

  factory ScoutOpening.fromJson(Map<String, Object?> json) => ScoutOpening(
    eco: json['eco'] as String? ?? '',
    name: json['name'] as String? ?? '',
    color: json['color'] as String? ?? 'unknown',
    tally: StatTally.fromJson(json),
  );

  final String eco;
  final String name;
  final String color; // white | black | unknown
  final StatTally tally;
}

/// A deep scouting report for a public opponent: profile, ratings and their
/// win/draw/loss aggregated by colour, time control, termination and opening —
/// computed in the native layer from recent archives, without persistence.
class ScoutReport {
  const ScoutReport({
    this.hasProfile = false,
    required this.profile,
    this.stats = const [],
    this.gamesAnalyzed = 0,
    this.monthsFetched = 0,
    this.overall = const StatTally(),
    this.white = const StatTally(),
    this.black = const StatTally(),
    this.byTimeControl = const [],
    this.terminations = const [],
    this.openings = const [],
    this.comparison = const PlayerComparison(),
  });

  factory ScoutReport.fromJson(Map<String, Object?> json) {
    final byColor = json['byColor'] as Map<String, Object?>? ?? const {};
    return ScoutReport(
      hasProfile: json['hasProfile'] as bool? ?? false,
      comparison: PlayerComparison.fromJson(
        json['comparison'] as Map<String, Object?>? ?? const {},
      ),
      profile: AppProfile.fromJson(json['profile']! as Map<String, Object?>),
      stats: (json['stats'] as List<Object?>? ?? const [])
          .cast<Map<String, Object?>>()
          .map(ProviderPerformance.fromJson)
          .toList(growable: false),
      gamesAnalyzed: json['gamesAnalyzed'] as int? ?? 0,
      monthsFetched: json['monthsFetched'] as int? ?? 0,
      overall: StatTally.fromJson(
        json['overall'] as Map<String, Object?>? ?? const {},
      ),
      white: StatTally.fromJson(
        byColor['white'] as Map<String, Object?>? ?? const {},
      ),
      black: StatTally.fromJson(
        byColor['black'] as Map<String, Object?>? ?? const {},
      ),
      byTimeControl: (json['byTimeControl'] as List<Object?>? ?? const [])
          .cast<Map<String, Object?>>()
          .map(StatTimeControl.fromJson)
          .toList(growable: false),
      terminations: (json['terminations'] as List<Object?>? ?? const [])
          .cast<Map<String, Object?>>()
          .map(GameTermination.fromJson)
          .toList(growable: false),
      openings: (json['openings'] as List<Object?>? ?? const [])
          .cast<Map<String, Object?>>()
          .map(ScoutOpening.fromJson)
          .toList(growable: false),
    );
  }

  final bool hasProfile;
  final AppProfile profile;
  final List<ProviderPerformance> stats;
  final int gamesAnalyzed;
  final int monthsFetched;
  final StatTally overall;
  final StatTally white;
  final StatTally black;
  final List<StatTimeControl> byTimeControl;
  final List<GameTermination> terminations;
  final List<ScoutOpening> openings;
  final PlayerComparison comparison;
}

enum AppThemeMode {
  system,
  light,
  dark;

  static AppThemeMode fromJson(String value) => switch (value) {
    'light' => AppThemeMode.light,
    'dark' => AppThemeMode.dark,
    _ => AppThemeMode.system,
  };
}

class AppSettings {
  const AppSettings({
    this.minAnalysisDepth = 12,
    this.depth = 18,
    this.multiPv = 3,
    this.timeLimitSeconds = 0,
    this.threads = 2,
    this.maxThreads = 2,
    this.hashMb = 128,
    this.sidelineDepth = 18,
    this.sidelineMultiPv = 3,
    this.sidelineThreads = 2,
    this.sidelineHashMb = 128,
    this.showBestMoveArrow = true,
    this.showThreatArrow = true,
    this.showEvaluationBar = true,
    this.showEngineLines = true,
    this.showClassifications = true,
    this.showAccuracy = true,
    this.showTheory = true,
    this.showResultSymbols = true,
    this.adaptiveEarlyStop = true,
    this.showBoardCoordinates = true,
    this.highlightLastMove = true,
    this.highlightSelectedSquare = true,
    this.autoSyncOnline = true,
    this.confirmBeforeDelete = true,
    this.useGlobalAnalysisCache = true,
    this.diagnosticLogging = true,
    this.themeMode = AppThemeMode.system,
    this.locale = 'de',
    this.engineId = 'stockfish18',
  });

  factory AppSettings.fromJson(Map<String, Object?> json) => AppSettings(
    minAnalysisDepth: json['minAnalysisDepth'] as int? ?? 12,
    depth: (json['maxAnalysisDepth'] as int?) ?? (json['depth'] as int?) ?? 18,
    multiPv: json['multiPv'] as int? ?? 3,
    timeLimitSeconds: json['timeLimitSeconds'] as int? ?? 0,
    threads: json['threads'] as int? ?? 2,
    maxThreads: json['maxThreads'] as int? ?? json['threads'] as int? ?? 2,
    hashMb: json['hashMb'] as int? ?? 128,
    sidelineDepth:
        json['sidelineDepth'] as int? ??
        ((json['maxAnalysisDepth'] as int?) ?? (json['depth'] as int?) ?? 18),
    sidelineMultiPv:
        json['sidelineMultiPv'] as int? ?? json['multiPv'] as int? ?? 3,
    sidelineThreads:
        json['sidelineThreads'] as int? ?? json['threads'] as int? ?? 2,
    sidelineHashMb:
        json['sidelineHashMb'] as int? ?? json['hashMb'] as int? ?? 128,
    showBestMoveArrow:
        json['showBestMoveArrow'] as bool? ??
        json['showBoardArrows'] as bool? ??
        true,
    showThreatArrow: json['showThreatArrow'] as bool? ?? true,
    showEvaluationBar: json['showEvaluationBar'] as bool? ?? true,
    showEngineLines: json['showEngineLines'] as bool? ?? true,
    showClassifications: json['showClassifications'] as bool? ?? true,
    showAccuracy: json['showAccuracy'] as bool? ?? true,
    showTheory: json['showTheory'] as bool? ?? true,
    showResultSymbols: json['showResultSymbols'] as bool? ?? true,
    adaptiveEarlyStop: json['adaptiveEarlyStop'] as bool? ?? true,
    showBoardCoordinates: json['showBoardCoordinates'] as bool? ?? true,
    highlightLastMove: json['highlightLastMove'] as bool? ?? true,
    highlightSelectedSquare: json['highlightSelectedSquare'] as bool? ?? true,
    autoSyncOnline: json['autoSyncOnline'] as bool? ?? true,
    confirmBeforeDelete: json['confirmBeforeDelete'] as bool? ?? true,
    useGlobalAnalysisCache: json['useGlobalAnalysisCache'] as bool? ?? true,
    diagnosticLogging: json['diagnosticLogging'] as bool? ?? true,
    themeMode: AppThemeMode.fromJson(json['themeMode'] as String? ?? 'system'),
    locale: json['locale'] as String? ?? 'de',
    engineId: json['engineId'] == 'stockfish19' ? 'stockfish19' : 'stockfish18',
  );

  final int minAnalysisDepth;
  final int depth;
  final int multiPv;
  final int timeLimitSeconds;
  final int threads;
  final int maxThreads;
  final int hashMb;
  final int sidelineDepth;
  final int sidelineMultiPv;
  final int sidelineThreads;
  final int sidelineHashMb;
  final bool showBestMoveArrow;
  final bool showThreatArrow;
  final bool showEvaluationBar;
  final bool showEngineLines;
  final bool showClassifications;
  final bool showAccuracy;
  final bool showTheory;
  final bool showResultSymbols;
  final bool adaptiveEarlyStop;
  final bool showBoardCoordinates;
  final bool highlightLastMove;
  final bool highlightSelectedSquare;
  final bool autoSyncOnline;
  final bool confirmBeforeDelete;
  final bool useGlobalAnalysisCache;
  final bool diagnosticLogging;
  final AppThemeMode themeMode;
  final String locale;
  final String engineId;

  // Compatibility alias for older widgets/tests while the setting is now
  // presented to users as the Best Move Arrow.
  bool get showBoardArrows => showBestMoveArrow;

  AppSettings copyWith({
    int? minAnalysisDepth,
    int? depth,
    int? multiPv,
    int? timeLimitSeconds,
    int? threads,
    int? maxThreads,
    int? hashMb,
    int? sidelineDepth,
    int? sidelineMultiPv,
    int? sidelineThreads,
    int? sidelineHashMb,
    bool? showBestMoveArrow,
    bool? showBoardArrows,
    bool? showThreatArrow,
    bool? showEvaluationBar,
    bool? showEngineLines,
    bool? showClassifications,
    bool? showAccuracy,
    bool? showTheory,
    bool? showResultSymbols,
    bool? adaptiveEarlyStop,
    bool? showBoardCoordinates,
    bool? highlightLastMove,
    bool? highlightSelectedSquare,
    bool? autoSyncOnline,
    bool? confirmBeforeDelete,
    bool? useGlobalAnalysisCache,
    bool? diagnosticLogging,
    AppThemeMode? themeMode,
    String? locale,
    String? engineId,
  }) => AppSettings(
    minAnalysisDepth: minAnalysisDepth ?? this.minAnalysisDepth,
    depth: depth ?? this.depth,
    multiPv: multiPv ?? this.multiPv,
    timeLimitSeconds: timeLimitSeconds ?? this.timeLimitSeconds,
    threads: threads ?? this.threads,
    maxThreads: maxThreads ?? this.maxThreads,
    hashMb: hashMb ?? this.hashMb,
    sidelineDepth: sidelineDepth ?? this.sidelineDepth,
    sidelineMultiPv: sidelineMultiPv ?? this.sidelineMultiPv,
    sidelineThreads: sidelineThreads ?? this.sidelineThreads,
    sidelineHashMb: sidelineHashMb ?? this.sidelineHashMb,
    showBestMoveArrow:
        showBestMoveArrow ?? showBoardArrows ?? this.showBestMoveArrow,
    showThreatArrow: showThreatArrow ?? this.showThreatArrow,
    showEvaluationBar: showEvaluationBar ?? this.showEvaluationBar,
    showEngineLines: showEngineLines ?? this.showEngineLines,
    showClassifications: showClassifications ?? this.showClassifications,
    showAccuracy: showAccuracy ?? this.showAccuracy,
    showTheory: showTheory ?? this.showTheory,
    showResultSymbols: showResultSymbols ?? this.showResultSymbols,
    adaptiveEarlyStop: adaptiveEarlyStop ?? this.adaptiveEarlyStop,
    showBoardCoordinates: showBoardCoordinates ?? this.showBoardCoordinates,
    highlightLastMove: highlightLastMove ?? this.highlightLastMove,
    highlightSelectedSquare:
        highlightSelectedSquare ?? this.highlightSelectedSquare,
    autoSyncOnline: autoSyncOnline ?? this.autoSyncOnline,
    confirmBeforeDelete: confirmBeforeDelete ?? this.confirmBeforeDelete,
    useGlobalAnalysisCache:
        useGlobalAnalysisCache ?? this.useGlobalAnalysisCache,
    diagnosticLogging: diagnosticLogging ?? this.diagnosticLogging,
    themeMode: themeMode ?? this.themeMode,
    locale: locale ?? this.locale,
    engineId: engineId ?? this.engineId,
  );
}

class FavoriteCollection {
  const FavoriteCollection({
    required this.id,
    required this.name,
    required this.gameCount,
  });

  factory FavoriteCollection.fromJson(Map<String, Object?> json) =>
      FavoriteCollection(
        id: json['id']! as String,
        name: json['name']! as String,
        gameCount: json['gameCount'] as int? ?? 0,
      );

  final String id;
  final String name;
  final int gameCount;
}

class GameSummary {
  const GameSummary({
    required this.id,
    required this.kind,
    required this.whiteName,
    required this.blackName,
    required this.result,
    required this.timeControl,
    this.whiteRating,
    this.blackRating,
    this.event = '',
    this.date = '',
    this.providerGameId,
    this.profileColor = 'unknown',
    this.openingEco,
    this.openingName,
    this.providerOutcome = 'unknown',
    this.timeControlType = 'unknown',
    this.localAccuracy,
    this.accuracy,
    this.favorite = false,
    this.favoriteCollectionId,
    this.downloaded = false,
    this.analyzed = false,
    this.termination = 'unknown',
    this.statisticsOutcome = 'unknown',
    this.endedAt = 0,
  });

  factory GameSummary.fromJson(Map<String, Object?> json) => GameSummary(
    id: json['id']! as String,
    kind: json['kind'] as String? ?? 'pgn',
    whiteName: json['whiteName']! as String,
    blackName: json['blackName']! as String,
    whiteRating: json['whiteRating'] as int?,
    blackRating: json['blackRating'] as int?,
    result: json['result']! as String,
    event: json['event'] as String? ?? '',
    date: json['date'] as String? ?? '',
    timeControl: json['timeControl'] as String? ?? '',
    providerGameId: json['providerGameId'] as String?,
    profileColor: json['profileColor'] as String? ?? 'unknown',
    openingEco: json['openingEco'] as String?,
    openingName: json['openingName'] as String?,
    providerOutcome: json['providerOutcome'] as String? ?? 'unknown',
    timeControlType: json['timeControlType'] as String? ?? 'unknown',
    localAccuracy: (json['localAccuracy'] as num?)?.toDouble(),
    accuracy: (json['accuracy'] as num?)?.toDouble(),
    favorite: json['favorite'] as bool? ?? false,
    favoriteCollectionId: json['favoriteCollectionId'] as String?,
    downloaded: json['downloaded'] as bool? ?? false,
    analyzed: json['analyzed'] as bool? ?? false,
    termination: json['termination'] as String? ?? 'unknown',
    statisticsOutcome: json['statisticsOutcome'] as String? ?? 'unknown',
    endedAt: json['endedAt'] as int? ?? 0,
  );

  final String id;
  final String kind;
  final String whiteName;
  final String blackName;
  final int? whiteRating;
  final int? blackRating;
  final String result;
  final String event;
  final String date;
  final String timeControl;
  final String? providerGameId;
  final String profileColor;
  final String? openingEco;
  final String? openingName;
  final String providerOutcome;
  final String timeControlType;
  final double? localAccuracy;
  final double? accuracy;
  final bool favorite;
  final String? favoriteCollectionId;
  final bool downloaded;
  final bool analyzed;
  final String statisticsOutcome;
  final String
  termination; // checkmate | resignation | timeout | draw | other | unknown
  final int endedAt;
}

class GameQuery {
  const GameQuery({
    this.search = '',
    this.outcome = 'all',
    this.color = 'all',
    this.openingName,
    this.statisticsOutcome = 'all',
    this.timeControls = const <String>[],
    this.sort = 'newest',
    this.month,
    this.favoriteOnly = false,
    this.applyMonth = false,
  });

  final String search;
  final String outcome;
  final String color;
  final String? openingName;
  final String statisticsOutcome;
  final List<String> timeControls;
  final String sort;
  final String? month;
  final bool favoriteOnly;
  final bool applyMonth;

  Map<String, Object?> toJson() => {
    'search': search,
    'outcome': outcome,
    'color': color,
    if (openingName != null) 'openingName': openingName,
    if (statisticsOutcome != 'all') 'statisticsOutcome': statisticsOutcome,
    'timeControls': timeControls,
    'sort': sort,
    // Only send `month` when set: the native query reads it with a string
    // default that is used solely when the key is absent, so a JSON `null`
    // would throw there instead of falling back.
    if (month != null) 'month': month,
    'favoriteOnly': favoriteOnly,
    'applyMonth': applyMonth,
  };
}

class ParsedMove {
  const ParsedMove({
    required this.plyIndex,
    required this.moveNumber,
    required this.sideToMove,
    required this.san,
    required this.uci,
    required this.fenAfter,
    this.clockMillis,
    this.positionAfter = BoardPosition.empty,
  });

  factory ParsedMove.fromJson(Map<String, Object?> json) => ParsedMove(
    plyIndex: json['plyIndex']! as int,
    moveNumber: json['moveNumber']! as int,
    sideToMove: json['sideToMove']! as String,
    san: json['san']! as String,
    uci: json['uci']! as String,
    fenAfter: json['fenAfter']! as String,
    clockMillis: json['clockMillis'] as int?,
    positionAfter: BoardPosition.fromJson(
      json['positionAfter']! as Map<String, Object?>,
    ),
  );

  final int plyIndex;
  final int moveNumber;
  final String sideToMove;
  final String san;
  final String uci;
  final String fenAfter;
  final int? clockMillis;
  final BoardPosition positionAfter;
}

class BoardPosition {
  const BoardPosition({
    required this.fen,
    required this.pieces,
    required this.sideToMove,
    required this.draggableColor,
    this.fullmoveNumber = 1,
  });

  static const initial = BoardPosition(
    fen: 'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1',
    pieces: <String>[
      'r','n','b','q','k','b','n','r',
      'p','p','p','p','p','p','p','p',
      '','','','','','','','',
      '','','','','','','','',
      '','','','','','','','',
      '','','','','','','','',
      'P','P','P','P','P','P','P','P',
      'R','N','B','Q','K','B','N','R',
    ],
    sideToMove: 'white',
    draggableColor: 'white',
    fullmoveNumber: 1,
  );

  static const empty = BoardPosition(
    fen: '8/8/8/8/8/8/8/8 w - - 0 1',
    pieces: <String>[
      '',
      '',
      '',
      '',
      '',
      '',
      '',
      '',
      '',
      '',
      '',
      '',
      '',
      '',
      '',
      '',
      '',
      '',
      '',
      '',
      '',
      '',
      '',
      '',
      '',
      '',
      '',
      '',
      '',
      '',
      '',
      '',
      '',
      '',
      '',
      '',
      '',
      '',
      '',
      '',
      '',
      '',
      '',
      '',
      '',
      '',
      '',
      '',
      '',
      '',
      '',
      '',
      '',
      '',
      '',
      '',
      '',
      '',
      '',
      '',
      '',
      '',
      '',
      '',
    ],
    sideToMove: 'white',
    draggableColor: 'white',
    fullmoveNumber: 1,
  );

  factory BoardPosition.fromJson(Map<String, Object?> json) => BoardPosition(
    fen: json['fen']! as String,
    pieces: (json['pieces']! as List<Object?>).cast<String>(),
    sideToMove: json['sideToMove']! as String,
    draggableColor: json['draggableColor']! as String,
    fullmoveNumber: json['fullmoveNumber'] as int? ?? 1,
  );

  final String fen;
  final List<String> pieces;
  final String sideToMove;
  final String draggableColor;
  final int fullmoveNumber;
}

class BoardMoveResolution {
  const BoardMoveResolution({
    required this.uci,
    required this.san,
    required this.fenAfter,
    required this.positionAfter,
    this.mainLinePly,
    this.terminal = false,
    this.checkmate = false,
    this.result = '*',
  });

  factory BoardMoveResolution.fromJson(Map<String, Object?> json) =>
      BoardMoveResolution(
        uci: json['uci']! as String,
        san: json['san']! as String,
        fenAfter: json['fenAfter']! as String,
        positionAfter: BoardPosition.fromJson(
          json['positionAfter']! as Map<String, Object?>,
        ),
        mainLinePly: json['mainLinePly'] as int?,
        terminal: json['terminal'] as bool? ?? false,
        checkmate: json['checkmate'] as bool? ?? false,
        result: json['result'] as String? ?? '*',
      );

  final String uci;
  final String san;
  final String fenAfter;
  final BoardPosition positionAfter;
  final int? mainLinePly;
  final bool terminal;
  final bool checkmate;
  final String result;
}

// -----------------------------------------------------------------------------
// Section: Local bot play DTOs
// -----------------------------------------------------------------------------

class BotGameSummary {
  const BotGameSummary({
    required this.gameId,
    required this.botElo,
    required this.playerColor,
    required this.botColor,
    required this.status,
    required this.result,
    required this.outcome,
    required this.moveCount,
    required this.createdAt,
  });

  factory BotGameSummary.fromJson(Map<String, Object?> json) => BotGameSummary(
    gameId: json['gameId']! as String,
    botElo: json['botElo']! as int,
    playerColor: json['playerColor']! as String,
    botColor: json['botColor']! as String,
    status: json['status']! as String,
    result: json['result'] as String? ?? '*',
    outcome: json['outcome'] as String? ?? 'unfinished',
    moveCount: json['moveCount'] as int? ?? 0,
    createdAt: json['createdAt'] as int? ?? 0,
  );

  final String gameId;
  final int botElo;
  final String playerColor;
  final String botColor;
  final String status;
  final String result;
  final String outcome;
  final int moveCount;
  final int createdAt;

  bool get isActive => status == 'active';
  bool get canAnalyze => !isActive && moveCount > 0;
}

class BotGameMove {
  const BotGameMove({
    required this.ply,
    required this.uci,
    required this.san,
    required this.fenAfter,
  });

  factory BotGameMove.fromJson(Map<String, Object?> json) => BotGameMove(
    ply: json['ply']! as int,
    uci: json['uci']! as String,
    san: json['san']! as String,
    fenAfter: json['fenAfter']! as String,
  );

  final int ply;
  final String uci;
  final String san;
  final String fenAfter;
}

class BotGameSession {
  const BotGameSession({
    required this.gameId,
    required this.botElo,
    required this.playerColor,
    required this.botColor,
    required this.status,
    required this.result,
    this.checkmate = false,
    required this.position,
    required this.positions,
    required this.moves,
    required this.createdAt,
    required this.showEvaluationBar,
  });

  factory BotGameSession.fromJson(Map<String, Object?> json) => BotGameSession(
    gameId: json['gameId']! as String,
    botElo: json['botElo']! as int,
    playerColor: json['playerColor']! as String,
    botColor: json['botColor']! as String,
    status: json['status']! as String,
    result: json['result'] as String? ?? '*',
    checkmate: json['checkmate'] as bool? ?? false,
    position: BoardPosition.fromJson(json['position']! as Map<String, Object?>),
    positions: (json['positions'] as List<Object?>? ?? const [])
        .cast<Map<String, Object?>>()
        .map(BoardPosition.fromJson)
        .toList(growable: false),
    moves: (json['moves'] as List<Object?>? ?? const [])
        .cast<Map<String, Object?>>()
        .map(BotGameMove.fromJson)
        .toList(growable: false),
    createdAt: json['createdAt'] as int? ?? 0,
    showEvaluationBar: json['showEvaluationBar'] as bool? ?? false,
  );

  final String gameId;
  final int botElo;
  final String playerColor;
  final String botColor;
  final String status;
  final String result;
  final bool checkmate;
  final BoardPosition position;
  final List<BoardPosition> positions;
  final List<BotGameMove> moves;
  final int createdAt;
  final bool showEvaluationBar;

  bool get isActive => status == 'active';
}

class BotMoveSnapshot {
  const BotMoveSnapshot({
    required this.jobId,
    required this.status,
    required this.engineId,
    required this.move,
    required this.bestMove,
    this.evaluationFen,
    this.evaluationCp,
    this.mateIn,
    this.wdl,
    this.postMoveEvaluationFen,
    this.postMoveEvaluationCp,
    this.postMoveMateIn,
    this.postMoveWdl,
    this.san,
    this.fenAfter,
    this.positionAfter,
    this.terminal = false,
    this.checkmate = false,
    this.result = '*',
    this.error,
  });

  factory BotMoveSnapshot.fromJson(Map<String, Object?> json) => BotMoveSnapshot(
    jobId: json['jobId']! as String,
    status: json['status']! as String,
    engineId: json['engineId']! as String,
    move: json['move'] as String? ?? '',
    bestMove: json['bestMove'] as String? ?? '',
    evaluationFen: json['evaluationFen'] as String?,
    evaluationCp: json['evaluationCp'] as int?,
    mateIn: json['mateIn'] as int?,
    wdl: json['wdl'] == null
        ? null
        : WdlScore.fromJson(json['wdl']! as Map<String, Object?>),
    postMoveEvaluationFen: json['postMoveEvaluationFen'] as String?,
    postMoveEvaluationCp: json['postMoveEvaluationCp'] as int?,
    postMoveMateIn: json['postMoveMateIn'] as int?,
    postMoveWdl: json['postMoveWdl'] == null
        ? null
        : WdlScore.fromJson(json['postMoveWdl']! as Map<String, Object?>),
    san: json['san'] as String?,
    fenAfter: json['fenAfter'] as String?,
    positionAfter: json['positionAfter'] == null
        ? null
        : BoardPosition.fromJson(json['positionAfter']! as Map<String, Object?>),
    terminal: json['terminal'] as bool? ?? false,
    checkmate: json['checkmate'] as bool? ?? false,
    result: json['result'] as String? ?? '*',
    error: json['error'] as String?,
  );

  final String jobId;
  final String status;
  final String engineId;
  final String move;
  final String bestMove;
  final String? evaluationFen;
  final int? evaluationCp;
  final int? mateIn;
  final WdlScore? wdl;
  final String? postMoveEvaluationFen;
  final int? postMoveEvaluationCp;
  final int? postMoveMateIn;
  final WdlScore? postMoveWdl;
  final String? san;
  final String? fenAfter;
  final BoardPosition? positionAfter;
  final bool terminal;
  final bool checkmate;
  final String result;
  final String? error;

  bool get isComplete => status == 'complete';
  bool get isRunning => status == 'queued' || status == 'running';
  bool get isFailed => status == 'failed';
}

class GameDetail {
  const GameDetail({
    required this.summary,
    required this.moves,
    this.startingPosition = BoardPosition.empty,
    this.outcome,
  });

  factory GameDetail.fromJson(Map<String, Object?> json) => GameDetail(
    summary: GameSummary.fromJson(json),
    moves: (json['moves'] as List<Object?>? ?? const [])
        .cast<Map<String, Object?>>()
        .map(ParsedMove.fromJson)
        .toList(growable: false),
    startingPosition: BoardPosition.fromJson(
      json['startingPosition']! as Map<String, Object?>,
    ),
    outcome: json['outcome'] == null
        ? null
        : GameOutcome.fromJson(json['outcome']! as Map<String, Object?>),
  );

  final GameSummary summary;
  final List<ParsedMove> moves;
  final BoardPosition startingPosition;
  final GameOutcome? outcome;
}

class GameOutcome {
  const GameOutcome({required this.result, required this.checkmate});

  factory GameOutcome.fromJson(Map<String, Object?> json) => GameOutcome(
    result: json['result']! as String,
    checkmate: json['checkmate']! as bool,
  );

  final String result;
  final bool checkmate;
}

enum MoveClassification {
  theory,
  forced,
  brilliant,
  critical,
  best,
  excellent,
  good,
  okay,
  miss,
  mistake,
  blunder,
  unknown;

  static MoveClassification? fromJson(String? value) => switch (value) {
    'theory' => theory,
    'forced' => forced,
    'brilliant' => brilliant,
    'critical' => critical,
    'best' => best,
    'excellent' => excellent,
    'good' => good,
    'okay' => okay,
    'miss' => miss,
    'mistake' => mistake,
    'blunder' => blunder,
    'unknown' => unknown,
    _ => null,
  };

  String? get assetPath => switch (this) {
    theory => '../img/move_book.png',
    forced => '../img/move_force.png',
    brilliant => '../img/move_brilliant.png',
    critical => null,
    best => '../img/move_best.png',
    excellent => '../img/move_excellent.png',
    good => '../img/move_okay.png',
    okay => '../img/move_okay.png',
    miss => '../img/move_miss.png',
    mistake => '../img/move_mistake.png',
    blunder => '../img/move_blunder.png',
    unknown => null,
  };
}

class PlayerAnalysisSummary {
  const PlayerAnalysisSummary({
    required this.theory,
    this.forced = 0,
    required this.brilliant,
    required this.critical,
    required this.best,
    required this.excellent,
    this.good = 0,
    required this.okay,
    required this.miss,
    required this.mistake,
    required this.blunder,
    required this.totalMoves,
    this.localAccuracy,
  });

  factory PlayerAnalysisSummary.fromJson(Map<String, Object?> json) =>
      PlayerAnalysisSummary(
        theory: json['theory']! as int,
        forced: json['forced'] as int? ?? 0,
        brilliant: json['brilliant']! as int,
        critical: json['critical'] as int? ?? 0,
        best: json['best']! as int,
        excellent: json['excellent']! as int,
        good: json['good'] as int? ?? 0,
        okay: json['okay']! as int,
        miss: json['miss']! as int,
        mistake: json['mistake']! as int,
        blunder: json['blunder']! as int,
        totalMoves: json['totalMoves']! as int,
        localAccuracy: (json['localAccuracy'] as num?)?.toDouble(),
      );

  final int theory;
  final int forced;
  final int brilliant;
  final int critical;
  final int best;
  final int excellent;
  final int good;
  final int okay;
  final int miss;
  final int mistake;
  final int blunder;
  final int totalMoves;
  final double? localAccuracy;
}

class AnalysisSummary {
  const AnalysisSummary({
    required this.profileSide,
    required this.white,
    required this.black,
    required this.classifierVersion,
    required this.accuracyAlgorithmVersion,
    required this.openingBookVersion,
    required this.engineDepth,
    required this.engineVersion,
  });

  factory AnalysisSummary.fromJson(Map<String, Object?> json) =>
      AnalysisSummary(
        profileSide: json['profileSide'] as String? ?? 'white',
        white: PlayerAnalysisSummary.fromJson(
          json['white']! as Map<String, Object?>,
        ),
        black: PlayerAnalysisSummary.fromJson(
          json['black']! as Map<String, Object?>,
        ),
        classifierVersion: json['classifierVersion']! as int,
        accuracyAlgorithmVersion: json['accuracyAlgorithmVersion']! as int,
        openingBookVersion: json['openingBookVersion'] as String? ?? '',
        engineDepth: json['engineDepth']! as int,
        engineVersion: json['engineVersion'] as String? ?? '',
      );

  final String profileSide;
  final PlayerAnalysisSummary white;
  final PlayerAnalysisSummary black;
  final int classifierVersion;
  final int accuracyAlgorithmVersion;
  final String openingBookVersion;
  final int engineDepth;
  final String engineVersion;

  PlayerAnalysisSummary get selected => profileSide == 'black' ? black : white;
}

class TheoryMoveInfo {
  const TheoryMoveInfo({
    required this.games,
  });

  factory TheoryMoveInfo.fromJson(Map<String, Object?> json) => TheoryMoveInfo(
    games: json['games']! as int,
  );

  final int games;
}

class WdlScore {
  const WdlScore({
    required this.wins,
    required this.draws,
    required this.losses,
  });

  factory WdlScore.fromJson(Map<String, Object?> json) => WdlScore(
    wins: json['wins']! as int,
    draws: json['draws']! as int,
    losses: json['losses']! as int,
  );

  final int wins;
  final int draws;
  final int losses;
}

class EngineLine {
  const EngineLine({
    required this.rank,
    required this.depth,
    required this.nodes,
    required this.moves,
    this.evaluationCp,
    this.mateIn,
    this.wdl,
    this.evaluationBarWhitePermille,
  });

  factory EngineLine.fromJson(Map<String, Object?> json) => EngineLine(
    rank: json['rank']! as int,
    depth: json['depth']! as int,
    evaluationCp: json['evaluationCp'] as int?,
    mateIn: json['mateIn'] as int?,
    evaluationBarWhitePermille: json['evaluationBarWhitePermille'] as int?,
    wdl: json['wdl'] == null
        ? null
        : WdlScore.fromJson(json['wdl']! as Map<String, Object?>),
    nodes: json['nodes']! as int,
    moves: (json['moves']! as List<Object?>).cast<String>(),
  );

  final int rank;
  final int depth;
  final int? evaluationCp;
  final int? mateIn;
  final WdlScore? wdl;
  // Native SF19-only bar projection. Null preserves the legacy SF18 cp scale.
  final int? evaluationBarWhitePermille;
  final int nodes;
  final List<String> moves;

  String get bestMove => moves.isEmpty ? '' : moves.first;
}

enum AnalysisJobState {
  queued,
  running,
  cancelling,
  cancelled,
  completed,
  failed;

  static AnalysisJobState fromJson(String? value, String legacyStatus) {
    switch (value) {
      case 'queued':
        return AnalysisJobState.queued;
      case 'running':
        return AnalysisJobState.running;
      case 'cancelling':
        return AnalysisJobState.cancelling;
      case 'cancelled':
        return AnalysisJobState.cancelled;
      case 'completed':
        return AnalysisJobState.completed;
      case 'failed':
        return AnalysisJobState.failed;
    }
    switch (legacyStatus) {
      case 'complete':
        return AnalysisJobState.completed;
      case 'cancelled':
        return AnalysisJobState.cancelled;
      case 'error':
        return AnalysisJobState.failed;
      default:
        return AnalysisJobState.running;
    }
  }
}

class AnalysisSnapshot {
  const AnalysisSnapshot({
    required this.gameId,
    required this.status,
    required this.jobState,
    required this.completedPlies,
    required this.totalPlies,
    required this.progress,
    required this.currentPly,
    this.liveDepth = 0,
    this.qualityComplete = false,
    required this.bestMove,
    required this.recommendedMove,
    required this.engineVersion,
    this.analyzedFen = '',
    required this.lines,
    this.error,
    this.summary,
    this.classification,
    this.theory,
    this.classifierVersion = 0,
  });

  factory AnalysisSnapshot.fromJson(Map<String, Object?> json) =>
      AnalysisSnapshot(
        gameId: json['gameId']! as String,
        status: json['status']! as String,
        jobState: AnalysisJobState.fromJson(
          json['jobState'] as String?,
          json['status']! as String,
        ),
        completedPlies: json['completedPlies']! as int,
        totalPlies: json['totalPlies']! as int,
        progress: (json['progress']! as num).toDouble(),
        currentPly: json['currentPly'] as int? ?? -1,
        liveDepth: json['liveDepth'] as int? ?? 0,
        qualityComplete: json['qualityComplete'] as bool? ?? false,
        bestMove: json['bestMove'] as String? ?? '',
        recommendedMove: json['recommendedMove'] as String? ?? '',
        engineVersion: json['engineVersion'] as String? ?? '',
        analyzedFen: json['analyzedFen'] as String? ?? '',
        error: json['error'] as String?,
        classification: MoveClassification.fromJson(
          json['classification'] as String?,
        ),
        classifierVersion: json['classifierVersion'] as int? ?? 0,
        theory: json['theory'] == null
            ? null
            : TheoryMoveInfo.fromJson(json['theory']! as Map<String, Object?>),
        lines: (json['lines'] as List<Object?>? ?? const [])
            .cast<Map<String, Object?>>()
            .map(EngineLine.fromJson)
            .toList(growable: false),
        summary: json['summary'] == null
            ? null
            : AnalysisSummary.fromJson(
                json['summary']! as Map<String, Object?>,
              ),
      );

  final String gameId;
  final String status;
  final AnalysisJobState jobState;
  final int completedPlies;
  final int totalPlies;
  final double progress;
  final int currentPly;
  final int liveDepth;
  final bool qualityComplete;
  final String bestMove;
  final String recommendedMove;
  final String engineVersion;
  final String analyzedFen;
  final String? error;
  final List<EngineLine> lines;
  final AnalysisSummary? summary;
  final MoveClassification? classification;
  final int classifierVersion;
  final TheoryMoveInfo? theory;

  bool get isComplete => jobState == AnalysisJobState.completed;
  bool get isRunning => jobState == AnalysisJobState.running;
  bool get isQueued => jobState == AnalysisJobState.queued;
  bool get isCancelling => jobState == AnalysisJobState.cancelling;
  bool get isCancelled => jobState == AnalysisJobState.cancelled;
  bool get isFailed => jobState == AnalysisJobState.failed;
}

class VariationAnalysisSnapshot {
  const VariationAnalysisSnapshot({
    required this.jobId,
    required this.status,
    required this.playedMove,
    required this.playedSan,
    required this.fen,
    this.position = BoardPosition.empty,
    required this.bestMove,
    required this.lines,
    this.engineVersion = '',
    this.liveDepth = 0,
    this.moverEvaluationCp,
    this.moverMateIn,
    this.classification,
    this.error,
  });

  factory VariationAnalysisSnapshot.fromJson(Map<String, Object?> json) =>
      VariationAnalysisSnapshot(
        jobId: json['jobId'] as String? ?? '',
        status: json['status'] as String? ?? 'error',
        playedMove: json['playedMove'] as String? ?? '',
        playedSan: json['playedSan'] as String? ?? '',
        fen: json['fen'] as String? ?? '',
        position: BoardPosition.fromJson(
          json['position']! as Map<String, Object?>,
        ),
        bestMove: json['bestMove'] as String? ?? '',
        engineVersion: json['engineVersion'] as String? ?? '',
        liveDepth: json['liveDepth'] as int? ?? 0,
        moverEvaluationCp: json['moverEvaluationCp'] as int?,
        moverMateIn: json['moverMateIn'] as int?,
        classification: MoveClassification.fromJson(
          json['classification'] as String?,
        ),
        error: json['error'] as String?,
        lines: (json['lines'] as List<Object?>? ?? const [])
            .cast<Map<String, Object?>>()
            .map(EngineLine.fromJson)
            .toList(growable: false),
      );

  final String jobId;
  final String status;
  final String playedMove;
  final String playedSan;
  final String fen;
  final BoardPosition position;
  final String bestMove;
  final String engineVersion;
  final int liveDepth;
  final int? moverEvaluationCp;
  final int? moverMateIn;
  final MoveClassification? classification;
  final String? error;
  final List<EngineLine> lines;

  bool get isRunning => status == 'running';
  bool get isComplete => status == 'complete';
  bool get isPaused => status == 'paused';

  VariationAnalysisSnapshot copyWith({
    String? status,
    String? bestMove,
    String? engineVersion,
    int? liveDepth,
    int? moverEvaluationCp,
    int? moverMateIn,
    MoveClassification? classification,
    String? error,
    List<EngineLine>? lines,
  }) => VariationAnalysisSnapshot(
    jobId: jobId,
    status: status ?? this.status,
    playedMove: playedMove,
    playedSan: playedSan,
    fen: fen,
    position: position,
    bestMove: bestMove ?? this.bestMove,
    engineVersion: engineVersion ?? this.engineVersion,
    liveDepth: liveDepth ?? this.liveDepth,
    moverEvaluationCp: moverEvaluationCp ?? this.moverEvaluationCp,
    moverMateIn: moverMateIn ?? this.moverMateIn,
    classification: classification ?? this.classification,
    error: error ?? this.error,
    lines: lines ?? this.lines,
  );
}
