import '../../../shared/models/models.dart';

// -----------------------------------------------------------------------------
// Section: Engine-specific analysis arrow compatibility
// -----------------------------------------------------------------------------

bool _usableArrowMove(String move) =>
    move.length >= 4 && move != '0000' && move != '(none)';

EngineLine? _rankOneLine(List<EngineLine> lines) {
  for (final line in lines) {
    if (line.rank == 1) return line;
  }
  return null;
}

/// Resolves the move drawn by both the best-move and threat arrows.
///
/// Stockfish 18 intentionally keeps the established behavior: the currently
/// selected PV line has priority over the result-level `bestMove`.
///
/// Stockfish 19 is stricter. Its arrow is a *best move* indicator, not a
/// selected-PV indicator, so rank 2+ must never replace rank 1. SF19 arrows are
/// also position-bound: a delayed snapshot from another FEN is hidden instead
/// of being painted on the current board. Finally, rank 1 and result-level
/// `bestMove` must agree when both are present; disagreement means the snapshot
/// is not coherent enough to draw a trustworthy arrow.
String resolveAnalysisArrowMove({
  required String engineVersion,
  required String engineId,
  required EngineLine? selectedLine,
  required List<EngineLine> lines,
  required String resultBestMove,
  required String boardFen,
  required String analysisFen,
}) {
  // A best-move arrow must never follow the currently selected PV. The selected
  // line can legitimately be rank 2+, while classification always evaluates
  // the engine's rank-1 recommendation. Keep the arrow position-bound and use
  // the same published rank-1 snapshot for SF18 and SF19.
  if (analysisFen.isNotEmpty && boardFen.isNotEmpty && analysisFen != boardFen) {
    return '';
  }

  final rankOneMove = _rankOneLine(lines)?.bestMove ?? '';
  if (_usableArrowMove(rankOneMove)) return rankOneMove;

  // Terminal/partial snapshots can lack a PV. In that narrow case the native
  // result-level bestMove remains a safe fallback.
  return _usableArrowMove(resultBestMove) ? resultBestMove : '';
}
