part of '../../../ui/app_root.dart';

class _EngineSettingsPage extends StatelessWidget {
  const _EngineSettingsPage({required this.controller});
  final AppController controller;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    return Scaffold(
      appBar: AppBar(title: Text(strings.engine)),
      body: ListView(
        padding: const EdgeInsets.fromLTRB(16, 12, 16, 28),
        children: [
          _SettingsSection(
            title: strings.engineQualityTitle,
            child: Column(
              children: [
                _DepthRangeSettingTile(
                  key: const Key('engine-depth-range'),
                  title: strings.depth,
                  description: strings.depthHelp,
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
                  description: strings.numberOfLinesHelp,
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
                  description: strings.timeLimitHelp,
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
                  subtitle: Text(strings.adaptiveEarlyStopHelp),
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
                  description: strings.threadsHelp,
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
                  description: strings.hashMemoryHelp,
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

