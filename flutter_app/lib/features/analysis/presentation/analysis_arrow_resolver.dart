import '../../../shared/models/models.dart';

// -----------------------------------------------------------------------------
// Section: Analysis best-move arrow resolution
// -----------------------------------------------------------------------------

bool _usableArrowMove(String move) =>
    move.length >= 4 && move != '0000' && move != '(none)';

EngineLine? _rankOneLine(List<EngineLine> lines) {
  for (final line in lines) {
    if (line.rank == 1) return line;
  }
  return null;
}

/// Resolves the position-bound rank-1 move drawn by the analysis arrow.
String resolveAnalysisArrowMove({
  required List<EngineLine> lines,
  required String resultBestMove,
  required String boardFen,
  required String analysisFen,
}) {
  // Keep the arrow tied to the analyzed board position and rank-1 PV.
  if (analysisFen.isNotEmpty && boardFen.isNotEmpty && analysisFen != boardFen) {
    return '';
  }

  final rankOneMove = _rankOneLine(lines)?.bestMove ?? '';
  if (_usableArrowMove(rankOneMove)) return rankOneMove;

  // Terminal/partial snapshots can lack a PV. In that narrow case the native
  // result-level bestMove remains a safe fallback.
  return _usableArrowMove(resultBestMove) ? resultBestMove : '';
}
