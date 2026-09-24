import '../../../shared/models/models.dart';

// -----------------------------------------------------------------------------
// Section: Analysis best-move arrow resolution
// -----------------------------------------------------------------------------

bool _usableArrowMove(String move) =>
    move.length >= 4 && move != '0000' && move != '(none)';

/// Resolves the native, position-bound best-move arrow contract.
///
/// Flutter deliberately does not infer rank 1 from visible PV lines.  The
/// native AnalysisService owns whether a completed engine snapshot is coherent
/// enough to publish an arrow and transports the exact move/FEN provenance.
String resolveAnalysisArrowMove({
  required AnalysisArrowContract? contract,
  required String boardFen,
}) {
  if (contract == null || !contract.renderable) return '';
  if (contract.schema != 'analysis.arrow.v1' || contract.snapshotId.isEmpty) {
    return '';
  }
  if (contract.fen.isEmpty || boardFen.isEmpty || contract.fen != boardFen) {
    return '';
  }
  return _usableArrowMove(contract.move) ? contract.move : '';
}
