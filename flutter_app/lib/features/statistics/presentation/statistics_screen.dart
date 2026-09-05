part of '../../../ui/app_root.dart';

class StatisticsScreen extends StatefulWidget {
  const StatisticsScreen({required this.controller, super.key});

  final AppController controller;

  @override
  State<StatisticsScreen> createState() => _StatisticsScreenState();
}

class _StatisticsScreenState extends State<StatisticsScreen> {
  late Future<StatisticsOverview> _overview;
  late Future<OpeningsStats> _openings;
  late Future<TerminationStats> _terminations;
  late Future<PhaseStats> _phases;
  late Future<List<GameSummary>> _games;
  late bool _providerSyncing;
  String? _profileId;
  String _timeControl = 'all';

  @override
  void initState() {
    super.initState();
    _providerSyncing = widget.controller.providerSyncing;
    _profileId = widget.controller.activeProfile?.id;
    widget.controller.addListener(_onControllerChanged);
    _loadStats();
    _loadGames();
  }

  @override
  void dispose() {
    widget.controller.removeListener(_onControllerChanged);
    super.dispose();
  }

  void _onControllerChanged() {
    final providerSyncing = widget.controller.providerSyncing;
    final profileId = widget.controller.activeProfile?.id;
    final shouldReload =
        _profileId != profileId || (_providerSyncing && !providerSyncing);
    _providerSyncing = providerSyncing;
    _profileId = profileId;
    if (shouldReload && mounted) {
      setState(() {
        _loadStats();
        _loadGames();
      });
    }
  }

  void _loadStats() {
    _overview = widget.controller.gateway.statisticsOverview();
    _openings = widget.controller.gateway.openingsStats();
    // Termination and phase data span the whole library (stored PGNs / move
    // counts), so they are not affected by the time-control filter.
    _terminations = widget.controller.gateway.terminationStats();
    _phases = widget.controller.gateway.phaseStats();
  }

  /// The form strip and rating trend derive from per-game rows; a filtered
  /// [queryGames] keeps the actual W/L/D→display selection and rating series in
  /// the C++ query, not in Dart aggregation.
  void _loadGames() {
    _games = widget.controller.queryGames(
      GameQuery(
        timeControls: _timeControl == 'all'
            ? const <String>[]
            : <String>[_timeControl],
        sort: 'newest',
      ),
    );
  }

  void _reloadAll() {
    setState(() {
      _loadStats();
      _loadGames();
    });
  }

  void _reloadGames() {
    setState(_loadGames);
  }

  void _onTimeControlChanged(String value) {
    if (value == _timeControl) return;
    setState(() {
      _timeControl = value;
      _loadGames();
    });
  }

  @override
  Widget build(BuildContext context) {
    final text = _statisticsText(context);
    final theme = Theme.of(context);

    return Scaffold(
      appBar: MediaQuery.sizeOf(context).width >= 900
          ? AppBar(title: Text(text.title))
          : null,
      body: ListView(
        padding: const EdgeInsets.fromLTRB(16, 12, 16, 28),
        children: [
          Center(
            child: ConstrainedBox(
              constraints: const BoxConstraints(maxWidth: 1100),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(
                    text.introTitle,
                    style: theme.textTheme.headlineSmall?.copyWith(
                      fontWeight: FontWeight.w800,
                    ),
                  ),
                  const SizedBox(height: 8),
                  Text(
                    text.introBody,
                    style: theme.textTheme.bodyMedium?.copyWith(
                      color: theme.colorScheme.onSurfaceVariant,
                    ),
                  ),
                  const SizedBox(height: 14),
                  Align(
                    alignment: AlignmentDirectional.centerStart,
                    child: OutlinedButton.icon(
                      onPressed: () => Navigator.of(context).push(
                        MaterialPageRoute<void>(
                          builder: (_) => _PlayerComparisonScreen(
                            controller: widget.controller,
                          ),
                        ),
                      ),
                      icon: const Icon(Icons.compare_arrows),
                      label: Text(_comparisonText(context).title),
                    ),
                  ),
                  const SizedBox(height: 18),
                  _TimeControlFilterBar(
                    selected: _timeControl,
                    onChanged: _onTimeControlChanged,
                  ),
                  const SizedBox(height: 20),
                  _buildBody(context),
                ],
              ),
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildBody(BuildContext context) {
    final overview = _OverviewCard(
      future: _overview,
      timeControl: _timeControl,
      onRetry: _reloadAll,
    );
    final form = _RecentFormCard(
      controller: widget.controller,
      future: _games,
      timeControl: _timeControl,
      onRetry: _reloadGames,
    );
    final rating = _RatingTrendCard(
      future: _games,
      timeControl: _timeControl,
      onRetry: _reloadGames,
    );
    final termination = _TerminationCard(
      future: _terminations,
      onRetry: _reloadAll,
    );
    final phase = _PhaseCard(future: _phases, onRetry: _reloadAll);
    final openings = _OpeningsCard(
      future: _openings,
      onRetry: _reloadAll,
      controller: widget.controller,
    );

    return LayoutBuilder(
      builder: (context, constraints) {
        if (constraints.maxWidth >= 820) {
          return Column(
            crossAxisAlignment: CrossAxisAlignment.stretch,
            children: [
              Row(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Expanded(
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.stretch,
                      children: [
                        overview,
                        const SizedBox(height: 20),
                        termination,
                      ],
                    ),
                  ),
                  const SizedBox(width: 20),
                  Expanded(
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.stretch,
                      children: [
                        form,
                        const SizedBox(height: 20),
                        rating,
                        const SizedBox(height: 20),
                        phase,
                      ],
                    ),
                  ),
                ],
              ),
              const SizedBox(height: 20),
              openings,
            ],
          );
        }
        return Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            overview,
            const SizedBox(height: 20),
            termination,
            const SizedBox(height: 20),
            phase,
            const SizedBox(height: 20),
            form,
            const SizedBox(height: 20),
            rating,
            const SizedBox(height: 20),
            openings,
          ],
        );
      },
    );
  }
}

({String title, String introTitle, String introBody}) _statisticsText(
  BuildContext context,
) {
  return switch (Localizations.localeOf(context).languageCode) {
    'ar' => (
      title: 'الإحصائيات',
      introTitle: 'أداؤك في الشطرنج',
      introBody: 'اعرض نتائجك وأداءك الأخير وسجل افتتاحياتك مفصولًا حسب اللون.',
    ),
    'en' => (
      title: 'Statistics',
      introTitle: 'Your chess performance',
      introBody: 'See your results, recent form and opening record separated by color.',
    ),
    _ => (
      title: 'Statistiken',
      introTitle: 'Deine Schachleistung',
      introBody: 'Sieh deine Ergebnisse, aktuelle Form und Eröffnungsbilanz getrennt nach Farbe.',
    ),
  };
}
