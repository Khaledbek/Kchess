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
                  subtitle: Text(strings.autoSyncOnlineHelp),
                  value: controller.settings.autoSyncOnline,
                  onChanged: controller.setAutoSyncOnline,
                ),
                const Divider(height: 1),
                SwitchListTile(
                  secondary: const Icon(Icons.delete_outline),
                  title: Text(strings.confirmBeforeDelete),
                  subtitle: Text(strings.confirmBeforeDeleteHelp),
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

