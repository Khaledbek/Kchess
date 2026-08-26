part of '../../../ui/app_root.dart';

class GamesScreen extends StatefulWidget {
  const GamesScreen({required this.controller, this.savedFilter, super.key});
  final AppController controller;
  final String? savedFilter;

  @override
  State<GamesScreen> createState() => _GamesScreenState();
}

/// In-memory only: filters survive opening a game and switching sections, but
/// intentionally reset when the app process is started again.
class _GamesFilterSession {
  String query = '';
  String outcome = 'all';
  String color = 'all';
  final Set<String> timeControls = <String>{};
  String sort = 'newest';
  double scrollOffset = 0;
}

final Map<String, _GamesFilterSession> _gamesFilterSessions = {};

class _GamesScreenState extends State<GamesScreen> {
  AppController get controller => widget.controller;
  late final _GamesFilterSession _filterSession;
  late final TextEditingController _search;
  late final ScrollController _scrollController;
  late String _outcome;
  late String _color;
  late Set<String> _timeControls;
  late String _sort;
  List<GameSummary> _filteredGames = const [];
  int _queryGeneration = 0;
  bool _queryEnabled = true;

  @override
  void initState() {
    super.initState();
    final sessionKey =
        '${controller.activeProfile?.id ?? 'no-profile'}:${widget.savedFilter ?? 'games'}';
    _filterSession = _gamesFilterSessions.putIfAbsent(
      sessionKey,
      _GamesFilterSession.new,
    );
    _search = TextEditingController(text: _filterSession.query);
    _scrollController = ScrollController(
      initialScrollOffset: _filterSession.scrollOffset,
    )..addListener(_persistScrollOffset);
    _outcome = _filterSession.outcome;
    _color = _filterSession.color;
    _timeControls = Set<String>.from(_filterSession.timeControls);
    _sort = _filterSession.sort;
    _filteredGames = controller.games;
    controller.addListener(_onControllerChanged);
    unawaited(_refreshNativeGames());
  }

  void _onControllerChanged() => unawaited(_refreshNativeGames());

  Future<void> _refreshNativeGames() async {
    if (!mounted || !_queryEnabled) return;
    final generation = ++_queryGeneration;
    final online = controller.activeProfile?.type != ProfileType.localPgnFen;
    final librarySection = widget.savedFilter != null;
    try {
      final games = await controller.queryGames(
        GameQuery(
          search: _search.text,
          outcome: _outcome,
          color: _color,
          timeControls: _timeControls.toList(growable: false),
          sort: _sort,
          month: controller.selectedMonth,
          favoriteOnly: widget.savedFilter == 'favorite',
          applyMonth: online && !librarySection,
        ),
      );
      if (!mounted || generation != _queryGeneration) return;
      setState(() => _filteredGames = games);
    } catch (_) {
      // The controller's existing error/notice surfaces own native failures.
    }
  }

  void _persistFilters() {
    _filterSession
      ..query = _search.text
      ..outcome = _outcome
      ..color = _color
      ..sort = _sort;
    _filterSession.timeControls
      ..clear()
      ..addAll(_timeControls);
    unawaited(_refreshNativeGames());
  }

  void _persistScrollOffset() {
    if (_scrollController.hasClients) {
      _filterSession.scrollOffset = _scrollController.offset;
    }
  }

  void _resetAllFilters() {
    setState(() {
      _search.clear();
      _outcome = 'all';
      _color = 'all';
      _timeControls.clear();
      _sort = 'newest';
      _persistFilters();
    });
  }

  int get _activeFilterCount =>
      [
        _outcome != 'all',
        _color != 'all',
        _sort != 'newest',
      ].where((active) => active).length +
      _timeControls.length;

  List<Widget> _activeFilterChips(_GameFilterLabels labels) {
    final chips = <Widget>[];
    void addChip(String label, VoidCallback onDeleted) {
      chips.add(
        InputChip(
          label: Text(label),
          visualDensity: VisualDensity.compact,
          onDeleted: onDeleted,
        ),
      );
    }

    if (_outcome != 'all') {
      addChip(
        switch (_outcome) {
          'win' => labels.won,
          'loss' => labels.lost,
          'draw' => labels.draw,
          _ => _outcome,
        },
        () => setState(() {
          _outcome = 'all';
          _persistFilters();
        }),
      );
    }
    if (_color != 'all') {
      addChip(
        _color == 'white' ? labels.white : labels.black,
        () => setState(() {
          _color = 'all';
          _persistFilters();
        }),
      );
    }
    for (final value in _orderedTimeAndStatusValues(_timeControls)) {
      addChip(
        _timeAndStatusLabel(labels, value),
        () => setState(() {
          _timeControls.remove(value);
          _persistFilters();
        }),
      );
    }
    if (_sort != 'newest') {
      addChip(
        switch (_sort) {
          'oldest' => labels.oldestFirst,
          'accuracyHigh' => labels.accuracyDescending,
          'accuracyLow' => labels.accuracyAscending,
          _ => _sort,
        },
        () => setState(() {
          _sort = 'newest';
          _persistFilters();
        }),
      );
    }
    return chips;
  }

  Future<void> _openFilters(BuildContext context) async {
    final result = MediaQuery.sizeOf(context).width >= 700
        ? await showDialog<_GameFilterSelection>(
            context: context,
            builder: (context) => Dialog(
              child: ConstrainedBox(
                constraints: const BoxConstraints(maxWidth: 500),
                child: _GameFilterPanel(
                  initial: _GameFilterSelection(
                    outcome: _outcome,
                    color: _color,
                    timeControls: _timeControls,
                    sort: _sort,
                  ),
                ),
              ),
            ),
          )
        : await showModalBottomSheet<_GameFilterSelection>(
            context: context,
            showDragHandle: true,
            isScrollControlled: true,
            builder: (context) => SafeArea(
              child: _GameFilterPanel(
                initial: _GameFilterSelection(
                  outcome: _outcome,
                  color: _color,
                  timeControls: _timeControls,
                  sort: _sort,
                ),
              ),
            ),
          );
    if (result == null || !mounted) return;
    setState(() {
      _outcome = result.outcome;
      _color = result.color;
      _timeControls = Set<String>.from(result.timeControls);
      _sort = result.sort;
      _persistFilters();
    });
  }

  @override
  void dispose() {
    _queryEnabled = false;
    _queryGeneration++;
    controller.removeListener(_onControllerChanged);
    _persistFilters();
    _persistScrollOffset();
    _scrollController
      ..removeListener(_persistScrollOffset)
      ..dispose();
    _search.dispose();
    super.dispose();
  }

  Future<void> _import(BuildContext context) async {
    final strings = AppLocalizations.of(context);
    final source = await showModalBottomSheet<String>(
      context: context,
      showDragHandle: true,
      builder: (context) => SafeArea(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            ListTile(
              leading: const Icon(Icons.file_open_outlined),
              title: Text(strings.importPgnFile),
              onTap: () => Navigator.pop(context, 'file'),
            ),
            ListTile(
              leading: const Icon(Icons.content_paste_outlined),
              title: Text(strings.pastePgn),
              onTap: () => Navigator.pop(context, 'text'),
            ),
            ListTile(
              leading: const Icon(Icons.grid_on_outlined),
              title: Text(strings.importFen),
              onTap: () => Navigator.pop(context, 'fen'),
            ),
          ],
        ),
      ),
    );
    if (!context.mounted || source == null) return;
    try {
      late final GameSummary game;
      if (source == 'file') {
        final file = await FilePicker.pickFile(
          type: FileType.custom,
          allowedExtensions: const ['pgn'],
        );
        if (file == null) return;
        final text = utf8.decode(await file.readAsBytes());
        game = await controller.importPgn(text);
      } else if (source == 'text') {
        final pgn = await showDialog<String>(
          context: context,
          builder: (_) => const _PgnImportDialog(),
        );
        if (pgn == null) return;
        game = await controller.importPgn(pgn);
      } else {
        final value = await showDialog<({String fen, String name})>(
          context: context,
          builder: (_) => const _FenImportDialog(),
        );
        if (value == null) return;
        game = await controller.importFen(fen: value.fen, name: value.name);
      }
      if (context.mounted) {
        await _openAnalysisAndRefreshSettings(
          context: context,
          controller: controller,
          game: game,
        );
      }
    } catch (error) {
      if (context.mounted) {
        ScaffoldMessenger.of(context)
            .showSnackBar(SnackBar(content: Text(error.toString())));
      }
    }
  }

  Future<void> _deleteLocalEntry(BuildContext context, GameSummary game) async {
    if (controller.settings.confirmBeforeDelete) {
      final strings = AppLocalizations.of(context);
      final confirmed = await showDialog<bool>(
        context: context,
        builder: (context) => AlertDialog(
          title: Text(strings.deleteLocalGameQuestion),
          content: Text(strings.deleteLocalGameBody),
          actions: [
            TextButton(
              onPressed: () => Navigator.pop(context, false),
              child: Text(strings.cancelAction),
            ),
            FilledButton(
              onPressed: () => Navigator.pop(context, true),
              child: Text(strings.deleteAction),
            ),
          ],
        ),
      );
      if (confirmed != true) return;
    }
    await controller.deleteLocalGame(game);
  }

  Future<void> _clearMonthCache(BuildContext context, String month) async {
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Monatscache löschen?'),
        content: Text(
          '$month wird aus dem normalen Cache entfernt. Favoriten und bereits analysierte Partien bleiben erhalten.',
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context, false),
            child: const Text('Abbrechen'),
          ),
          FilledButton(
            onPressed: () => Navigator.pop(context, true),
            child: const Text('Cache löschen'),
          ),
        ],
      ),
    );
    if (confirmed != true) return;
    await controller.clearCachedMonth(month);
    if (context.mounted) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('Cache für $month wurde bereinigt.')),
      );
    }
  }

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final online = controller.activeProfile?.type != ProfileType.localPgnFen;
    final librarySection = widget.savedFilter != null;
    final screenTitle = switch (widget.savedFilter) {
      'favorite' => strings.favorites,
      _ => strings.gameSection,
    };
    final selectedMonth = controller.selectedMonth;
    final playerIdentity =
        controller.activeProfile?.providerUsername ??
        controller.activeProfile?.displayName ??
        '';
    final filtered = _filteredGames;
    return Scaffold(
      appBar: MediaQuery.sizeOf(context).width >= 900
          ? AppBar(
              title: Text(screenTitle),
              actions: [
                if (online)
                  IconButton(
                    tooltip: 'Synchronisieren',
                    onPressed: controller.providerSyncing
                        ? null
                        : controller.syncProvider,
                    icon: const Icon(Icons.sync),
                  ),
              ],
            )
          : null,
      floatingActionButton: FloatingActionButton.extended(
        key: const Key('import-pgn-fen'),
        onPressed: () => _import(context),
        icon: const Icon(Icons.add),
        label: Text(strings.importData),
      ),
      body: Column(
        children: [
          if (controller.providerSyncing) const LinearProgressIndicator(),
          if (controller.providerNotice != null)
            MaterialBanner(
              content: Text(controller.providerNotice!),
              leading: const Icon(Icons.cloud_off_outlined),
              actions: [
                TextButton(
                  onPressed: () =>
                      controller.syncProvider(month: controller.selectedMonth),
                  child: Text(strings.retry),
                ),
              ],
            ),
          Container(
            padding: const EdgeInsets.fromLTRB(16, 12, 16, 12),
            decoration: BoxDecoration(
              color: Theme.of(context).colorScheme.surfaceContainerLow,
              border: Border(
                bottom: BorderSide(
                  color: Theme.of(context).colorScheme.outlineVariant,
                ),
              ),
            ),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                LayoutBuilder(
                  builder: (context, constraints) {
                    final labels = _GameFilterLabels.of(context);
                    final hasMonths =
                        online &&
                        !librarySection &&
                        (controller
                                .providerOverview
                                ?.availableMonths
                                .isNotEmpty ??
                            false);
                    final months = hasMonths
                        ? ([...controller.providerOverview!.availableMonths]
                            ..sort((left, right) => right.compareTo(left)))
                        : <String>[];
                    final currentMonth = months.isEmpty
                        ? null
                        : months.contains(controller.selectedMonth)
                        ? controller.selectedMonth!
                        : months.first;

                    Widget searchField() => TextField(
                      key: const Key('game-search'),
                      controller: _search,
                      onChanged: (_) {
                        _persistFilters();
                        setState(() {});
                      },
                      decoration: InputDecoration(
                        prefixIcon: const Icon(Icons.search),
                        labelText: labels.searchOpponent,
                        isDense: true,
                      ),
                    );

                    Widget filterButton() => Stack(
                      clipBehavior: Clip.none,
                      children: [
                        IconButton.filledTonal(
                          key: const Key('open-game-filters'),
                          tooltip: labels.filters,
                          onPressed: () => _openFilters(context),
                          icon: Icon(
                            _activeFilterCount == 0
                                ? Icons.filter_alt_outlined
                                : Icons.filter_alt,
                          ),
                        ),
                        if (_activeFilterCount > 0)
                          PositionedDirectional(
                            end: -3,
                            top: -3,
                            child: Container(
                              constraints: const BoxConstraints(
                                minWidth: 18,
                                minHeight: 18,
                              ),
                              padding: const EdgeInsets.symmetric(
                                horizontal: 4,
                              ),
                              alignment: Alignment.center,
                              decoration: BoxDecoration(
                                color: Theme.of(context).colorScheme.primary,
                                borderRadius: BorderRadius.circular(10),
                                border: Border.all(
                                  color: Theme.of(context)
                                      .colorScheme
                                      .surfaceContainerLow,
                                  width: 2,
                                ),
                              ),
                              child: Text(
                                '$_activeFilterCount',
                                style: Theme.of(context).textTheme.labelSmall
                                    ?.copyWith(
                                      color: Theme.of(context)
                                          .colorScheme
                                          .onPrimary,
                                      fontWeight: FontWeight.w700,
                                    ),
                              ),
                            ),
                          ),
                      ],
                    );

                    Widget monthControls() {
                      if (currentMonth == null) return const SizedBox.shrink();
                      final index = months.indexOf(currentMonth);
                      final olderMonth = index + 1 < months.length
                          ? months[index + 1]
                          : null;
                      final newerMonth = index > 0 ? months[index - 1] : null;
                      return Wrap(
                        spacing: 4,
                        runSpacing: 4,
                        crossAxisAlignment: WrapCrossAlignment.center,
                        children: [
                          IconButton(
                            tooltip: labels.previousMonth,
                            visualDensity: VisualDensity.compact,
                            onPressed:
                                olderMonth == null || controller.providerSyncing
                                ? null
                                : () => controller.syncProvider(
                                    month: olderMonth,
                                  ),
                            icon: const Icon(Icons.chevron_left),
                          ),
                          Container(
                            padding: const EdgeInsets.symmetric(horizontal: 12),
                            decoration: BoxDecoration(
                              color: Theme.of(context)
                                  .colorScheme
                                  .surfaceContainerHigh,
                              borderRadius: BorderRadius.circular(999),
                              border: Border.all(
                                color: Theme.of(context)
                                    .colorScheme
                                    .outlineVariant,
                              ),
                            ),
                            child: DropdownButtonHideUnderline(
                              child: DropdownButton<String>(
                                key: const Key('month-selector'),
                                value: currentMonth,
                                borderRadius: BorderRadius.circular(14),
                                icon: const Icon(Icons.expand_more, size: 18),
                                items: [
                                  for (final month in months)
                                    DropdownMenuItem(
                                      value: month,
                                      child: Text(
                                        _formatGameMonthLabel(context, month),
                                      ),
                                    ),
                                ],
                                onChanged: controller.providerSyncing
                                    ? null
                                    : (value) {
                                        if (value != null) {
                                          controller.syncProvider(month: value);
                                        }
                                      },
                              ),
                            ),
                          ),
                          IconButton(
                            tooltip: labels.nextMonth,
                            visualDensity: VisualDensity.compact,
                            onPressed:
                                newerMonth == null || controller.providerSyncing
                                ? null
                                : () => controller.syncProvider(
                                    month: newerMonth,
                                  ),
                            icon: const Icon(Icons.chevron_right),
                          ),
                          IconButton(
                            key: const Key('clear-month-cache'),
                            tooltip: labels.clearMonthCache,
                            visualDensity: VisualDensity.compact,
                            onPressed: controller.providerSyncing
                                ? null
                                : () => _clearMonthCache(context, currentMonth),
                            icon: const Icon(Icons.cleaning_services_outlined),
                          ),
                        ],
                      );
                    }

                    if (constraints.maxWidth >= 720) {
                      return Row(
                        children: [
                          Expanded(child: searchField()),
                          const SizedBox(width: 10),
                          filterButton(),
                          if (hasMonths) ...[
                            const SizedBox(width: 16),
                            monthControls(),
                          ],
                        ],
                      );
                    }

                    return Column(
                      crossAxisAlignment: CrossAxisAlignment.stretch,
                      children: [
                        Row(
                          children: [
                            Expanded(child: searchField()),
                            const SizedBox(width: 10),
                            filterButton(),
                          ],
                        ),
                        if (hasMonths) ...[
                          const SizedBox(height: 8),
                          Align(
                            alignment: AlignmentDirectional.centerStart,
                            child: monthControls(),
                          ),
                        ],
                      ],
                    );
                  },
                ),
                if (_activeFilterCount > 0) ...[
                  const SizedBox(height: 10),
                  Wrap(
                    spacing: 8,
                    runSpacing: 6,
                    children: _activeFilterChips(_GameFilterLabels.of(context)),
                  ),
                ],
              ],
            ),
          ),
          Expanded(
            child: filtered.isEmpty
                ? _GamesEmptyState(
                    labels: _GameFilterLabels.of(context),
                    fallbackText: strings.noGames,
                    hasActiveFilters:
                        _activeFilterCount > 0 ||
                        _search.text.trim().isNotEmpty,
                    month: online && !librarySection ? selectedMonth : null,
                    onResetFilters: _resetAllFilters,
                  )
                : ListView.separated(
                    controller: _scrollController,
                    padding: const EdgeInsets.all(16),
                    itemCount: filtered.length,
                    separatorBuilder: (_, _) => const SizedBox(height: 12),
                    itemBuilder: (context, index) {
                      final game = filtered[index];
                      return _GameCard(
                        game: game,
                        online: online,
                        playerIdentity: playerIdentity,
                        onOpen: () => _openAnalysisAndRefreshSettings(
                          context: context,
                          controller: controller,
                          game: game,
                        ),
                        onToggleFavorite: () => controller.toggleFavorite(game),
                        onSaveToDownloads: game.downloaded
                            ? null
                            : () => controller.saveToDownloads(game),
                        onDelete: () => _deleteLocalEntry(context, game),
                      );
                    },
                  ),
          ),
        ],
      ),
    );
  }
}

String _formatGameMonthLabel(BuildContext context, String value) {
  final parts = value.split('-');
  if (parts.length != 2) return value;
  final year = int.tryParse(parts[0]);
  final month = int.tryParse(parts[1]);
  if (year == null || month == null || month < 1 || month > 12) return value;
  final language = Localizations.localeOf(context).languageCode;
  const de = [
    'Januar',
    'Februar',
    'März',
    'April',
    'Mai',
    'Juni',
    'Juli',
    'August',
    'September',
    'Oktober',
    'November',
    'Dezember',
  ];
  const en = [
    'January',
    'February',
    'March',
    'April',
    'May',
    'June',
    'July',
    'August',
    'September',
    'October',
    'November',
    'December',
  ];
  const ar = [
    'يناير',
    'فبراير',
    'مارس',
    'أبريل',
    'مايو',
    'يونيو',
    'يوليو',
    'أغسطس',
    'سبتمبر',
    'أكتوبر',
    'نوفمبر',
    'ديسمبر',
  ];
  final names = language == 'ar'
      ? ar
      : language == 'en'
      ? en
      : de;
  return '${names[month - 1]} $year';
}

