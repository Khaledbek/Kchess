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
  late bool _providerSyncing;
  String? _profileId;

  @override
  void initState() {
    super.initState();
    _providerSyncing = widget.controller.providerSyncing;
    _profileId = widget.controller.activeProfile?.id;
    widget.controller.addListener(_onControllerChanged);
    _load();
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
    if (shouldReload && mounted) _reload();
  }

  void _load() {
    _overview = widget.controller.gateway.statisticsOverview();
    _openings = widget.controller.gateway.openingsStats();
  }

  void _reload() {
    setState(_load);
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
          ConstrainedBox(
            constraints: const BoxConstraints(maxWidth: 960),
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
                const SizedBox(height: 20),
                _OverviewCard(future: _overview, onRetry: _reload),
                const SizedBox(height: 20),
                _OpeningsCard(future: _openings, onRetry: _reload),
              ],
            ),
          ),
        ],
      ),
    );
  }
}

