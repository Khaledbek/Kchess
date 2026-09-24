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
