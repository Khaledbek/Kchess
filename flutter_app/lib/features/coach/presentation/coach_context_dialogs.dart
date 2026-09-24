import 'package:flutter/material.dart';

import '../../../localization/generated/app_localizations.dart';
import '../../../shared/models/models.dart';

// -----------------------------------------------------------------------------
// Section: Coach context input dialogs
// -----------------------------------------------------------------------------

enum CoachPgnSource { paste, game }

Future<String?> showCoachFenDialog(BuildContext context) =>
    _showTextContextDialog(
      context,
      title: AppLocalizations.of(context).coachPasteFen,
      label: AppLocalizations.of(context).fenText,
      minLines: 1,
      maxLines: 3,
    );

Future<String?> showCoachPgnDialog(BuildContext context) =>
    _showTextContextDialog(
      context,
      title: AppLocalizations.of(context).coachPastePgn,
      label: AppLocalizations.of(context).pgnText,
      minLines: 10,
      maxLines: 18,
    );

Future<String?> _showTextContextDialog(
  BuildContext context, {
  required String title,
  required String label,
  required int minLines,
  required int maxLines,
}) async {
  final controller = TextEditingController();
  try {
    return await showDialog<String>(
      context: context,
      builder: (context) {
        final strings = AppLocalizations.of(context);
        return AlertDialog(
          title: Text(title),
          content: SizedBox(
            width: 620,
            child: TextField(
              controller: controller,
              minLines: minLines,
              maxLines: maxLines,
              autofocus: true,
              textDirection: TextDirection.ltr,
              decoration: InputDecoration(labelText: label),
            ),
          ),
          actions: [
            TextButton(
              onPressed: () => Navigator.pop(context),
              child: Text(strings.close),
            ),
            FilledButton(
              onPressed: () {
                final value = controller.text.trim();
                if (value.isNotEmpty) Navigator.pop(context, value);
              },
              child: Text(strings.coachUseContext),
            ),
          ],
        );
      },
    );
  } finally {
    controller.dispose();
  }
}

Future<CoachPgnSource?> showCoachPgnSourceDialog(BuildContext context) {
  final strings = AppLocalizations.of(context);
  return showModalBottomSheet<CoachPgnSource>(
    context: context,
    showDragHandle: true,
    builder: (context) => SafeArea(
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          ListTile(
            leading: const Icon(Icons.content_paste_outlined),
            title: Text(strings.coachPastePgn),
            onTap: () => Navigator.pop(context, CoachPgnSource.paste),
          ),
          ListTile(
            leading: const Icon(Icons.sports_esports_outlined),
            title: Text(strings.coachChooseGame),
            onTap: () => Navigator.pop(context, CoachPgnSource.game),
          ),
          const SizedBox(height: 8),
        ],
      ),
    ),
  );
}

Future<GameSummary?> showCoachGamePicker(
  BuildContext context,
  List<GameSummary> games,
) {
  final strings = AppLocalizations.of(context);
  return showDialog<GameSummary>(
    context: context,
    builder: (context) => AlertDialog(
      title: Text(strings.coachChooseGame),
      content: SizedBox(
        width: 620,
        height: 460,
        child: games.isEmpty
            ? Center(child: Text(strings.coachNoGames))
            : ListView.separated(
                itemCount: games.length,
                separatorBuilder: (_, _) => const Divider(height: 1),
                itemBuilder: (context, index) {
                  final game = games[index];
                  return ListTile(
                    title: Text('${game.whiteName} – ${game.blackName}'),
                    subtitle: Text(
                      [game.date, game.openingName]
                          .whereType<String>()
                          .where((value) => value.isNotEmpty)
                          .join(' · '),
                    ),
                    trailing: Text(game.result),
                    onTap: () => Navigator.pop(context, game),
                  );
                },
              ),
      ),
      actions: [
        TextButton(
          onPressed: () => Navigator.pop(context),
          child: Text(strings.close),
        ),
      ],
    ),
  );
}
