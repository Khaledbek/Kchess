// -----------------------------------------------------------------------------
// Coach UI models
// -----------------------------------------------------------------------------

/// Presentation-only response depth. Native code decides what this means for
/// planning, token budgets and provider execution.
enum CoachResponseDepth { concise, balanced, deep }

/// Navigation/integration origin only. Native C++ decides chess semantics.
enum CoachSurface {
  general,
  analysis,
  freeBoard,
  pgn,
  play,
  training,
  opening,
  profile,
}

extension CoachSurfaceWire on CoachSurface {
  String get wireName => switch (this) {
    CoachSurface.general => 'general',
    CoachSurface.analysis => 'analysis',
    CoachSurface.freeBoard => 'free_board',
    CoachSurface.pgn => 'pgn',
    CoachSurface.play => 'play',
    CoachSurface.training => 'training',
    CoachSurface.opening => 'opening',
    CoachSurface.profile => 'profile',
  };
}

enum CoachMessageRole { user, coach }


class CoachClientAction {
  const CoachClientAction({required this.id, this.elo, this.color});

  final String id;
  final int? elo;
  final String? color;

  factory CoachClientAction.fromJson(Map<String, Object?> json) =>
      CoachClientAction(
        id: (json['id'] as String? ?? '').trim(),
        elo: json['elo'] as int?,
        color: (json['color'] as String?)?.trim(),
      );

  static List<CoachClientAction> fromReply(Map<String, Object?> reply) =>
      (reply['clientActions'] as List<Object?>? ?? const <Object?>[])
          .whereType<Map>()
          .map(
            (value) => CoachClientAction.fromJson(
              value.map((key, value) => MapEntry(key.toString(), value)),
            ),
          )
          .where((action) => action.id.isNotEmpty)
          .toList(growable: false);
}

class CoachUiMessage {
  const CoachUiMessage({
    required this.role,
    required this.text,
    this.question = '',
    this.eventKind = '',
    this.positionFen,
    this.boardMoves = const [],
    this.focusSquares = const [],
  });

  final CoachMessageRole role;
  final String text;
  final String question;
  final String eventKind;
  final String? positionFen;
  final List<String> boardMoves;
  final List<String> focusSquares;

  factory CoachUiMessage.fromReply(Map<String, Object?> reply) =>
      CoachUiMessage(
        role: CoachMessageRole.coach,
        text: (reply['answer'] as String? ?? '').trim(),
        question: (reply['followUpQuestion'] as String? ?? '').trim(),
        eventKind: reply['eventKind'] as String? ?? '',
        positionFen: reply['positionFen'] as String?,
        boardMoves: (reply['boardMoves'] as List<Object?>? ?? const [])
            .whereType<String>()
            .toList(growable: false),
        focusSquares: (reply['focusSquares'] as List<Object?>? ?? const [])
            .whereType<String>()
            .toList(growable: false),
      );
}

class CoachSessionSummary {
  const CoachSessionSummary({
    required this.id,
    required this.sessionNumber,
    required this.name,
    required this.createdAt,
    required this.updatedAt,
    required this.lastOpenedAt,
  });

  final String id;
  final int sessionNumber;
  final String name;
  final DateTime createdAt;
  final DateTime updatedAt;
  final DateTime lastOpenedAt;

  factory CoachSessionSummary.fromJson(Map<String, Object?> json) =>
      CoachSessionSummary(
        id: (json['id'] as String? ?? '').trim(),
        sessionNumber: (json['sessionNumber'] as num? ?? 0).toInt(),
        name: (json['name'] as String? ?? '').trim(),
        createdAt: _dateFromUnix(json['createdAt']),
        updatedAt: _dateFromUnix(json['updatedAt']),
        lastOpenedAt: _dateFromUnix(json['lastOpenedAt']),
      );
}

class CoachSessionTranscriptMessage {
  const CoachSessionTranscriptMessage({
    required this.sequence,
    required this.role,
    required this.content,
    required this.payload,
    required this.automaticTurn,
    required this.createdAt,
  });

  final int sequence;
  final String role;
  final String content;
  final Map<String, Object?> payload;
  final bool automaticTurn;
  final DateTime createdAt;

  factory CoachSessionTranscriptMessage.fromJson(Map<String, Object?> json) {
    final rawPayload = json['payload'];
    return CoachSessionTranscriptMessage(
      sequence: (json['sequence'] as num? ?? 0).toInt(),
      role: (json['role'] as String? ?? '').trim(),
      content: json['content'] as String? ?? '',
      payload: rawPayload is Map
          ? rawPayload.map((key, value) => MapEntry(key.toString(), value))
          : const <String, Object?>{},
      automaticTurn: json['automaticTurn'] as bool? ?? false,
      createdAt: _dateFromUnix(json['createdAt']),
    );
  }
}

DateTime _dateFromUnix(Object? value) {
  final seconds = (value as num? ?? 0).toInt();
  return DateTime.fromMillisecondsSinceEpoch(seconds * 1000, isUtc: true)
      .toLocal();
}
