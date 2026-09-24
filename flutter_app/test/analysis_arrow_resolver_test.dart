import 'package:flutter_test/flutter_test.dart';
import 'package:kchess/features/analysis/presentation/analysis_arrow_resolver.dart';
import 'package:kchess/shared/models/models.dart';

// -----------------------------------------------------------------------------
// Section: Engine-coherent best-move arrow regression coverage
// -----------------------------------------------------------------------------

const _fen = '8/8/8/8/8/8/8/8 w - - 0 1';
const _otherFen = '8/8/8/8/8/8/8/8 b - - 0 1';

AnalysisArrowContract _contract({
  String schema = 'analysis.arrow.v1',
  String snapshotId = 'snapshot-1',
  String fen = _fen,
  String move = 'e2e4',
  bool renderable = true,
  String reason = 'coherent',
}) => AnalysisArrowContract(
  schema: schema,
  snapshotId: snapshotId,
  fen: fen,
  move: move,
  renderable: renderable,
  reason: reason,
);

void main() {
  test('native rank-1 arrow contract renders its exact move', () {
    expect(
      resolveAnalysisArrowMove(
        contract: _contract(move: 'e2e4'),
        boardFen: _fen,
      ),
      'e2e4',
    );
  });

  test('non-renderable native arrow contract is hidden', () {
    expect(
      resolveAnalysisArrowMove(
        contract: _contract(
          move: 'e2e4',
          renderable: false,
          reason: 'classification_snapshot_mismatch',
        ),
        boardFen: _fen,
      ),
      isEmpty,
    );
  });

  test('unknown arrow contract schema is hidden', () {
    expect(
      resolveAnalysisArrowMove(
        contract: _contract(schema: 'analysis.arrow.v0'),
        boardFen: _fen,
      ),
      isEmpty,
    );
  });

  test('arrow contract without snapshot provenance is hidden', () {
    expect(
      resolveAnalysisArrowMove(
        contract: _contract(snapshotId: ''),
        boardFen: _fen,
      ),
      isEmpty,
    );
  });

  test('position-mismatched native arrow contract is hidden', () {
    expect(
      resolveAnalysisArrowMove(
        contract: _contract(fen: _otherFen),
        boardFen: _fen,
      ),
      isEmpty,
    );
  });

  test('invalid native bestmove marker is hidden', () {
    expect(
      resolveAnalysisArrowMove(
        contract: _contract(move: '0000'),
        boardFen: _fen,
      ),
      isEmpty,
    );
  });

  test('classification presentation contract fails closed on schema or provenance', () {
    const valid = AnalysisClassificationContract(
      schema: 'analysis.classification.v1',
      snapshotId: 'classification-1',
      fen: _fen,
      playedMove: 'e2e4',
      rank1Move: 'e2e4',
      playedMoveMatchesRank1: true,
      renderable: true,
      reason: 'coherent',
    );
    const wrongSchema = AnalysisClassificationContract(
      schema: 'analysis.classification.v0',
      snapshotId: 'classification-1',
      fen: _fen,
      playedMove: 'e2e4',
      rank1Move: 'e2e4',
      playedMoveMatchesRank1: true,
      renderable: true,
      reason: 'coherent',
    );
    const missingSnapshot = AnalysisClassificationContract(
      schema: 'analysis.classification.v1',
      snapshotId: '',
      fen: _fen,
      playedMove: 'e2e4',
      rank1Move: 'e2e4',
      playedMoveMatchesRank1: true,
      renderable: true,
      reason: 'coherent',
    );
    expect(valid.presentationRenderable, isTrue);
    expect(wrongSchema.presentationRenderable, isFalse);
    expect(missingSnapshot.presentationRenderable, isFalse);
  });

  test('missing native provenance never falls back to visible PV lines', () {
    expect(
      resolveAnalysisArrowMove(contract: null, boardFen: _fen),
      isEmpty,
    );
  });
}
