part of '../../../ui/app_root.dart';

class _DesignSettingsPage extends StatelessWidget {
  const _DesignSettingsPage({required this.controller});
  final AppController controller;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    return Scaffold(
      appBar: AppBar(title: Text(strings.designSettingsTitle)),
      body: ListView(
        padding: const EdgeInsets.fromLTRB(16, 12, 16, 28),
        children: [
          _SettingsSection(
            title: strings.designSettingsTitle,
            child: ListTile(
              leading: const Icon(Icons.contrast),
              title: Text(strings.theme),
              trailing: DropdownButtonHideUnderline(
                child: DropdownButton<AppThemeMode>(
                  value: controller.settings.themeMode,
                  onChanged: (value) {
                    if (value != null) controller.setThemeMode(value);
                  },
                  items: [
                    DropdownMenuItem(
                      value: AppThemeMode.system,
                      child: Text(strings.systemTheme),
                    ),
                    DropdownMenuItem(
                      value: AppThemeMode.light,
                      child: Text(strings.lightTheme),
                    ),
                    DropdownMenuItem(
                      value: AppThemeMode.dark,
                      child: Text(strings.darkTheme),
                    ),
                  ],
                ),
              ),
            ),
          ),
          const SizedBox(height: 12),
          _SettingsSection(
            title: strings.boardDisplayTitle,
            child: Column(
              children: [
                SwitchListTile(
                  secondary: const Icon(Icons.grid_on_outlined),
                  title: Text(strings.showBoardCoordinates),
                  subtitle: Text(strings.showBoardCoordinatesHelp),
                  value: controller.settings.showBoardCoordinates,
                  onChanged: controller.setShowBoardCoordinates,
                ),
                const Divider(height: 1),
                SwitchListTile(
                  secondary: const Icon(Icons.compare_arrows),
                  title: Text(strings.highlightLastMove),
                  subtitle: Text(strings.highlightLastMoveHelp),
                  value: controller.settings.highlightLastMove,
                  onChanged: controller.setHighlightLastMove,
                ),
                const Divider(height: 1),
                SwitchListTile(
                  secondary: const Icon(Icons.touch_app),
                  title: Text(strings.highlightSelectedSquare),
                  subtitle: Text(strings.highlightSelectedSquareHelp),
                  value: controller.settings.highlightSelectedSquare,
                  onChanged: controller.setHighlightSelectedSquare,
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}

