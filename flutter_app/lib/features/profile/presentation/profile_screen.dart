part of '../../../ui/app_root.dart';

class ProfileScreen extends StatelessWidget {
  const ProfileScreen({required this.controller, super.key});
  final AppController controller;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final profile = controller.activeProfile!;
    final mergeTargets = controller.profiles
        .where((value) => value.type != ProfileType.localPgnFen)
        .toList(growable: false);
    return Scaffold(
      appBar: MediaQuery.sizeOf(context).width >= 900
          ? AppBar(title: Text(strings.profile))
          : null,
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          Card(
            child: Padding(
              padding: const EdgeInsets.all(20),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Row(
                    children: [
                      Container(
                        padding: const EdgeInsets.all(3),
                        decoration: BoxDecoration(
                          shape: BoxShape.circle,
                          border: Border.all(
                            color: Theme.of(context).colorScheme.outlineVariant,
                            width: 2,
                          ),
                        ),
                        child: CircleAvatar(
                          radius: 36,
                          backgroundColor: Theme.of(context)
                              .colorScheme
                              .primaryContainer,
                          child: _profileAvatar(profile, iconSize: 36),
                        ),
                      ),
                      const SizedBox(width: 18),
                      Expanded(
                        child: Column(
                          crossAxisAlignment: CrossAxisAlignment.start,
                          children: [
                            Text(
                              [
                                if (profile.title != null) profile.title,
                                profile.displayName,
                              ].whereType<String>().join(' '),
                              style: Theme.of(context).textTheme.headlineSmall,
                            ),
                            const SizedBox(height: 8),
                            Wrap(
                              spacing: 8,
                              runSpacing: 6,
                              children: [
                                _ProfileTag(
                                  icon: Icons.hub_outlined,
                                  label: _profileTypeLabel(
                                    strings,
                                    profile.type,
                                  ),
                                ),
                                if (profile.providerUsername != null)
                                  _ProfileTag(
                                    icon: Icons.alternate_email,
                                    label: profile.providerUsername!,
                                  ),
                                if (profile.flair != null)
                                  _ProfileTag(
                                    icon: Icons.auto_awesome,
                                    label: profile.flair!,
                                  ),
                              ],
                            ),
                            if (profile.providerDisabled) ...[
                              const SizedBox(height: 8),
                              Text(
                                'Profil deaktiviert',
                                style: TextStyle(
                                  color: Theme.of(context).colorScheme.error,
                                ),
                              ),
                            ],
                          ],
                        ),
                      ),
                      if (profile.type != ProfileType.localPgnFen)
                        IconButton.filledTonal(
                          tooltip: 'Synchronisieren',
                          onPressed: controller.providerSyncing
                              ? null
                              : controller.syncProvider,
                          icon: const Icon(Icons.sync),
                        ),
                    ],
                  ),
                  if (profile.type == ProfileType.localPgnFen &&
                      mergeTargets.isNotEmpty)
                    Align(
                      alignment: AlignmentDirectional.centerEnd,
                      child: FilledButton.tonalIcon(
                        key: const Key('merge-local-profile'),
                        onPressed: () => _showMergeLocalProfileDialog(
                          context,
                          controller,
                          profile,
                        ),
                        icon: const Icon(Icons.merge_type, size: 18),
                        label: Text(
                          _profileMergeText(
                            context,
                            de: 'Mit Online-Profil zusammenführen',
                            en: 'Merge with online profile',
                            ar: 'دمج مع ملف شخصي عبر الإنترنت',
                          ),
                        ),
                      ),
                    ),
                  Align(
                    alignment: AlignmentDirectional.centerEnd,
                    child: TextButton.icon(
                      key: const Key('delete-active-profile'),
                      onPressed: () =>
                          _confirmDeleteProfile(context, controller, profile),
                      icon: const Icon(Icons.delete_outline, size: 18),
                      label: Text(strings.deleteAccount),
                      style: TextButton.styleFrom(
                        foregroundColor: Theme.of(context).colorScheme.error,
                      ),
                    ),
                  ),
                ],
              ),
            ),
          ),
          if (controller.providerNotice != null)
            Padding(
              padding: const EdgeInsets.only(top: 12),
              child: Text(controller.providerNotice!),
            ),
          if (controller.providerSyncing)
            const Padding(
              padding: EdgeInsets.only(top: 12),
              child: LinearProgressIndicator(),
            ),
          const SizedBox(height: 8),
          if (_ratingEntries(profile).isNotEmpty) ...[
            _ProfileSectionTitle(label: strings.profileRatings),
            LayoutBuilder(
              builder: (context, constraints) {
                final cardWidth = constraints.maxWidth >= 760
                    ? (constraints.maxWidth - 36) / 4
                    : constraints.maxWidth >= 480
                    ? (constraints.maxWidth - 12) / 2
                    : constraints.maxWidth;
                return Wrap(
                  spacing: 12,
                  runSpacing: 12,
                  children: [
                    for (final entry in _ratingEntries(profile))
                      SizedBox(
                        width: cardWidth,
                        child: _RatingCard(
                          label: _ratingLabel(strings, entry.$1),
                          rating: entry.$2,
                        ),
                      ),
                  ],
                );
              },
            ),
          ],
          const SizedBox(height: 8),
          _ProfileSectionTitle(label: strings.profileGameOverview),
          LayoutBuilder(
            builder: (context, constraints) {
              final overview = _gameOverview(profile);
              final values = <(IconData, String, int?)>[
                (Icons.sports_esports_outlined, strings.games, overview.games),
                (
                  Icons.emoji_events_outlined,
                  strings.profileWins,
                  overview.wins,
                ),
                (Icons.balance_outlined, strings.profileDraws, overview.draws),
                (Icons.close_rounded, strings.profileLosses, overview.losses),
              ].where((entry) => entry.$3 != null).toList(growable: false);
              final width = constraints.maxWidth >= 760
                  ? (constraints.maxWidth - 36) / 4
                  : constraints.maxWidth >= 480
                  ? (constraints.maxWidth - 12) / 2
                  : constraints.maxWidth;
              return Wrap(
                spacing: 12,
                runSpacing: 12,
                children: [
                  for (final value in values)
                    SizedBox(
                      width: width,
                      child: _ProfileMetricCard(
                        icon: value.$1,
                        label: value.$2,
                        value: '${value.$3}',
                      ),
                    ),
                ],
              );
            },
          ),
        ],
      ),
    );
  }

  List<(String, int)> _ratingEntries(AppProfile profile) {
    final entries = <(String, int)>[];
    for (final performance
        in controller.providerOverview?.stats ??
            const <ProviderPerformance>[]) {
      final rating = performance.currentRating;
      if (rating != null) entries.add((performance.key, rating));
    }
    if (profile.fide != null) entries.add(('fide', profile.fide!));
    return entries;
  }

  String _ratingLabel(AppLocalizations strings, String key) {
    final normalized = key.toLowerCase().replaceAll('chess_', '');
    return switch (normalized) {
      'rapid' => strings.ratingRapid,
      'blitz' => strings.ratingBlitz,
      'bullet' => strings.ratingBullet,
      'daily' => strings.ratingDaily,
      'correspondence' => strings.ratingDaily,
      'classical' => strings.ratingClassical,
      'chess960' => strings.ratingChess960,
      '960' => strings.ratingChess960,
      'fide' => strings.ratingFide,
      _ => key.replaceAll('_', ' ').toUpperCase(),
    };
  }

  ({int? games, int? wins, int? draws, int? losses}) _gameOverview(
    AppProfile profile,
  ) {
    final stats =
        controller.providerOverview?.stats ?? const <ProviderPerformance>[];
    int? sum(Iterable<int?> values) {
      final present = values.whereType<int>().toList(growable: false);
      return present.isEmpty ? null : present.fold<int>(0, (a, b) => a + b);
    }

    final summedGames = sum(stats.map((value) => value.games));
    final games =
        profile.providerGames ??
        summedGames ??
        (profile.type == ProfileType.localPgnFen
            ? controller.games.length
            : null);
    return (
      games: games,
      wins: profile.providerWins ?? sum(stats.map((value) => value.wins)),
      draws: profile.providerDraws ?? sum(stats.map((value) => value.draws)),
      losses: profile.providerLosses ?? sum(stats.map((value) => value.losses)),
    );
  }
}

class _ProfileSectionTitle extends StatelessWidget {
  const _ProfileSectionTitle({required this.label});

  final String label;

  @override
  Widget build(BuildContext context) => Padding(
    padding: const EdgeInsets.fromLTRB(4, 14, 4, 10),
    child: Text(
      label.toUpperCase(),
      style: Theme.of(context).textTheme.labelMedium?.copyWith(
        color: Theme.of(context).colorScheme.onSurfaceVariant,
        fontWeight: FontWeight.w700,
        letterSpacing: 1.1,
      ),
    ),
  );
}

class _RatingCard extends StatelessWidget {
  const _RatingCard({required this.label, required this.rating});

  final String label;
  final int rating;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return Card(
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 18, vertical: 16),
        child: Row(
          children: [
            Icon(
              Icons.speed_rounded,
              color: theme.colorScheme.primary,
              size: 22,
            ),
            const SizedBox(width: 12),
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(
                    label,
                    style: theme.textTheme.labelMedium?.copyWith(
                      color: theme.colorScheme.onSurfaceVariant,
                      fontWeight: FontWeight.w600,
                    ),
                  ),
                  const SizedBox(height: 2),
                  Text(
                    '$rating',
                    style: theme.textTheme.headlineSmall?.copyWith(
                      fontWeight: FontWeight.w800,
                    ),
                  ),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }
}

class _ProfileMetricCard extends StatelessWidget {
  const _ProfileMetricCard({
    required this.icon,
    required this.label,
    required this.value,
  });

  final IconData icon;
  final String label;
  final String value;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Row(
          children: [
            Container(
              width: 40,
              height: 40,
              decoration: BoxDecoration(
                color: theme.colorScheme.surfaceContainerHighest,
                borderRadius: BorderRadius.circular(12),
              ),
              alignment: Alignment.center,
              child: Icon(icon, size: 20),
            ),
            const SizedBox(width: 12),
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(
                    value,
                    style: theme.textTheme.titleLarge?.copyWith(
                      fontWeight: FontWeight.w800,
                    ),
                  ),
                  Text(
                    label,
                    style: theme.textTheme.bodySmall?.copyWith(
                      color: theme.colorScheme.onSurfaceVariant,
                    ),
                  ),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }
}

