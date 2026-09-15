// -----------------------------------------------------------------------------
// Section: Classical study catalogue presentation
// -----------------------------------------------------------------------------
import 'package:flutter/material.dart';

import '../../../ffi/core_gateway.dart';
import '../../../localization/generated/app_localizations.dart';
import 'practice_player.dart';
import 'practice_strings.dart';

class EndgameStudiesScreen extends StatefulWidget {
  const EndgameStudiesScreen({required this.gateway, super.key});
  final CoreGateway gateway;
  @override
  State<EndgameStudiesScreen> createState() => _EndgameStudiesScreenState();
}

class _EndgameStudiesScreenState extends State<EndgameStudiesScreen> {
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

  Future<void> _open(Map<String, Object?> study) async {
    final strings = AppLocalizations.of(context);
    await Navigator.of(context).push(
      MaterialPageRoute<void>(
        builder: (_) => PracticePlayer(
          gateway: widget.gateway,
          title: strings.practiceStudyNumber(study['number']! as int),
          request: {'kind': 'study', 'id': study['id']},
        ),
      ),
    );
    if (mounted) setState(_reload);
  }

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    return Scaffold(
      appBar: AppBar(title: Text(strings.practiceStudies)),
      body: FutureBuilder<Map<String, Object?>>(
        future: _data,
        builder: (context, snapshot) {
          if (snapshot.hasError) {
            return Center(child: Text(strings.trainingBoardError));
          }
          if (!snapshot.hasData) {
            return const Center(child: CircularProgressIndicator());
          }
          final catalogue = snapshot.data!['studies']! as Map<String, Object?>;
          final sections = (catalogue['sections']! as List<Object?>)
              .cast<Map<String, Object?>>();
          return ListView(
            padding: const EdgeInsets.all(16),
            children: [
              Text(strings.practiceStudySource),
              const SizedBox(height: 12),
              for (final section in sections)
                Card(
                  child: ExpansionTile(
                    title: Text(
                      studySectionTitle(strings, section['id']! as String),
                    ),
                    children: [
                      for (final study
                          in (section['studies']! as List<Object?>)
                              .cast<Map<String, Object?>>())
                        ListTile(
                          title: Text(
                            strings.practiceStudyNumber(
                              study['number']! as int,
                            ),
                          ),
                          subtitle: Text(
                            practiceDifficulty(
                              strings,
                              study['difficulty']! as String,
                            ),
                          ),
                          trailing:
                              (study['progress']!
                                      as Map<String, Object?>)['isMastered'] ==
                                  true
                              ? const Icon(Icons.star)
                              : const Icon(Icons.chevron_right),
                          onTap: () => _open(study),
                        ),
                    ],
                  ),
                ),
            ],
          );
        },
      ),
    );
  }
}
