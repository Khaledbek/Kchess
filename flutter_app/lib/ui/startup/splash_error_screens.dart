part of '../app_root.dart';

class _SplashScreen extends StatelessWidget {
  const _SplashScreen();

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final brightness = Theme.of(context).brightness;
    return Scaffold(
      body: DecoratedBox(
        decoration: BoxDecoration(
          gradient: LinearGradient(
            begin: Alignment.topLeft,
            end: Alignment.bottomRight,
            colors: AppTheme.ambientGradient(brightness),
          ),
        ),
        child: Center(
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              const BrandLogo(size: 116),
              const SizedBox(height: 30),
              const BrandWordmark(fontSize: 40),
              const SizedBox(height: 10),
              Text(
                strings.stockfishPending,
                style: Theme.of(context).textTheme.bodySmall?.copyWith(
                  color: Theme.of(context).colorScheme.onSurfaceVariant,
                ),
              ),
              const SizedBox(height: 44),
              Semantics(
                label: strings.loading,
                child: SizedBox(
                  width: 168,
                  child: ClipRRect(
                    borderRadius: BorderRadius.circular(4),
                    child: const LinearProgressIndicator(minHeight: 4),
                  ),
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }
}

class _ErrorScreen extends StatelessWidget {
  const _ErrorScreen({required this.controller});

  final AppController controller;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final scheme = Theme.of(context).colorScheme;
    return Scaffold(
      body: Center(
        child: ConstrainedBox(
          constraints: const BoxConstraints(maxWidth: 460),
          child: Padding(
            padding: const EdgeInsets.all(28),
            child: Column(
              mainAxisSize: MainAxisSize.min,
              children: [
                Container(
                  padding: const EdgeInsets.all(18),
                  decoration: BoxDecoration(
                    shape: BoxShape.circle,
                    color: scheme.errorContainer.withValues(alpha: 0.5),
                  ),
                  child: Icon(
                    Icons.cloud_off_rounded,
                    size: 40,
                    color: scheme.error,
                  ),
                ),
                const SizedBox(height: 22),
                Text(
                  strings.coreUnavailable,
                  textAlign: TextAlign.center,
                  style: Theme.of(context).textTheme.titleMedium,
                ),
                const SizedBox(height: 22),
                FilledButton.icon(
                  onPressed: controller.initialize,
                  icon: const Icon(Icons.refresh),
                  label: Text(strings.retry),
                ),
              ],
            ),
          ),
        ),
      ),
    );
  }
}

