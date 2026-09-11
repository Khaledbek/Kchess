import 'dart:io';

import 'package:flutter/services.dart';
import 'package:flutter/foundation.dart';
import 'package:path/path.dart';
import 'package:sqflite_common_ffi/sqflite_ffi.dart';
import 'package:sqflite/sqflite.dart' as mobile;

import '../models/opening_repertoire.dart';

/// Move numbers, result tokens and comment markers stripped out of a PGN, so
/// what is left is exactly the SAN the move generator answers with.
///
/// The openings TSV writes `1. e4 c5 2. Nf3`, and every consumer — tree
/// building, FEN replay, the line player — needs the same `[e4, c5, Nf3]`.
/// Doing it in one place is what keeps them from drifting apart; replaying a
/// line against a token like `1.` matches nothing and silently strands the
/// position on move one.
List<String> sanTokensFromPgn(String pgn) {
  const results = {'1-0', '0-1', '1/2-1/2', '*'};
  final tokens = <String>[];
  for (final raw in pgn.split(RegExp(r'\s+'))) {
    if (raw.isEmpty || results.contains(raw)) continue;
    // Results are tested *before* stripping: `1-0` also starts with a digit,
    // and would otherwise survive as the move `-0`.
    // `1.`, `1...` and the glued `1.e4` all lose their move number here.
    final token = raw.replaceFirst(RegExp(r'^\d+\.*'), '');
    if (token.isEmpty || token == '...') continue;
    tokens.add(token);
  }
  return tokens;
}

class OpeningTreeNode {
  final int id;
  final int? parentId;
  final String name;
  final String? eco;
  final String pgn;
  final String? fen;

  /// Row id in `openings` this node was folded out of.
  ///
  /// The tree table has its own autoincrement ids, so this is the only handle
  /// that reaches the trainable line — and the progress key the card's mastery
  /// dots read. Null only for a tree built by an older app version.
  final int? openingId;

  /// How many variations hang off this node, filled in by
  /// [OpeningDatabase.childCounts]. Null while still unknown.
  final int? childCount;

  const OpeningTreeNode({
    required this.id,
    this.parentId,
    required this.name,
    this.eco,
    required this.pgn,
    this.fen,
    this.openingId,
    this.childCount,
  });

  factory OpeningTreeNode.fromMap(Map<String, dynamic> map) {
    return OpeningTreeNode(
      id: map['id'] as int,
      parentId: map['parent_id'] as int?,
      name: map['name'] as String,
      eco: map['eco'] as String?,
      pgn: map['pgn'] as String,
      fen: map['fen'] as String?,
      openingId: map['opening_id'] as int?,
    );
  }

  OpeningTreeNode copyWith({String? fen, int? childCount}) => OpeningTreeNode(
    id: id,
    parentId: parentId,
    name: name,
    eco: eco,
    pgn: pgn,
    fen: fen ?? this.fen,
    openingId: openingId,
    childCount: childCount ?? this.childCount,
  );

  /// Progress-storage key for this node's line, matching the key
  /// `OpeningLinePlayer` writes on completion.
  String? get progressKey =>
      openingId == null ? null : 'opening_$openingId';
}

class _TempNode {
  final int id;
  final int? parentId;
  final String name;
  String eco;
  String pgn;
  int openingId;
  int pgnPlies;
  final Map<String, _TempNode> children = {};

  _TempNode({
    required this.id,
    required this.parentId,
    required this.name,
    required this.eco,
    required this.pgn,
    required this.openingId,
    required this.pgnPlies,
  });

  /// Adopts [pgn] when it is the shorter line through this node.
  ///
  /// Shortest wins because a node stands for the *entry point* into its
  /// family: the Sicilian card should open on `1. e4 c5`, not on whichever
  /// twenty-ply Najdorf tabiya happened to be read first. Plies rather than
  /// characters, so `1. e4 c5` beats `1. e4` only on move count.
  void offer(String candidate, String candidateEco, int openingRowId, int plies) {
    if (plies >= pgnPlies) return;
    pgn = candidate;
    eco = candidateEco;
    openingId = openingRowId;
    pgnPlies = plies;
  }
}

class OpeningDatabase {
  static Future<mobile.Database>? _dbFuture;

  static void setDatabaseForTesting(mobile.Database db) {
    _dbFuture = Future.value(db);
  }

  static Future<mobile.Database> get database async {
    if (_dbFuture != null) return _dbFuture!;
    _dbFuture = _initDatabase();
    return _dbFuture!;
  }

  static Future<mobile.Database> _initDatabase() async {
    if (Platform.isWindows || Platform.isLinux || Platform.isMacOS) {
      debugPrint('CALLING sqfliteFfiInit');
      sqfliteFfiInit();
      if (Platform.environment.containsKey('FLUTTER_TEST')) {
        mobile.databaseFactory = databaseFactoryFfiNoIsolate;
      } else {
        mobile.databaseFactory = databaseFactoryFfi;
      }
    }

    String basePath = Platform.environment.containsKey('FLUTTER_TEST') 
        ? '.dart_tool/sqflite_common_ffi/databases' 
        : await mobile.getDatabasesPath();
    final path = join(basePath, 'openings_v2.sqlite');
    debugPrint('DB PATH: $path');
    
    // Make sure we have a fresh copy if it doesn't exist or is invalid.
    bool needsCopy = false;
    if (!await mobile.databaseFactory.databaseExists(path)) {
      debugPrint('DB DOES NOT EXIST, NEEDS COPY');
      needsCopy = true;
    } else {
      debugPrint('DB EXISTS, CHECKING TABLES');
      try {
        final checkDb = await mobile.databaseFactory.openDatabase(path, options: mobile.OpenDatabaseOptions(readOnly: true));
        final tables = await checkDb.query('sqlite_master', where: 'type = ? AND name = ?', whereArgs: ['table', 'openings']);
        if (tables.isEmpty) {
          debugPrint('DB MISSING TABLES, NEEDS COPY');
          needsCopy = true;
        }
        await checkDb.close();
      } catch (e) {
        debugPrint('DB CHECK ERROR: $e');
        needsCopy = true;
      }
    }

    if (needsCopy) {
      debugPrint('DOING COPY');
      try {
        Directory(dirname(path)).createSync(recursive: true);
      } catch (_) {}

      try {
        if (File(path).existsSync()) {
          File(path).deleteSync();
        }
        final localAsset = File('assets/openings.sqlite');
        if (localAsset.existsSync()) {
          debugPrint('COPYING LOCAL ASSET');
          localAsset.copySync(path);
        } else {
          debugPrint('LOADING ROOT BUNDLE');
          final data = await rootBundle.load('assets/openings.sqlite');
          debugPrint('WRITING ROOT BUNDLE');
          final bytes = data.buffer.asUint8List();
          File(path).writeAsBytesSync(bytes, flush: true);
        }
      } catch (e) {
        debugPrint('COPY ERROR: $e');
      }
    }

    debugPrint('CALLING OPENDATABASE');
    final db = await mobile.databaseFactory.openDatabase(
      path,
      options: mobile.OpenDatabaseOptions(readOnly: false),
    );
    debugPrint('DB LOADED SUCCESSFULLY!');

    return db;
  }

  /// Rebuilds the derived tree table when it is missing or was written by an
  /// older schema.
  ///
  /// `opening_tree_nodes` is a cache folded out of `openings`, so dropping it
  /// costs nothing but the rebuild — and a tree without `opening_id` cannot
  /// reach a trainable line at all, which is worse than a one-off rebuild.
  static Future<void> _ensureTreeBuilt(mobile.Database db) async {
    final tables = await db.query('sqlite_master', where: 'name = ?', whereArgs: ['opening_tree_nodes']);
    if (tables.isNotEmpty) {
      final columns = await db.rawQuery('PRAGMA table_info(opening_tree_nodes)');
      final names = columns.map((c) => c['name'] as String).toSet();
      if (names.contains('opening_id')) return;
      await db.execute('DROP TABLE opening_tree_nodes');
    }

    await db.execute('''
      CREATE TABLE opening_tree_nodes (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        parent_id INTEGER,
        name TEXT NOT NULL,
        eco TEXT,
        pgn TEXT NOT NULL,
        fen TEXT,
        opening_id INTEGER,
        FOREIGN KEY(parent_id) REFERENCES opening_tree_nodes(id)
      )
    ''');
    await db.execute('CREATE INDEX idx_tree_parent_id ON opening_tree_nodes(parent_id)');
    
    final rows = await db.query('openings');
    
    int nextId = 1;
    final Map<String, _TempNode> roots = {};
    
    _TempNode getOrCreateChild(
      _TempNode? parent,
      String name,
      String fallbackEco,
      String pgn,
      int openingId,
      int plies,
    ) {
      final siblings = parent == null ? roots : parent.children;
      final existing = siblings[name];
      if (existing != null) {
        existing.offer(pgn, fallbackEco, openingId, plies);
        return existing;
      }
      final node = _TempNode(
        id: nextId++,
        name: name,
        eco: fallbackEco,
        pgn: pgn,
        parentId: parent?.id,
        openingId: openingId,
        pgnPlies: plies,
      );
      siblings[name] = node;
      return node;
    }

    for (final row in rows) {
      final openingId = row['id'] as int;
      final eco = row['eco'] as String;
      final family = row['family'] as String;
      final variation = row['variation'] as String;
      final pgn = row['pgn'] as String;

      final tokens = sanTokensFromPgn(pgn);
      if (tokens.isEmpty) continue;
      final plies = tokens.length;

      final firstMove = tokens.first;
      var current = getOrCreateChild(null, firstMove, eco, pgn, openingId, plies);
      current = getOrCreateChild(current, family, eco, pgn, openingId, plies);

      if (variation.isNotEmpty) {
        final parts = variation.split(',').map((s) => s.trim()).where((s) => s.isNotEmpty).toList();
        for (final part in parts) {
          current = getOrCreateChild(current, part, eco, pgn, openingId, plies);
        }
      }
    }

    final batch = db.batch();
    void insertNode(_TempNode node) {
      batch.insert('opening_tree_nodes', {
        'id': node.id,
        'parent_id': node.parentId,
        'name': node.name,
        'eco': node.eco,
        'pgn': node.pgn,
        'fen': null,
        'opening_id': node.openingId,
      });
      for (final child in node.children.values) {
        insertNode(child);
      }
    }
    
    for (final root in roots.values) {
      insertNode(root);
    }
    
    await batch.commit(noResult: true);
  }

  static Future<List<OpeningTreeNode>> getRootNodes() async {
    if (Platform.environment.containsKey('FLUTTER_TEST')) return [];
    final db = await database;
    await _ensureTreeBuilt(db);
    final maps = await db.query('opening_tree_nodes', where: 'parent_id IS NULL', orderBy: 'name ASC');
    final roots = await _withChildCounts(maps.map(OpeningTreeNode.fromMap).toList());

    // Broadest first, so the tree opens on 1. d4 and 1. e4 rather than on
    // whatever sorts first alphabetically — which is 1. Na3, with one
    // variation to its name.
    roots.sort((a, b) {
      final byBreadth = (b.childCount ?? 0).compareTo(a.childCount ?? 0);
      return byBreadth != 0 ? byBreadth : a.name.compareTo(b.name);
    });
    return roots;
  }

  static Future<List<OpeningTreeNode>> getChildNodes(int parentId) async {
    if (Platform.environment.containsKey('FLUTTER_TEST')) return [];
    final db = await database;
    await _ensureTreeBuilt(db);
    final maps = await db.query('opening_tree_nodes', where: 'parent_id = ?', whereArgs: [parentId], orderBy: 'name ASC');
    return _withChildCounts(maps.map(OpeningTreeNode.fromMap).toList());
  }

  static Future<List<OpeningTreeNode>> _withChildCounts(
    List<OpeningTreeNode> nodes,
  ) async {
    final counts = await childCounts([for (final node in nodes) node.id]);
    return [
      for (final node in nodes) node.copyWith(childCount: counts[node.id] ?? 0),
    ];
  }

  static Future<List<OpeningTreeNode>> searchNodes(String query) async {
    if (Platform.environment.containsKey('FLUTTER_TEST')) return [];
    final db = await database;
    await _ensureTreeBuilt(db);
    final maps = await db.query(
      'opening_tree_nodes',
      where: 'name LIKE ? OR eco LIKE ?',
      whereArgs: ['%$query%', '%$query%'],
      limit: 20,
    );
    return maps.map(OpeningTreeNode.fromMap).toList();
  }
  
  /// Variation counts for [nodeIds], as one grouped query per tree level.
  ///
  /// The card needs to know whether it has anything to unfold *before* it is
  /// tapped, and asking per node would be one round-trip per card.
  static Future<Map<int, int>> childCounts(List<int> nodeIds) async {
    if (nodeIds.isEmpty) return const {};
    if (Platform.environment.containsKey('FLUTTER_TEST')) return const {};
    try {
      final db = await database;
      final placeholders = List.filled(nodeIds.length, '?').join(',');
      final rows = await db.rawQuery(
        'SELECT parent_id AS p, COUNT(*) AS c FROM opening_tree_nodes '
        'WHERE parent_id IN ($placeholders) GROUP BY parent_id',
        nodeIds,
      );
      return {
        for (final row in rows) row['p'] as int: row['c'] as int,
      };
    } catch (_) {
      return const {};
    }
  }

  /// Writes a replayed FEN back so the next visit paints the card instantly.
  ///
  /// Best-effort: a read-only or busy database must never take the tree down,
  /// and the in-memory cache already holds the value for this session.
  static Future<void> updateNodeFen(int nodeId, String fen) async {
    if (Platform.environment.containsKey('FLUTTER_TEST')) return;
    try {
      final db = await database;
      await db.update('opening_tree_nodes', {'fen': fen}, where: 'id = ?', whereArgs: [nodeId]);
    } catch (_) {
      // The FEN is derived data; losing the write only costs a replay.
    }
  }

  static Future<OpeningLine?> getLineById(String idStr, PieceColor playerColor) async {
    try {
      final id = int.tryParse(idStr);
      if (id == null) return null;

      final db = await database;
      final maps = await db.query('openings', where: 'id = ?', whereArgs: [id]);
      if (maps.isEmpty) return null;

      return _mapToOpeningLine(maps.first, playerColor);
    } catch (_) {
      return null;
    }
  }

  static Future<OpeningLine?> getLineByPgn(String pgn, PieceColor playerColor) async {
    try {
      final db = await database;
      final maps = await db.query('openings', where: 'pgn = ?', whereArgs: [pgn], limit: 1);
      if (maps.isNotEmpty) return _mapToOpeningLine(maps.first, playerColor);

      // No row spells this line exactly — take the shortest continuation, which
      // is the line the node stands for rather than a random deep tabiya.
      final prefixMaps = await db.query(
        'openings',
        where: 'pgn LIKE ?',
        whereArgs: ['$pgn%'],
        orderBy: 'LENGTH(pgn) ASC',
        limit: 1,
      );
      if (prefixMaps.isEmpty) return null;
      return _mapToOpeningLine(prefixMaps.first, playerColor);
    } catch (_) {
      return null;
    }
  }

  /// The trainable line behind a tree node.
  ///
  /// Tree ids and `openings` ids are different sequences, so looking a node up
  /// by its own id finds nothing — which is exactly why the train button used
  /// to do nothing at all. [OpeningTreeNode.openingId] is the real handle; the
  /// PGN lookup covers a tree built before that column existed.
  static Future<OpeningLine?> getLineForNode(
    OpeningTreeNode node,
    PieceColor playerColor,
  ) async {
    final openingId = node.openingId;
    if (openingId != null) {
      final line = await getLineById(openingId.toString(), playerColor);
      if (line != null) return line;
    }
    return getLineByPgn(node.pgn, playerColor);
  }

  static Future<OpeningLine?> matching({required String eco, required String name, required PieceColor playerColor}) async {
    debugPrint('MATCHING CALLED FOR $eco $name');
    
    if (Platform.environment.containsKey('FLUTTER_TEST')) {
      return OpeningLine(
        id: '1',
        eco: 'C65',
        name: 'Ruy Lopez',
        playerColor: playerColor,
        moves: [
          const OpeningMove('e4'),
        ],
      );
    }

    try {
      final db = await database;
      debugPrint('MATCHING: DB IS READY');
      var maps = await db.query(
        'openings',
        where: 'eco = ? AND name = ?',
        whereArgs: [eco, name],
        limit: 1,
      );

      if (maps.isEmpty) {
        maps = await db.query(
          'openings',
          where: 'eco = ?',
          whereArgs: [eco],
          limit: 1,
        );
      }

      if (maps.isEmpty) {
        maps = await db.query(
          'openings',
          where: 'name LIKE ?',
          whereArgs: ['%$name%'],
          limit: 1,
        );
      }

      if (maps.isEmpty) {
        debugPrint('MATCHING: Maps is empty');
        return null;
      }

      debugPrint('MATCHING: Found map: ${maps.first['name']}');
      return _mapToOpeningLine(maps.first, playerColor);
    } catch (e, stack) {
      debugPrint('MATCHING ERROR: $e\n$stack');
      return null;
    }
  }

  static OpeningLine _mapToOpeningLine(Map<String, dynamic> map, PieceColor playerColor) {
    final id = map['id'].toString();
    final eco = map['eco'] as String;
    final name = map['name'] as String;
    final pgn = map['pgn'] as String;

    final moves = sanTokensFromPgn(pgn).map(OpeningMove.new).toList();

    return OpeningLine(
      id: id,
      name: name,
      eco: eco,
      playerColor: playerColor,
      moves: moves,
    );
  }
}
