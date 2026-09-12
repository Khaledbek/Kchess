part of '../../../ui/app_root.dart';

// -----------------------------------------------------------------------------
// Section: Bot strength selection
// -----------------------------------------------------------------------------

const int _botEloMin = 100;
const int _botEloMax = 3200;
const int _botEloStep = 100;
const int _botEloDefault = 1500;

class BotGameSetupScreen extends StatefulWidget {
  const BotGameSetupScreen({required this.gateway, super.key});

  final CoreGateway gateway;

  @override
  State<BotGameSetupScreen> createState() => _BotGameSetupScreenState();
}

class _BotGameSetupScreenState extends State<BotGameSetupScreen> {
  int _selectedElo = _botEloDefault;
  BotGameSession? _activeGame;
  bool _loadingActiveGame = true;

  @override
  void initState() {
    super.initState();
    unawaited(_refreshActiveGame());
  }

  Future<void> _refreshActiveGame() async {
    try {
      final active = await widget.gateway.activeBotGame();
      if (!mounted) return;
      setState(() {
        _activeGame = active;
        _loadingActiveGame = false;
      });
    } catch (_) {
      if (!mounted) return;
      setState(() => _loadingActiveGame = false);
    }
  }

  Future<void> _openGame({String? gameId, required int botElo}) async {
    await Navigator.of(context).push(
      MaterialPageRoute<void>(
        builder: (_) => BotGameScreen(
          gateway: widget.gateway,
          botElo: botElo,
          gameId: gameId,
        ),
      ),
    );
    if (mounted) await _refreshActiveGame();
  }

  void _changeElo(int delta) {
    final next = (_selectedElo + delta)
        .clamp(_botEloMin, _botEloMax)
        .toInt();
    if (next == _selectedElo) return;
    setState(() => _selectedElo = next);
  }

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;

    return Scaffold(
      appBar: AppBar(title: Text(strings.playAgainstBot)),
      body: ListView(
        padding: const EdgeInsets.fromLTRB(20, 24, 20, 32),
        children: [
          Center(
            child: ConstrainedBox(
              constraints: const BoxConstraints(maxWidth: 560),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.stretch,
                children: [
                  Icon(
                    Icons.smart_toy_outlined,
                    key: const Key('bot-setup-icon'),
                    size: 64,
                    color: scheme.primary,
                  ),
                  const SizedBox(height: 18),
                  Text(
                    strings.playAgainstBot,
                    textAlign: TextAlign.center,
                    style: theme.textTheme.headlineSmall?.copyWith(
                      fontWeight: FontWeight.w700,
                    ),
                  ),
                  const SizedBox(height: 24),
                  if (_loadingActiveGame)
                    const Center(child: CircularProgressIndicator())
                  else if (_activeGame != null) ...[
                    Card(
                      child: ListTile(
                        key: const Key('resume-bot-game'),
                        leading: const Icon(Icons.restore_rounded),
                        title: Text(strings.continueLabel),
                        subtitle: Text(
                          '${strings.botGameTitle} · ${strings.botElo} ${_activeGame!.botElo}',
                        ),
                        trailing: const Icon(Icons.chevron_right_rounded),
                        onTap: () => _openGame(
                          gameId: _activeGame!.gameId,
                          botElo: _activeGame!.botElo,
                        ),
                      ),
                    ),
                    const SizedBox(height: 14),
                  ],
                  Card(
                    child: Padding(
                      padding: const EdgeInsets.fromLTRB(18, 18, 18, 14),
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.stretch,
                        children: [
                          Text(
                            strings.botStrength,
                            style: theme.textTheme.titleMedium?.copyWith(
                              fontWeight: FontWeight.w700,
                            ),
                          ),
                          const SizedBox(height: 20),
                          Text(
                            '${strings.botElo}: $_selectedElo',
                            key: const Key('bot-elo-value'),
                            textAlign: TextAlign.center,
                            style: theme.textTheme.headlineMedium?.copyWith(
                              fontWeight: FontWeight.w800,
                            ),
                          ),
                          const SizedBox(height: 10),
                          Row(
                            children: [
                              IconButton.filledTonal(
                                key: const Key('bot-elo-decrease'),
                                tooltip: '-$_botEloStep',
                                onPressed: _selectedElo > _botEloMin
                                    ? () => _changeElo(-_botEloStep)
                                    : null,
                                icon: const Icon(Icons.remove_rounded),
                              ),
                              Expanded(
                                child: Slider(
                                  key: const Key('bot-elo-slider'),
                                  min: _botEloMin.toDouble(),
                                  max: _botEloMax.toDouble(),
                                  divisions:
                                      (_botEloMax - _botEloMin) ~/ _botEloStep,
                                  value: _selectedElo.toDouble(),
                                  label: '$_selectedElo',
                                  onChanged: (value) {
                                    setState(
                                      () => _selectedElo =
                                          (value / _botEloStep).round() *
                                          _botEloStep,
                                    );
                                  },
                                ),
                              ),
                              IconButton.filledTonal(
                                key: const Key('bot-elo-increase'),
                                tooltip: '+$_botEloStep',
                                onPressed: _selectedElo < _botEloMax
                                    ? () => _changeElo(_botEloStep)
                                    : null,
                                icon: const Icon(Icons.add_rounded),
                              ),
                            ],
                          ),
                          Padding(
                            padding: const EdgeInsets.symmetric(horizontal: 12),
                            child: Row(
                              mainAxisAlignment: MainAxisAlignment.spaceBetween,
                              children: [
                                Text('$_botEloMin'),
                                Text('$_botEloMax'),
                              ],
                            ),
                          ),
                        ],
                      ),
                    ),
                  ),
                  const SizedBox(height: 14),
                  Card(
                    child: ListTile(
                      leading: const Icon(Icons.memory_rounded),
                      title: Text(strings.stockfish18),
                    ),
                  ),
                  const SizedBox(height: 18),
                  FilledButton.icon(
                    key: const Key('start-bot-game'),
                    onPressed: _activeGame == null && !_loadingActiveGame
                        ? () => _openGame(botElo: _selectedElo)
                        : null,
                    icon: const Icon(Icons.play_arrow_rounded),
                    label: Padding(
                      padding: const EdgeInsets.symmetric(vertical: 12),
                      child: Text(strings.botStartGame),
                    ),
                  ),
                ],
              ),
            ),
          ),
        ],
      ),
    );
  }
}
