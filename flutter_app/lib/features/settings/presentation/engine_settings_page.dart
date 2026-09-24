part of '../../../ui/app_root.dart';

class _EngineSettingsPage extends StatefulWidget {
  const _EngineSettingsPage({required this.controller});
  final AppController controller;

  @override
  State<_EngineSettingsPage> createState() => _EngineSettingsPageState();
}

class _EngineSettingsPageState extends State<_EngineSettingsPage> {
  bool _switchingEngine = false;

  AppController get controller => widget.controller;

  Future<void> _selectEngine(String? value) async {
    if (_switchingEngine ||
        value == null ||
        value == controller.settings.engineId) {
      return;
    }

    setState(() => _switchingEngine = true);
    try {
      await controller.setEngineId(value);
    } catch (_) {
      if (!mounted) return;
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text(AppLocalizations.of(context).engineSelectionFailed),
        ),
      );
    } finally {
      if (mounted) setState(() => _switchingEngine = false);
    }
  }

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final activeEngineName = controller.settings.engineId == 'stockfish19'
        ? strings.stockfish19
        : strings.stockfish18;
    return Scaffold(
      appBar: AppBar(title: Text(strings.engine)),
      body: ListView(
        padding: const EdgeInsets.fromLTRB(16, 12, 16, 28),
        children: [
          _SettingsSection(
            title: strings.engineSelectionTitle,
            child: ListTile(
              key: const Key('engine-version'),
              leading: const Icon(Icons.smart_toy_outlined),
              title: Text(strings.engineVersion),
              subtitle: Text(strings.engineActiveLabel(activeEngineName)),
              trailing: Row(
                mainAxisSize: MainAxisSize.min,
                children: [
                  if (_switchingEngine) ...[
                    const SizedBox(
                      width: 18,
                      height: 18,
                      child: CircularProgressIndicator(strokeWidth: 2),
                    ),
                    const SizedBox(width: 10),
                  ],
                  DropdownButton<String>(
                    value: controller.settings.engineId,
                    items: [
                      DropdownMenuItem(
                        value: 'stockfish18',
                        child: Text(strings.stockfish18),
                      ),
                      DropdownMenuItem(
                        value: 'stockfish19',
                        child: Text(strings.stockfish19),
                      ),
                    ],
                    onChanged: _switchingEngine ? null : _selectEngine,
                  ),
                ],
              ),
            ),
          ),
          _SettingsSection(
            title: strings.engineQualityTitle,
            child: Column(
              children: [
                _DepthRangeSettingTile(
                  key: const Key('engine-depth-range'),
                  title: strings.depth,
                  minimumDepth: controller.settings.minAnalysisDepth,
                  maximumDepth: controller.settings.depth,
                  onMinimumChanged: controller.setMinAnalysisDepth,
                  onMaximumChanged: controller.setDepth,
                ),
                const Divider(height: 1),
                _IntegerSettingTile(
                  key: const Key('engine-lines'),
                  icon: Icons.format_list_numbered,
                  title: strings.numberOfLines,
                  value: controller.settings.multiPv,
                  minimum: 1,
                  maximum: 8,
                  onChanged: controller.setMultiPv,
                ),
                const Divider(height: 1),
                _IntegerSettingTile(
                  key: const Key('engine-time-limit'),
                  icon: Icons.timer_outlined,
                  title: strings.timeLimitSeconds,
                  value: controller.settings.timeLimitSeconds,
                  minimum: 0,
                  maximum: 60,
                  valueLabelBuilder: (value) => value == 0
                      ? strings.noTimeLimit
                      : '$value ${strings.secondsShort}',
                  onChanged: controller.setTimeLimitSeconds,
                ),
                const Divider(height: 1),
                SwitchListTile(
                  key: const Key('adaptive-early-stop'),
                  secondary: const Icon(Icons.speed_outlined),
                  title: Text(strings.adaptiveEarlyStop),
                  value: controller.settings.adaptiveEarlyStop,
                  onChanged: controller.setAdaptiveEarlyStop,
                ),
              ],
            ),
          ),
          _SettingsSection(
            title: strings.engineResourcesTitle,
            child: Column(
              children: [
                _IntegerSettingTile(
                  key: const Key('engine-threads'),
                  icon: Icons.memory_outlined,
                  title: strings.threads,
                  value: controller.settings.threads,
                  minimum: 1,
                  maximum: controller.settings.maxThreads,
                  onChanged: controller.setThreads,
                ),
                const Divider(height: 1),
                _IntegerSettingTile(
                  key: const Key('engine-hash'),
                  icon: Icons.storage_outlined,
                  title: strings.hashMemory,
                  value: controller.settings.hashMb,
                  minimum: 16,
                  maximum: 2048,
                  allowedValues: const [16, 32, 64, 128, 256, 512, 1024, 2048],
                  valueLabelBuilder: (value) => '$value MB',
                  onChanged: controller.setHashMb,
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}

