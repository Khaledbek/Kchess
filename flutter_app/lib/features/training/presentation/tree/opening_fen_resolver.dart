import '../../../../ffi/core_gateway.dart';
import '../../data/opening_database.dart';
import '../../models/models.dart';

/// Turns an opening's PGN into the FEN of the position it reaches, by
/// replaying it through the core's move generator.
///
/// Nothing here knows the rules of chess: every ply is looked up in the legal
/// move list the core returns for the current position, so an unplayable token
/// aborts the line instead of quietly leaving the board on move one.
///
/// Two caches make the tree affordable. [_fenByLine] keys on the *move prefix*,
/// so the hundred variations under `1. e4 c5` all reuse the work the first one
/// did; [_movesByFen] keeps each position's move list, so replaying a sibling
/// costs no round-trips at all.
class OpeningFenResolver {
  OpeningFenResolver({required this.gateway});

  final CoreGateway gateway;

  final Map<String, String> _fenByLine = {};
  final Map<String, Map<String, String>> _movesByFen = {};

  /// FEN after [pgn], or null when the line cannot be replayed.
  Future<String?> resolve(String pgn) => resolveMoves(sanTokensFromPgn(pgn));

  /// FEN after [moves], which must already be bare SAN.
  Future<String?> resolveMoves(List<String> moves) async {
    var fen = OpeningLine.startPosition;
    var key = '';

    for (final san in moves) {
      key = key.isEmpty ? san : '$key $san';

      final cached = _fenByLine[key];
      if (cached != null) {
        fen = cached;
        continue;
      }

      final next = (await _movesFor(fen))[_normalise(san)];
      if (next == null) return null;

      fen = next;
      _fenByLine[key] = fen;
    }

    return fen;
  }

  /// San → resulting FEN for [fen], fetched once per position.
  Future<Map<String, String>> _movesFor(String fen) async {
    final cached = _movesByFen[fen];
    if (cached != null) return cached;

    // Keyed on the normalised SAN so a book that writes `Qh5` against the
    // core's `Qh5+` still finds its move.
    final table = {
      for (final option in await gateway.boardLegalMoves(fen))
        _normalise(option.san): option.fenAfter,
    };
    _movesByFen[fen] = table;
    return table;
  }

  /// Drops the check and mate suffixes the opening book is inconsistent about.
  static String _normalise(String san) =>
      san.replaceAll(RegExp(r'[+#!?]+$'), '');
}
