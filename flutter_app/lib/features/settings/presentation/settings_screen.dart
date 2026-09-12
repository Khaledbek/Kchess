part of '../../../ui/app_root.dart';

class SettingsScreen extends StatelessWidget {
  const SettingsScreen({required this.controller, super.key});
  final AppController controller;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    return Scaffold(
      appBar: MediaQuery.sizeOf(context).width >= 900
          ? AppBar(title: Text(strings.settings))
          : null,
      body: ListView(
        padding: const EdgeInsets.fromLTRB(16, 12, 16, 28),
        children: [
          _SettingsCategoryTile(
            key: const Key('settings-category-engine'),
            icon: Icons.memory_outlined,
            title: strings.engine,
            onTap: () => _openSettingsPage(
              context,
              controller,
              () => _EngineSettingsPage(controller: controller),
            ),
          ),
          _SettingsCategoryTile(
            key: const Key('settings-category-analysis'),
            icon: Icons.analytics_outlined,
            title: strings.analysisSettingsTitle,
            onTap: () => _openSettingsPage(
              context,
              controller,
              () => _AnalysisSettingsPage(controller: controller),
            ),
          ),
          _SettingsCategoryTile(
            key: const Key('settings-category-design'),
            icon: Icons.palette_outlined,
            title: strings.designSettingsTitle,
            onTap: () => _openSettingsPage(
              context,
              controller,
              () => _DesignSettingsPage(controller: controller),
            ),
          ),
          _SettingsCategoryTile(
            key: const Key('settings-category-general'),
            icon: Icons.tune_outlined,
            title: strings.generalSettingsTitle,
            onTap: () => _openSettingsPage(
              context,
              controller,
              () => _GeneralSettingsPage(controller: controller),
            ),
          ),
          _SettingsCategoryTile(
            key: const Key('settings-category-data-storage'),
            icon: Icons.storage_outlined,
            title: strings.dataStorageSettingsTitle,
            onTap: () => _openSettingsPage(
              context,
              controller,
              () => _DataStorageSettingsPage(controller: controller),
            ),
          ),
        ],
      ),
    );
  }

  static void _openSettingsPage(
    BuildContext context,
    AppController controller,
    Widget Function() pageBuilder,
  ) {
    Navigator.of(context).push(
      MaterialPageRoute<void>(
        builder: (_) => AnimatedBuilder(
          animation: controller,
          builder: (context, _) => pageBuilder(),
        ),
      ),
    );
  }
}

class _SettingsCategoryTile extends StatelessWidget {
  const _SettingsCategoryTile({
    required this.icon,
    required this.title,
    required this.onTap,
    super.key,
  });

  final IconData icon;
  final String title;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) {
    final scheme = Theme.of(context).colorScheme;
    return Card(
      margin: const EdgeInsets.only(bottom: 10),
      clipBehavior: Clip.antiAlias,
      child: ListTile(
        leading: Container(
          width: 44,
          height: 44,
          decoration: BoxDecoration(
            color: scheme.primaryContainer,
            borderRadius: BorderRadius.circular(12),
          ),
          alignment: Alignment.center,
          child: Icon(icon, color: scheme.onPrimaryContainer),
        ),
        title: Text(title),
        trailing: const Icon(Icons.chevron_right_rounded),
        onTap: onTap,
      ),
    );
  }
}

