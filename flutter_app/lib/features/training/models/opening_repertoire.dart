/// Which side of the board a repertoire line is trained from.
///
/// The rest of the app — [OpeningTrainingRequest], `BoardPosition.sideToMove`
/// and the C ABI — passes the side as a `white` / `black` string, so [wire]
/// and [fromWire] are the only places that translation happens.
enum PieceColor {
  white,
  black;

  /// Parses the `white` / `black` wire format; null for anything else,
  /// including the `unknown` a statistics row can carry.
  static PieceColor? fromWire(String value) => switch (value) {
    'white' => PieceColor.white,
    'black' => PieceColor.black,
    _ => null,
  };

  bool get isWhite => this == PieceColor.white;

  /// The string form used across the app and the native boundary.
  String get wire => name;

  PieceColor get opponent =>
      this == PieceColor.white ? PieceColor.black : PieceColor.white;
}

/// One half-move of a repertoire line.
///
/// [san] is inert data: the player looks it up in the move list the core
/// returns for the current FEN, so nothing here decides what is legal.
class OpeningMove {
  const OpeningMove(this.san, {this.explanation});

  /// Standard algebraic notation as the core spells it, e.g. `Bf5`, `cxd5`,
  /// `O-O`. Must match `AppliedMove.san` exactly.
  final String san;

  /// ARB key of the strategic idea shown once the move is reached, or null
  /// when the move needs no comment.
  ///
  /// A key rather than the text itself, because the concept sentences are UI
  /// prose and have to exist in de/en/ar like every other string; resolve it
  /// with `openingRepertoireText`.
  final String? explanation;
}

/// One repertoire line: a named opening plus the mainline moves that make it.
///
/// [moves] is the complete line from White's first move, whichever side the
/// user trains — [isPlayerMove] decides who owns each ply, so a Black
/// repertoire simply starts with an opponent move.
class OpeningLine {
  const OpeningLine({
    required this.id,
    required this.name,
    required this.eco,
    required this.playerColor,
    required this.moves,
    this.startingFen = startPosition,
  });

  /// The standard starting position, which every seeded line begins from.
  static const startPosition =
      'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1';

  /// Stable key, e.g. `opening_caro_kann_advance`. Also the progress-storage
  /// key handed to `TrainingProgressService`.
  final String id;

  /// ARB key of the display name — see [OpeningMove.explanation] for why the
  /// model carries keys rather than text.
  final String name;

  /// ECO code of the line, e.g. `B12`.
  final String eco;

  /// Side the user plays; also the side the board is oriented to.
  final PieceColor playerColor;

  /// The mainline in order, starting with White's first move.
  final List<OpeningMove> moves;

  /// Position the line starts from.
  final String startingFen;

  /// Whether the ply at [index] is the user's move rather than the computer's.
  ///
  /// Plies alternate from White, so White's line owns the even indices and
  /// Black's the odd ones.
  bool isPlayerMove(int index) => index.isEven == playerColor.isWhite;

  /// How many moves the user has to find — the denominator of the progress
  /// line above the board.
  int get playerMoveCount {
    var count = 0;
    for (var index = 0; index < moves.length; index++) {
      if (isPlayerMove(index)) count++;
    }
    return count;
  }

  /// User moves already played once [ply] half-moves are on the board.
  int playerMovesPlayed(int ply) {
    var count = 0;
    for (var index = 0; index < ply && index < moves.length; index++) {
      if (isPlayerMove(index)) count++;
    }
    return count;
  }
}
