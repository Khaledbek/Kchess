import 'package:flutter_test/flutter_test.dart';
import 'package:kchess/features/analysis/presentation/analysis_variation_graph.dart';
import 'package:kchess/shared/models/models.dart';

// -----------------------------------------------------------------------------
// Section: Ephemeral sideline graph regression tests
// -----------------------------------------------------------------------------

VariationAnalysisSnapshot _snapshot(String move, String san) =>
    VariationAnalysisSnapshot(
      jobId: 'job-$move',
      status: 'complete',
      playedMove: move,
      playedSan: san,
      fen: BoardPosition.empty.fen,
      position: BoardPosition.empty,
      bestMove: '',
      lines: const <EngineLine>[],
    );

void main() {
  test('keeps sibling sidelines when branching from an older node', () {
    final graph = AnalysisVariationGraph();
    final root = graph.addOrSelect(
      rootMainLinePly: 7,
      parentId: null,
      snapshot: _snapshot('g1f3', 'Nf3'),
    );
    final firstChild = graph.addOrSelect(
      rootMainLinePly: 7,
      parentId: root.id,
      snapshot: _snapshot('b8c6', 'Nc6'),
    );

    expect(graph.selectNode(root.id), isTrue);
    final secondChild = graph.addOrSelect(
      rootMainLinePly: 7,
      parentId: root.id,
      snapshot: _snapshot('d7d6', 'd6'),
    );

    expect(graph.nodes.length, 3);
    expect(graph.node(root.id)!.childIds, containsAll(<int>[
      firstChild.id,
      secondChild.id,
    ]));
    expect(graph.preferredNext?.id, firstChild.id);
    expect(
      graph.terminalNodesAt(7).map((node) => node.id),
      <int>[firstChild.id, secondChild.id],
    );
  });

  test('returning to main line does not discard explored sidelines', () {
    final graph = AnalysisVariationGraph();
    final root = graph.addOrSelect(
      rootMainLinePly: 3,
      parentId: null,
      snapshot: _snapshot('c2c4', 'c4'),
    );
    final child = graph.addOrSelect(
      rootMainLinePly: 3,
      parentId: root.id,
      snapshot: _snapshot('g8f6', 'Nf6'),
    );

    graph.selectMainLine();

    expect(graph.isOnVariation, isFalse);
    expect(graph.nodes.length, 2);
    expect(graph.selectNode(child.id), isTrue);
    expect(graph.currentPath.map((node) => node.id), <int>[root.id, child.id]);
  });

  test(
    'preferred route keeps the first explored continuation after navigating backward',
    () {
      final graph = AnalysisVariationGraph();
      final first = graph.addOrSelect(
        rootMainLinePly: 1,
        parentId: null,
        snapshot: _snapshot('g1f3', 'Nf3'),
      );
      final second = graph.addOrSelect(
        rootMainLinePly: 1,
        parentId: first.id,
        snapshot: _snapshot('b8c6', 'Nc6'),
      );
      final third = graph.addOrSelect(
        rootMainLinePly: 1,
        parentId: second.id,
        snapshot: _snapshot('f1b5', 'Bb5'),
      );

      graph.selectNode(first.id);

      expect(
        graph.currentRoute.map((node) => node.id),
        <int>[first.id, second.id, third.id],
      );
      expect(graph.currentRouteIndex, 0);
      expect(graph.preferredLast?.id, third.id);
    },
  );
}
