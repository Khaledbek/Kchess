part of '../../../ui/app_root.dart';

class _ProfileHeader extends StatelessWidget {
  const _ProfileHeader({required this.controller, required this.onOpenProfile});

  final AppController controller;
  final VoidCallback onOpenProfile;

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    final theme = Theme.of(context);
    final scheme = theme.colorScheme;
    final profile = controller.activeProfile!;
    return Padding(
      padding: const EdgeInsets.fromLTRB(14, 18, 14, 10),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Padding(
            padding: const EdgeInsets.only(left: 4, bottom: 16),
            child: Row(
              children: [
                const BrandLogo(size: 30),
                const SizedBox(width: 10),
                const BrandWordmark(fontSize: 21),
              ],
            ),
          ),
          Material(
            color: scheme.surfaceContainerHigh,
            borderRadius: BorderRadius.circular(16),
            child: InkWell(
              onTap: onOpenProfile,
              borderRadius: BorderRadius.circular(16),
              child: Padding(
                padding: const EdgeInsets.all(12),
                child: Row(
                  children: [
                    Container(
                      decoration: BoxDecoration(
                        shape: BoxShape.circle,
                        border: Border.all(
                          color: scheme.outlineVariant,
                          width: 2,
                        ),
                      ),
                      padding: const EdgeInsets.all(2),
                      child: CircleAvatar(
                        radius: 22,
                        backgroundColor: scheme.primaryContainer,
                        child: _profileAvatar(profile),
                      ),
                    ),
                    const SizedBox(width: 12),
                    Expanded(
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          Text(
                            profile.displayName,
                            maxLines: 1,
                            overflow: TextOverflow.ellipsis,
                            style: theme.textTheme.titleMedium,
                          ),
                          const SizedBox(height: 2),
                          Text(
                            _profileTypeLabel(strings, profile.type),
                            style: theme.textTheme.bodySmall?.copyWith(
                              color: scheme.onSurfaceVariant,
                            ),
                          ),
                        ],
                      ),
                    ),
                    Icon(Icons.chevron_right, color: scheme.onSurfaceVariant),
                  ],
                ),
              ),
            ),
          ),
          const SizedBox(height: 8),
          Row(
            children: [
              Expanded(
                child: TextButton.icon(
                  onPressed: () => showDialog<void>(
                    context: context,
                    builder: (context) =>
                        _AddProfileDialog(controller: controller),
                  ),
                  icon: const Icon(Icons.person_add_alt_1, size: 18),
                  label: Text(strings.addAccount),
                ),
              ),
              if (controller.profiles.length > 1)
                Expanded(
                  child: TextButton.icon(
                    onPressed: () => _showProfileSwitcher(context, controller),
                    icon: const Icon(Icons.swap_horiz, size: 18),
                    label: Text(strings.switchAccount),
                  ),
                ),
            ],
          ),
        ],
      ),
    );
  }
}

class _AddProfileDialog extends StatefulWidget {
  const _AddProfileDialog({required this.controller});

  final AppController controller;

  @override
  State<_AddProfileDialog> createState() => _AddProfileDialogState();
}

class _AddProfileDialogState extends State<_AddProfileDialog> {
  final _input = TextEditingController();
  ProfileType _type = ProfileType.chessCom;
  bool _saving = false;
  bool _showRequiredError = false;

  @override
  void dispose() {
    _input.dispose();
    super.dispose();
  }

  Future<void> _save() async {
    if (_input.text.trim().isEmpty) {
      setState(() => _showRequiredError = true);
      return;
    }
    setState(() => _saving = true);
    try {
      await widget.controller.createProfile(_type, _input.text);
      if (mounted) Navigator.pop(context);
    } catch (error) {
      if (mounted) {
        setState(() => _saving = false);
        ScaffoldMessenger.of(context)
            .showSnackBar(SnackBar(content: Text(error.toString())));
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    final strings = AppLocalizations.of(context);
    return AlertDialog(
      title: Text(strings.addAccount),
      content: SizedBox(
        width: 480,
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            DropdownButtonFormField<ProfileType>(
              initialValue: _type,
              decoration: InputDecoration(labelText: strings.provider),
              items: [
                DropdownMenuItem(
                  value: ProfileType.chessCom,
                  child: Text(strings.chessCom),
                ),
                DropdownMenuItem(
                  value: ProfileType.lichess,
                  child: Text(strings.lichess),
                ),
                DropdownMenuItem(
                  value: ProfileType.localPgnFen,
                  child: Text(strings.localPgnFen),
                ),
              ],
              onChanged: (value) {
                if (value != null) setState(() => _type = value);
              },
            ),
            const SizedBox(height: 16),
            TextField(
              key: const Key('additional-profile-input'),
              controller: _input,
              autofocus: true,
              decoration: InputDecoration(
                labelText: _type == ProfileType.localPgnFen
                    ? strings.profileName
                    : strings.username,
                errorText: _showRequiredError ? strings.requiredField : null,
              ),
              onSubmitted: (_) => _save(),
            ),
          ],
        ),
      ),
      actions: [
        TextButton(
          onPressed: _saving ? null : () => Navigator.pop(context),
          child: Text(strings.close),
        ),
        FilledButton(
          key: const Key('create-additional-profile'),
          onPressed: _saving ? null : _save,
          child: Text(strings.continueLabel),
        ),
      ],
    );
  }
}

Future<void> _showProfileSwitcher(
  BuildContext context,
  AppController controller,
) async {
  final selected = await showDialog<AppProfile>(
    context: context,
    builder: (context) => SimpleDialog(
      title: Text(AppLocalizations.of(context).switchAccount),
      children: [
        for (final profile in controller.profiles)
          SimpleDialogOption(
            onPressed: () => Navigator.pop(context, profile),
            child: ListTile(
              leading: const Icon(Icons.account_circle),
              title: Text(profile.displayName),
              trailing: Row(
                mainAxisSize: MainAxisSize.min,
                children: [
                  if (profile.id == controller.activeProfile?.id)
                    const Icon(Icons.check),
                  IconButton(
                    key: Key('delete-profile-${profile.id}'),
                    tooltip: AppLocalizations.of(context).deleteAccount,
                    onPressed: () async {
                      final deleted = await _confirmDeleteProfile(
                        context,
                        controller,
                        profile,
                      );
                      if (deleted && context.mounted) Navigator.pop(context);
                    },
                    icon: const Icon(Icons.delete_outline),
                  ),
                ],
              ),
            ),
          ),
      ],
    ),
  );
  if (selected != null) {
    await controller.switchProfile(selected);
  }
}

Future<bool> _confirmDeleteProfile(
  BuildContext context,
  AppController controller,
  AppProfile profile,
) async {
  final strings = AppLocalizations.of(context);
  if (controller.settings.confirmBeforeDelete) {
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        title: Text(strings.deleteAccountQuestion),
        content: Text(
          profile.type == ProfileType.localPgnFen
              ? strings.deleteLocalProfileBody
              : strings.deleteOnlineProfileBody,
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context, false),
            child: Text(strings.cancelAction),
          ),
          FilledButton(
            key: const Key('confirm-delete-profile'),
            onPressed: () => Navigator.pop(context, true),
            child: Text(strings.deleteAction),
          ),
        ],
      ),
    );
    if (confirmed != true) return false;
  }
  try {
    await controller.deleteProfile(profile);
    return true;
  } catch (error) {
    if (context.mounted) {
      ScaffoldMessenger.of(context)
          .showSnackBar(SnackBar(content: Text(error.toString())));
    }
    return false;
  }
}

String _profileMergeText(
  BuildContext context, {
  required String de,
  required String en,
  required String ar,
}) => switch (Localizations.localeOf(context).languageCode) {
  'ar' => ar,
  'en' => en,
  _ => de,
};

Future<bool> _showMergeLocalProfileDialog(
  BuildContext context,
  AppController controller,
  AppProfile source,
) async {
  final targets = controller.profiles
      .where((profile) => profile.type != ProfileType.localPgnFen)
      .toList(growable: false);
  if (targets.isEmpty) return false;

  final target = await showDialog<AppProfile>(
    context: context,
    builder: (context) => SimpleDialog(
      title: Text(
        _profileMergeText(
          context,
          de: 'Mit Online-Profil zusammenführen',
          en: 'Merge with online profile',
          ar: 'دمج مع ملف شخصي عبر الإنترنت',
        ),
      ),
      children: [
        for (final profile in targets)
          SimpleDialogOption(
            onPressed: () => Navigator.pop(context, profile),
            child: ListTile(
              leading: const Icon(Icons.merge_type),
              title: Text(profile.displayName),
              subtitle: Text(
                _profileTypeLabel(AppLocalizations.of(context), profile.type),
              ),
            ),
          ),
      ],
    ),
  );
  if (target == null || !context.mounted) return false;

  final gameCount = controller.games.length;
  final confirmed = await showDialog<bool>(
    context: context,
    builder: (context) => AlertDialog(
      title: Text(
        _profileMergeText(
          context,
          de: 'Lokales Profil zusammenführen?',
          en: 'Merge local profile?',
          ar: 'دمج الملف الشخصي المحلي؟',
        ),
      ),
      content: Text(
        _profileMergeText(
          context,
          de: '$gameCount lokale Partien/Positionen werden zu „${target.displayName}“ verschoben. Doppelte Partien werden erkannt; Favoriten, Sammlungen und die beste vorhandene Analyse bleiben erhalten. Das lokale Profil wird danach entfernt.',
          en: '$gameCount local games/positions will be moved to “${target.displayName}”. Duplicate games are detected; favorites, collections and the best available analysis are kept. The local profile is removed afterwards.',
          ar: 'سيتم نقل $gameCount من المباريات/الوضعيات المحلية إلى «${target.displayName}». سيتم اكتشاف المباريات المكررة مع الاحتفاظ بالمفضلة والمجموعات وأفضل تحليل متاح. بعد ذلك سيتم حذف الملف الشخصي المحلي.',
        ),
      ),
      actions: [
        TextButton(
          onPressed: () => Navigator.pop(context, false),
          child: Text(AppLocalizations.of(context).cancelAction),
        ),
        FilledButton(
          onPressed: () => Navigator.pop(context, true),
          child: Text(
            _profileMergeText(
              context,
              de: 'Zusammenführen',
              en: 'Merge',
              ar: 'دمج',
            ),
          ),
        ),
      ],
    ),
  );
  if (confirmed != true) return false;

  try {
    await controller.mergeLocalProfile(source, target);
    if (context.mounted) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text(
            _profileMergeText(
              context,
              de: 'Lokales Profil wurde mit „${target.displayName}“ zusammengeführt.',
              en: 'Local profile was merged into “${target.displayName}”.',
              ar: 'تم دمج الملف الشخصي المحلي مع «${target.displayName}».',
            ),
          ),
        ),
      );
      Navigator.of(context).pop();
    }
    return true;
  } catch (error) {
    if (context.mounted) {
      ScaffoldMessenger.of(context)
          .showSnackBar(SnackBar(content: Text(error.toString())));
    }
    return false;
  }
}

String _profileTypeLabel(AppLocalizations strings, ProfileType type) =>
    switch (type) {
      ProfileType.chessCom => strings.chessCom,
      ProfileType.lichess => strings.lichess,
      ProfileType.localPgnFen => strings.localPgnFen,
    };

Widget _profileAvatar(AppProfile profile, {double iconSize = 24}) {
  Widget fallback() => Image.asset(
    '../img/${profile.avatarAsset}',
    errorBuilder: (_, _, _) => Icon(Icons.person, size: iconSize),
  );
  final file = profile.avatarFile;
  if (file == null || file.isEmpty) return fallback();
  return Image.file(
    File(file),
    fit: BoxFit.cover,
    errorBuilder: (_, _, _) => fallback(),
  );
}

Future<void> _openAnalysisAndRefreshSettings({
  required BuildContext context,
  required AppController controller,
  required GameSummary game,
}) async {
  await openAnalysisWorkflow(
    context: context,
    gateway: controller.gateway,
    game: game,
    settings: controller.settings,
  );
  await controller.reloadSettings();
  await controller.refreshAfterAnalysis();
}

