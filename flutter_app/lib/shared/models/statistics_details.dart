part of 'models.dart';

// -----------------------------------------------------------------------------
// Section: Native statistics detail DTOs
// -----------------------------------------------------------------------------

class TerminationSpotlight {
  const TerminationSpotlight({
    required this.termination,
    required this.sharePercent,
    required this.lossPercent,
    required this.costly,
  });
  factory TerminationSpotlight.fromJson(Map<String, Object?> json) =>
      TerminationSpotlight(
        termination: GameTermination.fromJson(json),
        sharePercent: json['sharePercent'] as int,
        lossPercent: json['lossPercent'] as int,
        costly: json['costly'] as bool,
      );
  final GameTermination termination;
  final int sharePercent;
  final int lossPercent;
  final bool costly;
}

class StatisticsStreak {
  const StatisticsStreak({required this.outcome, required this.length});
  factory StatisticsStreak.fromJson(Map<String, Object?> json) =>
      StatisticsStreak(
        outcome: json['outcome'] as String,
        length: json['length'] as int,
      );
  final String outcome;
  final int length;
}

class StatisticsRatingPoint {
  const StatisticsRatingPoint({required this.endedAt, required this.rating});
  factory StatisticsRatingPoint.fromJson(Map<String, Object?> json) =>
      StatisticsRatingPoint(
        endedAt: json['endedAt'] as int,
        rating: json['rating'] as int,
      );
  final int endedAt;
  final int rating;
}

class StatisticsRatingSeries {
  const StatisticsRatingSeries({
    required this.timeControl,
    required this.points,
    required this.currentRating,
  });
  factory StatisticsRatingSeries.fromJson(Map<String, Object?> json) =>
      StatisticsRatingSeries(
        timeControl: json['timeControl'] as String,
        currentRating: json['currentRating'] as int,
        points: (json['points'] as List<Object?>)
            .cast<Map<String, Object?>>()
            .map(StatisticsRatingPoint.fromJson)
            .toList(growable: false),
      );
  final String timeControl;
  final int currentRating;
  final List<StatisticsRatingPoint> points;
}

class StatisticsTimeline {
  const StatisticsTimeline({
    this.recentGames = const [],
    this.streak,
    this.ratingSeries = const [],
  });
  factory StatisticsTimeline.fromJson(Map<String, Object?> json) =>
      StatisticsTimeline(
        recentGames: (json['recentGames'] as List<Object?>? ?? const [])
            .cast<Map<String, Object?>>()
            .map(GameSummary.fromJson)
            .toList(growable: false),
        streak: json['streak'] == null
            ? null
            : StatisticsStreak.fromJson(
                json['streak']! as Map<String, Object?>,
              ),
        ratingSeries: (json['ratingSeries'] as List<Object?>? ?? const [])
            .cast<Map<String, Object?>>()
            .map(StatisticsRatingSeries.fromJson)
            .toList(growable: false),
      );
  final List<GameSummary> recentGames;
  final StatisticsStreak? streak;
  final List<StatisticsRatingSeries> ratingSeries;
}

// -----------------------------------------------------------------------------
// Section: Native player comparison and recommendations
// -----------------------------------------------------------------------------

class OpeningMatchup {
  const OpeningMatchup({
    required this.family,
    required this.opponent,
    required this.opponentColor,
    required this.exploitable,
  });
  factory OpeningMatchup.fromJson(Map<String, Object?> json) => OpeningMatchup(
    family: OpeningFamily.fromJson(json['family']! as Map<String, Object?>),
    opponent: StatTally.fromJson(json['opponent']! as Map<String, Object?>),
    opponentColor: json['opponentColor'] as String,
    exploitable: json['exploitable'] as bool,
  );
  final OpeningFamily family;
  final StatTally opponent;
  final String opponentColor;
  final bool exploitable;
}

class PlayerComparison {
  const PlayerComparison({
    this.profileId = '',
    this.isSelf = false,
    this.headToHead = const StatTally(),
    this.userOverview = const StatisticsOverview(),
    this.userOpenings = const OpeningsStats(),
    this.userTerminations = const TerminationStats(),
    this.userFlagRate,
    this.opponentFlagRate,
    this.matchups = const [],
    this.recommendations = const [],
    this.weaknesses = const [],
  });
  factory PlayerComparison.fromJson(Map<String, Object?> json) {
    Map<String, Object?> object(String key) =>
        json[key] as Map<String, Object?>? ?? const {};
    List<OpeningMatchup> matchups(String key) =>
        (json[key] as List<Object?>? ?? const [])
            .cast<Map<String, Object?>>()
            .map(OpeningMatchup.fromJson)
            .toList(growable: false);
    return PlayerComparison(
      profileId: json['profileId'] as String? ?? '',
      isSelf: json['isSelf'] as bool? ?? false,
      headToHead: StatTally.fromJson(object('headToHead')),
      userOverview: StatisticsOverview.fromJson(object('userOverview')),
      userOpenings: OpeningsStats.fromJson(object('userOpenings')),
      userTerminations: TerminationStats.fromJson(object('userTerminations')),
      userFlagRate: (json['userFlagRate'] as num?)?.toDouble(),
      opponentFlagRate: (json['opponentFlagRate'] as num?)?.toDouble(),
      matchups: matchups('matchups'),
      recommendations: matchups('recommendations'),
      weaknesses: (json['weaknesses'] as List<Object?>? ?? const [])
          .cast<Map<String, Object?>>()
          .map(OpeningFamily.fromJson)
          .toList(growable: false),
    );
  }
  final String profileId;
  final bool isSelf;
  final StatTally headToHead;
  final StatisticsOverview userOverview;
  final OpeningsStats userOpenings;
  final TerminationStats userTerminations;
  final double? userFlagRate;
  final double? opponentFlagRate;
  final List<OpeningMatchup> matchups;
  final List<OpeningMatchup> recommendations;
  final List<OpeningFamily> weaknesses;
}
