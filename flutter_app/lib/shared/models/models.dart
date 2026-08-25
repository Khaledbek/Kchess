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
    this.joinedAt,
    this.lastOnlineAt,
    this.country,
    this.location,
    this.publicUrl,
    this.followers,
    this.fide,
    this.providerGames,
    this.providerWins,
    this.providerLosses,
    this.providerDraws,
    this.playTimeSeconds,
    this.providerStatus,
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
    joinedAt: json['joinedAt'] as int?,
    lastOnlineAt: json['lastOnlineAt'] as int?,
    country: json['country'] as String?,
    location: json['location'] as String?,
    publicUrl: json['publicUrl'] as String?,
    followers: json['followers'] as int?,
    fide: json['fide'] as int?,
    providerGames: json['providerGames'] as int?,
    providerWins: json['providerWins'] as int?,
    providerLosses: json['providerLosses'] as int?,
    providerDraws: json['providerDraws'] as int?,
    playTimeSeconds: json['playTimeSeconds'] as int?,
    providerStatus: json['providerStatus'] as String?,
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
  final int? joinedAt;
  final int? lastOnlineAt;
  final String? country;
  final String? location;
  final String? publicUrl;
  final int? followers;
  final int? fide;
  final int? providerGames;
  final int? providerWins;
  final int? providerLosses;
  final int? providerDraws;
  final int? playTimeSeconds;
  final String? providerStatus;
  final bool providerDisabled;
}

class ProviderPerformance {
  const ProviderPerformance({
    required this.key,
    this.currentRating,
    this.bestRating,
    this.lowestRating,
    this.ratingProgress,
    this.games,
    this.wins,
    this.losses,
    this.draws,
    this.winRate,
    this.lossRate,
    this.drawRate,
  });

  factory ProviderPerformance.fromJson(Map<String, Object?> json) =>
      ProviderPerformance(
        key: json['key']! as String,
        currentRating: json['currentRating'] as int?,
        bestRating: json['bestRating'] as int?,
        lowestRating: json['lowestRating'] as int?,
        ratingProgress: json['ratingProgress'] as int?,
        games: json['games'] as int?,
        wins: json['wins'] as int?,
        losses: json['losses'] as int?,
        draws: json['draws'] as int?,
        winRate: (json['winRate'] as num?)?.toDouble(),
        lossRate: (json['lossRate'] as num?)?.toDouble(),
        drawRate: (json['drawRate'] as num?)?.toDouble(),
      );

  final String key;
  final int? currentRating;
  final int? bestRating;
  final int? lowestRating;
  final int? ratingProgress;
  final int? games;
  final int? wins;
  final int? losses;
  final int? draws;
  final double? winRate;
  final double? lossRate;
  final double? drawRate;
}

class ProviderOverview {
  const ProviderOverview({
    required this.profile,
    required this.stats,
    required this.availableMonths,
    required this.offlineReady,
    required this.retryAfterSeconds,
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
        offlineReady: json['offlineReady'] as bool? ?? false,
        retryAfterSeconds: json['retryAfterSeconds'] as int? ?? 0,
      );

  final AppProfile profile;
  final List<ProviderPerformance> stats;
  final List<String> availableMonths;
  final bool offlineReady;
  final int retryAfterSeconds;
}

/// A win/draw/loss tally from the profile's perspective, with derived rates.
/// [winRate] and [scorePercent] are null when no game has a decided result.
class StatTally {
  const StatTally({
    this.games = 0,
    this.wins = 0,
    this.draws = 0,
    this.losses = 0,
    this.undecided = 0,
    this.winRate,
    this.scorePercent,
  });

  factory StatTally.fromJson(Map<String, Object?> json) => StatTally(
    games: json['games'] as int? ?? 0,
    wins: json['wins'] as int? ?? 0,
    draws: json['draws'] as int? ?? 0,
    losses: json['losses'] as int? ?? 0,
    undecided: json['undecided'] as int? ?? 0,
    winRate: (json['winRate'] as num?)?.toDouble(),
    scorePercent: (json['scorePercent'] as num?)?.toDouble(),
  );

  final int games;
  final int wins;
  final int draws;
  final int losses;
  final int undecided;
  final double? winRate;
  final double? scorePercent;

  int get decided => wins + draws + losses;
}

/// One time-control bucket (bullet, blitz, rapid, ...) with its tally.
class StatTimeControl {
  const StatTimeControl({required this.type, required this.tally});

  factory StatTimeControl.fromJson(Map<String, Object?> json) => StatTimeControl(
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
    this.recentForm = const [],
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
      recentForm: (json['recentForm'] as List<Object?>? ?? const [])
          .cast<String>(),
    );
  }

  final bool hasProfile;
  final int totalGames;
  final StatTally overall;
  final StatTally white;
  final StatTally black;
  final List<StatTimeControl> byTimeControl;
  final List<String> recentForm;

  bool get isEmpty => totalGames == 0;
}

/// Performance in one named opening playing a given color, from the profile's
/// perspective.
class OpeningStat {
  const OpeningStat({
    required this.eco,
    required this.name,
    required this.color,
    required this.tally,
  });

  factory OpeningStat.fromJson(Map<String, Object?> json) => OpeningStat(
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

/// Win-rate-by-opening for the active profile, most played first.
class OpeningsStats {
  const OpeningsStats({
    this.hasProfile = false,
    this.gamesWithOpening = 0,
    this.gamesWithoutOpening = 0,
    this.distinctOpenings = 0,
    this.openings = const [],
  });

  factory OpeningsStats.fromJson(Map<String, Object?> json) => OpeningsStats(
    hasProfile: json['hasProfile'] as bool? ?? false,
    gamesWithOpening: json['gamesWithOpening'] as int? ?? 0,
    gamesWithoutOpening: json['gamesWithoutOpening'] as int? ?? 0,
    distinctOpenings: json['distinctOpenings'] as int? ?? 0,
    openings: (json['openings'] as List<Object?>? ?? const [])
        .cast<Map<String, Object?>>()
        .map(OpeningStat.fromJson)
        .toList(growable: false),
  );

  final bool hasProfile;
  final int gamesWithOpening;
  final int gamesWithoutOpening;
  final int distinctOpenings;
  final List<OpeningStat> openings;

  bool get isEmpty => gamesWithOpening == 0;
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
    this.hashMb = 128,
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
  });

  factory AppSettings.fromJson(Map<String, Object?> json) => AppSettings(
    minAnalysisDepth: json['minAnalysisDepth'] as int? ?? 12,
    depth: (json['maxAnalysisDepth'] as int?) ?? (json['depth'] as int?) ?? 18,
    multiPv: json['multiPv'] as int? ?? 3,
    timeLimitSeconds: json['timeLimitSeconds'] as int? ?? 0,
    threads: json['threads'] as int? ?? 2,
    hashMb: json['hashMb'] as int? ?? 128,
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
  );

  final int minAnalysisDepth;
  final int depth;
  final int multiPv;
  final int timeLimitSeconds;
  final int threads;
  final int hashMb;
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

  // Compatibility alias for older widgets/tests while the setting is now
  // presented to users as the Best Move Arrow.
  bool get showBoardArrows => showBestMoveArrow;

  AppSettings copyWith({
    int? minAnalysisDepth,
    int? depth,
    int? multiPv,
    int? timeLimitSeconds,
    int? threads,
    int? hashMb,
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
  }) => AppSettings(
    minAnalysisDepth: minAnalysisDepth ?? this.minAnalysisDepth,
    depth: depth ?? this.depth,
    multiPv: multiPv ?? this.multiPv,
    timeLimitSeconds: timeLimitSeconds ?? this.timeLimitSeconds,
    threads: threads ?? this.threads,
    hashMb: hashMb ?? this.hashMb,
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
  );
}

class FavoriteCollection {
  const FavoriteCollection({
    required this.id,
    required this.name,
    required this.gameCount,
    required this.createdAt,
  });

  factory FavoriteCollection.fromJson(Map<String, Object?> json) => FavoriteCollection(
    id: json['id']! as String,
    name: json['name']! as String,
    gameCount: json['gameCount'] as int? ?? 0,
    createdAt: json['createdAt'] as int? ?? 0,
  );

  final String id;
  final String name;
  final int gameCount;
  final int createdAt;
}

class GameSummary {
  const GameSummary({
    required this.id,
    required this.kind,
    required this.whiteName,
    required this.blackName,
    required this.result,
    required this.timeControl,
    required this.startingFen,
    this.whiteRating,
    this.blackRating,
    this.event = '',
    this.date = '',
    this.isFixture = false,
    this.providerGameId,
    this.providerUrl,
    this.profileColor = 'unknown',
    this.openingEco,
    this.openingName,
    this.openingPly,
    this.providerOutcome = 'unknown',
    this.timeControlType = 'unknown',
    this.providerAccuracy,
    this.localAccuracy,
    this.accuracy,
    this.accuracySource = 'none',
    this.favorite = false,
    this.favoriteCollectionId,
    this.downloaded = false,
    this.analyzed = false,
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
    startingFen: json['startingFen'] as String? ?? '',
    isFixture: json['isFixture'] as bool? ?? false,
    providerGameId: json['providerGameId'] as String?,
    providerUrl: json['providerUrl'] as String?,
    profileColor: json['profileColor'] as String? ?? 'unknown',
    openingEco: json['openingEco'] as String?,
    openingName: json['openingName'] as String?,
    openingPly: json['openingPly'] as int?,
    providerOutcome: json['providerOutcome'] as String? ?? 'unknown',
    timeControlType: json['timeControlType'] as String? ?? 'unknown',
    providerAccuracy: (json['providerAccuracy'] as num?)?.toDouble(),
    localAccuracy: (json['localAccuracy'] as num?)?.toDouble(),
    accuracy: (json['accuracy'] as num?)?.toDouble(),
    accuracySource: json['accuracySource'] as String? ?? 'none',
    favorite: json['favorite'] as bool? ?? false,
    favoriteCollectionId: json['favoriteCollectionId'] as String?,
    downloaded: json['downloaded'] as bool? ?? false,
    analyzed: json['analyzed'] as bool? ?? false,
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
  final String startingFen;
  final bool isFixture;
  final String? providerGameId;
  final String? providerUrl;
  final String profileColor;
  final String? openingEco;
  final String? openingName;
  final int? openingPly;
  final String providerOutcome;
  final String timeControlType;
  final double? providerAccuracy;
  final double? localAccuracy;
  final double? accuracy;
  final String accuracySource;
  final bool favorite;
  final String? favoriteCollectionId;
  final bool downloaded;
  final bool analyzed;
  final int endedAt;
}

class ParsedMove {
  const ParsedMove({
    required this.plyIndex,
    required this.moveNumber,
    required this.sideToMove,
    required this.san,
    required this.uci,
    required this.fenBefore,
    required this.fenAfter,
  });

  factory ParsedMove.fromJson(Map<String, Object?> json) => ParsedMove(
    plyIndex: json['plyIndex']! as int,
    moveNumber: json['moveNumber']! as int,
    sideToMove: json['sideToMove']! as String,
    san: json['san']! as String,
    uci: json['uci']! as String,
    fenBefore: json['fenBefore']! as String,
    fenAfter: json['fenAfter']! as String,
  );

  final int plyIndex;
  final int moveNumber;
  final String sideToMove;
  final String san;
  final String uci;
  final String fenBefore;
  final String fenAfter;
}

class GameDetail {
  const GameDetail({
    required this.summary,
    required this.pgn,
    required this.moves,
  });

  factory GameDetail.fromJson(Map<String, Object?> json) => GameDetail(
    summary: GameSummary.fromJson(json),
    pgn: json['pgn'] as String? ?? '',
    moves: (json['moves'] as List<Object?>? ?? const [])
        .cast<Map<String, Object?>>()
        .map(ParsedMove.fromJson)
        .toList(growable: false),
  );

  final GameSummary summary;
  final String pgn;
  final List<ParsedMove> moves;
}

enum MoveClassification {
  theory,
  brilliant,
  critical,
  best,
  excellent,
  okay,
  miss,
  mistake,
  blunder,
  unknown;

  static MoveClassification? fromJson(String? value) => switch (value) {
    'theory' => theory,
    'brilliant' => brilliant,
    'critical' => critical,
    'best' => best,
    'excellent' => excellent,
    'okay' => okay,
    'miss' => miss,
    'mistake' => mistake,
    'blunder' => blunder,
    'unknown' => unknown,
    _ => null,
  };

  String? get assetPath => switch (this) {
    theory => '../img/move_book.png',
    brilliant => '../img/move_brilliant.png',
    critical => null,
    best => '../img/move_best.png',
    excellent => '../img/move_excellent.png',
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
    required this.brilliant,
    required this.critical,
    required this.best,
    required this.excellent,
    required this.okay,
    required this.miss,
    required this.mistake,
    required this.blunder,
    required this.totalMoves,
    required this.analyzedMoves,
    this.localAccuracy,
  });

  factory PlayerAnalysisSummary.fromJson(Map<String, Object?> json) =>
      PlayerAnalysisSummary(
        theory: json['theory']! as int,
        brilliant: json['brilliant']! as int,
        critical: json['critical'] as int? ?? 0,
        best: json['best']! as int,
        excellent: json['excellent']! as int,
        okay: json['okay']! as int,
        miss: json['miss']! as int,
        mistake: json['mistake']! as int,
        blunder: json['blunder']! as int,
        totalMoves: json['totalMoves']! as int,
        analyzedMoves: json['analyzedMoves']! as int,
        localAccuracy: (json['localAccuracy'] as num?)?.toDouble(),
      );

  final int theory;
  final int brilliant;
  final int critical;
  final int best;
  final int excellent;
  final int okay;
  final int miss;
  final int mistake;
  final int blunder;
  final int totalMoves;
  final int analyzedMoves;
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
    required this.whiteWins,
    required this.draws,
    required this.blackWins,
  });

  factory TheoryMoveInfo.fromJson(Map<String, Object?> json) => TheoryMoveInfo(
    games: json['games']! as int,
    whiteWins: json['whiteWins']! as int,
    draws: json['draws']! as int,
    blackWins: json['blackWins']! as int,
  );

  final int games;
  final int whiteWins;
  final int draws;
  final int blackWins;
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
  });

  factory EngineLine.fromJson(Map<String, Object?> json) => EngineLine(
    rank: json['rank']! as int,
    depth: json['depth']! as int,
    evaluationCp: json['evaluationCp'] as int?,
    mateIn: json['mateIn'] as int?,
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
      case 'queued': return AnalysisJobState.queued;
      case 'running': return AnalysisJobState.running;
      case 'cancelling': return AnalysisJobState.cancelling;
      case 'cancelled': return AnalysisJobState.cancelled;
      case 'completed': return AnalysisJobState.completed;
      case 'failed': return AnalysisJobState.failed;
    }
    switch (legacyStatus) {
      case 'complete': return AnalysisJobState.completed;
      case 'cancelled': return AnalysisJobState.cancelled;
      case 'error': return AnalysisJobState.failed;
      default: return AnalysisJobState.running;
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
    required this.configHash,
    required this.lines,
    this.error,
    this.summary,
    this.classification,
    this.expectedScoreBefore,
    this.expectedScoreBest,
    this.expectedScorePlayed,
    this.expectedScoreLoss,
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
        configHash: json['configHash'] as String? ?? '',
        error: json['error'] as String?,
        classification: MoveClassification.fromJson(
          json['classification'] as String?,
        ),
        classifierVersion: json['classifierVersion'] as int? ?? 0,
        expectedScoreBefore: (json['expectedScoreBefore'] as num?)?.toDouble(),
        expectedScoreBest: (json['expectedScoreBest'] as num?)?.toDouble(),
        expectedScorePlayed: (json['expectedScorePlayed'] as num?)?.toDouble(),
        expectedScoreLoss: (json['expectedScoreLoss'] as num?)?.toDouble(),
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
  final String configHash;
  final String? error;
  final List<EngineLine> lines;
  final AnalysisSummary? summary;
  final MoveClassification? classification;
  final int classifierVersion;
  final double? expectedScoreBefore;
  final double? expectedScoreBest;
  final double? expectedScorePlayed;
  final double? expectedScoreLoss;
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
    required this.bestMove,
    required this.lines,
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
        bestMove: json['bestMove'] as String? ?? '',
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
  final String bestMove;
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
    bestMove: bestMove ?? this.bestMove,
    liveDepth: liveDepth ?? this.liveDepth,
    moverEvaluationCp: moverEvaluationCp ?? this.moverEvaluationCp,
    moverMateIn: moverMateIn ?? this.moverMateIn,
    classification: classification ?? this.classification,
    error: error ?? this.error,
    lines: lines ?? this.lines,
  );
}
