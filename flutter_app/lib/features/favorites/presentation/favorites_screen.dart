part of '../../../ui/app_root.dart';

class FavoritesScreen extends StatelessWidget {
  const FavoritesScreen({required this.controller, super.key});

  final AppController controller;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final looseFavorites = controller.favoriteGames
        .where((game) => game.favorite && game.favoriteCollectionId == null)
        .toList(growable: false);

    Future<void> createCollection() async {
      final name = await _showFavoriteCollectionNameDialog(
        context,
        title: strings.favoriteCreateCollection,
        label: strings.favoriteCollectionName,
      );
      if (name == null || !context.mounted) return;
      try {
        await controller.createFavoriteCollection(name);
      } catch (error) {
        if (context.mounted) {
          ScaffoldMessenger.of(context)
              .showSnackBar(SnackBar(content: Text(error.toString())));
        }
      }
    }

    return Scaffold(
      appBar: MediaQuery.sizeOf(context).width >= 900
          ? AppBar(title: Text(strings.favorites))
          : null,
      body: ListView(
        padding: const EdgeInsets.fromLTRB(16, 12, 16, 28),
        children: [
          Row(
            children: [
              Expanded(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(
                      strings.favoriteCollectionsTitle,
                      style: Theme.of(context).textTheme.titleLarge,
                    ),
                    const SizedBox(height: 4),
                    Text(
                      strings.favoriteCollectionRule,
                      style: Theme.of(context).textTheme.bodySmall?.copyWith(
                        color: Theme.of(context).colorScheme.onSurfaceVariant,
                      ),
                    ),
                  ],
                ),
              ),
              const SizedBox(width: 12),
              FilledButton.icon(
                key: const Key('create-favorite-collection'),
                onPressed: createCollection,
                icon: const Icon(Icons.create_new_folder_outlined),
                label: Text(strings.favoriteCreateCollection),
              ),
            ],
          ),
          const SizedBox(height: 14),
          if (controller.favoriteCollections.isEmpty)
            _FavoriteEmptyCard(
              icon: Icons.folder_open_outlined,
              text: strings.favoriteNoCollections,
            )
          else
            LayoutBuilder(
              builder: (context, constraints) {
                final width = constraints.maxWidth >= 760
                    ? (constraints.maxWidth - 12) / 2
                    : constraints.maxWidth;
                return Wrap(
                  spacing: 12,
                  runSpacing: 12,
                  children: [
                    for (final collection in controller.favoriteCollections)
                      SizedBox(
                        width: width,
                        child: _FavoriteCollectionCard(
                          collection: collection,
                          onOpen: () => Navigator.of(context).push(
                            MaterialPageRoute<void>(
                              builder: (_) => FavoriteCollectionScreen(
                                controller: controller,
                                collection: collection,
                              ),
                            ),
                          ),
                          onRename: () async {
                            final name =
                                await _showFavoriteCollectionNameDialog(
                                  context,
                                  title: strings.favoriteRenameCollection,
                                  label: strings.favoriteCollectionName,
                                  initialValue: collection.name,
                                );
                            if (name == null || !context.mounted) return;
                            try {
                              await controller.renameFavoriteCollection(
                                collection,
                                name,
                              );
                            } catch (error) {
                              if (context.mounted) {
                                ScaffoldMessenger.of(context).showSnackBar(
                                  SnackBar(content: Text(error.toString())),
                                );
                              }
                            }
                          },
                          onDelete: () async {
                            final confirmed = await showDialog<bool>(
                              context: context,
                              builder: (dialogContext) => AlertDialog(
                                title: Text(strings.favoriteDeleteCollection),
                                content: Text(
                                  strings.favoriteDeleteCollectionBody,
                                ),
                                actions: [
                                  TextButton(
                                    onPressed: () =>
                                        Navigator.pop(dialogContext, false),
                                    child: Text(strings.cancelAction),
                                  ),
                                  FilledButton(
                                    onPressed: () =>
                                        Navigator.pop(dialogContext, true),
                                    child: Text(strings.deleteAction),
                                  ),
                                ],
                              ),
                            );
                            if (confirmed != true || !context.mounted) return;
                            try {
                              await controller.deleteFavoriteCollection(
                                collection,
                              );
                            } catch (error) {
                              if (context.mounted) {
                                ScaffoldMessenger.of(context).showSnackBar(
                                  SnackBar(content: Text(error.toString())),
                                );
                              }
                            }
                          },
                        ),
                      ),
                  ],
                );
              },
            ),
          const SizedBox(height: 26),
          Text(
            strings.favoriteLooseTitle,
            style: Theme.of(context).textTheme.titleLarge,
          ),
          const SizedBox(height: 12),
          if (looseFavorites.isEmpty)
            _FavoriteEmptyCard(
              icon: Icons.favorite_border,
              text: strings.favoriteNoLooseGames,
            )
          else ...[
            for (final game in looseFavorites) ...[
              _GameCard(
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
              ),
              const SizedBox(height: 12),
            ],
          ],
        ],
      ),
    );
  }
}

