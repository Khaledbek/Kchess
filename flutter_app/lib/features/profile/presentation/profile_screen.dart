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
    final ratingEntries = _ratingEntries(profile);
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
                                strings.profileDisabled,
                                style: TextStyle(
                                  color: Theme.of(context).colorScheme.error,
                                ),
                              ),
                            ],
                          ],
                        ),
                      ),
                      IconButton(
                        key: const Key('open-coach-from-profile'),
                        tooltip: strings.coach,
                        onPressed: () => unawaited(
                          openCoachSession(
                            context,
                            gateway: controller.gateway,
                            surface: CoachSurface.profile,
                            profileId: profile.id,
                          ),
                        ),
                        icon: const Icon(Icons.school_outlined),
                      ),
                      if (profile.type != ProfileType.localPgnFen)
                        IconButton.filledTonal(
                          tooltip: strings.profileSync,
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
          if (ratingEntries.isNotEmpty) ...[
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
                    for (final entry in ratingEntries)
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
          const SizedBox(height: 8),
          _PlayerDevelopmentSection(controller: controller),
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

class _PlayerDevelopmentSection extends StatefulWidget {
  const _PlayerDevelopmentSection({required this.controller});

  final AppController controller;

  @override
  State<_PlayerDevelopmentSection> createState() =>
      _PlayerDevelopmentSectionState();
}

class _PlayerDevelopmentSectionState extends State<_PlayerDevelopmentSection> {
  Map<String, Object?>? _profile;
  Timer? _timer;
  bool _loading = true;
  bool _failed = false;
  bool _refreshing = false;

  @override
  void initState() {
    super.initState();
    unawaited(_refresh());
    _timer = Timer.periodic(const Duration(milliseconds: 800), (_) {
      unawaited(_refresh());
    });
  }

  @override
  void dispose() {
    _timer?.cancel();
    super.dispose();
  }

  Future<void> _refresh() async {
    if (_refreshing) return;
    _refreshing = true;
    try {
      final value = await widget.controller.gateway.playerProfile();
      if (!mounted) return;
      setState(() {
        _profile = value;
        _failed = false;
        _loading = false;
      });
    } catch (_) {
      if (!mounted) return;
      setState(() {
        _failed = true;
        _loading = false;
      });
    } finally {
      _refreshing = false;
    }
  }

  List<Map<String, Object?>> _maps(Object? value) =>
      (value as List<Object?>? ?? const <Object?>[])
          .whereType<Map<String, Object?>>()
          .toList(growable: false);

  String _patternLabel(AppLocalizations strings, String id) => switch (id) {
    'blunder_control' => strings.profilePatternBlunderControl,
    'missed_opportunities' => strings.profilePatternMissedOpportunities,
    'calculation_consistency' => strings.profilePatternCalculationConsistency,
    'opening_errors' => strings.profilePatternOpeningErrors,
    'endgame_errors' => strings.profilePatternEndgameErrors,
    'time_pressure_errors' => strings.profilePatternTimePressureErrors,
    'move_consistency' => strings.profilePatternMoveConsistency,
    'opening_stability' => strings.profilePatternOpeningStability,
    'endgame_stability' => strings.profilePatternEndgameStability,
    _ => id,
  };

  String _trendLabel(AppLocalizations strings, String trend) => switch (trend) {
    'improving' => strings.profileTrendImproving,
    'worsening' => strings.profileTrendWorsening,
    _ => strings.profileTrendStable,
  };

  String _statusLabel(AppLocalizations strings, String status) => switch (status) {
    'empty' => strings.playerProfileStatusEmpty,
    'syncing_history' => strings.playerProfileStatusSyncingHistory,
    'analyzing' => strings.playerProfileStatusAnalyzing,
    'complete' => strings.playerProfileStatusComplete,
    _ => strings.playerProfileStatusQueued,
  };

  Map<String, Map<String, Object?>> _patternsById(
    Map<String, Object?> profile,
  ) => {
    for (final pattern in _maps(profile['patterns']))
      if (pattern['id'] is String) pattern['id']! as String: pattern,
  };

  List<Map<String, Object?>> _trainingExamples(
    Map<String, Object?> profile,
  ) {
    final byId = _patternsById(profile);
    final ordered = <Map<String, Object?>>[];
    final seen = <String>{};
    for (final priority in _maps(profile['coachPriorities'])) {
      final id = priority['patternId'] as String?;
      final pattern = id == null ? null : byId[id];
      if (pattern == null) continue;
      for (final example in _maps(pattern['examplePositions'])) {
        final gameId = example['gameId'] as String?;
        final ply = example['ply'] as int?;
        if (gameId == null || ply == null) continue;
        final key = '$gameId:$ply';
        if (seen.add(key)) ordered.add(example);
        if (ordered.length >= 3) return ordered;
      }
    }
    for (final pattern in _maps(profile['patterns'])) {
      if (pattern['type'] != 'weakness') continue;
      for (final example in _maps(pattern['examplePositions'])) {
        final gameId = example['gameId'] as String?;
        final ply = example['ply'] as int?;
        if (gameId == null || ply == null) continue;
        final key = '$gameId:$ply';
        if (seen.add(key)) ordered.add(example);
        if (ordered.length >= 3) return ordered;
      }
    }
    return ordered;
  }

  Future<void> _openTraining(Map<String, Object?> example) async {
    final strings = AppLocalizations.of(context);
    final gameId = example['gameId'] as String?;
    final ply = example['ply'] as int?;
    if (gameId == null || ply == null) return;
    try {
      final detail = await widget.controller.gateway.game(gameId);
      if (!mounted) return;
      BoardPosition position = detail.startingPosition;
      final previousPly = ply - 1;
      if (previousPly >= 0 && detail.moves.isNotEmpty) {
        final previous = detail.moves.lastWhere(
          (move) => move.plyIndex <= previousPly,
          orElse: () => detail.moves.first,
        );
        position = previous.positionAfter;
      }
      await openCoachSession(
        context,
        gateway: widget.controller.gateway,
        position: position,
        surface: CoachSurface.training,
        contextId: gameId,
        contextPly: previousPly >= 0 ? previousPly : null,
        profileId: widget.controller.activeProfile?.id,
        blackAtBottom: detail.summary.profileColor == 'black',
      );
    } catch (_) {
      if (!mounted) return;
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text(strings.playerProfileUnavailable)),
      );
    }
  }

  Future<void> _openKnowledgeInspector() async {
    final strings = AppLocalizations.of(context);
    final search = TextEditingController();
    Map<String, Object?>? result;
    var loading = true;

    Future<void> load(StateSetter setDialogState) async {
      setDialogState(() => loading = true);
      try {
        final profileId = widget.controller.activeProfile?.id;
        final value = await widget.controller.gateway.knowledgeInspector(
          <String, Object?>{
            if (profileId != null) 'profileId': profileId,
            if (search.text.trim().isNotEmpty) 'query': search.text.trim(),
            'limit': 12,
          },
        );
        if (!mounted) return;
        final diagnostics = Map<String, Object?>.from(value)
          ..['flutterStartup'] = AppStartupDiagnostics.instance.snapshot();
        setDialogState(() {
          result = diagnostics;
          loading = false;
        });
      } catch (_) {
        if (!mounted) return;
        setDialogState(() {
          result = <String, Object?>{'status': strings.playerProfileUnavailable};
          loading = false;
        });
      }
    }

    await showDialog<void>(
      context: context,
      builder: (dialogContext) => StatefulBuilder(
        builder: (dialogContext, setDialogState) {
          if (loading && result == null) {
            WidgetsBinding.instance.addPostFrameCallback((_) {
              if (dialogContext.mounted) unawaited(load(setDialogState));
            });
          }
          final rendered = result == null
              ? ''
              : const JsonEncoder.withIndent('  ').convert(result);
          return AlertDialog(
            title: Text(strings.diagnosticsTitle),
            content: SizedBox(
              width: 760,
              height: 560,
              child: Column(
                children: [
                  TextField(
                    controller: search,
                    onSubmitted: (_) => unawaited(load(setDialogState)),
                    decoration: InputDecoration(
                      prefixIcon: const Icon(Icons.hub_outlined),
                      suffixIcon: IconButton(
                        onPressed: loading
                            ? null
                            : () => unawaited(load(setDialogState)),
                        icon: const Icon(Icons.search),
                      ),
                    ),
                  ),
                  const SizedBox(height: 12),
                  Expanded(
                    child: loading
                        ? const Center(child: CircularProgressIndicator())
                        : SingleChildScrollView(
                            child: Align(
                              alignment: Alignment.topLeft,
                              child: SelectableText(
                                rendered,
                                style: Theme.of(dialogContext)
                                    .textTheme
                                    .bodySmall
                                    ?.copyWith(fontFamily: 'monospace'),
                              ),
                            ),
                          ),
                  ),
                ],
              ),
            ),
            actions: [
              TextButton(
                onPressed: () => Navigator.of(dialogContext).pop(),
                child: Text(strings.close),
              ),
            ],
          );
        },
      ),
    );
    search.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final theme = Theme.of(context);
    final profile = _profile;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        _ProfileSectionTitle(label: strings.playerProfileTitle),
        Card(
          child: Padding(
            padding: const EdgeInsets.all(18),
            child: _loading
                ? Row(
                    children: [
                      const SizedBox.square(
                        dimension: 20,
                        child: CircularProgressIndicator(strokeWidth: 2),
                      ),
                      const SizedBox(width: 12),
                      Expanded(child: Text(strings.playerProfileLoading)),
                    ],
                  )
                : _failed || profile == null
                ? Text(strings.playerProfileUnavailable)
                : _buildProfile(context, profile, theme, strings),
          ),
        ),
      ],
    );
  }

  Widget _buildProfile(
    BuildContext context,
    Map<String, Object?> profile,
    ThemeData theme,
    AppLocalizations strings,
  ) {
    final confidence = ((profile['confidence'] as num?)?.toDouble() ?? 0.0)
        .clamp(0.0, 1.0);
    final averageAccuracy = (profile['averageAccuracy'] as num?)?.toDouble();
    final estimatedStrength = profile['estimatedStrengthRating'] as int?;
    final estimatedStrengthConfidence =
        ((profile['estimatedStrengthConfidence'] as num?)?.toDouble() ?? 0.0)
            .clamp(0.0, 1.0);
    final analyzedGames = profile['analyzedGames'] as int? ?? 0;
    final background = profile['background'] as Map<String, Object?>? ??
        const <String, Object?>{};
    final total = background['totalGames'] as int? ?? 0;
    final indexed = background['indexedGames'] as int? ?? 0;
    final historicalSample = background['historicalSampleGames'] as int? ?? 0;
    final historicalSampleBudget =
        background['historicalSampleBudget'] as int? ?? historicalSample;
    final samplingCoverage =
        ((background['samplingCoverage'] as num?)?.toDouble() ?? 0.0)
            .clamp(0.0, 1.0)
            .toDouble();
    final interestingGames = background['interestingGames'] as int? ?? 0;
    final enginePromotedGames = background['enginePromotedGames'] as int? ?? 0;
    final relevant = background['relevantGames'] as int? ?? 0;
    final resolvedRelevant = background['resolvedRelevantGames'] as int? ?? 0;
    final reusedAnalysis = background['reusedAnalysisGames'] as int? ?? 0;
    final status = background['status'] as String? ?? 'queued';
    final historyAccounts = background['historyAccounts'] as int? ?? 0;
    final historyDiscoveredAccounts =
        background['historyDiscoveredAccounts'] as int? ?? 0;
    final historyAvailableMonths =
        background['historyAvailableMonths'] as int? ?? 0;
    final historySyncedMonths = background['historySyncedMonths'] as int? ?? 0;
    final historyPendingMonths =
        background['historyPendingMonths'] as int? ?? 0;
    final historyComplete = background['historyComplete'] as bool? ?? true;
    final queuedGames = background['queuedGames'] as int? ?? 0;
    final enginePendingGames = background['enginePendingGames'] as int? ?? 0;
    final liveOverallProgress =
        (background['overallProgress'] as num?)?.toDouble();
    final currentGameProgress =
        (background['currentGameProgress'] as num?)?.toDouble();
    final currentGameDepth = background['currentGameDepth'] as int?;
    final currentGameTargetDepth = background['currentGameTargetDepth'] as int?;
    final workProgress = (liveOverallProgress != null
            ? liveOverallProgress
            : relevant > 0
            ? resolvedRelevant / relevant
            : total <= 0
            ? 0.0
            : indexed / total)
        .clamp(0.0, 1.0)
        .toDouble();
    final priorities = _maps(profile['coachPriorities']);
    final patterns = _patternsById(profile);
    final strengths = (profile['strengths'] as List<Object?>? ?? const <Object?>[])
        .whereType<String>()
        .toList(growable: false);
    final weaknesses =
        (profile['weaknesses'] as List<Object?>? ?? const <Object?>[])
            .whereType<String>()
            .toList(growable: false);
    final examples = _trainingExamples(profile);

    Widget chips(String title, List<String> ids) => Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(title, style: theme.textTheme.titleSmall),
        const SizedBox(height: 8),
        Wrap(
          spacing: 8,
          runSpacing: 8,
          children: [
            for (final id in ids)
              Chip(label: Text(_patternLabel(strings, id))),
          ],
        ),
      ],
    );

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(strings.playerProfileSubtitle),
        const SizedBox(height: 14),
        _ProfileOverviewCard(
          estimatedStrength: estimatedStrength,
          estimatedStrengthConfidence: estimatedStrengthConfidence,
          averageAccuracy: averageAccuracy,
          confidence: confidence,
          coverage: samplingCoverage,
          analyzedGames: analyzedGames,
          strings: strings,
        ),
        const SizedBox(height: 18),
        _ProfileLiveProcessingCard(
          title: strings.playerProfileBackground,
          status: _statusLabel(strings, status),
          progress: workProgress,
          active: status != 'complete' && status != 'empty',
          scanned: indexed,
          total: total,
          sampled: historicalSample,
          sampleBudget: historicalSampleBudget,
          interesting: interestingGames,
          enginePromoted: enginePromotedGames,
          resolved: resolvedRelevant,
          relevant: relevant,
          queued: queuedGames,
          enginePending: enginePendingGames,
          currentGameProgress: currentGameProgress,
          currentDepth: currentGameDepth,
          targetDepth: currentGameTargetDepth,
          strings: strings,
        ),
        if (historyAccounts > 0 || !historyComplete) ...[
          const SizedBox(height: 10),
          Text(
            strings.playerProfileHistoryProgress(
              historySyncedMonths,
              historyAvailableMonths,
              historyDiscoveredAccounts,
              historyAccounts,
            ),
            style: theme.textTheme.bodySmall?.copyWith(
              color: theme.colorScheme.onSurfaceVariant,
            ),
          ),
          if (historyPendingMonths > 0)
            Text(
              strings.playerProfileHistoryPending(historyPendingMonths),
              style: theme.textTheme.bodySmall?.copyWith(
                color: theme.colorScheme.onSurfaceVariant,
              ),
            ),
        ],
        if (reusedAnalysis > 0) ...[
          const SizedBox(height: 8),
          Wrap(
            spacing: 8,
            runSpacing: 6,
            children: [
              Chip(
                visualDensity: VisualDensity.compact,
                label: Text(strings.playerProfileReusedAnalysis(reusedAnalysis)),
              ),
            ],
          ),
        ],
        const SizedBox(height: 18),
        if (priorities.isEmpty && strengths.isEmpty && weaknesses.isEmpty)
          Text(strings.playerProfileNoPatterns)
        else ...[
          if (priorities.isNotEmpty) ...[
            Text(strings.playerProfileMainGoal, style: theme.textTheme.titleSmall),
            const SizedBox(height: 8),
            _ProfilePriorityTile(
              label: _patternLabel(
                strings,
                priorities.first['patternId'] as String? ?? '',
              ),
              trend: _trendLabel(
                strings,
                patterns[priorities.first['patternId']]?['trend'] as String? ??
                    'stable',
              ),
              primary: true,
            ),
          ],
          if (priorities.length > 1) ...[
            const SizedBox(height: 14),
            Text(
              strings.playerProfileSecondaryGoals,
              style: theme.textTheme.titleSmall,
            ),
            const SizedBox(height: 8),
            for (final priority in priorities.skip(1).take(2))
              Padding(
                padding: const EdgeInsets.only(bottom: 6),
                child: _ProfilePriorityTile(
                  label: _patternLabel(
                    strings,
                    priority['patternId'] as String? ?? '',
                  ),
                  trend: _trendLabel(
                    strings,
                    patterns[priority['patternId']]?['trend'] as String? ??
                        'stable',
                  ),
                ),
              ),
          ],
          if (strengths.isNotEmpty) ...[
            const SizedBox(height: 16),
            chips(strings.playerProfileStrengths, strengths.take(3).toList()),
          ],
          if (weaknesses.isNotEmpty) ...[
            const SizedBox(height: 16),
            chips(strings.playerProfileWeaknesses, weaknesses.take(3).toList()),
          ],
        ],
        if (examples.isNotEmpty) ...[
          const SizedBox(height: 18),
          Text(strings.playerProfileTrainFromGames, style: theme.textTheme.titleSmall),
          const SizedBox(height: 8),
          Wrap(
            spacing: 8,
            runSpacing: 8,
            children: [
              for (var index = 0; index < examples.length; index++)
                OutlinedButton.icon(
                  onPressed: () => unawaited(_openTraining(examples[index])),
                  icon: const Icon(Icons.school_outlined, size: 18),
                  label: Text(strings.playerProfileTrainingPosition(index + 1)),
                ),
            ],
          ),
        ],
        const SizedBox(height: 18),
        OutlinedButton.icon(
          onPressed: () => unawaited(_openKnowledgeInspector()),
          icon: const Icon(Icons.hub_outlined, size: 18),
          label: Text(strings.diagnosticsTitle),
        ),
      ],
    );
  }
}


class _ProfileOverviewCard extends StatelessWidget {
  const _ProfileOverviewCard({
    required this.estimatedStrength,
    required this.estimatedStrengthConfidence,
    required this.averageAccuracy,
    required this.confidence,
    required this.coverage,
    required this.analyzedGames,
    required this.strings,
  });

  final int? estimatedStrength;
  final double estimatedStrengthConfidence;
  final double? averageAccuracy;
  final double confidence;
  final double coverage;
  final int analyzedGames;
  final AppLocalizations strings;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final readiness = (confidence * 100).round();
    final coveragePercent = (coverage * 100).round();
    return Container(
      width: double.infinity,
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(
        color: theme.colorScheme.surfaceContainerHighest.withValues(alpha: 0.34),
        borderRadius: BorderRadius.circular(18),
        border: Border.all(color: theme.colorScheme.outlineVariant),
      ),
      child: LayoutBuilder(
        builder: (context, constraints) {
          final compact = constraints.maxWidth < 520;
          final strength = Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text(
                strings.playerProfileEstimatedStrength,
                style: theme.textTheme.labelLarge?.copyWith(
                  color: theme.colorScheme.onSurfaceVariant,
                ),
              ),
              const SizedBox(height: 4),
              Text(
                estimatedStrength?.toString() ?? '—',
                style: theme.textTheme.headlineMedium?.copyWith(
                  fontWeight: FontWeight.w800,
                ),
              ),
              if (estimatedStrength != null && estimatedStrengthConfidence > 0)
                Text(
                  strings.playerProfileStrengthEstimateConfidence(
                    (estimatedStrengthConfidence * 100).round(),
                  ),
                  style: theme.textTheme.bodySmall?.copyWith(
                    color: theme.colorScheme.onSurfaceVariant,
                  ),
                ),
            ],
          );
          final details = Wrap(
            spacing: 10,
            runSpacing: 10,
            children: [
              if (averageAccuracy != null)
                _ProfileCompactMetric(
                  icon: Icons.gps_fixed_rounded,
                  label: strings.playerProfileAverageAccuracy,
                  value: '${averageAccuracy!.toStringAsFixed(1)}%',
                ),
              _ProfileCompactMetric(
                icon: Icons.verified_outlined,
                label: strings.playerProfileConfidenceShort,
                value: '$readiness%',
              ),
              _ProfileCompactMetric(
                icon: Icons.donut_large_outlined,
                label: strings.playerProfileCoverageShort,
                value: '$coveragePercent%',
              ),
              _ProfileCompactMetric(
                icon: Icons.analytics_outlined,
                label: strings.playerProfileAnalyzedEvidence,
                value: '$analyzedGames',
              ),
            ],
          );
          if (compact) {
            return Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [strength, const SizedBox(height: 14), details],
            );
          }
          return Row(
            crossAxisAlignment: CrossAxisAlignment.center,
            children: [
              SizedBox(width: 190, child: strength),
              const SizedBox(width: 18),
              Expanded(child: details),
            ],
          );
        },
      ),
    );
  }
}

class _ProfileCompactMetric extends StatelessWidget {
  const _ProfileCompactMetric({
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
    return Container(
      constraints: const BoxConstraints(minWidth: 126),
      padding: const EdgeInsets.symmetric(horizontal: 11, vertical: 9),
      decoration: BoxDecoration(
        color: theme.colorScheme.surface,
        borderRadius: BorderRadius.circular(13),
        border: Border.all(color: theme.colorScheme.outlineVariant),
      ),
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          Icon(icon, size: 17, color: theme.colorScheme.primary),
          const SizedBox(width: 8),
          Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            mainAxisSize: MainAxisSize.min,
            children: [
              Text(
                value,
                style: theme.textTheme.titleSmall?.copyWith(
                  fontWeight: FontWeight.w800,
                ),
              ),
              Text(
                label,
                style: theme.textTheme.labelSmall?.copyWith(
                  color: theme.colorScheme.onSurfaceVariant,
                ),
              ),
            ],
          ),
        ],
      ),
    );
  }
}

class _ProfileLiveProcessingCard extends StatelessWidget {
  const _ProfileLiveProcessingCard({
    required this.title,
    required this.status,
    required this.progress,
    required this.active,
    required this.scanned,
    required this.total,
    required this.sampled,
    required this.sampleBudget,
    required this.interesting,
    required this.enginePromoted,
    required this.resolved,
    required this.relevant,
    required this.queued,
    required this.enginePending,
    required this.currentGameProgress,
    required this.currentDepth,
    required this.targetDepth,
    required this.strings,
  });

  final String title;
  final String status;
  final double progress;
  final bool active;
  final int scanned;
  final int total;
  final int sampled;
  final int sampleBudget;
  final int interesting;
  final int enginePromoted;
  final int resolved;
  final int relevant;
  final int queued;
  final int enginePending;
  final double? currentGameProgress;
  final int? currentDepth;
  final int? targetDepth;
  final AppLocalizations strings;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return Container(
      width: double.infinity,
      padding: const EdgeInsets.all(14),
      decoration: BoxDecoration(
        color: theme.colorScheme.surfaceContainerHighest.withValues(alpha: 0.48),
        borderRadius: BorderRadius.circular(16),
        border: Border.all(color: theme.colorScheme.outlineVariant),
      ),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          TweenAnimationBuilder<double>(
            tween: Tween<double>(begin: 0, end: progress.clamp(0.0, 1.0).toDouble()),
            duration: const Duration(milliseconds: 650),
            curve: Curves.easeOutCubic,
            builder: (context, value, _) => SizedBox.square(
              dimension: 64,
              child: Stack(
                alignment: Alignment.center,
                children: [
                  CircularProgressIndicator(
                    value: value,
                    strokeWidth: 5,
                    backgroundColor: theme.colorScheme.surfaceContainerHighest,
                  ),
                  Text(
                    '${(value * 100).toStringAsFixed(1)}%',
                    style: theme.textTheme.labelSmall?.copyWith(
                      fontWeight: FontWeight.w800,
                    ),
                  ),
                ],
              ),
            ),
          ),
          const SizedBox(width: 14),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Row(
                  children: [
                    Expanded(
                      child: Text(
                        title,
                        style: theme.textTheme.titleSmall?.copyWith(
                          fontWeight: FontWeight.w800,
                        ),
                      ),
                    ),
                    if (active)
                      const SizedBox.square(
                        dimension: 16,
                        child: CircularProgressIndicator(strokeWidth: 2),
                      )
                    else
                      Icon(
                        Icons.check_circle_outline_rounded,
                        size: 18,
                        color: theme.colorScheme.primary,
                      ),
                  ],
                ),
                const SizedBox(height: 3),
                AnimatedSwitcher(
                  duration: const Duration(milliseconds: 250),
                  child: Text(
                    status,
                    key: ValueKey(status),
                    style: theme.textTheme.bodyMedium?.copyWith(
                      color: theme.colorScheme.onSurfaceVariant,
                    ),
                  ),
                ),
                const SizedBox(height: 10),
                LayoutBuilder(
                  builder: (context, constraints) {
                    final cellWidth = constraints.maxWidth >= 620
                        ? (constraints.maxWidth - 24) / 4
                        : constraints.maxWidth >= 360
                        ? (constraints.maxWidth - 8) / 2
                        : constraints.maxWidth;
                    return Wrap(
                      spacing: 8,
                      runSpacing: 8,
                      children: [
                        SizedBox(
                          width: cellWidth,
                          child: _LiveProfileStep(
                            icon: Icons.library_books_outlined,
                            label: strings.playerProfileLiveLibrary(scanned, total),
                          ),
                        ),
                        SizedBox(
                          width: cellWidth,
                          child: _LiveProfileStep(
                            icon: Icons.filter_alt_outlined,
                            label: strings.playerProfileLiveSample(sampled, sampleBudget),
                          ),
                        ),
                        SizedBox(
                          width: cellWidth,
                          child: _LiveProfileStep(
                            icon: Icons.auto_awesome_outlined,
                            label: strings.playerProfileLiveInteresting(interesting),
                          ),
                        ),
                        SizedBox(
                          width: cellWidth,
                          child: _LiveProfileStep(
                            icon: Icons.memory_rounded,
                            label: strings.playerProfileLiveEngine(enginePromoted),
                          ),
                        ),
                      ],
                    );
                  },
                ),
                if (relevant > 0 || queued > 0 || enginePending > 0) ...[
                  const SizedBox(height: 9),
                  Wrap(
                    spacing: 8,
                    runSpacing: 7,
                    children: [
                      if (relevant > 0)
                        _LiveProfilePill(
                          label: strings.playerProfileLiveEvidence(resolved, relevant),
                        ),
                      if (queued > 0 || enginePending > 0)
                        _LiveProfilePill(
                          label: strings.playerProfileLiveQueue(queued + enginePending),
                        ),
                      if (currentDepth != null && targetDepth != null)
                        _LiveProfilePill(
                          label: strings.playerProfileLiveDepth(
                            currentDepth!,
                            targetDepth!,
                          ),
                        ),
                    ],
                  ),
                ],
                if (currentGameProgress != null &&
                    currentGameProgress! > 0 &&
                    currentGameProgress! < 1) ...[
                  const SizedBox(height: 10),
                  TweenAnimationBuilder<double>(
                    tween: Tween<double>(
                      begin: 0,
                      end: currentGameProgress!.clamp(0.0, 1.0).toDouble(),
                    ),
                    duration: const Duration(milliseconds: 500),
                    curve: Curves.easeOut,
                    builder: (context, value, _) => Text(
                      strings.playerProfileLiveCurrentGame(
                        (value * 100).round(),
                      ),
                      style: theme.textTheme.bodySmall?.copyWith(
                        color: theme.colorScheme.onSurfaceVariant,
                      ),
                    ),
                  ),
                ],
              ],
            ),
          ),
        ],
      ),
    );
  }
}

class _LiveProfileStep extends StatelessWidget {
  const _LiveProfileStep({required this.icon, required this.label});

  final IconData icon;
  final String label;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 9),
      decoration: BoxDecoration(
        color: theme.colorScheme.surface,
        borderRadius: BorderRadius.circular(12),
        border: Border.all(color: theme.colorScheme.outlineVariant),
      ),
      child: Row(
        children: [
          Icon(icon, size: 17, color: theme.colorScheme.primary),
          const SizedBox(width: 8),
          Expanded(
            child: Text(
              label,
              maxLines: 1,
              overflow: TextOverflow.ellipsis,
              style: theme.textTheme.labelMedium?.copyWith(
                fontWeight: FontWeight.w700,
              ),
            ),
          ),
        ],
      ),
    );
  }
}

class _LiveProfilePill extends StatelessWidget {
  const _LiveProfilePill({required this.label});
  final String label;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 9, vertical: 5),
      decoration: BoxDecoration(
        color: theme.colorScheme.surface,
        borderRadius: BorderRadius.circular(999),
        border: Border.all(color: theme.colorScheme.outlineVariant),
      ),
      child: Text(label, style: theme.textTheme.labelSmall),
    );
  }
}

class _ProfilePriorityTile extends StatelessWidget {
  const _ProfilePriorityTile({
    required this.label,
    required this.trend,
    this.primary = false,
  });

  final String label;
  final String trend;
  final bool primary;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return Container(
      width: double.infinity,
      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 10),
      decoration: BoxDecoration(
        color: primary
            ? theme.colorScheme.primaryContainer
            : theme.colorScheme.surfaceContainerHighest,
        borderRadius: BorderRadius.circular(12),
      ),
      child: Row(
        children: [
          Icon(
            primary ? Icons.flag_outlined : Icons.adjust_rounded,
            size: 18,
          ),
          const SizedBox(width: 10),
          Expanded(
            child: Text(
              label,
              style: theme.textTheme.bodyMedium?.copyWith(
                fontWeight: FontWeight.w700,
              ),
            ),
          ),
          const SizedBox(width: 10),
          Text(
            trend,
            style: theme.textTheme.labelMedium?.copyWith(
              color: theme.colorScheme.onSurfaceVariant,
            ),
          ),
        ],
      ),
    );
  }
}

