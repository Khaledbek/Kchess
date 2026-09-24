import 'dart:async';

import 'package:flutter/material.dart';

import '../../../ffi/core_gateway.dart';
import '../../../localization/generated/app_localizations.dart';
import '../models/coach_ui_models.dart';

Future<CoachSessionSummary?> showCoachSessionsSheet(
  BuildContext context, {
  required CoreGateway gateway,
  required String profileId,
  required String activeSessionId,
}) => showModalBottomSheet<CoachSessionSummary>(
  context: context,
  isScrollControlled: true,
  showDragHandle: true,
  builder: (_) => _CoachSessionsSheet(
    gateway: gateway,
    profileId: profileId,
    activeSessionId: activeSessionId,
  ),
);

class _CoachSessionsSheet extends StatefulWidget {
  const _CoachSessionsSheet({
    required this.gateway,
    required this.profileId,
    required this.activeSessionId,
  });

  final CoreGateway gateway;
  final String profileId;
  final String activeSessionId;

  @override
  State<_CoachSessionsSheet> createState() => _CoachSessionsSheetState();
}

class _CoachSessionsSheetState extends State<_CoachSessionsSheet> {
  List<CoachSessionSummary> _sessions = const [];
  bool _loading = true;
  String? _error;

  @override
  void initState() {
    super.initState();
    _reload();
  }

  Future<void> _reload() async {
    try {
      final reply = await widget.gateway.coachSessions(widget.profileId);
      final sessions = (reply['sessions'] as List<Object?>? ?? const [])
          .whereType<Map>()
          .map(
            (value) => CoachSessionSummary.fromJson(
              value.map((key, value) => MapEntry(key.toString(), value)),
            ),
          )
          .toList(growable: false);
      if (!mounted) return;
      setState(() {
        _sessions = sessions;
        _loading = false;
        _error = null;
      });
    } catch (caught) {
      if (!mounted) return;
      setState(() {
        _loading = false;
        _error = caught.toString();
      });
    }
  }

  Future<CoachSessionSummary?> _create() async {
    final reply = await widget.gateway.createCoachSession(widget.profileId);
    final raw = reply['session'];
    if (raw is! Map) return null;
    return CoachSessionSummary.fromJson(
      raw.map((key, value) => MapEntry(key.toString(), value)),
    );
  }

  Future<void> _newSession() async {
    setState(() => _loading = true);
    try {
      final session = await _create();
      if (!mounted || session == null) return;
      Navigator.of(context).pop(session);
    } catch (caught) {
      if (!mounted) return;
      setState(() {
        _loading = false;
        _error = caught.toString();
      });
    }
  }

  Future<void> _rename(CoachSessionSummary session) async {
    final strings = AppLocalizations.of(context);
    final controller = TextEditingController(text: session.name);
    final name = await showDialog<String>(
      context: context,
      builder: (dialogContext) => AlertDialog(
        title: Text(strings.coachRenameSession),
        content: TextField(
          controller: controller,
          autofocus: true,
          maxLength: 160,
          onSubmitted: (value) {
            final trimmed = value.trim();
            if (trimmed.isNotEmpty) Navigator.of(dialogContext).pop(trimmed);
          },
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(dialogContext).pop(),
            child: Text(strings.coachSessionCancel),
          ),
          FilledButton(
            onPressed: () {
              final trimmed = controller.text.trim();
              if (trimmed.isNotEmpty) Navigator.of(dialogContext).pop(trimmed);
            },
            child: Text(strings.coachSessionSave),
          ),
        ],
      ),
    );
    controller.dispose();
    if (!mounted || name == null || name == session.name) return;
    try {
      await widget.gateway.renameCoachSession({
        'profileId': widget.profileId,
        'sessionId': session.id,
        'name': name,
      });
      await _reload();
    } catch (caught) {
      if (!mounted) return;
      setState(() => _error = caught.toString());
    }
  }

  Future<void> _delete(CoachSessionSummary session) async {
    final strings = AppLocalizations.of(context);
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (dialogContext) => AlertDialog(
        title: Text(strings.coachDeleteSession),
        content: Text(strings.coachDeleteSessionConfirm),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(dialogContext).pop(false),
            child: Text(strings.coachSessionCancel),
          ),
          FilledButton(
            onPressed: () => Navigator.of(dialogContext).pop(true),
            child: Text(strings.coachDeleteSession),
          ),
        ],
      ),
    );
    if (confirmed != true || !mounted) return;
    try {
      await widget.gateway.deleteCoachSession({
        'profileId': widget.profileId,
        'sessionId': session.id,
      });
      if (!mounted) return;
      if (session.id == widget.activeSessionId) {
        final replacement = await _create();
        if (!mounted || replacement == null) return;
        Navigator.of(context).pop(replacement);
        return;
      }
      await _reload();
    } catch (caught) {
      if (!mounted) return;
      setState(() => _error = caught.toString());
    }
  }

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final media = MediaQuery.of(context);
    return SafeArea(
      child: SizedBox(
        height: media.size.height * 0.72,
        child: Column(
          children: [
            Padding(
              padding: const EdgeInsets.fromLTRB(16, 0, 8, 8),
              child: Row(
                children: [
                  Expanded(
                    child: Text(
                      strings.coachSessionsTitle,
                      style: Theme.of(context).textTheme.titleLarge,
                    ),
                  ),
                  FilledButton.icon(
                    onPressed: _loading ? null : _newSession,
                    icon: const Icon(Icons.add_rounded),
                    label: Text(strings.coachNewSession),
                  ),
                ],
              ),
            ),
            if (_error != null)
              Padding(
                padding: const EdgeInsets.symmetric(horizontal: 16),
                child: Text(
                  _error!,
                  style: TextStyle(color: Theme.of(context).colorScheme.error),
                ),
              ),
            Expanded(
              child: _loading
                  ? const Center(child: CircularProgressIndicator())
                  : _sessions.isEmpty
                  ? Center(child: Text(strings.coachSessionEmpty))
                  : ListView.separated(
                      padding: const EdgeInsets.fromLTRB(8, 8, 8, 24),
                      itemCount: _sessions.length,
                      separatorBuilder: (_, _) => const Divider(height: 1),
                      itemBuilder: (context, index) {
                        final session = _sessions[index];
                        final material = MaterialLocalizations.of(context);
                        final date = material.formatMediumDate(session.updatedAt);
                        final time = material.formatTimeOfDay(
                          TimeOfDay.fromDateTime(session.updatedAt),
                        );
                        return ListTile(
                          selected: session.id == widget.activeSessionId,
                          leading: const Icon(Icons.forum_outlined),
                          title: Text(
                            session.name,
                            maxLines: 1,
                            overflow: TextOverflow.ellipsis,
                          ),
                          subtitle: Text('$date · $time'),
                          onTap: () => Navigator.of(context).pop(session),
                          trailing: PopupMenuButton<_SessionAction>(
                            onSelected: (action) {
                              if (action == _SessionAction.rename) {
                                unawaited(_rename(session));
                              } else {
                                unawaited(_delete(session));
                              }
                            },
                            itemBuilder: (context) => [
                              PopupMenuItem(
                                value: _SessionAction.rename,
                                child: ListTile(
                                  dense: true,
                                  leading: const Icon(Icons.edit_outlined),
                                  title: Text(strings.coachRenameSession),
                                ),
                              ),
                              PopupMenuItem(
                                value: _SessionAction.delete,
                                child: ListTile(
                                  dense: true,
                                  leading: const Icon(Icons.delete_outline),
                                  title: Text(strings.coachDeleteSession),
                                ),
                              ),
                            ],
                          ),
                        );
                      },
                    ),
            ),
          ],
        ),
      ),
    );
  }
}

enum _SessionAction { rename, delete }
