part of '../../../ui/app_root.dart';

class FavoriteCollectionScreen extends StatelessWidget {
  const FavoriteCollectionScreen({
    required this.controller,
    required this.collection,
    super.key,
  });

  final AppController controller;
  final FavoriteCollection collection;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final games = controller.favoriteGames
        .where(
          (game) => game.favorite && game.favoriteCollectionId == collection.id,
        )
        .toList(growable: false);
    return Scaffold(
      appBar: AppBar(title: Text(collection.name)),
      body: games.isEmpty
          ? Center(
              child: Padding(
                padding: const EdgeInsets.all(28),
                child: _FavoriteEmptyCard(
                  icon: Icons.folder_open_outlined,
                  text: strings.favoriteEmptyCollection,
                ),
              ),
            )
          : ListView.separated(
              padding: const EdgeInsets.all(16),
              itemCount: games.length,
              separatorBuilder: (_, _) => const SizedBox(height: 12),
              itemBuilder: (context, index) {
                final game = games[index];
                return _GameCard(
                  game: game,
                  online: game.providerGameId != null,
                  onOpen: () => _openAnalysisAndRefreshSettings(
                    context: context,
                    controller: controller,
                    game: game,
                  ),
                  onToggleFavorite: () => controller.toggleFavorite(game),
                  onSaveToDownloads: null,
                  onMoveFavorite: () =>
                      _showFavoriteCollectionPicker(context, controller, game),
                  onDelete: null,
                );
              },
            ),
    );
  }
}

class _FavoriteCollectionCard extends StatelessWidget {
  const _FavoriteCollectionCard({
    required this.collection,
    required this.onOpen,
    required this.onRename,
    required this.onDelete,
  });

  final FavoriteCollection collection;
  final VoidCallback onOpen;
  final VoidCallback onRename;
  final VoidCallback onDelete;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final scheme = Theme.of(context).colorScheme;
    return Card(
      child: ListTile(
        onTap: onOpen,
        contentPadding: const EdgeInsets.fromLTRB(16, 8, 8, 8),
        leading: Container(
          width: 44,
          height: 44,
          alignment: Alignment.center,
          decoration: BoxDecoration(
            color: scheme.primaryContainer,
            borderRadius: BorderRadius.circular(12),
          ),
          child: Icon(
            Icons.folder_special_outlined,
            color: scheme.onPrimaryContainer,
          ),
        ),
        title: Text(
          collection.name,
          maxLines: 1,
          overflow: TextOverflow.ellipsis,
          style: const TextStyle(fontWeight: FontWeight.w700),
        ),
        subtitle: Text('${collection.gameCount} ${strings.games}'),
        trailing: PopupMenuButton<String>(
          onSelected: (value) {
            if (value == 'rename') onRename();
            if (value == 'delete') onDelete();
          },
          itemBuilder: (context) => [
            PopupMenuItem(
              value: 'rename',
              child: ListTile(
                dense: true,
                leading: const Icon(Icons.edit_outlined),
                title: Text(strings.favoriteRenameCollection),
              ),
            ),
            PopupMenuItem(
              value: 'delete',
              child: ListTile(
                dense: true,
                leading: const Icon(Icons.delete_outline),
                title: Text(strings.favoriteDeleteCollection),
              ),
            ),
          ],
        ),
      ),
    );
  }
}

class _FavoriteEmptyCard extends StatelessWidget {
  const _FavoriteEmptyCard({required this.icon, required this.text});

  final IconData icon;
  final String text;

  @override
  Widget build(BuildContext context) => Card(
    child: Padding(
      padding: const EdgeInsets.all(22),
      child: Row(
        children: [
          Icon(icon, color: Theme.of(context).colorScheme.onSurfaceVariant),
          const SizedBox(width: 12),
          Expanded(
            child: Text(
              text,
              style: TextStyle(
                color: Theme.of(context).colorScheme.onSurfaceVariant,
              ),
            ),
          ),
        ],
      ),
    ),
  );
}

