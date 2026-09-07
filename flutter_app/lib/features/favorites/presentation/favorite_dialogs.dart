part of '../../../ui/app_root.dart';

Future<void> _showFavoriteCollectionPicker(
  BuildContext context,
  AppController controller,
  GameSummary game,
) async {
  final strings = AppLocalizations.of(context);
  final currentId = game.favoriteCollectionId ?? '';
  final selectedId = await showDialog<String>(
    context: context,
    builder: (dialogContext) => SimpleDialog(
      title: Text(strings.favoriteMoveToCollection),
      children: [
        Padding(
          padding: const EdgeInsets.fromLTRB(24, 0, 24, 8),
          child: Text(
            strings.favoriteMoveHelp,
            style: Theme.of(dialogContext).textTheme.bodySmall?.copyWith(
              color: Theme.of(dialogContext).colorScheme.onSurfaceVariant,
            ),
          ),
        ),
        SimpleDialogOption(
          key: const Key('favorite-collection-loose'),
          onPressed: () => Navigator.pop(dialogContext, ''),
          child: ListTile(
            contentPadding: EdgeInsets.zero,
            leading: const Icon(Icons.favorite_outline),
            title: Text(strings.favoriteLooseTitle),
            trailing: currentId.isEmpty ? const Icon(Icons.check) : null,
          ),
        ),
        for (final collection in controller.favoriteCollections)
          SimpleDialogOption(
            key: Key('favorite-collection-${collection.id}'),
            onPressed: () => Navigator.pop(dialogContext, collection.id),
            child: ListTile(
              contentPadding: EdgeInsets.zero,
              leading: const Icon(Icons.folder_outlined),
              title: Text(
                collection.name,
                maxLines: 1,
                overflow: TextOverflow.ellipsis,
              ),
              trailing: currentId == collection.id
                  ? const Icon(Icons.check)
                  : null,
            ),
          ),
      ],
    ),
  );
  if (selectedId == null || selectedId == currentId || !context.mounted) {
    return;
  }

  FavoriteCollection? target;
  if (selectedId.isNotEmpty) {
    for (final collection in controller.favoriteCollections) {
      if (collection.id == selectedId) {
        target = collection;
        break;
      }
    }
    if (target == null) return;
  }

  try {
    await controller.moveFavoriteToCollection(game, target);
  } catch (error) {
    if (context.mounted) {
      ScaffoldMessenger.of(context)
          .showSnackBar(SnackBar(content: Text(error.toString())));
    }
  }
}

Future<String?> _showFavoriteCollectionNameDialog(
  BuildContext context, {
  required String title,
  required String label,
  String initialValue = '',
}) async {
  final strings = AppLocalizations.of(context);
  final controller = TextEditingController(text: initialValue);
  final result = await showDialog<String>(
    context: context,
    builder: (dialogContext) => AlertDialog(
      title: Text(title),
      content: TextField(
        controller: controller,
        autofocus: true,
        maxLength: 80,
        textInputAction: TextInputAction.done,
        decoration: InputDecoration(labelText: label),
        onSubmitted: (value) {
          final trimmed = value.trim();
          if (trimmed.isNotEmpty) Navigator.pop(dialogContext, trimmed);
        },
      ),
      actions: [
        TextButton(
          onPressed: () => Navigator.pop(dialogContext),
          child: Text(strings.cancelAction),
        ),
        FilledButton(
          onPressed: () {
            final trimmed = controller.text.trim();
            if (trimmed.isNotEmpty) Navigator.pop(dialogContext, trimmed);
          },
          child: Text(strings.continueLabel),
        ),
      ],
    ),
  );
  controller.dispose();
  return result;
}

