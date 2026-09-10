part of '../../../ui/app_root.dart';

// -----------------------------------------------------------------------------
// Section: Play mode selection
// -----------------------------------------------------------------------------

class PlayScreen extends StatelessWidget {
  const PlayScreen({required this.gateway, super.key});

  final CoreGateway gateway;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    return Scaffold(
      appBar: MediaQuery.sizeOf(context).width >= 900
          ? AppBar(title: Text(strings.play))
          : null,
      body: ListView(
        padding: const EdgeInsets.fromLTRB(16, 12, 16, 28),
        children: [
          _PlayModeTile(
            key: const Key('play-against-bot'),
            icon: Icons.smart_toy_outlined,
            title: strings.playAgainstBot,
            subtitle: strings.playAgainstBotSubtitle,
            onTap: () => Navigator.of(context).push(
              MaterialPageRoute<void>(
                builder: (_) => BotGameSetupScreen(gateway: gateway),
              ),
            ),
          ),
          const SizedBox(height: 8),
          _PlayModeTile(
            key: const Key('bot-game-log'),
            icon: Icons.history_rounded,
            title: strings.botGameLog,
            subtitle: strings.botGameLogSubtitle,
            onTap: () => Navigator.of(context).push(
              MaterialPageRoute<void>(
                builder: (_) => BotGameLogScreen(gateway: gateway),
              ),
            ),
          ),
        ],
      ),
    );
  }
}

class _PlayModeTile extends StatelessWidget {
  const _PlayModeTile({
    required this.icon,
    required this.title,
    required this.subtitle,
    required this.onTap,
    super.key,
  });

  final IconData icon;
  final String title;
  final String subtitle;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) {
    final scheme = Theme.of(context).colorScheme;
    return Card(
      clipBehavior: Clip.antiAlias,
      child: ListTile(
        contentPadding: const EdgeInsets.symmetric(horizontal: 18, vertical: 8),
        leading: Container(
          width: 52,
          height: 52,
          decoration: BoxDecoration(
            color: scheme.primaryContainer,
            borderRadius: BorderRadius.circular(14),
          ),
          alignment: Alignment.center,
          child: Icon(
            icon,
            size: 30,
            color: scheme.onPrimaryContainer,
          ),
        ),
        title: Text(
          title,
          style: Theme.of(context).textTheme.titleMedium?.copyWith(
            fontWeight: FontWeight.w700,
          ),
        ),
        subtitle: Padding(
          padding: const EdgeInsets.only(top: 4),
          child: Text(subtitle),
        ),
        trailing: const Icon(Icons.chevron_right_rounded),
        onTap: onTap,
      ),
    );
  }
}

// -----------------------------------------------------------------------------
// Section: Shared empty-state fallback
// -----------------------------------------------------------------------------

class _EmptySection extends StatelessWidget {
  const _EmptySection({required this.title, this.message});
  final String title;
  final String? message;

  @override
  Widget build(BuildContext context) => Scaffold(
    appBar: MediaQuery.sizeOf(context).width >= 900
        ? AppBar(title: Text(title))
        : null,
    body: Center(
      child: Text(message ?? AppLocalizations.of(context).emptySection),
    ),
  );
}

/// A rich game row: outcome accent, time-control badge, both players with
/// piece markers and rating pills, a result pill, accuracy and quick actions.
