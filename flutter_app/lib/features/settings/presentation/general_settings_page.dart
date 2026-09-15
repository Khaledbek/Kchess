part of '../../../ui/app_root.dart';

class _GeneralSettingsPage extends StatelessWidget {
  const _GeneralSettingsPage({required this.controller});
  final AppController controller;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    return Scaffold(
      appBar: AppBar(title: Text(strings.generalSettingsTitle)),
      body: ListView(
        padding: const EdgeInsets.fromLTRB(16, 12, 16, 28),
        children: [
          _SettingsSection(
            title: strings.language,
            child: ListTile(
              leading: const Icon(Icons.translate),
              title: Text(strings.language),
              trailing: DropdownButtonHideUnderline(
                child: DropdownButton<String>(
                  value: controller.settings.locale,
                  onChanged: (value) {
                    if (value != null) controller.setLocale(value);
                  },
                  items: const [
                    DropdownMenuItem(value: 'de', child: Text('Deutsch')),
                    DropdownMenuItem(value: 'en', child: Text('English')),
                    DropdownMenuItem(value: 'ar', child: Text('العربية')),
                  ],
                ),
              ),
            ),
          ),
          const SizedBox(height: 12),
          _SettingsSection(
            title: strings.behaviorTitle,
            child: Column(
              children: [
                SwitchListTile(
                  secondary: const Icon(Icons.sync),
                  title: Text(strings.autoSyncOnline),
                  value: controller.settings.autoSyncOnline,
                  onChanged: controller.setAutoSyncOnline,
                ),
                const Divider(height: 1),
                _BackgroundAnalysisSwitch(gateway: controller.gateway),
                const Divider(height: 1),
                SwitchListTile(
                  secondary: const Icon(Icons.delete_outline),
                  title: Text(strings.confirmBeforeDelete),
                  value: controller.settings.confirmBeforeDelete,
                  onChanged: controller.setConfirmBeforeDelete,
                ),
              ],
            ),
          ),
          const SizedBox(height: 12),
          _SettingsSection(
            title: strings.licensesAbout,
            child: ListTile(
              leading: const Icon(Icons.info_outline),
              title: Text(strings.licensesAbout),
              subtitle: const Text(
                'KChess 0.1.0 · SQLite 3.53.4 · Stockfish GPLv3',
              ),
            ),
          ),
        ],
      ),
    );
  }
}

/// On/off for background analysis, with how far it has come. The choice is
/// saved natively, next to the queue it controls.
class _BackgroundAnalysisSwitch extends StatefulWidget {
  const _BackgroundAnalysisSwitch({required this.gateway});

  final CoreGateway gateway;

  @override
  State<_BackgroundAnalysisSwitch> createState() => _BackgroundAnalysisSwitchState();
}

class _BackgroundAnalysisSwitchState extends State<_BackgroundAnalysisSwitch> {
  BackgroundAnalysisStatus? _status;
  bool _saving = false;

  @override
  void initState() {
    super.initState();
    unawaited(_load());
  }

  Future<void> _load() async {
    try {
      final status = await widget.gateway.backgroundAnalysisStatus();
      if (mounted) setState(() => _status = status);
    } catch (_) {}
  }

  Future<void> _toggle(bool enabled) async {
    setState(() => _saving = true);
    try {
      await widget.gateway.setBackgroundAnalysisEnabled(enabled);
    } catch (_) {}
    await _load();
    if (mounted) setState(() => _saving = false);
  }

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final status = _status;
    return SwitchListTile(
      key: const Key('settings-background-analysis'),
      secondary: const Icon(Icons.insights_outlined),
      title: Text(strings.backgroundAnalysisSetting),
      subtitle: Text(
        status == null || status.totalGames == 0
            ? strings.backgroundAnalysisSettingSubtitle
            : '${strings.backgroundAnalysisSettingSubtitle}\n'
                '${strings.backgroundAnalysisProgress(status.analysedGames, status.totalGames)}',
      ),
      isThreeLine: status != null && status.totalGames > 0,
      value: status?.enabled ?? true,
      onChanged: status == null || _saving ? null : _toggle,
    );
  }
}
