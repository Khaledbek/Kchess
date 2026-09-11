import 'dart:async';

import '../../../ffi/core_gateway.dart';
import '../../../shared/models/models.dart';

/// The defender's reply, plus what the engine knew while producing it.
class DefenderReply {
  const DefenderReply({
    required this.move,
    required this.fromEngine,
    this.mateIn,
  });

  final BoardMoveOption move;

  /// False when the engine failed or timed out and a legal move was played to
  /// keep the drill moving. The player surfaces this so a weak defence is never
  /// mistaken for the real thing.
  final bool fromEngine;

  /// Forced-mate distance the engine reported, when it saw one. Drives the
  /// "optimal was N moves" par line.
  final int? mateIn;
}

/// Plays the defending side of an endgame drill using the app's Stockfish.
///
/// There is no bot in the Spielen tab to reuse, so this drives the analysis
/// pipeline instead: [CoreGateway.startVariationAnalysis] applies the player's
/// move and searches the position it reaches, which makes its `bestMove` the
/// defender's best reply.
///
/// Two consequences worth knowing: the core stops any running game analysis
/// when a variation job starts, and the search is capped at [depth] rather than
/// the user's analysis depth so replies stay snappy — in a three or four piece
/// endgame that is already perfect play.
class EndgameDefenderBot {
  EndgameDefenderBot(
    this._gateway, {
    this.depth = 12,
    this.threads = 1,
    this.hashMb = 16,
    this.pollInterval = const Duration(milliseconds: 120),
    this.timeout = const Duration(seconds: 8),
  });

  final CoreGateway _gateway;
  final int depth;

  /// Engine resources for the drill search. A three or four piece endgame needs
  /// neither parallelism nor a transposition table, so these stay at the
  /// minimum and leave the user's configured analysis resources alone.
  ///
  /// They are passed explicitly because the gateway treats engine overrides as
  /// all-or-nothing: sending only depth and multiPv throws, which previously
  /// left the defender silent and the board frozen.
  final int threads;
  final int hashMb;

  final Duration pollInterval;
  final Duration timeout;

  String? _activeJobId;
  bool _disposed = false;

  /// Picks the defender's reply to [playerUci] played in [fenBeforePlayerMove].
  ///
  /// [defenderMoves] are the legal moves in the resulting position, which the
  /// caller has already fetched to render the board — the engine returns a UCI
  /// string and this resolves it back to one of them.
  Future<DefenderReply?> reply({
    required String fenBeforePlayerMove,
    required String playerUci,
    required List<BoardMoveOption> defenderMoves,
  }) async {
    if (defenderMoves.isEmpty) return null;

    var snapshot = await _search(fenBeforePlayerMove, playerUci);
    if (snapshot != null) {
      final resolved = _resolve(snapshot.bestMove, defenderMoves);
      if (resolved != null) {
        return DefenderReply(
          move: resolved,
          fromEngine: true,
          mateIn: snapshot.moverMateIn,
        );
      }
    }
    // Never strand the drill on an engine hiccup; play on and say so.
    return DefenderReply(move: defenderMoves.first, fromEngine: false);
  }

  Future<VariationAnalysisSnapshot?> _search(String fen, String uci) async {
    try {
      var snapshot = await _gateway.startVariationAnalysis(
        fen: fen,
        uci: uci,
        depth: depth,
        multiPv: 1,
        threads: threads,
        hashMb: hashMb,
      );
      _activeJobId = snapshot.jobId;
      final deadline = DateTime.now().add(timeout);
      while (snapshot.isRunning && !_disposed) {
        if (DateTime.now().isAfter(deadline)) return null;
        await Future<void>.delayed(pollInterval);
        if (_disposed) return null;
        snapshot = await _gateway.variationAnalysisStatus(snapshot.jobId);
      }
      return _disposed ? null : snapshot;
    } catch (_) {
      // Deliberately broad: a silent throw here used to escape through the
      // caller's unawaited future, so the defender never moved and the board
      // stayed locked. Any failure must degrade to the fallback move instead.
      return null;
    } finally {
      _activeJobId = null;
    }
  }

  /// Matches the engine's UCI against the legal moves, ignoring a promotion
  /// suffix the move list may spell differently.
  BoardMoveOption? _resolve(String best, List<BoardMoveOption> moves) {
    if (best.length < 4) return null;
    for (final move in moves) {
      if (move.uci == best) return move;
    }
    final squares = best.substring(0, 4);
    for (final move in moves) {
      if (move.squares == squares) return move;
    }
    return null;
  }

  /// Stops any in-flight search so leaving a drill does not leave Stockfish
  /// running behind the user's back.
  void dispose() {
    _disposed = true;
    final jobId = _activeJobId;
    if (jobId != null) {
      unawaited(_gateway.cancelVariationAnalysis(jobId).catchError((_) {}));
    }
  }
}
