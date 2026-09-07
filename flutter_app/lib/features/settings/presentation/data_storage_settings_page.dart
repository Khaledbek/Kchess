part of '../../../ui/app_root.dart';

class _DataStorageSettingsPage extends StatelessWidget {
  const _DataStorageSettingsPage({required this.controller});
  final AppController controller;

  Future<void> _clearEngineCache(BuildContext context) async {
    final strings = AppLocalizations.of(context);
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        title: Text(strings.clearAnalysisCacheQuestion),
        content: Text(strings.clearAnalysisCacheBody),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context, false),
            child: Text(strings.cancelAction),
          ),
          FilledButton(
            onPressed: () => Navigator.pop(context, true),
            child: Text(strings.clearAnalysisCache),
          ),
        ],
      ),
    );
    if (confirmed != true || !context.mounted) return;
    try {
      await controller.clearEngineCache();
      if (context.mounted) {
        ScaffoldMessenger.of(
          context,
        ).showSnackBar(SnackBar(content: Text(strings.analysisCacheCleared)));
      }
    } catch (error) {
      if (context.mounted) {
        ScaffoldMessenger.of(context)
            .showSnackBar(SnackBar(content: Text(error.toString())));
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    return Scaffold(
      appBar: AppBar(title: Text(strings.dataStorageSettingsTitle)),
      body: ListView(
        padding: const EdgeInsets.fromLTRB(16, 12, 16, 28),
        children: [
          _SettingsSection(
            title: strings.analysisCacheTitle,
            child: Column(
              children: [
                SwitchListTile(
                  secondary: const Icon(Icons.cached),
                  title: Text(strings.useGlobalAnalysisCache),
                  subtitle: Text(strings.useGlobalAnalysisCacheHelp),
                  value: controller.settings.useGlobalAnalysisCache,
                  onChanged: controller.setUseGlobalAnalysisCache,
                ),
                const Divider(height: 1),
                ListTile(
                  leading: const Icon(Icons.delete_sweep),
                  title: Text(strings.clearAnalysisCache),
                  subtitle: Text(strings.clearAnalysisCacheHelp),
                  onTap: () => _clearEngineCache(context),
                ),
              ],
            ),
          ),
          const SizedBox(height: 12),
          _SettingsSection(
            title: strings.diagnosticsTitle,
            child: SwitchListTile(
              secondary: const Icon(Icons.bug_report),
              title: Text(strings.diagnosticLogging),
              subtitle: Text(strings.diagnosticLoggingHelp),
              value: controller.settings.diagnosticLogging,
              onChanged: controller.setDiagnosticLogging,
            ),
          ),
        ],
      ),
    );
  }
}

