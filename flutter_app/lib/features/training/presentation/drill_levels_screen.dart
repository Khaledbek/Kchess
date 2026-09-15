// -----------------------------------------------------------------------------
// Section: Native drill levels and progress presentation
// -----------------------------------------------------------------------------
import 'package:flutter/material.dart';

import '../../../ffi/core_gateway.dart';
import '../../../localization/generated/app_localizations.dart';
import 'practice_player.dart';
import 'practice_strings.dart';

class DrillLevelsScreen extends StatefulWidget {
  const DrillLevelsScreen({required this.gateway, super.key});
  final CoreGateway gateway;
  @override
  State<DrillLevelsScreen> createState() => _DrillLevelsScreenState();
}

class _DrillLevelsScreenState extends State<DrillLevelsScreen> {
  late Future<Map<String, Object?>> _data;
  @override
  void initState() {
    super.initState();
    _reload();
  }

  void _reload() {
    _data = widget.gateway
        .practiceCommand({'op': 'catalog'})
        .then((value) => value! as Map<String, Object?>);
  }

  Future<void> _open(Map<String, Object?> level) async {
    final strings = AppLocalizations.of(context);
    final id = level['id']! as String;
    await Navigator.of(context).push(
      MaterialPageRoute<void>(
        builder: (_) => PracticePlayer(
          gateway: widget.gateway,
          title: drillTitle(strings, id),
          hint: drillHint(strings, id),
          request: {'kind': 'drill', 'id': id, 'level': level['level']},
        ),
      ),
    );
    if (mounted) setState(_reload);
  }

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    return Scaffold(
      appBar: AppBar(title: Text(strings.practiceDrills)),
      body: FutureBuilder<Map<String, Object?>>(
        future: _data,
        builder: (context, snapshot) {
          if (snapshot.hasError) {
            return Center(child: Text(strings.trainingBoardError));
          }
          if (!snapshot.hasData) {
            return const Center(child: CircularProgressIndicator());
          }
          final drills = (snapshot.data!['drills']! as List<Object?>)
              .cast<Map<String, Object?>>();
          return ListView(
            padding: const EdgeInsets.all(16),
            children: [
              for (final drill in drills)
                Card(
                  child: Padding(
                    padding: const EdgeInsets.all(16),
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Text(
                          drillTitle(strings, drill['id']! as String),
                          style: Theme.of(context).textTheme.titleLarge,
                        ),
                        Text(drillHint(strings, drill['id']! as String)),
                        for (final level
                            in (drill['levels']! as List<Object?>)
                                .cast<Map<String, Object?>>())
                          ListTile(
                            title: Text(
                              strings.practiceLevel(level['level']! as int),
                            ),
                            subtitle: Text(
                              strings.trainingMoveProgress(
                                0,
                                level['maxMoves']! as int,
                              ),
                            ),
                            leading: Icon(
                              level['unlocked'] == true
                                  ? Icons.play_arrow
                                  : Icons.lock_outline,
                            ),
                            trailing:
                                (level['progress']!
                                        as Map<
                                          String,
                                          Object?
                                        >)['isMastered'] ==
                                    true
                                ? const Icon(Icons.star)
                                : null,
                            onTap: level['unlocked'] == true
                                ? () => _open(level)
                                : null,
                          ),
                      ],
                    ),
                  ),
                ),
            ],
          );
        },
      ),
    );
  }
}
