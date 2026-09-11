/// One study from Kling & Horwitz, *Chess Studies, Or, Endings of Games*
/// (London, 1851) — public domain.
///
/// A study is a fixed position rather than a generated one, so unlike
/// [EndgameDrill] it carries no level shape: the difficulty is a property of
/// the position itself.
class EndgameStudy {
  const EndgameStudy({
    required this.id,
    required this.fen,
    required this.solverColor,
    required this.difficulty,
  });

  factory EndgameStudy.fromJson(Map<String, Object?> json) => EndgameStudy(
    id: json['id']! as String,
    fen: json['fen']! as String,
    solverColor: json['solverColor'] as String? ?? 'white',
    difficulty: json['difficulty'] as String? ?? 'intermediate',
  );

  final String id;
  final String fen;

  /// `white` or `black` — the side the user plays, and the side to move.
  /// `native/tests/training_board_tests.cpp` asserts the FEN agrees.
  final String solverColor;

  /// One of `DrillDifficulties`.
  final String difficulty;

  /// 1-based position within its section, taken from the id suffix.
  int get number => int.tryParse(id.split('_').last) ?? 0;
}

/// One of the eleven material-based sections.
class EndgameStudySection {
  const EndgameStudySection({
    required this.id,
    required this.title,
    required this.description,
    required this.studies,
  });

  factory EndgameStudySection.fromJson(Map<String, Object?> json) =>
      EndgameStudySection(
        id: json['id']! as String,
        title: json['title']! as String,
        description: json['description'] as String? ?? '',
        studies: (json['studies'] as List<Object?>? ?? const [])
            .cast<Map<String, Object?>>()
            .map(EndgameStudy.fromJson)
            .toList(growable: false),
      );

  final String id;
  final String title;
  final String description;
  final List<EndgameStudy> studies;

  int countOf(String difficulty) =>
      studies.where((study) => study.difficulty == difficulty).length;
}

/// The whole catalogue plus where it came from, so the source can be credited
/// in the UI rather than buried in a comment.
class EndgameStudyCatalogue {
  const EndgameStudyCatalogue({
    required this.sections,
    required this.sourceTitle,
    required this.sourceAuthors,
    required this.sourceYear,
  });

  factory EndgameStudyCatalogue.fromJson(Map<String, Object?> json) {
    final source = json['source'] as Map<String, Object?>? ?? const {};
    return EndgameStudyCatalogue(
      sections: (json['sections'] as List<Object?>? ?? const [])
          .cast<Map<String, Object?>>()
          .map(EndgameStudySection.fromJson)
          .toList(growable: false),
      sourceTitle: source['title'] as String? ?? '',
      sourceAuthors: source['authors'] as String? ?? '',
      sourceYear: source['year'] as int? ?? 0,
    );
  }

  static const empty = EndgameStudyCatalogue(
    sections: [],
    sourceTitle: '',
    sourceAuthors: '',
    sourceYear: 0,
  );

  final List<EndgameStudySection> sections;
  final String sourceTitle;
  final String sourceAuthors;
  final int sourceYear;

  int get studyCount =>
      sections.fold(0, (sum, section) => sum + section.studies.length);

  EndgameStudySection? sectionById(String id) {
    for (final section in sections) {
      if (section.id == id) return section;
    }
    return null;
  }
}
