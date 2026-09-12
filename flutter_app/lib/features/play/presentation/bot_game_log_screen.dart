part of '../../../ui/app_root.dart';

// -----------------------------------------------------------------------------
// Section: Persisted bot game history
// -----------------------------------------------------------------------------

class BotGameLogScreen extends StatefulWidget {
  const BotGameLogScreen({required this.gateway, super.key});

  final CoreGateway gateway;

  @override
  State<BotGameLogScreen> createState() => _BotGameLogScreenState();
}

class _BotGameLogScreenState extends State<BotGameLogScreen> {
  List<BotGameSummary> _games = const [];
  Object? _error;
  bool _loading = true;
  String? _openingAnalysisGameId;
  String? _deletingGameId;

  @override
  void initState() {
    super.initState();
    unawaited(_reload());
  }

  Future<void> _reload() async {
    try {
      final games = await widget.gateway.botGames();
      if (!mounted) return;
      setState(() {
        _games = games;
        _error = null;
        _loading = false;
      });
    } catch (error) {
      if (!mounted) return;
      setState(() {
        _error = error;
        _loading = false;
      });
    }
  }

  Future<void> _resume(BotGameSummary game) async {
    await Navigator.of(context).push(
      MaterialPageRoute<void>(
        builder: (_) => BotGameScreen(
          gateway: widget.gateway,
          botElo: game.botElo,
          gameId: game.gameId,
        ),
      ),
    );
    if (mounted) await _reload();
  }

  Future<void> _openAnalysis(BotGameSummary game) async {
    if (!game.canAnalyze || _openingAnalysisGameId != null) return;
    setState(() {
      _openingAnalysisGameId = game.gameId;
      _error = null;
    });
    try {
      final analysisGame = await widget.gateway.botGameAnalysisGame(game.gameId);
      final settings = await widget.gateway.settings();
      if (!mounted) return;
      setState(() => _openingAnalysisGameId = null);
      await openAnalysisWorkflow(
        context: context,
        gateway: widget.gateway,
        game: analysisGame,
        settings: settings,
      );
      if (mounted) await _reload();
    } catch (error) {
      if (!mounted) return;
      setState(() {
        _openingAnalysisGameId = null;
        _error = error;
      });
    }
  }


  Future<void> _deleteGame(BotGameSummary game) async {
    if (game.isActive || _deletingGameId != null) return;
    final strings = AppLocalizations.of(context);
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        title: Text(strings.botGameDeleteQuestion),
        content: Text(strings.botGameDeleteBody),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(context).pop(false),
            child: Text(strings.close),
          ),
          FilledButton(
            onPressed: () => Navigator.of(context).pop(true),
            child: Text(strings.deleteAction),
          ),
        ],
      ),
    );
    if (confirmed != true || !mounted) return;
    setState(() {
      _deletingGameId = game.gameId;
      _error = null;
    });
    try {
      await widget.gateway.deleteBotGame(game.gameId);
      if (!mounted) return;
      setState(() => _deletingGameId = null);
      await _reload();
    } catch (error) {
      if (!mounted) return;
      setState(() {
        _deletingGameId = null;
        _error = error;
      });
    }
  }

  String _outcomeLabel(AppLocalizations strings, BotGameSummary game) =>
      switch (game.outcome) {
        'win' => strings.botGameHistoryWin,
        'loss' => strings.botGameHistoryLoss,
        'draw' => strings.botGameHistoryDraw,
        _ => strings.botGameHistoryActive,
      };

  IconData _outcomeIcon(BotGameSummary game) => switch (game.outcome) {
    'win' => Icons.emoji_events_outlined,
    'loss' => Icons.close_rounded,
    'draw' => Icons.balance_rounded,
    _ => Icons.schedule_rounded,
  };

  String _timestamp(BuildContext context, BotGameSummary game) {
    final dateTime = DateTime.fromMillisecondsSinceEpoch(
      game.createdAt * 1000,
    ).toLocal();
    final localizations = MaterialLocalizations.of(context);
    final date = localizations.formatShortDate(dateTime);
    final time = localizations.formatTimeOfDay(
      TimeOfDay.fromDateTime(dateTime),
      alwaysUse24HourFormat: MediaQuery.of(context).alwaysUse24HourFormat,
    );
    return '$date · $time';
  }

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;

    return Scaffold(
      appBar: AppBar(title: Text(strings.botGameLog)),
      body: RefreshIndicator(
        onRefresh: _reload,
        child: _loading
            ? ListView(
                physics: AlwaysScrollableScrollPhysics(),
                children: [
                  SizedBox(height: 220),
                  Center(child: CircularProgressIndicator()),
                ],
              )
            : _error != null && _games.isEmpty
            ? ListView(
                physics: const AlwaysScrollableScrollPhysics(),
                padding: const EdgeInsets.all(24),
                children: [
                  const SizedBox(height: 100),
                  Icon(
                    Icons.error_outline_rounded,
                    size: 56,
                    color: scheme.error,
                  ),
                  const SizedBox(height: 16),
                  Text(
                    strings.botGameLogLoadFailed,
                    textAlign: TextAlign.center,
                    style: theme.textTheme.titleMedium,
                  ),
                  const SizedBox(height: 8),
                  Text(
                    _error.toString(),
                    textAlign: TextAlign.center,
                    style: theme.textTheme.bodySmall,
                  ),
                  const SizedBox(height: 16),
                  Center(
                    child: IconButton.filledTonal(
                      onPressed: _reload,
                      icon: const Icon(Icons.refresh_rounded),
                    ),
                  ),
                ],
              )
            : _games.isEmpty
            ? ListView(
                physics: const AlwaysScrollableScrollPhysics(),
                padding: const EdgeInsets.all(24),
                children: [
                  const SizedBox(height: 120),
                  Icon(
                    Icons.history_rounded,
                    size: 56,
                    color: scheme.onSurfaceVariant,
                  ),
                  const SizedBox(height: 16),
                  Text(
                    strings.botGameLogEmpty,
                    textAlign: TextAlign.center,
                    style: theme.textTheme.titleMedium,
                  ),
                ],
              )
            : ListView.separated(
                padding: const EdgeInsets.fromLTRB(16, 12, 16, 28),
                itemCount: _games.length + (_error == null ? 0 : 1),
                separatorBuilder: (_, _) => const SizedBox(height: 8),
                itemBuilder: (context, index) {
                  if (_error != null && index == 0) {
                    return Card(
                      color: scheme.errorContainer,
                      child: ListTile(
                        leading: Icon(
                          Icons.error_outline_rounded,
                          color: scheme.onErrorContainer,
                        ),
                        title: Text(strings.botGameLogLoadFailed),
                        subtitle: Text(_error.toString()),
                        trailing: IconButton(
                          onPressed: _reload,
                          icon: const Icon(Icons.refresh_rounded),
                        ),
                      ),
                    );
                  }
                  final gameIndex = index - (_error == null ? 0 : 1);
                  final game = _games[gameIndex];
                  return _BotGameHistoryCard(
                    game: game,
                    outcomeLabel: _outcomeLabel(strings, game),
                    outcomeIcon: _outcomeIcon(game),
                    timestamp: _timestamp(context, game),
                    openingAnalysis: _openingAnalysisGameId == game.gameId,
                    deleting: _deletingGameId == game.gameId,
                    onContinue: game.isActive ? () => _resume(game) : null,
                    onAnalyze: game.canAnalyze ? () => _openAnalysis(game) : null,
                    onDelete: game.isActive ? null : () => _deleteGame(game),
                  );
                },
              ),
      ),
    );
  }
}

// -----------------------------------------------------------------------------
// Section: Bot history cards
// -----------------------------------------------------------------------------

class _BotGameHistoryCard extends StatelessWidget {
  const _BotGameHistoryCard({
    required this.game,
    required this.outcomeLabel,
    required this.outcomeIcon,
    required this.timestamp,
    required this.openingAnalysis,
    required this.deleting,
    required this.onContinue,
    required this.onAnalyze,
    required this.onDelete,
  });

  final BotGameSummary game;
  final String outcomeLabel;
  final IconData outcomeIcon;
  final String timestamp;
  final bool openingAnalysis;
  final bool deleting;
  final VoidCallback? onContinue;
  final VoidCallback? onAnalyze;
  final VoidCallback? onDelete;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;
    final playerIsWhite = game.playerColor == 'white';

    return Card(
      clipBehavior: Clip.antiAlias,
      child: Padding(
        padding: const EdgeInsets.fromLTRB(16, 14, 16, 14),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Row(
              children: [
                SizedBox(
                  width: 42,
                  height: 42,
                  child: SvgPicture.asset(
                    playerIsWhite
                        ? 'assets/analysis_img/piece_white_pawn.svg'
                        : 'assets/analysis_img/piece_black_pawn.svg',
                    fit: BoxFit.contain,
                  ),
                ),
                const SizedBox(width: 12),
                Expanded(
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(
                        '${strings.botGameTitle} · ${strings.botElo} ${game.botElo}',
                        style: theme.textTheme.titleMedium?.copyWith(
                          fontWeight: FontWeight.w700,
                        ),
                      ),
                      const SizedBox(height: 3),
                      Text(
                        '$timestamp · ${game.moveCount} ${strings.totalMoves}',
                        style: theme.textTheme.bodySmall?.copyWith(
                          color: scheme.onSurfaceVariant,
                        ),
                      ),
                    ],
                  ),
                ),
                const SizedBox(width: 8),
                Chip(
                  avatar: Icon(outcomeIcon, size: 17),
                  label: Text(outcomeLabel),
                  visualDensity: VisualDensity.compact,
                ),
              ],
            ),
            const SizedBox(height: 12),
            Row(
              mainAxisAlignment: MainAxisAlignment.end,
              children: [
                if (onDelete != null) ...[
                  IconButton.filledTonal(
                    tooltip: strings.deleteAction,
                    onPressed: deleting ? null : onDelete,
                    icon: deleting
                        ? const SizedBox.square(
                            dimension: 18,
                            child: CircularProgressIndicator(strokeWidth: 2),
                          )
                        : const Icon(Icons.delete_outline_rounded),
                  ),
                  const SizedBox(width: 8),
                ],
                if (onContinue != null)
                  FilledButton.tonalIcon(
                    onPressed: onContinue,
                    icon: const Icon(Icons.play_arrow_rounded),
                    label: Text(strings.continueLabel),
                  ),
                if (onContinue != null && onAnalyze != null)
                  const SizedBox(width: 8),
                if (onAnalyze != null)
                  FilledButton.icon(
                    onPressed: openingAnalysis ? null : onAnalyze,
                    icon: openingAnalysis
                        ? const SizedBox.square(
                            dimension: 18,
                            child: CircularProgressIndicator(strokeWidth: 2),
                          )
                        : const Icon(Icons.analytics_outlined),
                    label: Text(strings.openAnalysis),
                  ),
              ],
            ),
          ],
        ),
      ),
    );
  }
}
