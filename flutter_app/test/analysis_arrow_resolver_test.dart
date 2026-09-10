import 'package:flutter_test/flutter_test.dart';
import 'package:kchess/features/analysis/presentation/analysis_arrow_resolver.dart';
import 'package:kchess/models/models.dart';

// -----------------------------------------------------------------------------
// Section: Engine-coherent best-move arrow regression coverage
// -----------------------------------------------------------------------------

const _rank1 = EngineLine(
  rank: 1,
  depth: 20,
  nodes: 1000,
  moves: ['e2e4', 'e7e5'],
);

const _rank2 = EngineLine(
  rank: 2,
  depth: 20,
  nodes: 900,
  moves: ['d2d4', 'd7d5'],
);

const _fen = '8/8/8/8/8/8/8/8 w - - 0 1';
const _otherFen = '8/8/8/8/8/8/8/8 b - - 0 1';

void main() {
  test('Stockfish 18 best-move arrow ignores selected rank 2 PV', () {
    expect(
      resolveAnalysisArrowMove(
        engineVersion: 'Stockfish 18',
        engineId: 'stockfish18',
        selectedLine: _rank2,
        lines: const [_rank1, _rank2],
        resultBestMove: 'g1f3',
        boardFen: _fen,
        analysisFen: _fen,
      ),
      'e2e4',
    );
  });

  test('Stockfish 19 best-move arrow ignores selected rank 2 PV', () {
    expect(
      resolveAnalysisArrowMove(
        engineVersion: 'Stockfish 19 (sf_19-edb0d9d; live-exact-v1)',
        engineId: 'stockfish19',
        selectedLine: _rank2,
        lines: const [_rank1, _rank2],
        resultBestMove: 'e2e4',
        boardFen: _fen,
        analysisFen: _fen,
      ),
      'e2e4',
    );
  });

  test('Stockfish 18 hides a position-mismatched arrow snapshot', () {
    expect(
      resolveAnalysisArrowMove(
        engineVersion: 'Stockfish 18',
        engineId: 'stockfish18',
        selectedLine: _rank1,
        lines: const [_rank1, _rank2],
        resultBestMove: 'e2e4',
        boardFen: _fen,
        analysisFen: _otherFen,
      ),
      isEmpty,
    );
  });

  test('Stockfish 19 hides a position-mismatched arrow snapshot', () {
    expect(
      resolveAnalysisArrowMove(
        engineVersion: 'Stockfish 19 (sf_19-edb0d9d; live-exact-v1)',
        engineId: 'stockfish19',
        selectedLine: _rank1,
        lines: const [_rank1, _rank2],
        resultBestMove: 'e2e4',
        boardFen: _fen,
        analysisFen: _otherFen,
      ),
      isEmpty,
    );
  });

  test('rank 1 wins over a stale result-level bestmove for both engines', () {
    for (final engine in const ['stockfish18', 'stockfish19']) {
      expect(
        resolveAnalysisArrowMove(
          engineVersion: engine == 'stockfish19' ? 'Stockfish 19' : 'Stockfish 18',
          engineId: engine,
          selectedLine: _rank2,
          lines: const [_rank1, _rank2],
          resultBestMove: 'g1f3',
          boardFen: _fen,
          analysisFen: _fen,
        ),
        'e2e4',
      );
    }
  });

  test('result-level bestmove is fallback only when rank 1 is unavailable', () {
    expect(
      resolveAnalysisArrowMove(
        engineVersion: 'Stockfish 18',
        engineId: 'stockfish18',
        selectedLine: _rank2,
        lines: const [_rank2],
        resultBestMove: 'g1f3',
        boardFen: _fen,
        analysisFen: _fen,
      ),
      'g1f3',
    );
  });
}
