part of '../../../ui/app_root.dart';

class _AnalysisSettingsPage extends StatelessWidget {
  const _AnalysisSettingsPage({required this.controller});
  final AppController controller;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    return Scaffold(
      appBar: AppBar(title: Text(strings.analysisSettingsTitle)),
      body: ListView(
        padding: const EdgeInsets.fromLTRB(16, 12, 16, 28),
        children: [
          _SettingsSection(
            title: strings.analysisBoardGuidance,
            child: Column(
              children: [
                SwitchListTile(
                  key: const Key('show-best-move-arrow'),
                  secondary: const Icon(Icons.trending_flat),
                  title: Text(strings.bestMoveArrow),
                  subtitle: Text(strings.bestMoveArrowHelp),
                  value: controller.settings.showBestMoveArrow,
                  onChanged: controller.setShowBestMoveArrow,
                ),
                const Divider(height: 1),
                SwitchListTile(
                  key: const Key('show-threat-arrow'),
                  secondary: const Icon(Icons.warning_amber_rounded),
                  title: Text(strings.threatArrow),
                  subtitle: Text(strings.threatArrowHelp),
                  value: controller.settings.showThreatArrow,
                  onChanged: controller.setShowThreatArrow,
                ),
                const Divider(height: 1),
                SwitchListTile(
                  key: const Key('show-evaluation-bar'),
                  secondary: const Icon(Icons.stacked_bar_chart),
                  title: Text(strings.evaluationBarSetting),
                  subtitle: Text(strings.evaluationBarSettingHelp),
                  value: controller.settings.showEvaluationBar,
                  onChanged: controller.setShowEvaluationBar,
                ),
              ],
            ),
          ),
          const SizedBox(height: 16),
          _SettingsSection(
            title: strings.analysisInformation,
            child: Column(
              children: [
                SwitchListTile(
                  key: const Key('show-engine-lines'),
                  secondary: const Icon(Icons.account_tree_outlined),
                  title: Text(strings.showEngineLinesSetting),
                  subtitle: Text(strings.showEngineLinesSettingHelp),
                  value: controller.settings.showEngineLines,
                  onChanged: controller.setShowEngineLines,
                ),
                const Divider(height: 1),
                SwitchListTile(
                  key: const Key('show-classifications'),
                  secondary: const Icon(Icons.auto_awesome_outlined),
                  title: Text(strings.showClassificationsSetting),
                  subtitle: Text(strings.showClassificationsSettingHelp),
                  value: controller.settings.showClassifications,
                  onChanged: controller.setShowClassifications,
                ),
                const Divider(height: 1),
                SwitchListTile(
                  key: const Key('show-accuracy'),
                  secondary: const Icon(Icons.percent),
                  title: Text(strings.showAccuracySetting),
                  subtitle: Text(strings.showAccuracySettingHelp),
                  value: controller.settings.showAccuracy,
                  onChanged: controller.setShowAccuracy,
                ),
                const Divider(height: 1),
                SwitchListTile(
                  key: const Key('show-theory'),
                  secondary: const Icon(Icons.menu_book_outlined),
                  title: Text(strings.showTheorySetting),
                  subtitle: Text(strings.showTheorySettingHelp),
                  value: controller.settings.showTheory,
                  onChanged: controller.setShowTheory,
                ),
                const Divider(height: 1),
                SwitchListTile(
                  key: const Key('show-result-symbols'),
                  secondary: const Icon(Icons.emoji_events_outlined),
                  title: Text(strings.showResultSymbolsSetting),
                  subtitle: Text(strings.showResultSymbolsSettingHelp),
                  value: controller.settings.showResultSymbols,
                  onChanged: controller.setShowResultSymbols,
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}

